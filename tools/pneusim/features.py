"""Bridge to the shared 12-feature extraction and nearest-centroid scoring.

Feature order and definitions come from tools/analyze_pressure.py, which mirrors
src/pneutouch-solist/S_PneuTouch/pressure_features.c (f[0]..f[11]).
"""
import importlib.util
from pathlib import Path

import numpy as np

_PATH = Path(__file__).resolve().parents[1] / 'analyze_pressure.py'
_spec = importlib.util.spec_from_file_location('pneusim_analyze_pressure', _PATH)
analysis = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(analysis)

FEATURES = tuple(analysis.POS_SHAPE + analysis.NEG_SHAPE + analysis.RATIOS)
assert len(FEATURES) == 12


def event_features(t, y, zone):
    """Return the 12 firmware features of the first complete event, or None (INVALID).

    detect() already enforces the >= 100,000 count positive peak and extract()
    the >= 30,000 count trough, matching the firmware acceptance rules.
    """
    meta = dict(file='simulated', kind='seven_zone', zone_id=zone, label_basis='simulated')
    try:
        events = analysis.extract(meta, np.asarray(t, float), np.asarray(y, float))
    except AssertionError:
        return None
    complete = [e for e in events if e['complete']]
    if not complete:
        return None
    return np.array([complete[0][k] for k in FEATURES], dtype=float)


def loo_predictions(x, labels):
    """Leave-one-out nearest-centroid predictions; scaling fit on each training fold."""
    ids = np.arange(len(labels))
    return np.array([analysis.predict(x[ids != i], labels[ids != i], x[i:i + 1], 'centroid', True)[0]
                     for i in ids])


def loo_accuracy(x, labels):
    if len(np.unique(labels)) < 2:
        return float('nan')
    return float(np.mean(loo_predictions(x, labels) == labels))


def eta_squared(x, labels):
    """Between-zone share of variance for each (log) feature column."""
    total = np.sum((x - x.mean(axis=0))**2, axis=0)
    between = sum(np.sum(labels == k) * (x[labels == k].mean(axis=0) - x.mean(axis=0))**2
                  for k in np.unique(labels))
    return between / np.where(total > 0, total, 1.0)
