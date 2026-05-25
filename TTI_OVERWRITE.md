# RISC-V TTI Cost-Model Knob Overrides

Branch `tti-overwrite` adds command-line knobs to three RVV memory cost
functions in `llvm/lib/Target/RISCV/RISCVTargetTransformInfo.cpp` so that
cost-model sweeps can be driven from the outside without recompiling.

When no knob is set, behavior is **identical to main**.

## Usage

Two equivalent input channels. Both accept the same `key=value` entries.
Unknown keys, non-integer values, or malformed entries are fatal.

### Inline

```bash
opt -mtriple=riscv64 -mattr=+v \
    -riscv-tti-overrides=gather.setup=2,gather.overhead=3,strided.overhead=4 \
    ...
```

### File

```bash
opt ... -riscv-tti-config=/path/to/knobs.cfg
```

`knobs.cfg`:

```
# one key=value per line; '#' comments and blank lines OK
gather.setup    = 10
gather.overhead = 2
strided.overhead = 3
interleave.seg.factor_overhead = 4
```

Both can be combined. File is applied first; `-riscv-tti-overrides` then
wins for conflicting keys.

### Available knobs

| Key | Default | Affects |
|---|---|---|
| `gather.setup` | 0 | `getGatherScatterOpCost` |
| `gather.overhead` | 1 | `getGatherScatterOpCost` |
| `strided.setup` | 0 | `getStridedMemoryOpCost` |
| `strided.overhead` | 1 | `getStridedMemoryOpCost` |
| `interleave.seg.setup` | 0 | Interleaved, optimized-segment path |
| `interleave.seg.mem_overhead` | 1 | " |
| `interleave.seg.factor_overhead` | 1 | " |
| `interleave.vlseg.setup` | 0 | Interleaved, generic vlseg/vsseg fallback |
| `interleave.vlseg.overhead` | 1 | " |
| `interleave.shuf.setup` | 0 | Interleaved, shuffle fallback (load + store) |
| `interleave.shuf.mem_overhead` | 1 | " |
| `interleave.shuf.shuf_overhead` | 1 | " |
| `shuffle.setup` | 0 | `getShuffleCost` (wrapper, all kinds) |
| `shuffle.overhead` | 1 | " |
| `vrgather.vv.setup` | 0 | `VRGATHER_VV` opcode in `getRISCVInstructionCost` |
| `vrgather.vv.overhead` | 1 | " |
| `reduction.arith.setup` | 0 | `getArithmeticReductionCost` (non-i1, non-ordered-FP path) |
| `reduction.arith.split_overhead` | 1 | " |
| `reduction.arith.core_overhead` | 1 | " |
| `reduction.minmax.setup` | 0 | `getMinMaxReductionCost` (non-i1 paths) |
| `reduction.minmax.split_overhead` | 1 | " (smax/smin/umax/umin/maxnum/minnum sub-path) |
| `reduction.minmax.extra_overhead` | 1 | " (maximum/minimum NaN-handling sub-path) |
| `reduction.minmax.core_overhead` | 1 | " |

## Cost formulas (only when overridden)

`anyOverride("<prefix>.")` gates each branch. If no knob under that prefix
is set, the original main formula runs unchanged.

### `getGatherScatterOpCost`
```
cost = gather.setup + NumLoads * LaneMemCost * gather.overhead
```
`LaneMemCost = getMemoryOpCost(Opcode, elemType, Align, 0, CostKind)`.
Replaces `NumLoads * TCC_Basic`.

### `getStridedMemoryOpCost`
```
cost = strided.setup + NumLoads * MemOpCost * strided.overhead
```
Replaces `NumLoads * MemOpCost`.

### `getInterleavedMemoryOpCost` — three sub-paths

**Optimized segment** (`ST->hasOptimizedSegmentLoadStore(Factor)`):
```
cost = interleave.seg.setup
     + LT.first * ( MemOpCost(VTy) * interleave.seg.mem_overhead
                  + Factor * LMULCost(SubVecVT) * interleave.seg.factor_overhead )
```

**vlseg/vsseg fallback** (otherwise, when the access type is legal):
```
cost = interleave.vlseg.setup
     + NumLoads * MemOpCost(elemTy) * interleave.vlseg.overhead
```

**Shuffle fallback** (legality failed; load loop and the Factor=2 store):
```
cost = interleave.shuf.setup
     + MemCost * interleave.shuf.mem_overhead
     + AccumulatedShuffleCost * interleave.shuf.shuf_overhead
```
Load path accumulates one `getShuffleCost` per index, then applies the
overhead once. Store path uses its single `ShuffleCost`.

### `getShuffleCost` (wrapper)
```
cost = shuffle.setup + originalShuffleCost * shuffle.overhead
```
Applied once around the entire function result — the inner per-kind formulas
(SK_PermuteSingleSrc, SK_Reverse, SK_Broadcast, etc.) are unchanged. Setup is
added exactly once per `getShuffleCost` call.

### `VRGATHER_VV` (inside `getRISCVInstructionCost`)
```
opcodeCost = vrgather.vv.setup + TLI->getVRGatherVVCost(VT) * vrgather.vv.overhead
```
Replaces the single `TLI->getVRGatherVVCost(VT)` contribution. Note: setup is
added **per VRGATHER_VV opcode occurrence** within a single
`getRISCVInstructionCost` call. Most call sites pass exactly one VRGATHER_VV
opcode; the `SK_Transpose` / `SK_PermuteTwoSrc` paths in `getShuffleCost` pass
two (`{VRGATHER_VV, VRGATHER_VV}`), so the setup will count twice for those.
Because `getShuffleCost`'s outer wrapper (`shuffle.*`) sits above this,
combining the two prefixes multiplies: `cost = shuffle.setup + (… +
N * (vrgather.vv.setup + base * vrgather.vv.overhead) + …) * shuffle.overhead`.

### `getArithmeticReductionCost`
Only the standard (non-i1, non-ordered-FP) trailing return is hooked:
```
cost = reduction.arith.setup
     + SplitCost * reduction.arith.split_overhead
     + CoreCost  * reduction.arith.core_overhead
```
`SplitCost = (LT.first > 1) ? (LT.first - 1) * instrCost(SplitOp) : 0`.
`CoreCost  = instrCost({VMV_S_X?, VRED{SUM,OR,XOR,AND}_VS, VMV_X_S})` or the
equivalent FP add reduction opcodes. The i1 mask-reduction paths and the
FP ordered-sum loop keep their original formulas (their structure does not fit
the Split+Core model).

### `getMinMaxReductionCost`
Two sub-paths share the `reduction.minmax.` prefix.

**Standard sub-path** (smax/smin/umax/umin/maxnum/minnum):
```
cost = reduction.minmax.setup
     + SplitCost * reduction.minmax.split_overhead
     + CoreCost  * reduction.minmax.core_overhead
```
`SplitCost` is identical in shape to the arith case; `extra_overhead` is
unused here.

**NaN-handling sub-path** (`Intrinsic::maximum` / `Intrinsic::minimum`):
```
cost = reduction.minmax.setup
     + ExtraCost * reduction.minmax.extra_overhead
     + CoreCost  * reduction.minmax.core_overhead
```
`ExtraCost` is the canonical-NaN + branch cost from main; it is 0 when
`FMF.noNaNs()`. `split_overhead` is unused here. The i1 paths at the top of
`getMinMaxReductionCost` delegate to `getArithmeticReductionCost`, so
`reduction.arith.*` already covers them.

## Implementation notes (minimum for analysis)

- All additions live in
  `llvm/lib/Target/RISCV/RISCVTargetTransformInfo.cpp` (~163 lines).
- Knob registry: `KnownKnobs[]` table of `{name, default}` near the file
  top — adding a knob = appending one row and reading it in the relevant
  cost function.
- State: two file-scope structures, `knobValues()` (`StringMap<unsigned>`)
  and `knobOverridden()` (`StringSet<>`). Both initialized once via
  `std::call_once` on first `getKnob`/`anyOverride` call.
- Init order inside `initRVVTTIKnobs()`:
  1. Seed defaults from `KnownKnobs`.
  2. Parse `-riscv-tti-config` file (if any) via
     `MemoryBuffer::getFile` + `line_iterator(MB, SkipBlanks=true,
     CommentMarker='#')`.
  3. Parse `-riscv-tti-overrides` (comma-split). Later writes win.
- Each parsed entry goes through `applyKVPair`, which trims, validates
  the key is in `knobValues()`, parses the value with
  `StringRef::getAsInteger(0, ...)`, updates `knobValues()`, and records
  the key in `knobOverridden()`.
- `anyOverride(prefix)` scans `knobOverridden()` for keys starting with
  `prefix`. Used by each cost function so the original code path runs
  unchanged when no knob in that group is set.
- Errors use `report_fatal_error` so sweep scripts fail loudly instead
  of silently using stale numbers.
- No subtarget / TableGen / header changes. No tests added.

## Verifying a sweep value

For a quick sanity check on the formula:

```bash
# Baseline
build/bin/opt -mtriple=riscv64 -mattr=+v \
  -passes='print<cost-model>' -cost-kind=throughput -disable-output \
  llvm/test/Analysis/CostModel/RISCV/scalable-gather.ll \
  | grep V8F64

# Override: cost should be gather.setup + NumLoads * LaneMemCost * gather.overhead
build/bin/opt -mtriple=riscv64 -mattr=+v \
  -riscv-tti-overrides=gather.setup=100,gather.overhead=2 \
  -passes='print<cost-model>' -cost-kind=throughput -disable-output \
  llvm/test/Analysis/CostModel/RISCV/scalable-gather.ll \
  | grep V8F64
# nxv8f64 → NumLoads=16, LaneMemCost=1 → expected 100 + 16*1*2 = 132
```
