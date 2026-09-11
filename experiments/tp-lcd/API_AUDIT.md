> This file records the starting API investigation. Statements below about work
> remaining refer to that stage. The implemented and validated final behavior is
> documented in [FINAL_REPORT.md](FINAL_REPORT.md).

# Source audit at ca7933e47d3a3451d81e72ac174dcb5aa28b59d1

These are current source observations, not performance measurements or claims
that the TP/LCD implementation exists. No model or target parameters have been
changed. The initial checkout was `vplan-cost-adjustment` at the historical
`89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb`, with only the two supplied documents
untracked. The explicitly requested preparation fetched `ca7933e4` from origin
and reset local `main` to that exact commit, preserving the old branch and user
documents. Both `HEAD` and `refs/heads/main` were verified at the requested SHA.
No applicable AGENTS.md was found in ancestor directories or LLVM/Clang/CMake.

## Baseline observation and automatic search

- `LoopVectorize.cpp:901`, `selectUserVectorizationFactor`, calls
  `expectedCost(UserVF)` to validate forced costs. `expectedCost` at line 5141
  prints individual IR costs with `-debug-only=loop-vectorize`. For vector VFs
  it sums them without scalar block probability division. This provides a
  pristine **legacy IR cost**, not final VPlan cost.
- `computeBestVF` at line 7150 returns zero cost immediately for a single plan
  with a single VF. A new forced-candidate TP/LCD diagnostic path will therefore
  be required. Pristine assembly and existing legacy costs can be preserved
  before that observation change.
- Automatic VPlan cost at line 6992 is `precomputeCosts + Plan.cost`; debug output
  prints the total `Cost for VF` without rounding and an estimated per-lane cost
  rounded to one decimal. The collector uses the unrounded total.
- `computeBestVF` prints actual selected VF and visits each plan/VF, subject to
  vector generation, legality, size policy, and register-pressure checks. Its
  debug-build legacy-agreement assertion must be accounted for when introducing
  experimental ranking, while retaining the assertion for normal behavior.
- `riscv-v-register-bit-width-lmul` exists and defaults to 2 (RISC-V TTI line 28).
  Expanded automatic collection uses 8. Actual visited candidates still require
  inspection; the flag alone is not proof of four-candidate coverage.
- `getVScaleForTuning` (RISC-V TTI line 376) returns real minimum VLEN / 64.
  The report's minimum 128 therefore supplies estimated vscale 2, independently
  of the unknown upper VLEN. No maximum VLEN is added to the measured-condition
  baseline.
- `riscv-enable-pipeliner` exists (`RISCVTargetMachine.cpp:103`) and defaults to
  false; the explicit historical `false` option can be retained unchanged.

## Cost units and target selection

- `xiangshan-kunminghu` has `NoSchedModel` (`RISCVProcessors.td:711`). There is no
  Saturn CPU/profile in that processor file. An independent explicit software
  tuning profile remains to be added; ISA features cannot identify Saturn.
- Vector arithmetic (`RISCVTargetTransformInfo.cpp:2589`) handles only reciprocal
  throughput in the target implementation. Latency calls go to BasicTTI before
  vector legalization or opcode-specific RISC-V throughput handling.
- `getLMULCost` (`RISCVISelLowering.cpp:3268`) assumes m1 reciprocal throughput 1
  and scales by LMUL and DLenFactor. These are generic costs, not measured
  XiangShan execution cycles.
- Ordinary memory (`RISCVTargetTransformInfo.cpp:2198`) uses legalization cost
  for non-throughput kinds and multiplies non-code-size vector costs by LMUL.
  Gather (line 1179) handles only throughput, at estimated element count cost;
  other kinds fall back. Strided memory (line 1248) charges estimated elements
  times scalar memory cost. These differing approximations cannot be assumed to
  give calibrated and comparable TP/LCD cycle units.
- Splice (TTI line 901) is two slide instructions with immediate/register choice
  based on index. `getRISCVInstructionCost` uses the same slide helpers for
  throughput and latency; both slide helpers currently return LMUL cost.
  This supports a small shared primitive implementation for experimental target
  profiles, rather than a new scheduling simulator.

## VPlan accounting constraints

- `VPCostContext` lives in `VPlanHelpers.h:329`, not a separate cost-model header.
  `getLegacyCost` forwards to the loop cost model's current kind; changing only
  the context's `CostKind` does not make that fallback a latency query.
- `VPRecipeBase::cost` at `VPlanRecipes.cpp:265` consults legacy ignore/precompute
  state. Latency graph analysis must use an independent helper and explicitly
  supported TTI latency queries.
- `precomputeCosts` at `LoopVectorize.cpp:6860` charges original induction and
  exit expressions, branches and scalarized/forced-scalar operations, then marks
  their underlying instructions skipped. A domain sum cannot simply combine
  a second recipe walk with this total without auditing omissions/duplicates.
- `VPlan::cost` at `VPlan.cpp:1004` costs the vector-loop region, then visits
  external basic blocks only to check for invalid costs. Domain accounting must
  aggregate the whole loop before taking max and exclude those external costs.
- Header PHI cost is zero/control-flow TTI cost (`VPlanRecipes.cpp:2341`). Its
  recurrence latency must come from closed def-use/backedge paths, not PHI cost.

- `VPWidenInductionRecipe` (`VPlan.h:2085`) explicitly makes
  `getBackedgeValue/getBackedgeRecipe` unreachable: it synthesizes its update.
  Its operand 1 is a step, not a backedge. A dependency graph must handle these
  recipes before the generic `VPHeaderPHIRecipe` case. Canonical/EVL IV recipes
  are converted to scalar PHIs before execution and require their own audit.
- `FirstOrderRecurrenceSplice` (`VPlanRecipes.cpp:1198`) has a direct TTI shuffle
  cost path, usable with an independent latency context. The Jacobi source writes
  a separate output array; its adjacent input loads are feed-forward. A PHI fed
  by an independent load must not acquire a closed-cycle LCD simply because its
  consumer chain is long.

Scalar induction lowering, concrete reduction/splice plans, FMA contraction
evidence, memory dependence fallback, and generated code-shape comparison remain
to be inspected in the pristine compiler output before model implementation.

## Additional baseline audit points

- `MemoryDepChecker::getDependences` can return null when recording is
  unavailable; an empty recorded list is distinct from that state. Lexically
  forward dependences are not necessarily proof of no loop-carried latency,
  and bounded safe vector width does not model that latency. The experimental
  graph must conservatively fall back for unsupported memory-carried cases.
  See `LoopAccessAnalysis.h:90-260`. Same-index read-modify-write can be deemed
  safe before enumeration, as documented in that API.
- Current register-pressure calculation (`VPlanAnalysis.cpp:401`) treats live
  VPValues as separate intervals and has artificial-value exclusions. Whether
  this excludes any measured candidate must be established from actual expanded
  automatic output before changing that logic. No pressure bypass is justified
  merely by the report's no-spill observations.
- The source revision identifies itself as LLVM 22.1.8 in
  `cmake/Modules/LLVMVersion.cmake`; installed Homebrew Clang also has that version
  string. Therefore compiler version strings alone cannot prove baseline
  identity. The collector checks the successfully built compiler's SHA-256.
- The collector captures machine-function output after Virtual Register Rewriter
  while checking that diagnostic observation preserves assembly. Frame-index
  evidence can distinguish compiler allocation from the subsequently applied
  assembly workaround. This is an observation of the existing lowering, not a
  cost-model lowering pipeline.

- Following the arithmetic fallback completely reaches
  `TargetTransformInfoImpl.h:668`: it assumes **3 cycles for FP latency** and
  1 for most other arithmetic, independent of vector width. BasicTTI at line
  1053 forwards non-throughput queries there. Thus the existing arithmetic
  latency is a cycle-like generic assumption, while RISC-V throughput is an
  LMUL/DLenFactor estimate. Explicit profiles need comparable cycle-like units
  and width behavior; a blanket claim that every fallback is unit cost would
  be incorrect.
