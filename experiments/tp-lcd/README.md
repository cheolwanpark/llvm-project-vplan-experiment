# TP/LCD compiler experiment

The report-only goal is implemented and evaluated. See [REPORT.md](REPORT.md)
for the Korean report with coefficient history and all compare-value terms. Start with
[FINAL_REPORT.md](FINAL_REPORT.md) for results, limits, source design, tests and
reproduction commands. Both groups meet the specified ranking/regret/relative
thresholds, including actual expanded automatic selection. This is historical
calibration, not a measurement of new binaries. All work is validation-informed;
Saturn has independent software coverage and no supplied measurements.

## Files

- `frozen-model.json`: final parameters, degrees of freedom and rule defaults.
- `frozen-xiangshan/`: 36 forced + 9 actual expanded compiler artifacts.
- `frozen-saturn/`: the same software coverage on generic-rv64, with its separate
  profile. The evaluator deliberately rejects joining XiangShan cycles to Saturn.
- `frozen-none-observe/`: all 45 assemblies equal pristine; explicit unsupported
  model fallbacks are retained.
- `final-report/`: candidates, selections, relative/pair/gap metrics, ablations,
  every automatic visit/exclusion, code-shape CSVs, source assembly differences
  and machine-readable completion/coverage audits.
- `baseline-v2/`: complete immutable-result pristine collection, 36 forced and
  18 automatic jobs (default and expanded). `baseline/` preserves the earlier
  failed workaround collection. `toolchain/pristine` preserves executable bytes
  and resource files independently of the modified working build directory.
- `round-*`, grids and their plans: every tuning trial, including rejected rules,
  compiler failures and coverage failures. `WORK_LOG.md` explains the progression.
- `final-*`/`pre-format-report`: pre-format frozen collections and final rule
  ablations, retained for provenance. `frozen-*` is the final formatted build.

## Clean baseline preparation

These preparation commands apply to a **clean checkout at the required starting
main SHA**, before applying the implementation. They refuse to overwrite outputs
and the pristine build script checks the starting revision and tracked changes.
Do not run the pristine builder on the already modified workspace and call its
output a new pristine baseline.

```sh
python3 experiments/tp-lcd/extract.py VF_IC1_SWP_OFF_REPORT.md experiments/tp-lcd/inputs
python3 experiments/tp-lcd/prepare_headers.py
python3 experiments/tp-lcd/build_pristine.py --jobs 3
python3 experiments/tp-lcd/snapshot_pristine.py
python3 experiments/tp-lcd/collect_baseline.py --cc build-tp-lcd-main/bin/clang --out experiments/tp-lcd/baseline-v2
python3 experiments/tp-lcd/summarize_baseline.py experiments/tp-lcd/baseline-v2
```

Python3.10+, CMake, Ninja and a host C++ compiler are required. This run used
Apple Clang17, Release O2, assertions/dumps, RISCV + Clang, three compilation jobs
and one link job on the 8GiB host. `pristine-build.json`, `pristine-snapshot.json`,
`input-manifest.json` and `header-provenance.json` preserve versions and hashes.
Original extracted files are immutable. Headers are newly obtained, not a
reconstructed historical lockfile.

## Modified compiler and evaluation

After applying the implementation, build the existing configured tree:

```sh
cmake --build build-tp-lcd-main --target clang opt VectorizeTests --parallel 3
python3 experiments/tp-lcd/collect_model.py --out NEW_COLLECTION
python3 experiments/tp-lcd/evaluate.py NEW_COLLECTION/scores.json --out NEW_EVALUATION
```

The collector replays preserved full-translation-unit baseline commands and adds
explicit profile/ranking flags. Each output directory must be new. Optional
arguments: `--profile none|xiangshan|saturn`, `--model observe|rank`,
`--scores-only`, and repeated `--extra-llvm=-flag=value`. Ranking/profile defaults
in LLVM itself remain off/none; the collector explicitly enables the experiment.
The collector's `--model off` option is not used for model collections because
that mode intentionally emits no model diagnostics; default preservation is
checked with LLVM off/observe regression tests and the none/observe collection.

```sh
python3 experiments/tp-lcd/collect_model.py --profile saturn --out NEW_SATURN
python3 experiments/tp-lcd/collect_model.py --profile none --model observe --out NEW_NONE
python3 experiments/tp-lcd/collect_model.py --scores-only --extra-llvm=-vplan-tp-lcd-fma=false --out NEW_ABLATION
```

`evaluation-policy.json` froze ties, percentile interpolation, missing-data
handling and group boundaries before tuning. `evaluate.py` joins actual
compiler-emitted costs with the report; it contains no duplicate cost model.
Missing values invalidate affected aggregates with explicit coverage. LCD zero
references remain undefined. Forced-score argmin and actual compiler selection
are separate. The model components also support TP-only/LCD-only evaluation.

`final_report.py` rebuilds the final joined tables from the preserved named
collections and asserts target/coverage/freeze agreement. It refuses to overwrite
`final-report`; move that directory aside before reproducing it.

## Tests

```sh
build-tp-lcd-main/bin/llvm-lit -v llvm/test/Transforms/LoopVectorize/RISCV llvm/test/Analysis/CostModel/RISCV
build-tp-lcd-main/unittests/Transforms/Vectorize/VectorizeTests
python3 -m unittest discover -s experiments/tp-lcd -p 'test_*.py' -v
git diff --check
```

Final results are 169 lit, 80 unit and 17 Python tests passing. The final report
links the logs. No RTL/QEMU/SSH or additional measurements are required or used.
Changed code is explicitly unmeasured; equality with reconstructed pristine
assembly does not establish historical binary identity.
