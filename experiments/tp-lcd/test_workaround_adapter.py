import unittest

from workaround_adapter import rewrite_assembly


class WorkaroundAdapterTests(unittest.TestCase):
    def asm(self, middle='\tfmv.w.x fa5, a0\n'):
        return ('.type selected_kernel,@function\nselected_kernel:\n'
                '\tvsetvli a0, zero, e32, m1, ta, ma\n'
                '\tvfredusum.vs v8, v8, v10\n'
                '\tvfredusum.vs v9, v9, v10\n' + middle +
                '\tvfmv.f.s fa4, v8\n\tvfmv.f.s fa3, v9\n'
                '\tret\n.size selected_kernel, .-selected_kernel\n')

    def test_two_scheduled_independent_extractions(self):
        patched, applied, moved, adjacent = rewrite_assembly(self.asm(), 'm1', preserve_state=True)
        self.assertEqual((applied, moved), (2, 2))
        self.assertEqual(patched.count('vmv.x.s'), 2)
        self.assertEqual(patched.count('csrr\tt1, vl'), 2)
        self.assertNotIn('vfmv.f.s', patched)
        self.assertIn('vfredusum.vs v8, v8, v10\n\tvfmv.f.s fa4, v8', adjacent)

    def test_fp_register_hazard_rejected(self):
        with self.assertRaises(ValueError):
            rewrite_assembly(self.asm('\tfadd.s fa5, fa4, fa3\n'), 'm1', preserve_state=True)

    def test_control_and_vector_state_hazards_rejected(self):
        for middle in ('\tcall other\n', '.Llabel:\n', '\tvsetvli zero, zero, e32, m1, tu, ma\n'):
            with self.subTest(middle=middle), self.assertRaises(ValueError):
                rewrite_assembly(self.asm(middle), 'm1', preserve_state=True)


if __name__ == '__main__':
    unittest.main()
