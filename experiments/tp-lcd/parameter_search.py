#!/usr/bin/env python3
"""Run a predeclared one-parameter compiler grid; retain every trial."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--plan', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--parameter', required=True)
p.add_argument('--flag', required=True)
p.add_argument('--fixed-llvm', action='append', default=[])
a = p.parse_args()
plan = json.loads(a.plan.read_text())
a.out.mkdir(parents=True, exist_ok=False)
(a.out / 'search-command.json').write_text(json.dumps(sys.argv, indent=2) + '\n')
results = []
for value in plan['search'][a.parameter]:
    trial = a.out / str(value)
    command = [sys.executable, str(ROOT / 'collect_model.py'), '--out', str(trial),
               '--scores-only', f'--extra-llvm={a.flag}={value}']
    command += [f'--extra-llvm={flag}' for flag in a.fixed_llvm]
    with (a.out / f'{value}.log').open('w') as log:
        compiled = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    evaluated = subprocess.run([sys.executable, str(ROOT / 'evaluate.py'), str(trial / 'scores.json'),
                                '--out', str(trial / 'evaluation')], capture_output=True, text=True)
    (a.out / f'{value}-evaluation.log').write_text(evaluated.stdout + evaluated.stderr)
    rec = dict(parameter=a.parameter, value=value, compiler_exit=compiled.returncode,
               evaluator_exit=evaluated.returncode, path=str(trial))
    if evaluated.returncode == 0:
        result = json.loads((trial / 'evaluation/evaluation.json').read_text())
        rec['summary'] = result['summary']
        for s in result['summary']:
            if s['mode'] == 'max_tp_lcd':
                print(value, s['group'], 'relative GM/P90', s['relative_error_geomean'],
                      s['relative_error_p90'], 'non-tie', s['non_tie_pair_accuracy'],
                      'regret', s['regret_geomean'], s['regret_worst'], flush=True)
    results.append(rec)
    (a.out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
if any(r['compiler_exit'] or r['evaluator_exit'] for r in results):
    raise SystemExit(1)
