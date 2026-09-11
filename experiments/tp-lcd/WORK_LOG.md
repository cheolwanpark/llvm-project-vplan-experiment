# Work log

## 2026-09-11: environment preparation and pristine baseline build

This initial goal turn made progress: authoritative revision preparation,
verified immutable inputs, installed build/header tools, new reproducible
harness, fixed evaluation policy, and source API evidence. There was no previous
goal turn to classify. No tuning results or implementation completion are claimed.

- Initial tracked tree was clean; only the two supplied Markdown inputs existed.
  Initial branch was `vplan-cost-adjustment`, HEAD and main were historical
  `89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb`.
- Ran the explicitly requested `git fetch origin ca7933e47d3a3451d81e72ac174dcb5aa28b59d1`
  and `git switch -C main FETCH_HEAD`. Verified HEAD/main at that exact SHA.
  Preserved the original branch and user documents. Origin URL remains
  `https://github.com/llvm/llvm-project.git`; no remote ref was changed.
- All 22 report files extracted and hashed; a fresh temporary extraction using
  the saved script was byte-identical to the initial extraction and manifest.
- Installed CMake 4.4.3 and Ninja 1.13.2. The Codex-bundled rg process hung, so
  searches use `/opt/homebrew/bin/rg` (15.2.0). This was a local tool issue and
  did not block repository work.
- Downloaded Picolibc 1.8.6-2 from Ubuntu archive, preserved package SHA-256 and
  all 270 header hashes. Extracted headers only. Verified the saved preparation
  script by fresh extraction. All nine full source translation units passed a
  RISC-V header syntax smoke test with installed Clang 22.1.8; that compiler is
  **not** the goal's pristine baseline compiler.
- Fixed evaluation thresholds and edge cases before baseline/tuning. Thirteen
  evaluator/collector tests passed. Python files also passed compilation checks.
- Saved `API_AUDIT.md` with source evidence on cost kinds, forced-VF diagnostics,
  automatic search, region/precompute accounting, and PHI/splice handling.
- Started `python3 experiments/tp-lcd/build_pristine.py --jobs 3`, output in
  `pristine-build.log`, structured provenance/status in `pristine-build.json`.
  At the last check Ninja was live and beyond step 1180/3187 with no failure.
  Tool session **49983**, build-driver PID **7994**, Ninja PID **12922** belong to
  this environment. Revalidate the session/process before relying on them;
  never infer liveness from this file alone and never restart a live build.

## Next required actions

1. Poll/revalidate the existing pristine build, inspect any failure, and finish
   the build. Do not change compiler sources before collecting baseline.
2. Run the new `collect_baseline.py` against the completed pristine compiler.
   Its compiler hash/revision checks prevent relabeling installed/modified
   compiler output as pristine. The collector has parser unit tests but has
   **not yet been exercised against this built compiler**; inspect and correct
   any diagnostic/assembly mismatches, preserving failed output directories.
3. Preserve immutable pristine executables and Clang resource headers before
   any rebuild. Inspect all 36 legacy candidate costs, default/expanded actual
   automatic choices and visits, generated IR/VPlans, workaround and code shape.
   Compute baseline metrics using the fixed policy and preserve the results.
4. Implement the complete TP/LCD and independent target profiles required by the
   goal, including compiler diagnostics and meaningful LLVM tests. No compiler
   implementation has been made yet. Follow the full objective, not just this
   next-action list. Tuning, fixed-parameter evaluation, regression checks and
   final completion audit all remain pending.

The goal remains active. Saturn measurement work stays deferred as required.

## Continuation 1: preserve baseline artifacts and finish prerequisite audit

The previous turn is classified as progress. The existing build tool session
49983 and build/Ninja processes were revalidated live; no restart occurred.
The compiler source tree remains unchanged.

- Added `snapshot_pristine.py` to copy tools and Clang resource files, verify
  hashes and resource discovery, and make snapshot files read-only.
- Baseline collection now verifies the compiler against the completed pristine
  build SHA-256, which matters because the installed compiler and this revision
  both identify as LLVM 22.1.8.
- Collection additionally captures post-register-allocation machine-function
  dumps and frame references before applying the assembly workaround.
- Added `summarize_baseline.py`, requiring the exact 36 forced / 18 automatic
  record set, producing baseline CSV metrics, actual visited/selected VFs, body
  comparisons, and per-loop vsetvli/frame summaries. Execution against the new
  compiler is still pending; script syntax checks pass.
- Saved `largest-vf-reference.json`, derived only from the report's 36 cells:
  core regret GM 1.0576888336595451 / worst 1.457382236537608; BiCG/GEMVER 1.0 /
  1.0. These are measurement-only choice references, not compiler results.
- Extended the source audit through BasicTTI to the generic FP latency of 3
  cycles, and documented memory-dependence enumeration and pressure caveats.

A live orchestration **functions.exec cell 62** now owns polling of build
session 49983. It waits for successful completion, then runs, sequentially:

```sh
python3 experiments/tp-lcd/snapshot_pristine.py
python3 experiments/tp-lcd/collect_baseline.py --cc experiments/tp-lcd/toolchain/pristine/bin/clang --out experiments/tp-lcd/baseline
python3 experiments/tp-lcd/summarize_baseline.py experiments/tp-lcd/baseline
```

Resume by polling **functions.wait cell_id=62**. Do not also poll session 49983
while that cell owns it. The orchestration stops at the first failed command;
inspect the output and preserve failed directories before adapting/retrying.
At the last direct observation the build was live around step 2446/3187.
Revalidate current state; this log is not liveness evidence.

The next substantive milestone remains complete pristine baseline inspection,
then the full compiler implementation/tuning/tests demanded by the objective.
The goal is active, with no blocker and no claim of model completion.

## Continuation 2: completed baseline and initial compiler implementation

This section supersedes the pending-session notes above. Build session 49983
and orchestration cell 62 completed; do not resume them. The pristine compiler
and 288 resource/tool files are hashed and preserved read-only under
`toolchain/pristine`. `build-tp-lcd-main` now contains modified builds.

The first collection failed at GESUMMV's scheduled extraction (after 36
records), retained in `baseline`. A conservative adapter moves each extraction
across only independent instructions before applying the report's unchanged
workaround. The successful `baseline-v2` contains all 54 records: 36 forced,
9 default automatic and 9 expanded automatic, optimized IR, assembly, allocator
dumps and assembled workaround objects. Python checks passed (16 tests).

Baseline metrics and all comparisons are in `baseline-v2/summary`. Core legacy
forced-cost regret GM/worst are 1.109637/1.457382, non-tie accuracy 84.6154%,
relative error GM/P90 1.570488/2.098064. Expanded actual automatic regret is
1.085638/1.457382. Default actual s1111 chooses fixed 8 and is explicitly outside
the measured scalable candidate set. Expanded search visits all four candidates;
GESUMMV m8 is excluded for pressure and actually spills two vector groups in
this revision. This differs from the historical no-spill measurement. GESUMMV
m4/m8 has two loop vset instructions versus four reported historically; s4112
m8 has six versus four. New binaries are unmeasured; historical identity cannot
be established from the supplied report alone. Forced/automatic bodies agree
where the actual selected VF belongs to the measured set.

Initial compiler work (not a completed model):
- Bounded closed-def-use recurrence analysis, explicit widened-IV implicit
  update, distance 1..3, independent-chain exclusion and explicit graph errors.
  Nine new recurrence unit tests passed (`recurrence-tests.txt`).
- Explicit independent XiangShan/Saturn software profile selectors, initially
  two parameters each (128-bit reference beat, latency startup 3). Both start
  from the same generic assumptions; neither is a hardware specification.
  Existing TTI latency dispatch needed a vector-load override because generic
  getInstructionCost otherwise returns constant 4 before reaching target costs.
  Profile/default/code-size regression passed (`profile-tests-2.log`).
- Initial structure frozen before tuning in `initial-model-structure.json`.
- Added whole-loop Memory/Compute/Other aggregation, an independent explicit
  latency helper and experimental observe/rank modes, JSON candidate/selection
  diagnostics, ordinary scalar-profitability and pressure gating, and whole-loop
  ordinary ranking fallback when an eligible candidate is unsupported.
  This integration is currently being built and has not yet passed suite
  evaluation. Do not infer completion or metric improvement from its presence.

`collect_model.py` replays the exact preserved full-TU commands with the modified
compiler, retaining failed cases, source/hash provenance, diagnostics and code
shape comparison. Its scores-only mode is for coverage discovery; full rounds
also preserve IR, allocator dumps and assembled workaround output. All subsequent
model changes and tuning are validation-informed. Saturn measurements remain
out of scope. All final objective validation/tuning requirements remain active.

## Integration and early tuning results

`round-00-coverage` explicitly rejected 12/36 candidates (integer induction
opcode, derived-IV support, scalable gather latency). These API coverage gaps
were repaired without cycle fitting. `round-01-coverage` supports all 36 cells;
`round-02-initial-full` preserves all 45 full compile records and all emitted
assembly is byte-identical to pristine. All automatic selections match their
same-VF forced bodies. The initial model misses the targets: core non-tie
84.6154%, relative GM/P90 1.429581/1.814900, forced-score regret
1.071227/1.457382, actual expanded regret 1.085638/1.457382.

Tests at this point: 80 Vectorize unit tests passed, including 9 closed-LCD
and 3 domain/cost tests. The new forced-VF observation/regression test passes,
including byte-identical off/observe IR and explicit memory-carried-dependence
fallback. RISC-V cost+vectorizer regression: 167 tests passed. Later FMA and
IC=1 scope tests pass as well. Python tests now total 17, including rejection of
attempts to evaluate Saturn with the supplied XiangShan measurements.

Recorded validation-informed rounds (all input/measurement files unchanged):
- Round 03 separates memory reference beat width (128/256) from FP. At 256,
  core relative GM/P90 improves to 1.268184/1.502618; all 45 assemblies still
  match pristine. Additional group remains 1.546243/1.678479.
- Round 04 cost-only native FMA contraction (both contract flags, same block,
  single-use multiply, legal vector type, strictly cheaper TTI FMA) improves
  core relative GM/P90 to 1.250030/1.400051, non-tie to 89.7436%; additional
  group improves to 1.131719/1.203668 with 90.9091% non-tie. All 45 assemblies
  remain identical. Turning FMA accounting off reproduces every prior score.
- Round 05 predeclared a 4x4 slide startup/quadratic-coefficient grid, evaluating
  all 36 forced and 9 actual expanded candidates at every point. All 16 trials
  succeeded and are retained. S=8/Q=3 is the best declared eligible configuration:
  Jacobi selects VF8 both by forced-score argmin and actual automatic; core
  forced-score regret GM/worst 1.015112/1.093117, non-tie 94.8718%, relative
  GM/P90 1.200724/1.331440. Actual automatic worst remains 1.161080 (GESUMMV VF8).
  This is not goal completion: relative targets and actual worst still fail.
  The slide rule is a software hypothesis, not established hardware behavior.

Important automatic-plan limitation: expanded search uses EVL plans even for
VF2/VF4 in several kernels, whereas forced small-VF plans use ordinary loads
and no vp.merge. Candidate scores can therefore differ even before actual
selection. Diagnostics preserve both; forced argmin is never reported as actual
selection. GESUMMV VF16 remains excluded for actual register pressure; no blanket
pressure bypass has been introduced.

Round 05 retained point is being recollected with all full artifacts. Round 06
is predeclared in `round-06-plan.json`: test existing latency startup 3/4/5/6 to
address reduction relative scale, retaining all throughput settings and no new
numeric fields. The goal remains active; final freeze/audit/report remain pending.

## Rounds 06–09 and current work

- Round06 latency startup 3/4/5/6: retained 5, no new parameter. Full artifacts
  `round-06-latency-best-full`; core relative GM/P90 1.145437/1.303404;
  additional group 1.106704/1.188672, 100% non-tie and regret 1.0.
- Round07 blanket startup for all nonconsecutive memory was rejected and its
  flag removed: relative GM worsened to 1.153216. Trial artifacts/decision kept.
- Round08 uses original IR supplied via existing MemIntrinsicCostAttributes to
  recognize a GEP index directly loaded from memory (through integer casts).
  Reuses startup 5 only for that data-indexed access, on either load or store.
  Core forced metrics now all meet targets: regret 1.015112/1.093117, non-tie
  94.8718%, relative GM/P90 1.118502/1.246743. Additional group remains fully
  within targets. Off ablation exactly reproduces round06 scores. No new numeric
  fields. Actual automatic worst remains 1.161080 (GESUMMV8), so not complete.
- Scalar domain classification was tightened: only-first-lane uses do not make
  a VPWidenRecipe execute scalar. VPInstruction's existing execution-capability
  query is now public for the analysis; pointer latency uses DataLayout widths.
  The float-compare test now explicitly consumes only lane zero. 80 unit tests
  pass with the updated implementation (`vectorize-unit-tests-2.log`).
- Round09 introduces an explicit full-EVL experiment, addressing the actual
  computeMaxVF source FIXME (tail policy chosen from largest candidate).
  Proves divisibility by VF * max legal power-of-two vscale, with IC1, no early
  exit and no bounded dependence. It never treats tuning vscale2 as exact.
  Cost recognizes invariant EVL/casts and all-true merge identities; the same
  simplification is applied only during actual main-plan execution, preserving
  the shared plan during costing and epilogue selection. No plan cloning.
  First full collection `round-09-full-evl` retained two compiler assertion
  failures: canonicalizeEVLLoops required an AVL after it became dead. Fixed
  by replacing the now-equivalent EVL counter with the canonical counter.
  Successful rerun is `round-09b-full-evl`, all 45 artifacts preserved.

An independent compiler-only `pressure-audit` enables vplan diagnostics using
exact saved full-TU commands. s1111 VF16 reports 56 vector registers versus
32 available, while its pristine forced assembly actually has no spill and
lowers the address to a scalar strided store. Its widened i64 address chain
(IV, shift, GEP) is a candidate source of phantom vector pressure. GESUMMV16
reports 40 vector registers and really spills; that exclusion must remain.
Any next change needs proof of affine stride, target native strided support,
exclusive address uses, scalar register accounting, and negative tests. No
blanket pressure bypass is authorized or planned.

Current profile defaults are still the initial software assumptions. Retained
round08 settings are explicit overrides: memory beat256, FP beat128, latency
startup5, slide startup8/quadratic3, FMA on, data-index-startup on. Full-EVL
experiment defaults off. Final parameter freeze, full audit/report, final
regression tests and actual automatic target satisfaction remain pending.

## Rounds 10–11 and freeze

- Round10 affine/native/exclusive address proof marks the widened IV, shift and
  GEP in s1111 as scalar GPR values. GESUMMV's genuine VF16 exclusion remains.
  All45 compile, but actual s1111 still chooses4: core automatic regret
  1.045130/1.226292, still failing. No-strided ablation preserved.
- Added diagnostic live-value audit, preserved in
  `pressure-audit/s1111-adjusted.log`. The remaining eight phantom vector
  registers belong to ExplicitVectorLength: its widened-induction consumer
  does not report scalar-only use, although its definition explicitly produces
  one scalar. Trust that execution property within the optional model register
  analysis; keep GPR accounting and every genuine vector interval.
- Round11 all45 compile. Actual choices equal forced-score argmins: core
  regret1.015112/1.093117, non-tie94.8718%, relative1.118502/1.246743;
  additional group regret1/1, non-tie100%, relative1.106704/1.188672.
  GESUMMV16 remains excluded with actual scalable spills; s1111 now chooses16.
- Frozen values and degrees of freedom are recorded in `frozen-model.json`.
  XiangShan explicit-profile defaults now encode the retained parameters.
  Saturn stays independent and unvalidated. Ordinary defaults remain modeloff
  and profilenone. Model full-EVL and scalar-address rules now defaulton when
  experimental ranking is explicitly enabled.
- New negative address-use test found a SmallPtrSet iterator epoch assertion:
  early-inc iteration is insufficient when erasing from that set. Prune into a
  temporary list, erase after traversal, and repeat to fixed point. No scoring
  parameters changed. The same test shows an extra vector index use retains
  vector IV/shift and reduces legal maximumVF to8; expecting16 was corrected.
- Full-EVL tests cover architectural maximum, bounded maximum, nonmultiple and
  unknown counts, actual selected-plan cleanup, verifier, and off/observe IR
  equality. Initial fixture vscale_range(1,8) contradicted Zvl128 (vscale>=2);
  corrected to(2,8). This was a fixture failure, not a relaxed proof.
- New TTI loaded-index test covers integer extension and startup presence;
  argument-only gathered pointers retain the negative case.
- Final frozen full collections: `final-xiangshan`, `final-saturn`,
  `final-none-observe`, all45 each. XiangShan all36 forced assemblies remain
  pristine-identical; three actual automatic changes are recorded. Saturn has
  software coverage only. None/observe all45 assemblies match pristine.
- Final initial RISC-V sweep168/169 passed; the only remaining failure was the
  negative fixture's nonexistent VF16 expectation. Corrected test passes in
  `scalar-register-tests-2.log`. Vectorize80 and Python17 pass. Final ablation
  and report generation continue; goal not marked complete at this point.

## Final completion audit

LLVM clang-format was applied, followed by successful build20 and complete
recollection into frozen-xiangshan/frozen-saturn/frozen-none-observe. Each has
45 successful full artifacts. Final source/diff/compiler hashes agree with all
three provenances. The formatted build reproduces every round11 forced score
and all automatic choices. All36 forced XiangShan assemblies still equal
pristine; all45 none/observe assemblies equal pristine. Original report and22
embedded inputs remain unchanged.

The strengthened full-EVL cleanup test now explicitly requires vector code:
two reductions under a pressure limit select VF4 from a larger EVL plan and
verify no residual EVL counter or vp.merge. An earlier absence-only check could
pass on scalar fallback and was replaced before final validation. All169 RISC-V
lit tests,80 Vectorize unit tests and17 Python tests pass. The final source
patch is implementation.patch; final-source-manifest.json binds its inputs.

FINAL_REPORT.md and final-report/*.csv contain baseline/best, every candidate,
actual selection, pair/relative/gap metrics, ablations, coverage and code shape.
completion-audit.json maps every required output to evidence. All in-scope
numeric thresholds pass in both groups. This completion is report-only,
validation-informed calibration; no new timing or Saturn hardware claim is made.
GESUMMV's extra scalar countdown operation is explicitly unmeasured, and the
remaining adjacent gap magnitudes are listed without treating them as exact.
