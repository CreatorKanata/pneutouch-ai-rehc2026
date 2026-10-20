"""Lumped-parameter pneumatic simulator for the PneuTouch dinosaur.

Which internal structure makes the single-sensor 40 SPS waveform most
distinguishable by touch location, scored with the firmware's 12 features?

  python tools/pneumatic_sim.py run --out docs/analysis/2026-10-15-pneumatic-sim/
  python tools/pneumatic_sim.py calibrate
  python tools/pneumatic_sim.py custom --topology star --hole-d 0.8 --hole-L 10

Requires numpy and matplotlib (requirements-analysis.txt). Simulated data only.
"""
import argparse
from dataclasses import asdict
from pathlib import Path
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.pneusim import calibrate, defaults, physics, report, scenarios, study  # noqa: E402

NOTES = (
    'Walls are quasi-static (massless): c x\' = F - k x - A p; inertance and wall mass are ignored.',
    'Wall damping is direction dependent (c_out = c_out_ratio * c_in while the wall springs back). '
    'This "snap-back" term is an addition to the plan: with symmetric damping the best calibration '
    f'residual is {defaults.SYMMETRIC_DAMPING_RESIDUAL:.3f} versus {defaults.BASELINE.calibration_residual:.3f}, '
    'because a linear model cannot give both a 3.4x larger trough and a faster (50 ms vs 240 ms) recovery.',
    'Wall area, stiffness and finger force are only identifiable together (displaced volume A F / k '
    'and time constant c / k); individual values are not physical measurements.',
    'Gas is adiabatic and linearised (C = V / (gamma p0)) with no heat exchange; the sensor tube volume is '
    'folded into its chamber. The linearisation is stretched by the release snap-back: a 1 kHz check '
    '(5 presses per zone) found pressed-chamber minima of about -12 kPa (series_current), -25 kPa '
    '(series_thin) and -73 kPa (star_orifice), so star_orifice troughs are qualitative only.',
    'Channels use Poiseuille + orifice loss solved as one quadratic, quasi-steady (no inertance or entry development).',
    'Integration is linearly-implicit Euler at dt = 0.2 ms (channel time constants reach nanoseconds, '
    'so explicit RK4 would be unstable); 40 SPS samples are instantaneous values every 25 ms.',
    'Every zone uses one wall with the same area. The mech_* scenarios keep series_current connectivity '
    'and change per-zone k, c and c_out ratio (multipliers in parameters.json); mechanical_encoding is '
    'an alias of mech_tau4.',
    'Leg channel grading in star_graded follows series order RH, RF, LF, LH.',
    'Accuracy is computed on valid (complete) events only; INVALID events are reported separately.',
)


def parse_volumes(text):
    base = dict(defaults.VOLUMES_CM3)
    if text:
        for item in text.split(','):
            key, value = item.split('=')
            if key.strip() not in base:
                raise SystemExit(f'unknown chamber {key!r}; choose from {sorted(base)}')
            base = {**base, key.strip(): float(value)}
    return tuple(base.items())


def baseline_from(args):
    return defaults.BASELINE.with_(volumes_cm3=parse_volumes(args.volumes),
                                   counts_per_pa=args.counts_per_pa, noise_counts=args.noise)


def describe_network(net):
    return dict(chambers=[asdict(c) for c in net.chambers],
                channels=[dict(asdict(c), r=c.r, k=c.k) for c in net.channels],
                walls=[asdict(w) for w in net.walls], leak=asdict(net.leak) if net.leak else None, sensor=net.sensor)


def calibration_trace(base):
    cand = {'leak_r': np.array([base.leak_r]), 'leak_k': np.array([base.leak_k]),
            'wall_k': np.array([base.wall_k]), 'tau_in': np.array([base.wall_c / base.wall_k]),
            'c_out_ratio': np.array([base.c_out_ratio]), 'wall_area': np.array([base.wall_area]),
            'press_rise': np.array([base.press_rise]), 'press_hold': np.array([base.press_hold]),
            'press_release': np.array([base.press_release]), 'f0': np.array([base.f0])}
    real = calibrate.real_trace()
    sim = calibrate.simulate_candidates(cand, base)
    rms, _, shift = calibrate.score(sim, real, fixed_scale=True)
    return real, calibrate.aligned(sim[0], shift[0], len(real)), float(rms[0])


def evaluate(name, net, base, rng, out, per_zone):
    campaign = study.run_campaign(net, base, rng, per_zone)
    sep = study.separability(campaign)
    if out:
        report.plot_waveforms(out / f'waveforms-{name}.png', name, campaign)
        report.plot_features(out / f'features-{name}.png', name, sep)
    return sep


def evaluate_with_high_rate(name, net, base, rng, out, per_zone):
    """One 1 kHz simulation serves both the 40 SPS features and the rise-time table."""
    fine = study.run_campaign(net, base, rng, per_zone, record_dt=0.001, record_all=True, noise=False)
    stride = 25
    counts = fine.counts[:, ::stride] + rng.normal(0.0, base.noise_counts, fine.counts[:, ::stride].shape)
    coarse = study.Campaign(fine.time[::stride], fine.zones, counts, None)
    sep = study.separability(coarse)
    if out:
        report.plot_waveforms(out / f'waveforms-{name}.png', name, coarse)
        report.plot_features(out / f'features-{name}.png', name, sep)
    return sep, study.high_rate_table(fine, net)


def public(sep):
    return {k: v for k, v in sep.items() if k not in ('x', 'zones')}


def cmd_run(args):
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    base = baseline_from(args)
    rows, high_rate, nets = {}, {}, {}
    for i, name in enumerate(scenarios.FEATURE_SCENARIOS):
        rng = np.random.default_rng([args.seed, i])
        nets[name] = scenarios.build(name, base)
        if name in scenarios.HIGH_RATE_SCENARIOS.values():
            high_name = next(k for k, v in scenarios.HIGH_RATE_SCENARIOS.items() if v == name)
            rows[name], high_rate[high_name] = evaluate_with_high_rate(name, nets[name], base, rng, out, args.per_zone)
        else:
            rows[name] = evaluate(name, nets[name], base, rng, out, args.per_zone)
    if args.topology:
        nets['custom'] = scenarios.custom(base, args.topology, args.hole_d, args.hole_L, args.wall_k_scale)
        rows['custom'] = evaluate('custom', nets['custom'], base, np.random.default_rng([args.seed, 99]),
                                  out, args.per_zone)
    real, model, rms = calibration_trace(base)
    report.plot_calibration(out / 'calibration.png', real, model, rms)
    notes = NOTES + (f'Calibration residual recomputed for the stored baseline: {rms:.3f}.',)
    (out / 'summary.md').write_text(report.summary_markdown(rows, high_rate, base, notes), encoding='utf-8')
    report.write_parameters(out / 'parameters.json', dict(
        seed=args.seed, events_per_zone=args.per_zone, sample_rate_sps=40, dt_s=0.0002,
        event_duration_s=study.EVENT_S, quiet_before_press_s=study.QUIET_S, press_ranges=study.TIMING,
        physics=dict(gamma=physics.GAMMA, p0=physics.P0, mu=physics.MU, rho=physics.RHO, cd=physics.CD,
                     end_correction=physics.END_CORRECTION),
        baseline=base.as_dict(), calibration=dict(source=defaults.CALIBRATION_SOURCE, residual=rms,
                                                  symmetric_damping_residual=defaults.SYMMETRIC_DAMPING_RESIDUAL),
        mechanical_designs={k: {n: dict(k_mult=v[0], c_mult=v[1], c_out_ratio=v[2]) for n, v in d.items()}
                            for k, d in scenarios.MECH_DESIGNS.items()},
        zone_chamber=dict(scenarios.ZONE_CHAMBER),
        networks={k: describe_network(v) for k, v in nets.items()},
        results={k: public(v) for k, v in rows.items()}, high_rate=high_rate, notes=list(notes)))
    sys.stdout.write((out / 'summary.md').read_text(encoding='utf-8'))


def cmd_calibrate(args):
    base = baseline_from(args)
    best, rms, _, _ = calibrate.fit(base, seed=args.seed)
    _, sym_rms, _, _ = calibrate.fit(base, seed=args.seed, fixed={'c_out_ratio': 1.0})
    fitted = calibrate.to_baseline(best, rms, base)
    sys.stdout.write(f'fitted: {fitted}\nresidual: {rms:.4f}\n'
                     f'residual with symmetric damping (c_out_ratio=1): {sym_rms:.4f}\n'
                     'Copy the fitted values into tools/pneusim/defaults.py to adopt them.\n')


def cmd_custom(args):
    base = baseline_from(args)
    out = Path(args.out) if args.out else None
    if out:
        out.mkdir(parents=True, exist_ok=True)
    net = scenarios.custom(base, args.topology or 'series', args.hole_d, args.hole_L, args.wall_k_scale)
    sep = evaluate('custom', net, base, np.random.default_rng([args.seed, 99]), out, args.per_zone)
    table = [line for line in report.summary_markdown({'custom': sep}, {}, base, ()).splitlines()
             if line.startswith('|')]
    sys.stdout.write('\n'.join(table) + '\n')


def parser():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='command', required=True)
    for name, func in (('run', cmd_run), ('calibrate', cmd_calibrate), ('custom', cmd_custom)):
        s = sub.add_parser(name)
        s.set_defaults(func=func)
        s.add_argument('--out', default='docs/analysis/2026-10-15-pneumatic-sim/' if name == 'run' else None)
        s.add_argument('--seed', type=int, default=0)
        s.add_argument('--per-zone', type=int, default=40)
        s.add_argument('--counts-per-pa', type=float, default=350.0)
        s.add_argument('--noise', type=float, default=3000.0, help='ADC noise sigma [counts]')
        s.add_argument('--volumes', default='', help='e.g. back=25,tail=8,head=12 [cm^3]')
        s.add_argument('--topology', choices=('series', 'star'),
                       help='custom network (run: adds a "custom" row)')
        s.add_argument('--hole-d', type=float, default=2.5, help='custom hole diameter [mm]')
        s.add_argument('--hole-L', type=float, default=1.5, help='custom hole length [mm]')
        s.add_argument('--wall-k-scale', type=float, default=1.0)
    return p


if __name__ == '__main__':
    arguments = parser().parse_args()
    arguments.func(arguments)
