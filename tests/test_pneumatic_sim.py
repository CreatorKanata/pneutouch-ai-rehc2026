"""Checks for the lumped-parameter pneumatic simulator (tools/pneusim)."""
from pathlib import Path
import sys
import unittest

try:
    import numpy as np
except ImportError as exc:
    raise unittest.SkipTest('Pneumatic simulator tests require numpy (requirements-analysis.txt)') from exc

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.pneusim import calibrate, physics, network, simulate, scenarios, defaults, features  # noqa: E402


def single_chamber(leak_r=None, leak_k=0.0, c=0.05):
    wall = network.Wall(chamber=0, zone=1, area=1e-4, k=1000.0, c_in=c, c_out=c)
    leak = None if leak_r is None else network.Leak(chamber=0, r=leak_r, k=leak_k)
    return network.Network(chambers=(network.Chamber('one', 50e-6),), channels=(),
                           walls=(wall,), leak=leak, sensor=0)


def held_press(force=2.0):
    return simulate.Press(zone=1, start=0.1, rise=0.1, hold=1.5, release=0.1, force=force)


class FormulaTests(unittest.TestCase):
    def test_laminar_resistance_hand_value(self):
        self.assertAlmostEqual(physics.laminar_resistance(1.2e-3, 35e-3) / 1.30e7, 1.0, delta=0.01)

    def test_gas_compliance_hand_value(self):
        self.assertAlmostEqual(physics.gas_compliance(15e-6) / 1.06e-10, 1.0, delta=0.01)

    def test_orifice_coefficient(self):
        d = 1e-3
        area = np.pi * d**2 / 4
        self.assertAlmostEqual(physics.orifice_coefficient(d), 1.2 / (2 * 0.6**2 * area**2))

    def test_flow_inverts_quadratic_both_signs(self):
        r, k = 1.3e7, 2.0e12
        for dp in (-5000.0, -1.0, 0.0, 3.0, 2500.0):
            q = physics.flow(np.array(dp), r, k)
            self.assertAlmostEqual(float(r * q + k * q * abs(q)), dp, delta=1e-9 * max(1, abs(dp)))
            self.assertEqual(np.sign(q), np.sign(dp))

    def test_flow_without_orifice_term_is_linear(self):
        self.assertAlmostEqual(float(physics.flow(np.array(100.0), 1e7, 0.0)), 1e-5)


class DynamicsTests(unittest.TestCase):
    def run_single(self, net):
        result = simulate.simulate(net, [held_press()], duration=2.2, record_dt=0.005)
        return result.time, result.sensor_pa[0]

    def test_no_leak_holds_pressure_at_static_equilibrium(self):
        net = single_chamber()
        t, p = self.run_single(net)
        hold = (t > 0.5) & (t < 1.6)
        self.assertLess(np.ptp(p[hold]) / np.max(p), 1e-3)
        c_gas = physics.gas_compliance(50e-6)
        expected = 1e-4 * 2.0 / (1000.0 * c_gas + 1e-8)
        self.assertAlmostEqual(float(np.mean(p[hold])) / expected, 1.0, delta=0.01)

    def test_leak_decays_pressure_during_hold(self):
        net = single_chamber(leak_r=5e8, leak_k=1e13)
        t, p = self.run_single(net)
        peak = int(np.argmax(p))
        late = int(np.searchsorted(t, 1.6))
        self.assertLess(p[late], 0.1 * p[peak])
        self.assertLess(np.min(p), -0.5 * p[peak])  # release pulls a vacuum

    def test_batch_members_are_independent(self):
        net = single_chamber(leak_r=5e8)
        both = simulate.simulate(net, [held_press(1.0), held_press(3.0)], duration=2.2, record_dt=0.005)
        alone = simulate.simulate(net, [held_press(1.0)], duration=2.2, record_dt=0.005)
        np.testing.assert_allclose(both.sensor_pa[0], alone.sensor_pa[0])


class ScenarioTests(unittest.TestCase):
    def test_series_current_matches_calibration_peak(self):
        net = scenarios.build('series_current', defaults.BASELINE)
        base = defaults.BASELINE
        press = simulate.Press(zone=1, start=0.5, rise=base.press_rise, hold=base.press_hold,
                               release=base.press_release, force=base.f0)
        result = simulate.simulate(net, [press], duration=3.0, record_dt=0.025)
        peak = np.max(result.sensor_pa[0]) * base.counts_per_pa
        self.assertAlmostEqual(peak / defaults.CALIBRATION_PEAK_COUNTS, 1.0, delta=0.3)

    def test_every_scenario_builds_seven_zones(self):
        for name in scenarios.FEATURE_SCENARIOS:
            net = scenarios.build(name, defaults.BASELINE)
            self.assertEqual(sorted(w.zone for w in net.walls), list(range(1, 8)))


    def test_custom_topologies_and_volume_override(self):
        base = defaults.BASELINE.with_(volumes_cm3=(('back', 30.0),) + defaults.VOLUMES_CM3[1:])
        star = scenarios.custom(base, 'star', 0.8, 10.0, wall_k_scale=2.0)
        self.assertEqual(len(star.channels), 6)
        self.assertEqual(star.chambers[star.sensor].name, 'back')
        self.assertAlmostEqual(star.chambers[star.sensor].volume_m3, 30e-6)
        self.assertAlmostEqual(star.walls[0].k, 2.0 * base.wall_k)
        chain = scenarios.custom(base, 'series', 1.0, 5.0)
        self.assertEqual(chain.chambers[chain.sensor].name, 'tail')
        with self.assertRaises(ValueError):
            scenarios.custom(base, 'ring', 1.0, 1.0)

    def test_tau_designs_scale_damping_not_stiffness(self):
        base = defaults.BASELINE
        net = scenarios.build('mech_tau4', base)
        head, tail = net.walls[net.wall_index(7)], net.walls[net.wall_index(1)]
        self.assertAlmostEqual(head.k, base.wall_k)
        self.assertAlmostEqual(head.c_in / head.k, 4.0 * base.wall_c / base.wall_k)
        self.assertAlmostEqual(tail.c_in / tail.k, 0.25 * base.wall_c / base.wall_k)
        self.assertAlmostEqual(head.c_out, head.c_in * base.c_out_ratio)
        self.assertIsNotNone(net.leak)

    def test_snap_designs_set_per_zone_release_ratio(self):
        net = scenarios.build('mech_tau7_snap', defaults.BASELINE)
        ratios = {w.zone: w.c_out / w.c_in for w in net.walls}
        self.assertAlmostEqual(ratios[7], 0.3)
        self.assertAlmostEqual(ratios[1], 0.02)
        self.assertEqual([round(ratios[z], 2) for z in (3, 4, 5, 6)], [0.03, 0.05, 0.08, 0.12])
        taus = {w.zone: w.c_in / w.k for w in net.walls}
        base_tau = defaults.BASELINE.wall_c / defaults.BASELINE.wall_k
        self.assertEqual([round(taus[z] / base_tau, 2) for z in (3, 4, 5, 6)], [0.7, 1.0, 1.4, 2.0])

    def test_k_only_design_keeps_tau_and_sealed_drops_leak(self):
        base = defaults.BASELINE
        net = scenarios.build('mech_k_only', base)
        for w in net.walls:
            self.assertAlmostEqual(w.c_in / w.k, base.wall_c / base.wall_k)
        self.assertGreater(len({round(w.k, 6) for w in net.walls}), 1)
        self.assertIsNone(scenarios.build('mech_tau4_sealed', base).leak)
        alias = scenarios.build('mechanical_encoding', base)
        self.assertEqual(alias.walls, scenarios.build('mech_tau4', base).walls)


class CliTests(unittest.TestCase):
    def test_parameters_describe_sealed_network_and_volume_override(self):
        from tools import pneumatic_sim
        described = pneumatic_sim.describe_network(scenarios.build('mech_tau4_sealed', defaults.BASELINE))
        self.assertIsNone(described['leak'])
        self.assertEqual(len(described['walls']), 7)
        volumes = dict(pneumatic_sim.parse_volumes('back=30,head=10'))
        self.assertEqual((volumes['back'], volumes['head'], volumes['tail']), (30.0, 10.0, 8.0))


class CalibrationTests(unittest.TestCase):
    def test_stored_baseline_reproduces_documented_residual(self):
        base = defaults.BASELINE
        cand = {k: np.array([v]) for k, v in dict(
            leak_r=base.leak_r, leak_k=base.leak_k, wall_k=base.wall_k, tau_in=base.wall_c / base.wall_k,
            c_out_ratio=base.c_out_ratio, wall_area=base.wall_area, press_rise=base.press_rise,
            press_hold=base.press_hold, press_release=base.press_release, f0=base.f0).items()}
        sim = calibrate.simulate_candidates(cand, base)
        rms, _, _ = calibrate.score(sim, calibrate.real_trace(), fixed_scale=True)
        self.assertAlmostEqual(float(rms[0]), base.calibration_residual, delta=0.005)


class FeatureTests(unittest.TestCase):
    def test_synthetic_event_yields_twelve_positive_features(self):
        t = np.arange(0, 4, 0.025)
        y = 300000 * np.exp(-((t - 1.0) / 0.15)**2) - 400000 * np.exp(-((t - 2.0) / 0.1)**2)
        vec = features.event_features(t, y, zone=1)
        self.assertEqual(len(vec), 12)
        self.assertTrue(np.all(vec > 0))

    def test_small_event_is_invalid(self):
        t = np.arange(0, 4, 0.025)
        y = 50000 * np.exp(-((t - 1.0) / 0.15)**2) - 400000 * np.exp(-((t - 2.0) / 0.1)**2)
        self.assertIsNone(features.event_features(t, y, zone=1))

    def test_nearest_centroid_loo_separates_clusters(self):
        rng = np.random.default_rng(0)
        x = np.r_[rng.normal(0, 0.1, (10, 3)), rng.normal(5, 0.1, (10, 3))]
        labels = np.r_[np.ones(10, int), 2 * np.ones(10, int)]
        self.assertEqual(features.loo_accuracy(x, labels), 1.0)


if __name__ == '__main__':
    unittest.main()
