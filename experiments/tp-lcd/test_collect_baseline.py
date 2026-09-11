import unittest

import collect_baseline as cb


class BaselineParserTests(unittest.TestCase):
    def test_select_function_and_single_cost_evaluation(self):
        log = """
LV: Checking a loop in 'builder_main' from test.c:1
LV: Found an estimated cost of 99 for VF vscale x 2 For instruction: ignored
LV: Checking a loop in 'selected_kernel' from test.c:2
LV: Found an estimated cost of 2 for VF vscale x 2 For instruction: load
LV: Found an estimated cost of 3 for VF vscale x 2 For instruction: store
LV: Using user VF vscale x 2.
LV: Found an estimated cost of 55 for VF vscale x 2 For instruction: later
"""
        self.assertEqual(cb.forced_cost(cb.selected_log(log), 2), 5)

    def test_invalid_cost_preserved(self):
        self.assertIsNone(cb.forced_cost(
            'LV: Found an estimated cost of Invalid for VF vscale x 4 For instruction: load', 4))

    def test_repeated_cost_or_multiple_loops_rejected(self):
        entry = 'LV: Found an estimated cost of 2 for VF vscale x 2 For instruction: load\n'
        with self.assertRaises(ValueError):
            cb.forced_cost(entry * 2, 2)
        with self.assertRaises(ValueError):
            cb.selected_log("\nLV: Checking a loop in 'selected_kernel' from x" * 2)

    def test_hot_loop_excludes_exit_reduction(self):
        asm = """selected_kernel:
\tvsetvli a0, zero, e32, m1, ta, ma
.LBB0_1:
\tvl1re32.v v8, (a1)
\tvfadd.vv v9, v9, v8
\tbnez a0, .LBB0_1
\tvfredusum.vs v8, v9, v8
\tret
\t.size selected_kernel, .-selected_kernel
"""
        shape = cb.assembly_shape(asm)
        self.assertEqual(len(shape['vector_loops']), 1)
        ops = shape['vector_loops'][0]['opcodes']
        self.assertEqual(ops['vfadd.vv'], 1)
        self.assertNotIn('vfredusum.vs', ops)
        self.assertNotIn('vsetvli', ops)


if __name__ == '__main__':
    unittest.main()
