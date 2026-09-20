"""Numerical checks for the offline study, separate from hardware tests."""
import importlib.util
import json
from pathlib import Path
import unittest
import sys

try:
    import numpy as np
except ImportError as exc:
    raise unittest.SkipTest('Offline analysis tests require requirements-analysis.txt') from exc

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('analysis', ROOT/'tools/analyze_pressure.py')
a = importlib.util.module_from_spec(spec)
spec.loader.exec_module(a)
sys.path.insert(0,str(ROOT/'tools'))
from replay_pressure_events import Detector


class PressureAnalysisTests(unittest.TestCase):
    def test_linear_crossing_and_integral(self):
        t = np.array([0., 1., 2.])
        z = 2*t
        self.assertAlmostEqual(a.crossing(t,z,1.,0,2,True), .5)
        self.assertIsNone(a.crossing(t,z,1.,0,2,False))
        self.assertAlmostEqual(a.integral(t,z,.25,1.75), 3.)

    def test_known_exponential_decay(self):
        t = np.arange(0,1,.025)
        tau, r2 = a.decay_fit(t,np.exp(-t/.15),.05,.4)
        self.assertAlmostEqual(tau,150.)
        self.assertAlmostEqual(r2,1.)

    def test_shape_invariance_to_gain_and_offset(self):
        # Re-extract actual traces with transformed counts, not just rescale
        # existing features. The detector threshold scales with gain as well.
        manifest = json.loads((a.DEFAULT/'manifest.json').read_text(encoding='utf8'))
        for meta in [manifest['captures'][0], manifest['captures'][6]]:
            _, _, _, t, raw = a.load(a.DEFAULT/meta['file'])
            original = a.extract(meta,t,raw)
            transformed = a.extract(meta,t,2*raw+123456,threshold=100000)
            self.assertEqual(len(original),len(transformed))
            for first, second in zip(original,transformed):
                self.assertEqual(first['complete'],second['complete'])
                if first['complete']:
                    for key in a.POS_SHAPE+a.NEG_SHAPE+a.RATIOS:
                        self.assertAlmostEqual(first[key],second[key],places=7,msg=key)
                    self.assertAlmostEqual(second['peak_counts'],2*first['peak_counts'])

    def test_training_scale_does_not_use_test_batch(self):
        # An unrelated extreme test point must not change the first prediction.
        tr=np.array([[0.,0.],[1.,4.],[8.,2.],[9.,5.]])
        labels=np.array([1,1,2,2])
        one=np.array([[2.,3.]])
        for model in ['centroid','1nn']:
            p=a.predict(tr,labels,one,model)[0]
            q=a.predict(tr,labels,np.r_[one,[[1e12,-1e12]]],model)[0]
            self.assertEqual(p,q)

    def test_recording_boundary_is_excluded(self):
        manifest = json.loads((a.DEFAULT/'manifest.json').read_text(encoding='utf8'))
        meta = manifest['captures'][1]
        _,_,_,t,raw = a.load(a.DEFAULT/meta['file'])
        first=a.extract(meta,t,raw)[0]
        self.assertFalse(first['complete'])
        self.assertEqual(first['exclusion'],'recording_boundary')

    def test_detector_rejects_single_sample_spike(self):
        d=Detector()
        for i in range(120):
            d.step(i*.025,1000000+(100000 if i==50 else 0))
        self.assertIsNone(d.active)
        self.assertEqual(d.events,[])

    def test_detector_waits_for_release_and_times_out(self):
        d=Detector()
        for i in range(260):
            d.step(i*.025,1000000+(200000 if 60<=i<100 else 0))
        self.assertEqual(len(d.events),1)
        self.assertFalse(d.events[0]['negative_seen'])
        self.assertEqual(d.events[0]['completion'],'timeout')

    def test_detected_prefix_is_unchanged_by_later_samples(self):
        t=np.arange(0,8,.025)
        raw=1000000+300000*np.exp(-((t-2)/.15)**2)-400000*np.exp(-((t-3)/.15)**2)
        outputs=[]
        for variant in [raw,np.where(t>5,raw+9000000,raw)]:
            d=Detector()
            for tt,yy in zip(t,variant):
                d.step(float(tt),float(yy))
            outputs.append(d.events[0])
        self.assertEqual(outputs[0],outputs[1])
        self.assertEqual(outputs[0]['completion'],'negative_then_quiet')
        self.assertEqual(len(outputs[0]['prefix']),33)


if __name__=='__main__':
    unittest.main()
