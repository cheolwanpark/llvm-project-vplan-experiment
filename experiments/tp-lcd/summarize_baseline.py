#!/usr/bin/env python3
"""Evaluate and summarize a complete pristine collection, with coverage checks."""
import argparse
import csv
import json
from pathlib import Path

from evaluate import evaluate, VFS, GROUPS

ROOT = Path(__file__).resolve().parent


def display(value):
    return 'undefined' if value is None else f'{value:.6f}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    args = parser.parse_args()
    baseline = args.baseline.resolve()
    records = json.loads((baseline / 'records.json').read_text())
    keys = [(r['kernel'], r['mode']) for r in records]
    expected = {(kernel, mode) for members in GROUPS.values() for kernel in members
                for mode in [*VFS, 'automatic', 'expanded']}
    if len(keys) != 54 or set(keys) != expected:
        raise SystemExit('Expected 36 forced and 18 automatic records, with no duplicates')
    measured = list(csv.DictReader((ROOT / 'inputs/measured.csv').open()))
    output = baseline / 'summary'
    output.mkdir(exist_ok=False)
    lines = ['# Pristine baseline', '',
             'These results use the compiler built from the specified local main. '
             'Cycle comparisons are historical calibration, not measurements of the new binaries.', '',
             '| Group | Choice/cost | Regret GM | Worst | Non-tie accuracy | Relative error GM | P90 |',
             '|---|---|---:|---:|---:|---:|---:|']
    for search in ['default', 'expanded']:
        scores = json.loads((baseline / f'scores-{search}.json').read_text())
        result = evaluate(scores, measured)
        (output / f'evaluation-{search}.json').write_text(json.dumps(result, indent=2) + '\n')
        for kind in ['summary', 'relative', 'pairs', 'gaps']:
            with (output / f'{search}-{kind}.csv').open('w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=list(result[kind][0]))
                writer.writeheader()
                writer.writerows(result[kind])
        for row in result['summary']:
            mode = row['mode']
            if mode not in ('legacy', 'largest_vf', 'automatic') or (search == 'expanded' and mode != 'automatic'):
                continue
            name = f'{search} automatic' if mode == 'automatic' else mode
            fields = [row['regret_geomean'], row['regret_worst'], row['non_tie_pair_accuracy'],
                      row['relative_error_geomean'], row['relative_error_p90']]
            lines.append(f"| {row['group']} | {name} | " + ' | '.join(map(display, fields)) + ' |')
            if row['issues']:
                lines.append(f"\n{name}/{row['group']} coverage issues: {row['issues']}\n")
    lines += ['', 'The legacy candidate score is the forced legacy IR validity cost. '
              'Actual automatic search uses VPlan costing and its existing legality/pressure checks. '
              'No TP/LCD implementation is present in this pristine compiler.', '',
              '| Kernel | Search | Actual VF | Scalable candidates costed | Forced body identical | Frame evidence |',
              '|---|---|---|---|---|---|']
    by_key = {(r['kernel'], r['mode']): r for r in records}
    for r in records:
        if r['mode'] not in ('automatic', 'expanded'):
            continue
        visits = sorted({v['vf'] for v in r['visited'] if v['scalable']})
        forced = by_key.get((r['kernel'], r['actual_vf'])) if r['scalable'] else None
        identical = (forced['shape']['body_sha256'] == r['shape']['body_sha256']) if forced else 'outside measured set'
        frames = r['register_allocation_frame_lines'] if r.get('register_allocation_observed') else 'dump absent'
        lines.append(f"| {r['kernel']} | {r['mode']} | {'scalable ' if r['scalable'] else 'fixed '}{r['actual_vf']} | "
                     f"{visits} | {identical} | {'none' if frames == [] else frames} |")
    lines += ['', 'Costed candidates can still be excluded by register-pressure or profitability checks. '
              'Inspect selected-debug.txt for the reason. A matching body proves equality only '
              'between these newly generated functions, not historical binary identity.', '',
              '| Kernel | VF | Vector loops | vsetvli per vector loop | Allocation frame objects |',
              '|---|---:|---:|---|---|']
    for r in records:
        if r['mode'] not in VFS:
            continue
        loops = r['shape']['vector_loops']
        counts = [l['opcodes'].get('vsetvli', 0) + l['opcodes'].get('vsetivli', 0) for l in loops]
        frames = r['register_allocation_frame_lines'] if r.get('register_allocation_observed') else 'dump absent'
        lines.append(f"| {r['kernel']} | {r['mode']} | {len(loops)} | {counts} | {'none' if frames == [] else frames} |")
    lines += ['', 'Frame evidence is taken before the assembly reduction workaround. '
              'Opcode inventories and loop text are preserved in each record; historical '
              'memory/LMUL/splice/index-split correspondence requires inspecting those records.', '',
              'All later model changes are validation-informed. Core and BiCG/GEMVER groups '
              'remain separate. Saturn has no supplied measurements.', '']
    (output / 'README.md').write_text('\n'.join(lines))
    print(output / 'README.md')


if __name__ == '__main__':
    main()
