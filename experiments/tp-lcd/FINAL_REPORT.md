# TP/LCD report-only implementation and evaluation

The frozen model meets every specified **historical calibration threshold** for
both groups, including actual expanded automatic selection. This establishes a
compiler cost/ranking result against the supplied report, not measured speedups
of the newly generated binaries. All work is **validation-informed**. Saturn has
software coverage only; no hardware configuration or measurement is inferred.

## Frozen results

| Group | Actual automatic regret GM / worst | Non-tie accuracy | Relative error GM / P90 |
|---|---:|---:|---:|
| Core seven | 1.015112 / 1.093117 | 94.8718% (37/39) | 1.118502 / 1.246743 |
| BiCG/GEMVER, separate validation-informed group | 1.000000 / 1.000000 | 100% (11/11) | 1.106704 / 1.188672 |
| Required | ≤1.05 / ≤1.15 | ≥90% | ≤1.15 / ≤1.30 |

Non-tie accuracy and relative errors use all 36 forced compiler scores. Actual
selection is independently observed, not substituted with their argmin. Here the
observed choices happen to equal those argmins. Coverage is 36/36 forced cells,
9/9 automatic loops, all four scalable candidates visited in each automatic
search. Core pair/relative coverage is 42/42 and 21/21; additional coverage is
12/12 and 6/6. All-pair accuracy, including measured near-ties, is 88.0952% and
91.6667%; this is distinct from the required non-tie statistic.

Actual scalable VFs: s000=16, s125=16, s311=16, s1111=16, s4112=16,
Jacobi=8, GESUMMV=4, BiCG=16, GEMVER=16. GESUMMV's measured best is VF16;
selecting VF4 costs a historical regret of 1.093117 and remains within the
required worst-regret bound. Its current VF16 binary really spills.

[Completion audit](completion-audit.json), [implementation patch](implementation.patch),
[source manifest](final-source-manifest.json),
[Metric audit](final-report/metric-audit.json),
[summary and ablations](final-report/summary.csv),
[36 candidates with domain totals](final-report/candidates.csv),
[actual selections](final-report/selections.csv),
[every automatic candidate and exclusion](final-report/automatic-candidates.csv),
[relative errors](final-report/relative.csv),
[pairs](final-report/pairs.csv), [adjacent gaps](final-report/gaps.csv).

## Provenance and conditions

The starting local main is exactly
`ca7933e47d3a3451d81e72ac174dcb5aa28b59d1` (LLVM 22.1.8). The historical
measurement compiler `89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb` is a different
revision. The worktree was prepared using the specifically authorized fetch and
main reset; no remote ref was modified. Original input documents and extracted
sources remain unchanged. Compiler costs use full original C translation units.

The report is the only source of measured cycles. The input manifest verifies
all embedded file hashes. The new Picolibc 1.8.6-2 header package is independently
recorded; it is not a recovered historical lockfile. Pristine compiler/resources
are preserved read-only under `toolchain/pristine` with their own hashes. The
working build directory subsequently contains the modified compiler.

Conditions: RISCV64, `rv64gcv_zba_zbb_zbc_zbs_zicbom_zicboz_zvl128b`,
XiangShan Kunminghu CPU for XiangShan collections, `-O2 -ffast-math`, IC=1,
SWP disabled, no loop unrolling, scalable vectorization enabled. VF=2/4/8/16
means estimated runtime VF=4/8/16/32 at tuning vscale=2. VLEN128 is a **minimum**,
never treated as exact. Expanded automatic search uses the existing
`-riscv-v-register-bit-width-lmul=8` tuning setting and actually visits scalable
1,2,4,8,16. Smaller extra candidates are preserved in diagnostics.

[Pristine provenance](pristine-build.json), [snapshot](pristine-snapshot.json),
[inputs](input-manifest.json), [headers](header-provenance.json),
[final compiler/source/flags provenance](frozen-xiangshan/provenance.json).
Every collection preserves the complete commands, logs, tracked diff, new common
source files, compiler hash, actual assembly/IR and allocator dumps. Final
XiangShan/Saturn/none-observe collections each contain 45 full artifacts and zero
failures. Debug and non-debug compilation emit identical assembly in every cell.

## Implementation and model boundary

`VPlanCostModel.cpp` sums Memory and Compute over the complete loop region,
then computes `TP = Other + max(sum(Memory), sum(Compute))`. Scalar replication
is Other; comparisons follow execution opcode rather than their i1 result type.
Composite/unclassified costs stay additive. Ordinary precompute/skip accounting
is reconciled with the sum, including explicit savings for supported identities.
Invalid or unsupported costs remain explicit fallbacks.

`VPlanAnalysis.cpp` builds def-use edges and header-PHI backedges. LCD is the
maximum latency/distance among **closed** paths at iteration distances 1–3.
A feed-forward chain cannot become LCD. Independent reductions compete by max,
not sum. Widened induction synthesizes its implicit update without invoking its
unreachable generic backedge accessor. Load-fed first-order splice is supported
without inventing a closed recurrence. The independent latency helper explicitly
queries TTI latency; it does not reuse throughput skip state or the legacy helper
that ignores the cost kind. Operand-to-result latency is an approximation;
cycles whose shortest closure exceeds distance three can be missed.

Ranking uses `max(TP,LCD)/estimated runtime VF`, with smaller-VF score ties.
Ordinary costs still decide scalar profitability and remain in downstream VF
records for interleaving/epilogue logic. The model requires explicit IC1 and
preserves code-size behavior, legality, invalid costs, and genuine pressure
exclusions. Any eligible unsupported candidate causes whole-loop ordinary
fallback; costs in different units are not mixed. Memory-carried/unresolved
relationships unsupported by the SSA graph explicitly fall back. Nested or
replicated regions, same-iteration graph cycles and unsupported latency recipes
also have explicit reasons. The none-profile observation collection supports
14/36 model cells and exposes 22 fallbacks while preserving all 45 pristine
assemblies; XiangShan and Saturn explicit profiles support 36/36.

Three structural corrections have no kernel-name or numeric fitting input:

1. Same-block, single-use, contract-enabled multiply/add pairs use native FMA
   costs only when TTI legality and a strictly cheaper FMA establish that case.
   Extra multiply uses and missing contraction flags keep separate costs.
2. Full-EVL simplification requires a nonzero trip count divisible by candidate
   VF times the largest legal power-of-two vscale, plus IC1 and safety checks.
   It recognizes invariant EVL/casts and all-true merge identities, then applies
   the same simplification only while executing the selected main plan. Analysis
   does not clone/unroll plans or mutate the shared candidate plan. Unknown and
   nonmultiple counts retain tail handling.
3. Affine, constant-stride, target-native and exclusively address-used integer
   chains are counted as scalar GPR values in experimental pressure analysis.
   Explicitly scalar EVL definitions remain scalar even when a widened induction
   consumes them. Additional vector uses invalidate the address proof. The
   ordinary pressure/profitability path is retained separately; genuine vector
   intervals are not dropped. s1111's 56-register ordinary estimate becomes
   32 actual vector registers plus GPR accounting; GESUMMV16 stays excluded.

The model and profiles default off/none, preserving existing compilation.
Explicit `-vplan-tp-lcd=rank -riscv-tp-lcd-profile=xiangshan` enables the frozen
experiment. Its three structural rules default on and have ablation switches.

## Parameters and rounds

[Frozen parameters](frozen-model.json) have five numeric fields per independent
profile, starting from two. FP width stayed fixed; four numeric fields were
calibrated. Indexed startup reuses the common latency startup and adds no numeric
field. These are software cost hypotheses, not asserted datapath dimensions or
physical cycle latencies.

| Profile | FP beat bits | Memory/reference bits | Latency startup | Slide startup | Slide quadratic coefficient | Loaded-index startup |
|---|---:|---:|---:|---:|---:|---|
| XiangShan | 128 | 256 | 5 | 8 | 3 | on, reuses 5 |
| Saturn | 128 | 128 | 3 | 0 | 0 (linear) | off |

Native legal FP throughput is reference beats; latency adds startup. Memory uses
its independent beat width, retaining legalization multiplicity. The nonlinear
slide hypothesis is `startup + reference_beats² * coefficient` per primitive
slide. A splice uses its actual primitive slide costs. Irregular memory retains
per-estimated-lane scaling; a directly loaded GEP index through integer casts
adds the shared startup. No hidden spill/vset/accumulator penalty was fitted.

All rounds and failures remain in [rounds-and-ablations.csv](final-report/rounds-and-ablations.csv),
[work log](WORK_LOG.md), and their original directories and predeclared plans.

| Retained stage | Core relative GM / P90 | Main result |
|---|---:|---|
| Initial complete coverage | 1.42958 / 1.81490 | Ordinary actual worst regret still 1.45738 |
| Memory beat 256 | 1.26818 / 1.50262 | Separate memory/FP scale |
| Native FMA accounting | 1.25003 / 1.40005 | Non-tie 89.7436% |
| Slide grid, startup8/coefficient3 | 1.20072 / 1.33144 | Jacobi actual and forced choice8 |
| Latency startup5 | 1.14544 / 1.30340 | Relative P90 narrowly failed |
| Loaded-index distinction | 1.11850 / 1.24674 | Forced thresholds pass; actual GES choice8 fails worst |
| Full EVL alone | unchanged | GES choice4, but s1111 choice4 fails worst |
| Scalar address plus scalar EVL pressure | unchanged | s1111 choice16; actual thresholds pass |

Slide grid: startup {4,8,12,16} × coefficient {1,2,3,4}, all 16 complete
compiler trials retained. Latency grid: {3,4,5,6}. Blanket startup on every
nonconsecutive access was rejected because it worsened core relative GM to
1.15322. Early coverage failures, the first full-EVL assertion failures and the
negative-test iterator failure are preserved, not erased from the history.
No additional numeric fitting was used to resolve automatic-plan/pressure issues.

Frozen-model ablations:

| Ablation | Core non-tie | Relative GM / P90 | Actual worst regret |
|---|---:|---:|---:|
| TP only | 92.3077% | 1.20877 / 1.31843 | selection-only ablation: forced regret worst1.27966 |
| LCD only | 79.4872% | 1.45698 / 2.02737 | selection-only ablation: forced regret worst1.45738 |
| FMA off | 89.7436% | 1.15307 / 1.30633 | 1.09312 |
| Loaded-index startup off | 94.8718% | 1.14544 / 1.30340 | 1.09312 |
| Full EVL off | 94.8718% | 1.11850 / 1.24674 | 1.16108 |
| Scalar register correction off | 94.8718% | 1.11850 / 1.24674 | 1.22629 |

TP-only/LCD-only are evaluator choices from actual emitted component costs;
they are not separate automatic compiler policy runs. The four structural-rule
ablations each recompile all 45 cells. Final default parameters reproduce every
round11 forced numeric score and all actual choices exactly.

## Baseline and code correspondence

The pristine baseline is independently built from the starting main, not the
historical compiler. Forced legacy IR cost argmin: core regret GM/worst
1.109637/1.457382, non-tie84.6154%, relative GM/P90 1.570488/2.098064.
Largest VF reference: regret1.057689/1.457382. Actual expanded baseline:
1.085638/1.457382. Default automatic baseline is undefined for the measured
candidate set because s1111 selects fixed8. These distinctions are preserved in
[baseline summary](baseline-v2/summary/README.md); current-profile legacy
component costs must not be mislabeled as the pristine baseline.

Every frozen XiangShan forced assembly remains byte-identical to its pristine
forced assembly: 36/36. Three automatic assemblies differ from pristine because
s1111 changes8→16, Jacobi16→8 and GESUMMV8→4. Eight of nine selected function
bodies exactly match their same-VF forced bodies. GESUMMV4 differs: both loops
have three whole-register loads and two FMAs, but automatic scheduling reuses a
load group earlier and uses an integer countdown with one extra scalar `sub`
and `bnez` instead of pointer-end `bne`. It has no spill and no loop vset.
The full diff is preserved under [assembly differences](final-report/assembly-differences/).
This changed automatic binary is **unmeasured**; its historical VF4 regret is a
selection proxy, not demonstrated timing for that binary.

Historical correspondence is incomplete even where new forced assemblies agree
with pristine. The report lacks full historical assembly/ELFs. In particular:

- GESUMMV current VF16 has two scalable stack slots, two vector stores and two
  reloads in the allocator dump; the report described no spills. It remains
  pressure-excluded. Current VF8/VF16 loop vset counts2/2 versus historical4/4.
- s4112 current VF16 has six loop vset instructions versus historical four;
  loaded i64 indices cause EMUL splitting. This is a data-indexed access, not an
  affine strided-address case.
- Jacobi retains four loads, two slides, four adds, one multiply and one store,
  one loop vset and no spills. Its load-fed splice is not a closed LCD.
- s000/s125 loop vset counts0/0/1/1, s3110/0/2/2, s1111 0/0/1/1,
  BiCG0/0/3/3 and GEMVER0/0/1/1 over VF2/4/8/16. No other forced case spills.

The reduction workaround is applied separately, with assembled object checks.
A conservative adapter reorders extraction only across proved independent safe
instructions; its input and output are preserved. This does not prove historical
binary identity. [Code-shape table](final-report/code-shape.csv) contains all
135 final-profile records, opcodes, LMULs, vset counts and allocator frame evidence.

## Residual gaps and unverified claims

Aggregate success does not mean individual gaps are exact:

| Adjacent gap | Predicted ratio | Measured ratio | Predicted / measured change |
|---|---:|---:|---:|
| Jacobi4→8 | 0.914286 | 0.726043 | −8.57% / −27.40% |
| Jacobi8→16 | 1.218750 | 1.457382 | +21.88% / +45.74% |
| GESUMMV4→8 | 1.357143 | 1.062174 | +35.71% / +6.22% |
| GESUMMV8→16 | 0.921053 | 0.861267 | −7.89% / −13.87% |

The desired regression directions are captured, with substantial magnitude
residuals. No per-kernel multiplier, absolute-cycle calibration or extra gap
parameter was introduced. No numeric aggregate threshold remains unmet.
Absolute-cycle accuracy, performance of changed binaries, physical slide causes,
Saturn hardware/performance, and historical binary identity remain unverified.
Those are explicitly outside this report-only completion claim.

## Validation and reproduction

The final assertion-enabled LLVM build succeeds. Vectorize unit tests: 80 pass,
including nine closed-recurrence tests and three domain/accounting tests. RISC-V
cost-model/vectorizer suite: 169 pass. Python evaluator/collector/workaround:
17 pass. Tests cover independent long chains, one/two/three-distance cycles,
independent accumulators, splice variants, widened induction, whole-region
max-after-sum, scalar/compare domains, invalid/nested/cyclic fallbacks, FMA use
constraints, memory dependence, IC/code-size fallback, target-profile isolation,
loaded-index startup, maximum-vscale full-EVL proof and real selected-plan
cleanup, and extra vector address uses. `git diff --check` passes.

Logs: [build](model-build-20-format.log), [unit](vectorize-unit-tests-final-2.log),
[RISC-V lit](riscv-regression-final-2.log), [Python](python-tests-final.log).
The negative tests found and fixed real implementation issues; failed logs remain.
Final sources use LLVM clang-format. No external execution, new benchmark
measurement, RTL/QEMU, SSH, publishing or remote write was performed.

From the repository root after the preserved build:

```sh
python3 experiments/tp-lcd/collect_model.py --out NEW_XIANGSHAN_DIRECTORY
python3 experiments/tp-lcd/evaluate.py NEW_XIANGSHAN_DIRECTORY/scores.json --out NEW_EVALUATION_DIRECTORY
python3 experiments/tp-lcd/collect_model.py --profile saturn --out NEW_SATURN_DIRECTORY
python3 experiments/tp-lcd/collect_model.py --profile none --model observe --out NEW_NONE_DIRECTORY
build-tp-lcd-main/bin/llvm-lit -v llvm/test/Transforms/LoopVectorize/RISCV llvm/test/Analysis/CostModel/RISCV
build-tp-lcd-main/unittests/Transforms/Vectorize/VectorizeTests
python3 -m unittest discover -s experiments/tp-lcd -p 'test_*.py' -v
```

Each collector output must be new. Compiler overrides use repeated
`--extra-llvm=-flag=value`. `final_report.py` reproduces the joined final CSVs
and assertions from the named preserved collections; move its existing output
aside before rerunning. [README](README.md) documents clean baseline preparation,
modified builds and artifact locations. The complete goal and original report
remain authoritative inputs at the repository root.
