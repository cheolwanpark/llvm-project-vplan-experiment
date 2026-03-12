# `-vplan-use-vf` limitations

## What the flag means

`-vplan-use-vf` is a hidden debug/testing override in
`llvm/lib/Transforms/Vectorize/LoopVectorize.cpp`.

It does not mean:

- "prefer this VF if it is profitable"
- "use this VF but otherwise keep the normal vectorizer decision unchanged"

It means:

- for each processed loop, look up the per-loop override entry by the
  `Loop[N]` explain/debug order
- if the entry is `fixed:<N>` or `scalable:<N>`, force selection of that VF
  if a built `VPlan` for that loop supports it
- if the entry is `-`, do not override that loop

Supported syntax:

- `fixed:<N>`
- `scalable:<N>`
- `-`

Current parsing constraints:

- `N` must be a non-zero power of two
- malformed entries are rejected
- extra comma-separated entries past the number of processed loops are ignored

Current support constraints:

- forcing is implemented only for the legacy inner-loop path
- forced execution is unsupported for outer loops in the VPlan-native path
- if no built `VPlan` supports the requested VF, the loop is not vectorized for
  that request

## What is affected

With an active `-vplan-use-vf` override, LLVM deliberately changes several
normal post-planning decisions for that loop.

### 1. Final VF selection

Normal behavior:

- the planner builds candidate `VPlan`s
- `computeBestVF()` chooses the profitable final `VF`

Forced behavior:

- the requested VF is selected directly if `LVP.hasPlanWithVF(ForcedVF)`

This is a hard override of final VF selection, not a hint.

### 2. Interleave-count selection is bypassed

Normal behavior:

- LLVM runs `selectInterleaveCount(...)`

Forced behavior:

- LLVM does not run the interleave heuristic
- `IC` becomes `UserIC` if one was provided, otherwise `1`

Practical effect:

- no profitability-driven IC choice is made for the forced loop
- forcing a VF does not imply that LLVM also chooses the normal best `UF/IC`

### 3. Outside-loop-work profitability is bypassed

Normal behavior:

- LLVM checks `isOutsideLoopWorkProfitable(...)`
- this accounts for work outside the main vector loop, including:
  - runtime safety checks
  - extra early-exit handling work
  - middle-block cost
  - minimum profitable trip-count computation

Forced behavior:

- LLVM skips this profitability gate for the forced loop

Practical effect:

- a forced VF can be executed even when the full end-to-end profitability model
  would have rejected it

### 4. Epilogue vectorization is disabled

Normal behavior:

- LLVM may choose a separate epilogue VF with
  `selectEpilogueVectorizationFactor(...)`

Forced behavior:

- LLVM disables epilogue vectorization for the forced loop

Practical effect:

- the main loop uses the forced VF
- the remainder is not given a separately selected vectorized epilogue

## What is not affected

`-vplan-use-vf` does not bypass correctness and availability checks.

These still apply:

- legality still has to permit vectorization
- the requested VF must correspond to a built `VPlan`
- runtime checks are still created when needed
- if SCEV or memory runtime checks are known to fail unconditionally, LLVM
  still bails out

So the flag is not "vectorize no matter what". It is "force this VPlan VF,
while keeping correctness checks".

## What the flag does not represent

`-vplan-use-vf` is not a faithful model of the normal end-to-end vectorizer
decision.

In particular, it does not preserve:

- normal final VF profitability selection
- normal interleave-count selection
- normal outside-loop profitability gating
- normal epilogue-vectorization selection

Because of that, this flag should not be interpreted as:

- "what LLVM would normally choose"
- "the full profitability model evaluated at a fixed VF"

It is better understood as:

- "force execution of the VPlan associated with this VF, then keep only the
  correctness-critical gates"

## Why these stages are not already covered by VPlan selection

The forced VF picks a main-loop `VPlan`, but some later decisions are still
separate from that plan selection.

- Interleave count is chosen by a separate heuristic after VF selection.
- Outside-loop profitability is a separate gate that adds runtime-check and
  control-flow overheads not captured by main-loop plan selection alone.
- Epilogue vectorization is a separate follow-up decision that may use a
  different VF.

So these stages are not simply "already inside the selected VPlan".

## Cost-model evaluation caveat

If the goal is to evaluate the normal loop-vectorizer cost model, using
`-vplan-use-vf` is usually not sufficient, because it skips some profitability
stages on purpose.

## Why the bypasses are intentional

The current bypass behavior is intentional for a specific evaluation goal:

- enumerate candidate `(VPlan, VF)` pairs that the planner considered
- force each candidate to execute
- compare the observed result against the cost model's predicted ordering

For that goal, keeping all later heuristics enabled would mix two different
questions:

- "How good is this candidate `(VPlan, VF)` itself?"
- "Would the full end-to-end vectorizer pipeline still choose this loop shape
  after later profitability and epilogue decisions?"

The current bypasses try to isolate the first question.

### Why bypass interleave-count selection

If normal IC selection stays enabled, then the measured result is no longer just
the forced `(VPlan, VF)` candidate. It becomes:

- forced `VF`
- plus a separately chosen profitability-driven `IC`

That makes it harder to compare candidate rankings fairly, because differences
may come from later IC heuristics rather than from the candidate `VF` itself.

There is an additional semantic problem for `fixed:1`:

- allowing normal IC selection with scalar `VF=1` can still produce scalar
  interleaving
- that means a request that looks like "force scalar" may still transform the
  loop substantially

By bypassing IC selection, forced-VF mode avoids that ambiguity and keeps the
meaning closer to "execute the candidate for this VF".

### Why bypass outside-loop profitability

Outside-loop profitability adds costs that are not part of the main-loop
candidate itself:

- runtime safety checks
- early-exit handling work
- middle-block overhead
- minimum profitable trip-count filtering

If those remain enabled, a candidate may be rejected for reasons that are
downstream of the chosen `(VPlan, VF)` pair. That is useful for normal LLVM
decision making, but it is not ideal when the experiment wants to force all
candidate pairs and compare their realized behavior.

In other words, bypassing this stage helps answer:

- "What happens if this candidate is actually executed?"

rather than:

- "Would LLVM's full profitability pipeline still admit this candidate?"

### Why disable epilogue vectorization

Epilogue vectorization is another later choice that may introduce a different
VF for the remainder loop.

If it stays enabled, the forced experiment stops being about a single forced
main-loop candidate and becomes a mixture of:

- forced main-loop `VF`
- separately selected epilogue `VF`

That makes attribution harder. Observed behavior is no longer explained only by
the forced candidate `(VPlan, VF)`.

Disabling epilogue vectorization keeps the experiment focused on the selected
main-loop candidate and avoids contaminating the comparison with a second
profitability-driven VF decision.

## Practical interpretation

Because of these bypasses, `-vplan-use-vf` is best understood as an
experimental mechanism for isolated candidate realization, not as a faithful
replay of LLVM's full final vectorization decision.

Reasonable use cases for `-vplan-use-vf`:

- debugging code generation for a specific candidate VF
- comparing code shape for an already-built but normally unchosen VPlan
- validating that a candidate VPlan is executable
- evaluating the realized behavior of forced candidate `(VPlan, VF)` pairs and
  comparing that against the cost model's predicted ranking

Better tool for observing the normal decision:

- `-vplan-explain`

That flag is intended to explain candidate VPlans and the selected VF without
changing the normal vectorizer decision.
