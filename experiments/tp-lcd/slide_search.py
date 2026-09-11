#!/usr/bin/env python3
"""Execute the predeclared round-05 grid using actual compiler outputs."""
import csv
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
plan = json.loads((ROOT / 'round-05-plan.json').read_text())
out = ROOT / 'round-05-slide-grid'
out.mkdir(exist_ok=False)
results = []
for startup in plan['search']['slide_startup']:
    for quadratic in plan['search']['slide_quadratic_cost']:
        name = f's{startup}-q{quadratic}'
        trial = out / name
        command = [sys.executable, str(ROOT / 'collect_model.py'), '--out', str(trial),
                   '--scores-only', '--extra-llvm=-riscv-tp-lcd-memory-beat-bits=256',
                   f'--extra-llvm=-riscv-tp-lcd-slide-startup={startup}',
                   f'--extra-llvm=-riscv-tp-lcd-slide-quadratic-cost={quadratic}']
        with (out / (name + '.log')).open('w') as log:
            compiled = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        evaluated = subprocess.run([sys.executable, str(ROOT / 'evaluate.py'),
                                    str(trial / 'scores.json'), '--out', str(trial / 'evaluation')],
                                   capture_output=True, text=True)
        (out / (name + '-evaluation.log')).write_text(evaluated.stdout + evaluated.stderr)
        rec = dict(startup=startup, quadratic=quadratic, compiler_exit=compiled.returncode,
                   evaluator_exit=evaluated.returncode, path=str(trial))
        if evaluated.returncode == 0:
            evaluation = json.loads((trial / 'evaluation/evaluation.json').read_text())
            for group in ('core', 'validation_informed'):
                for mode in ('max_tp_lcd', 'automatic'):
                    summary = next(x for x in evaluation['summary'] if x['group'] == group and x['mode'] == mode)
                    rec[f'{group}_{mode}'] = summary
        results.append(rec)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        x = rec.get('core_max_tp_lcd', {})
        print(name, 'exit', compiled.returncode,
              'relative GM', x.get('relative_error_geomean'),
              'P90', x.get('relative_error_p90'),
              'non-tie', x.get('non_tie_pair_accuracy'),
              'Jacobi', x.get('selections', {}).get('jacobi-2d-imper-l00'), flush=True)
