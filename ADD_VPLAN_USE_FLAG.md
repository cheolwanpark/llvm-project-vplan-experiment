# Add `-vplan-explain` / `-vplan-use-vf`

## Goal

Document a hidden debug CLI interface for `loop-vectorize` that lets a
developer:

- inspect each processed loop's VPlan candidates without changing behavior
- force execution of the plan selected by a specific `VF`
- do the above in multi-loop functions with per-loop overrides

The main use case is debugging candidate VPlans that are already built but not
chosen by the normal cost model.

## Status

Implemented in the current v2 patch:

- `-vplan-explain`
  - hidden debug flag in `LoopVectorize.cpp`
  - compact per-loop explain output
  - covers both inner-loop and outer-loop native planning paths
  - reports only loops that reach VPlan planning
- `-vplan-use-vf`
  - hidden debug flag in `LoopVectorize.cpp`
  - comma-separated per-loop override list
  - supports `fixed:<N>`, `scalable:<N>`, and `-`
  - loop indices match the explain-order `Loop[N]` numbering
  - forces execution on the legacy inner-loop path only
  - bypasses profitability-driven IC / outside-loop-work / epilogue selection
    for overridden inner loops
  - keeps legality checks and runtime-check correctness
  - soft-fails invalid, unavailable, and unsupported override requests
  - exposes a dedicated debug-only channel via
    `-debug-only=loop-vectorize-vplan-use-vf`

Deferred / TODO:

- outer-loop / VPlan-native forced execution
- any plan-index-based forcing interface
- broader `llvm-lit` coverage in this workspace once the missing LLVM test
  tools are built locally

Current loop numbering rule:

- `Loop[N]` is the explain-order index for loops that actually build or attempt
  to build VPlans
- it is not a raw `processLoop(...)` visitation count

Verified against:

- `llvmorg-22.1.1` (`fef02d48c`)

## Feasibility summary

This update is feasible.

Why:

- `LoopVectorizePass::runImpl()` already processes loops one-at-a-time
- each processed loop gets a fresh `LoopVectorizationPlanner`
- candidate `VPlans` are already available at the right point in the inner-loop
  path, before normal final selection
- the current planner/execution flow is `VF`-driven, not plan-index-driven

Main caveat:

- outer-loop/VPlan-native forced execution should still be deferred in v1
- analysis-only reporting can cover both paths
- forced-VF execution should initially target the legacy inner-loop path only

## Why `-vplan-use-vf` is sufficient in the current planner

Two facts matter here:

- a `VPlan` may contain multiple supported `VF`s
- in the current planner, each concrete `VF` maps to exactly one `VPlan`

That second property is not just accidental documentation wording. It is the
current planner invariant:

- `LoopVectorizationPlanner::getPlanFor(ElementCount VF)` says "At the moment,
  there is always a single VPlan for each VF"
- the implementation asserts that exactly one plan matches a given `VF`

Relevant code:

- `llvm/lib/Transforms/Vectorize/LoopVectorizationPlanner.h`
- `llvm/lib/Transforms/Vectorize/VPlan.cpp`

This means a forcing interface only needs to name the `VF`:

- selection can override the chosen `VF`
- the planner can recover the plan with `getPlanFor(ForcedVF)`
- execution already flows through `VF.Width` and `getPlanFor(VF.Width)`

So the documentation should not propose `-vplan-use-plan` as part of the
current interface. Plan indices are still useful for explain/debug output, but
they are observability details, not required control inputs.

Future-proofing note:

- if LLVM later supports multiple VPlans for the same concrete `VF`, this
  document should be revisited and a separate plan-index override can be
  reconsidered then

## Recommended scope

This section reflects the current narrower design, not the earlier broader
two-flag proposal.

Recommended first implementation:

- support `-vplan-explain` for every loop processed by `processLoop(...)`
- support forced execution via `-vplan-use-vf` on the legacy inner-loop path
- keep legality and correctness checks
- skip profitability-driven selection only for loops with an active VF override
- disable epilogue-vectorization auto-selection while forced-VF mode is active
- optionally keep outer-loop/VPlan-native forced execution unsupported in v1
- support multi-loop functions using loop indices and comma-separated VF
  override lists

Reason:

- inner-loop execution is already selected by a concrete `VF`
- `getPlanFor(VF)` is already the normal bridge from chosen width to chosen
  plan
- loops are already processed one-at-a-time, so the multi-loop problem is
  mainly one of CLI scoping

Relevant comment for native-path deferral:

- `llvm/lib/Transforms/Vectorize/LoopVectorize.cpp:6681`

## Analysis-only feature: `-vplan-explain`

Recommended debug feature:

- `-vplan-explain`

Purpose:

- run the normal vectorizer path with no behavioral change
- emit per-loop analysis describing:
  - loop index
  - whether the loop uses the inner-loop path or outer-loop native path
  - number of built `VPlan` candidates
  - candidate plan indices
  - supported `VF`s for each plan
  - optionally the selected final `VF` and selected plan index after normal
    profitability

This should be treated as a pure observability feature:

- it should not modify the selected `VF`
- it should not change candidate generation
- it should not disable profitability gates
- current implementation emits only through `LLVM_DEBUG`

Primary value:

- make plan/VF relationships visible
- make multi-loop use of `-vplan-use-vf` practical
- reduce the need to read raw `printPlans()` dumps

## Recommended flag semantics

### Flag definitions

Add hidden opts near the existing VPlan debug flags in:

- `llvm/lib/Transforms/Vectorize/LoopVectorize.cpp`

Recommended forms:

```cpp
static cl::opt<bool> VPlanExplain(
    "vplan-explain", cl::init(false), cl::Hidden,
    cl::desc("Explain per-loop VPlan candidates and selected vectorization "
             "factor without changing the normal vectorizer decision."));

static cl::opt<std::string> VPlanUseVF(
    "vplan-use-vf", cl::init(""), cl::Hidden,
    cl::desc("Comma-separated per-loop VFs to force, in loop explanation "
             "order. Example: fixed:4,-,scalable:4"));
```

### Recommended global backdoor pattern

If a debug-only global override is needed, prefer hidden `cl::opt` globals in
`LoopVectorize.cpp`, not a separate mutable singleton.

Reason:

- this file already uses many hidden `cl::opt` debug/testing backdoors
- in LLVM style, hidden `cl::opt` is already the normal process-global
  singleton-like mechanism
- the new behavior only needs explain state plus a forced-VF query

Recommended compromise if cleaner call sites are desired:

- keep `cl::opt` globals as the source of truth
- optionally add a tiny file-local helper that parses/queries them

Example shape:

```cpp
namespace {
struct ForcedVPlanConfig {
  bool explain() const;
  std::optional<ElementCount> getForcedVF(unsigned LoopIndex) const;
  bool anyOverride(unsigned LoopIndex) const;
};

ForcedVPlanConfig getForcedVPlanConfig();
} // namespace
```

## Indexing rules

### Plan indexing

Recommend `0`-based indexing in debug output.

Reason:

- `VPlans` is a `SmallVector<VPlanPtr, 4>`
- internal code naturally uses vector indices

Plan indices remain useful for:

- `-vplan-explain` output
- optional `printPlans()` discoverability improvements

Plan indices are not part of the recommended forcing interface.

### Loop indexing

For multi-loop functions, also recommend `0`-based loop indexing.

Recommended meaning of loop index:

- the ordinal of the loop as processed by `processLoop(...)`
- `-vplan-explain` should print this index explicitly
- `-vplan-use-vf=` should interpret comma-separated elements by the same loop
  index

Example:

```text
-vplan-explain
LV: Loop[0] path=inner
LV: Loop[1] path=inner
LV: Loop[2] path=outer-native

-vplan-use-vf=fixed:4,-,scalable:4
```

Recommended placeholder for "no override for this loop":

- `-`

Short-list rule:

- if the list is shorter than the number of processed loops, remaining loops use
  normal behavior
- if the list is longer, extra elements should be ignored with a debug remark or
  soft error

This positional scheme is feasible because loops are already processed
one-at-a-time. The only new requirement is to maintain a pass-local loop
ordinal.

## Precedence rules

Recommended precedence for a given loop index:

1. If `-vplan-use-vf` provides an entry for the current loop:
   - override the normal selected `VF`
   - resolve the plan by `getPlanFor(ForcedVF)`
2. If no override applies to the current loop:
   - keep current behavior unchanged
3. `-vplan-explain` is orthogonal:
   - it may be enabled together with the VF override list
   - it does not itself force any decision

## Important interaction with existing `-force-vector-width`

Current inner-loop planning uses a user-provided width if present.

Relevant code:

- `llvm/lib/Transforms/Vectorize/LoopVectorize.cpp`

If `UserVF` is already set, `plan()` may build only that VF. That aligns well
with the recommended forcing interface:

- a loop-local `-vplan-use-vf` override can reuse the existing VF-driven
  selection path
- the documentation no longer needs any special "build all plans, then choose
  plan N" rule

Recommended rule:

- when a loop has an active `-vplan-use-vf` entry, override `UserVF` before
  planning so the requested VF is guaranteed to be built

## Interaction with existing hint/global override plumbing

There is already pre-existing global/hint plumbing for VF and IC.

Implication:

- reusing the existing width-oriented plumbing is feasible for a VF-only
  override
- a separate plan-index interface is not needed to describe current behavior

Why the new list form is still feasible:

- `LoopVectorizePass::runImpl()` already processes loops one at a time
- each processed loop gets a fresh `LoopVectorizationPlanner`
- therefore a simple pass-local loop ordinal is enough to scope debug overrides
- no cross-loop `VPlan` container is required

## Current code map

### Where VPlans are stored / declared

- `LoopVectorizationPlanner` declaration:
  `llvm/lib/Transforms/Vectorize/LoopVectorizationPlanner.h`
- `VPlans` member:
  `llvm/lib/Transforms/Vectorize/LoopVectorizationPlanner.h`
- `getPlanFor(ElementCount VF)`:
  declaration in `LoopVectorizationPlanner.h`, impl in `VPlan.cpp`
- `computeBestVF()`:
  impl in `LoopVectorize.cpp`
- `executePlan(...)`:
  impl in `LoopVectorize.cpp`

### Where candidate VPlans are built

Inner-loop path:

- planner entry in `LoopVectorize.cpp`
- candidate building in `LoopVectorize.cpp`

Outer-loop native path:

- planner entry in `LoopVectorize.cpp`
- native-path builder in `VPlan.cpp` / `LoopVectorize.cpp`

### Where final selection happens today

Inner loop:

- `LVP.plan(UserVF, UserIC)`
- `VectorizationFactor VF = LVP.computeBestVF()`
- `LVP.getPlanFor(VF.Width)`
- `LVP.executePlan(VF.Width, ...)`

Outer loop:

- `const VectorizationFactor VF = LVP.planInVPlanNativePath(UserVF)`
- `VPlan &BestPlan = LVP.getPlanFor(VF.Width)`
- `LVP.executePlan(...)`

The important point is that both paths already become concrete through `VF`.

### Where loop indexing naturally fits

- loops are collected into a worklist in `LoopVectorizePass::runImpl()`
- each loop is then processed one-by-one by `processLoop(L)`
- a pass-local counter can be incremented immediately before or inside
  `processLoop(L)`

### Where plan dumps happen today

- `printPlans()` prints every `VPlan`
- `VPlan::getName()` includes `VF={...}` and `UF={...}`

## Important companion change: print plan indices

Plan indices are still useful as debug output even though they are not needed
for forcing.

Strongly recommended companion change:

- modify `LoopVectorizationPlanner::printPlans(raw_ostream &O)` to prefix each
  plan with an index

For `-vplan-explain`, a lighter-weight structured summary is also recommended,
because full `printPlans()` output is too verbose for multi-loop diagnosis.

Recommended summary shape:

```text
LV: Loop[3] path=inner
LV:   VPlan[0] VFs={1,2,4}
LV:   VPlan[1] VFs={8}
LV:   selected VF=4 plan=0
```

Here `plan=0` is informative output derived from `getPlanIndexForVF(SelectedVF)`,
not a separate CLI selector.

## Recommended implementation plan

### 1. Add hidden CLI opts

File:

- `llvm/lib/Transforms/Vectorize/LoopVectorize.cpp`

Recommended additions:

- `VPlanExplain`
- `VPlanUseVF` as a comma-separated string list

### 2. Add planner helpers

File:

- `llvm/lib/Transforms/Vectorize/LoopVectorizationPlanner.h`

Recommended helpers:

```cpp
unsigned getNumPlans() const { return VPlans.size(); }
VPlan &getPlanByIndex(unsigned I) const;
std::optional<unsigned> getPlanIndexForVF(ElementCount VF) const;
```

`getPlanByIndex()` and `getPlanIndexForVF()` are for explain/debug reporting.
The forcing path itself only requires `getPlanFor(ForcedVF)`.

### 3. Add small local utilities to parse per-loop VF overrides

File:

- `llvm/lib/Transforms/Vectorize/LoopVectorize.cpp`

Recommended approach:

- parse comma-separated VF list into
  `SmallVector<std::optional<ElementCount>>`
- add a local helper to query the override for a specific loop index
- parse either:
  - `fixed:4`
  - `scalable:4`
  - `-` meaning "no override for this loop"

Recommended validation:

- reject malformed VF spellings
- reject malformed list entries early
- if the current function has fewer loops than the list length, ignore the tail
  with a debug remark

### 4. Track a pass-local loop index

File:

- `llvm/lib/Transforms/Vectorize/LoopVectorize.cpp`

Recommended approach:

- keep a counter in `LoopVectorizePass` or local to `runImpl()`
- increment it exactly once per processed loop
- pass the current loop index to code that evaluates `VPlanExplain` and
  `VPlanUseVF`

### 5. Override inner-loop selection flow

Primary hook point:

- inner-loop flow in `LoopVectorize.cpp`

Recommended flow:

1. Read `UserVF` / `UserIC` as today.
2. Resolve current loop index and fetch the per-loop forced VF.
3. If the current loop has a forced VF entry, override local `UserVF` before
   calling `LVP.plan(...)`.
4. Call `LVP.plan(UserVF, UserIC)`.
5. If `VPlanExplain` is active, emit the per-loop candidate summary after
   planning and before final selection.
6. If forced-VF mode is active for this loop:
   - bypass `computeBestVF()`
   - resolve `BestPlan` directly with `getPlanFor(ForcedVF)`
7. Otherwise keep the normal selection flow.
8. Skip or clamp downstream profitability choices only for loops with an active
   override.

### 6. When forced mode is active, bypass profitability-driven choices

Recommended forced-mode behavior for the targeted loop:

- `VF`: forced by `-vplan-use-vf`
- `IC`: do not call `selectInterleaveCount`; use:
  - `UserIC` if explicitly set and valid
  - otherwise `1`
- epilogue vectorization: disable in v1
- outside-loop work profitability gate: skip in forced mode
- non-target loops in the same function: keep existing behavior unchanged

Do not skip legality/correctness checks.

### 7. Keep runtime-check correctness

Recommended:

- still create runtime checks if needed
- still bail if SCEV or memory checks are provably always failing

The new flag should ignore profitability, not correctness.

### 8. Add `-vplan-explain` reporting

Recommended hook points:

- inner-loop path: immediately after `LVP.plan(UserVF, UserIC)`
- outer-loop native path: immediately after `planInVPlanNativePath(...)` builds
  candidate plans

Suggested output content per loop:

- `Loop[Index]`
- path kind
- number of plans
- for each plan:
  - plan index
  - supported VFs
- if normal selection continues:
  - chosen VF
  - chosen plan index, if any

### 9. Add debug diagnostics

Recommended debug-only logging:

- current loop index
- loop path kind (`inner` / `outer-native`)
- number of plans built
- candidate VFs per plan
- selected plan index
- selected VF
- whether a VF override was active
- whether interleave/epilogue profitability was bypassed

## Recommended v1 limitations

These limitations keep the patch small and defensible:

- inner-loop path only for forced execution
- `-vplan-explain` may still cover outer loops
- no outer-loop native-path forced execution
- no forced epilogue vectorization

Why outer loop should be deferred:

- current outer-loop path usually builds one plan with one VF
- the file already contains a comment that the native path cannot currently
  choose among multiple generated VPlans

## Error handling recommendations

Prefer soft failure for per-loop invalid requests, not hard process aborts.

Recommended cases:

- malformed entry in a comma-separated VF override list
- requested VF is not available for the current loop
- requested VF cannot be planned for the current target/path

Recommended behavior:

- emit `LLVM_DEBUG(...)`
- emit `reportVectorizationInfo(...)` or `reportVectorizationFailure(...)`
- return `false` for that loop

## Specific code edits likely needed

### Must touch

- `llvm/lib/Transforms/Vectorize/LoopVectorize.cpp`
- `llvm/lib/Transforms/Vectorize/LoopVectorizationPlanner.h`

### Probably touch

- `llvm/lib/Transforms/Vectorize/VPlan.cpp`

Reason:

- these are `cl::opt` globals consumed by the pass implementation
- explain output already needs planner/query helpers

## Testing strategy

### 1. Analysis discoverability

Add a focused test for `-vplan-explain`:

- new suggested file:
  `llvm/test/Transforms/LoopVectorize/vplan-explain.ll`

Suggested checks:

- loop index is shown
- plan indices are shown
- each plan reports its supported VFs
- enabling `-vplan-explain` does not change the chosen vectorization result

### 2. Plan numbering / debug discoverability

If `printPlans()` is changed to show indices:

- update or add a debug-only test under
  `llvm/test/Transforms/LoopVectorize/VPlan/`

### 3. Multi-plan candidate case

Use or adapt a case where different VFs naturally lead to different plans.

Goal:

- show that `-vplan-explain` makes the VF-to-plan relationship visible

### 4. New focused regression file for forced selection

Suggested new test:

- `llvm/test/Transforms/LoopVectorize/vplan-use-vf.ll`

Suggested checks:

- `-vplan-use-vf=fixed:4` forces a single-loop case to use VF 4
- invalid VF causes graceful non-vectorization
- explain output still reports the selected plan index for the forced VF

### 5. Multi-loop positional-list case

Suggested additional test:

- `llvm/test/Transforms/LoopVectorize/vplan-use-vf-multiloop.ll`

Suggested checks:

- `-vplan-explain` reports `Loop[0]`, `Loop[1]`, ...
- `-vplan-use-vf=fixed:4,-,fixed:8` only affects loops 0 and 2
- shorter lists leave remaining loops unchanged

### 6. Keep outer-loop forced path unchanged

Optional negative test:

- if `-enable-vplan-native-path` plus forced-VF execution is unsupported in v1,
  add a test that the flag is ignored or produces a remark

## Notes about existing flow that matter

### `executePlan()` mutates IR heavily

The new feature should still execute only one chosen plan.

`executePlan()` and `VPlan::execute()` mutate CFG/DT/SE and may delete the
original loop. So this feature is about forcing which `VF` and therefore which
plan gets executed, not about lowering multiple plans.

### `VPlan::duplicate()` is not enough for "run every plan"

`duplicate()` clones the plan graph but still preserves IR-backed live-ins and
wrapped IR blocks.

This is relevant only as background; it does not block the current
forced-single-VF feature.

## Useful search commands

```bash
rg -n "vplan-explain|vplan-use-vf|EnableVPlanNativePath|VPlanBuildStressTest|printPlans|getPlanFor|computeBestVF|executePlan" llvm/lib/Transforms/Vectorize
rg -n "class LoopVectorizationPlanner|VPlans;|getPlanFor|getPlanIndexForVF|computeBestVF|executePlan" llvm/lib/Transforms/Vectorize/LoopVectorizationPlanner.h
rg -n "buildVPlansWithVPRecipes|tryToBuildVPlanWithVPRecipes|buildVPlans|tryToBuildVPlan" llvm/lib/Transforms/Vectorize
rg -n "selectInterleaveCount|selectEpilogueVectorizationFactor|isOutsideLoopWorkProfitable|processLoop" llvm/lib/Transforms/Vectorize/LoopVectorize.cpp
```
