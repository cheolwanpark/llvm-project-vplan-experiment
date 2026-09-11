import copy
import unittest

import evaluate as ev


class EvaluationTests(unittest.TestCase):
    def setUp(self):
        self.measurements, candidates = [], []
        kernels = ev.GROUPS['core'] + ev.GROUPS['validation_informed']
        for k in kernels:
            for vf, cycles in zip(ev.VFS, (400, 200, 100, 50)):
                self.measurements.append(dict(kernel=k, vf=vf, raw_roi_cycles=cycles))
                candidates.append(dict(kernel=k, vf=vf, runtime_vf=2 * vf,
                                       legacy_cost=cycles * 2 * vf, tp=cycles * 2 * vf,
                                       lcd=0, final_score=cycles))
        self.data = dict(candidates=candidates, automatic={k: 16 for k in kernels})

    def result(self, mode='max_tp_lcd'):
        result = ev.evaluate(self.data, self.measurements)
        return next(r for r in result['summary'] if r['group'] == 'core' and r['mode'] == mode)

    def test_perfect_and_useful_denominator(self):
        result = self.result()
        for field in ('regret_geomean', 'regret_worst', 'all_pair_accuracy',
                      'non_tie_pair_accuracy', 'relative_error_geomean', 'relative_error_p90'):
            self.assertEqual(result[field], 1)
        rows = ev.evaluate(self.data, self.measurements)['relative']
        self.assertEqual(rows[0]['cycles_per_element'], 400 / 131072)

    def test_missing_candidate_invalidates_aggregate(self):
        self.data['candidates'].pop(0)
        result = self.result()
        self.assertIsNone(result['regret_geomean'])
        self.assertIsNone(result['non_tie_pair_accuracy'])
        self.assertIsNone(result['relative_error_geomean'])
        self.assertEqual(result['selection_coverage'], '6/7')

    def test_predicted_tie_is_wrong_and_selects_small_vf(self):
        for row in self.data['candidates']:
            row['final_score'] = 1
        result = self.result()
        self.assertEqual(result['non_tie_pair_accuracy'], 0)
        self.assertEqual(result['selections']['s000'], 2)
        self.assertAlmostEqual(result['regret_geomean'], 8)

    def test_near_tie_excluded_only_from_primary_ranking(self):
        self.measurements[1]['raw_roi_cycles'] = 399
        self.data['candidates'][1]['final_score'] = 401
        result = self.result()
        self.assertEqual(result['non_tie_pair_accuracy'], 1)
        self.assertLess(result['all_pair_accuracy'], 1)

    def test_outside_actual_selection_and_distinct_argmin(self):
        self.data['automatic']['s000'] = 1
        self.assertIsNone(self.result('automatic')['regret_geomean'])
        self.assertEqual(self.result()['regret_geomean'], 1)

    def test_zero_lcd_scale_is_undefined(self):
        result = self.result('lcd_only')
        self.assertIsNone(result['relative_error_geomean'])
        self.assertEqual(result['relative_coverage'], '0/21')
        self.assertEqual(result['selections']['s000'], 2)

    def test_missing_measurement_and_invalid_cost(self):
        self.measurements.pop(0)
        self.assertIsNone(self.result()['regret_geomean'])
        self.setUp()
        self.data['candidates'][0]['final_score'] = float('nan')
        self.assertIsNone(self.result()['regret_geomean'])

    def test_duplicate_candidate_rejected(self):
        self.data['candidates'].append(copy.copy(self.data['candidates'][0]))
        with self.assertRaises(ValueError):
            self.result()

    def test_interpolated_p90(self):
        self.assertAlmostEqual(ev.p90([1, 2, 3, 4]), 3.7)

    def test_saturn_cannot_reuse_xiangshan_measurements(self):
        self.data['candidates'][0]['profile'] = 'saturn'
        with self.assertRaisesRegex(ValueError, 'XiangShan only'):
            self.result()


if __name__ == '__main__':
    unittest.main()
