# Pristine baseline

These results use the compiler built from the specified local main. Cycle comparisons are historical calibration, not measurements of the new binaries.

| Group | Choice/cost | Regret GM | Worst | Non-tie accuracy | Relative error GM | P90 |
|---|---|---:|---:|---:|---:|---:|
| core | legacy | 1.109637 | 1.457382 | 0.846154 | 1.570488 | 2.098064 |
| core | largest_vf | 1.057689 | 1.457382 | undefined | undefined | undefined |
| core | default automatic | undefined | undefined | undefined | undefined | undefined |

default automatic/core coverage issues: ['s1111: actual selection absent or outside measured candidates: fixed:8']

| validation_informed | legacy | 1.032342 | 1.065731 | 0.909091 | 1.703224 | 1.755840 |
| validation_informed | largest_vf | 1.000000 | 1.000000 | undefined | undefined | undefined |
| validation_informed | default automatic | 1.096937 | 1.129056 | undefined | undefined | undefined |
| core | expanded automatic | 1.085638 | 1.457382 | undefined | undefined | undefined |
| validation_informed | expanded automatic | 1.000000 | 1.000000 | undefined | undefined | undefined |

The legacy candidate score is the forced legacy IR validity cost. Actual automatic search uses VPlan costing and its existing legality/pressure checks. No TP/LCD implementation is present in this pristine compiler.

| Kernel | Search | Actual VF | Scalable candidates costed | Forced body identical | Frame evidence |
|---|---|---|---|---|---|
| s000 | automatic | scalable 4 | [1, 2, 4] | True | none |
| s000 | expanded | scalable 16 | [1, 2, 4, 8, 16] | True | none |
| s1111 | automatic | fixed 8 | [1, 2, 4] | outside measured set | none |
| s1111 | expanded | scalable 8 | [1, 2, 4, 8, 16] | True | none |
| s125 | automatic | scalable 4 | [1, 2, 4] | True | none |
| s125 | expanded | scalable 16 | [1, 2, 4, 8, 16] | True | none |
| s311 | automatic | scalable 4 | [1, 2, 4] | True | none |
| s311 | expanded | scalable 16 | [1, 2, 4, 8, 16] | True | none |
| s4112 | automatic | scalable 4 | [1, 2, 4] | True | none |
| s4112 | expanded | scalable 16 | [1, 2, 4, 8, 16] | True | none |
| jacobi-2d-imper-l00 | automatic | scalable 4 | [1, 2, 4] | True | none |
| jacobi-2d-imper-l00 | expanded | scalable 16 | [1, 2, 4, 8, 16] | True | none |
| gesummv-l00 | automatic | scalable 4 | [1, 2, 4] | True | none |
| gesummv-l00 | expanded | scalable 8 | [1, 2, 4, 8, 16] | True | none |
| bicg-l01 | automatic | scalable 4 | [1, 2, 4] | True | none |
| bicg-l01 | expanded | scalable 16 | [1, 2, 4, 8, 16] | True | none |
| gemver-l00 | automatic | scalable 4 | [1, 2, 4] | True | none |
| gemver-l00 | expanded | scalable 16 | [1, 2, 4, 8, 16] | True | none |

Costed candidates can still be excluded by register-pressure or profitability checks. Inspect selected-debug.txt for the reason. A matching body proves equality only between these newly generated functions, not historical binary identity.

| Kernel | VF | Vector loops | vsetvli per vector loop | Allocation frame objects |
|---|---:|---:|---|---|
| s000 | 2 | 1 | [0] | none |
| s000 | 4 | 1 | [0] | none |
| s000 | 8 | 1 | [1] | none |
| s000 | 16 | 1 | [1] | none |
| s1111 | 2 | 1 | [0] | none |
| s1111 | 4 | 1 | [0] | none |
| s1111 | 8 | 1 | [1] | none |
| s1111 | 16 | 1 | [1] | none |
| s125 | 2 | 1 | [0] | none |
| s125 | 4 | 1 | [0] | none |
| s125 | 8 | 1 | [1] | none |
| s125 | 16 | 1 | [1] | none |
| s311 | 2 | 1 | [0] | none |
| s311 | 4 | 1 | [0] | none |
| s311 | 8 | 1 | [2] | none |
| s311 | 16 | 1 | [2] | none |
| s4112 | 2 | 1 | [0] | none |
| s4112 | 4 | 1 | [0] | none |
| s4112 | 8 | 1 | [1] | none |
| s4112 | 16 | 1 | [6] | none |
| jacobi-2d-imper-l00 | 2 | 1 | [1] | none |
| jacobi-2d-imper-l00 | 4 | 1 | [1] | none |
| jacobi-2d-imper-l00 | 8 | 1 | [1] | none |
| jacobi-2d-imper-l00 | 16 | 1 | [1] | none |
| gesummv-l00 | 2 | 1 | [0] | none |
| gesummv-l00 | 4 | 1 | [0] | none |
| gesummv-l00 | 8 | 1 | [2] | none |
| gesummv-l00 | 16 | 1 | [2] | ['  fi#0: id=2 size=64, align=8, at location [SP]', '  fi#1: id=2 size=64, align=8, at location [SP]', '408B\t  VS8R_V killed renamable $v24m8, %stack.0 :: (store (<vscale x 1 x s512>) into %stack.0, align 8)', '500B\t  VS8R_V killed renamable $v0m8, %stack.1 :: (store (<vscale x 1 x s512>) into %stack.1, align 8)', '504B\t  renamable $v0m8 = VL8RE8_V %stack.0 :: (load (<vscale x 1 x s512>) from %stack.0, align 8)', '584B\t  renamable $v0m8 = VL8RE8_V %stack.1 :: (load (<vscale x 1 x s512>) from %stack.1, align 8)', '  fi#0: id=2 size=64, align=8, at location [SP]', '  fi#1: id=2 size=64, align=8, at location [SP]', '432B\t  VS8R_V killed renamable $v24m8, %stack.0 :: (store (<vscale x 1 x s512>) into %stack.0, align 8)', '544B\t  VS8R_V killed renamable $v0m8, %stack.1 :: (store (<vscale x 1 x s512>) into %stack.1, align 8)', '560B\t  renamable $v0m8 = VL8RE8_V %stack.0 :: (load (<vscale x 1 x s512>) from %stack.0, align 8)', '672B\t  renamable $v0m8 = VL8RE8_V %stack.1 :: (load (<vscale x 1 x s512>) from %stack.1, align 8)'] |
| bicg-l01 | 2 | 1 | [0] | none |
| bicg-l01 | 4 | 1 | [0] | none |
| bicg-l01 | 8 | 1 | [3] | none |
| bicg-l01 | 16 | 1 | [3] | none |
| gemver-l00 | 2 | 1 | [0] | none |
| gemver-l00 | 4 | 1 | [0] | none |
| gemver-l00 | 8 | 1 | [1] | none |
| gemver-l00 | 16 | 1 | [1] | none |

Frame evidence is taken before the assembly reduction workaround. Opcode inventories and loop text are preserved in each record; historical memory/LMUL/splice/index-split correspondence requires inspecting those records.

All later model changes are validation-informed. Core and BiCG/GEMVER groups remain separate. Saturn has no supplied measurements.
