#!/usr/bin/env python3
"""Join compiler-emitted costs with report measurements (no replica cost model).

Input JSON: {"candidates": [{"kernel": ..., "vf": ..., "runtime_vf": ...,
"legacy_cost": ..., "tp": ..., "lcd": ..., "final_score": ...}],
"automatic": {kernel: selected_vf}}. Costs are per vector iteration except
final_score, which is already normalized per useful element. Missing/invalid
fields remain undefined and invalidate the affected aggregate, never dropped.
"""
import argparse
import csv
import itertools
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
POLICY = json.loads((ROOT / 'evaluation-policy.json').read_text())
VFS = POLICY['vfs']
GROUPS = {k: POLICY[k] for k in ('core', 'validation_informed')}
MODES = ('legacy', 'largest_vf', 'tp_only', 'lcd_only', 'max_tp_lcd', 'automatic')


def number(x):
    return isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(x)


def geomean(xs):
    return math.exp(sum(map(math.log, xs)) / len(xs)) if xs else None


def p90(xs):
    if not xs:
        return None
    xs = sorted(xs)
    pos = (len(xs) - 1) * .9
    lo = math.floor(pos)
    hi = math.ceil(pos)
    return xs[lo] + (xs[hi] - xs[lo]) * (pos - lo)


def direction(x):
    return (x > 0) - (x < 0)


def score(row, mode):
    if row is None:
        return None
    if mode == 'max_tp_lcd':
        value = row.get('final_score')
        return value if number(value) and value >= 0 else None
    width = row.get('runtime_vf')
    value = row.get({'legacy': 'legacy_cost', 'tp_only': 'tp', 'lcd_only': 'lcd'}[mode])
    if not number(width) or width <= 0 or not number(value) or value < 0:
        return None
    return value / width


def evaluate(data, measurements):
    kernels = GROUPS['core'] + GROUPS['validation_informed']
    measured = {}
    for row in measurements:
        key = (row['kernel'], int(row['vf']))
        if key in measured or key[0] not in kernels or key[1] not in VFS:
            raise ValueError('Duplicate or unexpected measurement: ' + repr(key))
        cycles = int(row['raw_roi_cycles'])
        if cycles <= 0:
            raise ValueError('Nonpositive measurement')
        measured[key] = cycles
    candidates = {}
    for row in data['candidates']:
        if row.get('profile') == 'saturn':
            raise ValueError('The supplied measurements are XiangShan only; Saturn cannot be evaluated with them')
        key = (row['kernel'], row['vf'])
        if key in candidates or key[0] not in kernels or key[1] not in VFS:
            raise ValueError('Duplicate or unexpected candidate: ' + repr(key))
        candidates[key] = row
    details, pairs, gaps, summaries = [], [], [], []
    for group, members in GROUPS.items():
        for mode in MODES:
            issues, regrets, errors = [], [], []
            all_correct = non_tie_correct = all_count = non_tie_count = 0
            selections = {}
            for kernel in members:
                m = {vf: measured.get((kernel, vf)) for vf in VFS}
                if any(v is None for v in m.values()):
                    issues.append(f'{kernel}: missing measurement')
                    continue
                if mode in ('largest_vf', 'automatic'):
                    chosen = max(VFS) if mode == 'largest_vf' else data.get('automatic', {}).get(kernel)
                    selections[kernel] = chosen
                    if chosen not in VFS:
                        issues.append(f'{kernel}: actual selection absent or outside measured candidates: {chosen}')
                    else:
                        regrets.append(m[chosen] / min(m.values()))
                    continue
                s = {vf: score(candidates.get((kernel, vf)), mode) for vf in VFS}
                for vf in VFS:
                    pred_rel = s[vf] / s[2] if s[vf] is not None and s[2] is not None and s[2] > 0 else None
                    meas_rel = m[vf] / m[2]
                    error = max(pred_rel / meas_rel, meas_rel / pred_rel) if pred_rel is not None and pred_rel > 0 else None
                    details.append(dict(group=group, mode=mode, kernel=kernel, vf=vf,
                                        score=s[vf], raw_roi_cycles=m[vf],
                                        cycles_per_element=m[vf] / POLICY['useful_iterations'],
                                        pred_rel=pred_rel, meas_rel=meas_rel,
                                        multiplicative_error=error))
                    if vf != 2 and error is not None:
                        errors.append(error)
                if any(v is None for v in s.values()):
                    issues.append(f'{kernel}: missing or invalid compiler score')
                    continue
                chosen = min(VFS, key=lambda vf: (s[vf], vf))
                selections[kernel] = chosen
                regrets.append(m[chosen] / min(m.values()))
                for a, b in itertools.combinations(VFS, 2):
                    near_tie = max(m[a], m[b]) / min(m[a], m[b]) <= POLICY['near_tie_max_over_min']
                    correct = direction(s[b] - s[a]) == direction(m[b] - m[a])
                    all_count += 1
                    all_correct += correct
                    if not near_tie:
                        non_tie_count += 1
                        non_tie_correct += correct
                    pairs.append(dict(group=group, mode=mode, kernel=kernel, vf_a=a,
                                      vf_b=b, near_tie=near_tie, correct=correct))
                for a, b in zip(VFS, VFS[1:]):
                    pr = s[b] / s[a] if s[a] > 0 else None
                    mr = m[b] / m[a]
                    gaps.append(dict(group=group, mode=mode, kernel=kernel,
                                     vf_a=a, vf_b=b, predicted_ratio=pr,
                                     measured_ratio=mr, predicted_change=pr - 1 if pr is not None else None,
                                     measured_change=mr - 1))
            cost_mode = mode not in ('largest_vf', 'automatic')
            valid_selection = len(regrets) == len(members)
            valid_pairs = cost_mode and all_count == 6 * len(members)
            valid_scale = cost_mode and len(errors) == 3 * len(members)
            if cost_mode and not valid_scale:
                issues.append('Relative scale undefined: missing/invalid/zero score or reference')
            summaries.append(dict(
                group=group, mode=mode, selections=selections, issues=issues,
                selection_coverage=f'{len(regrets)}/{len(members)}',
                pair_coverage=f'{all_count}/{6 * len(members)}' if cost_mode else 'not applicable',
                relative_coverage=f'{len(errors)}/{3 * len(members)}' if cost_mode else 'not applicable',
                regret_geomean=geomean(regrets) if valid_selection else None,
                regret_worst=max(regrets) if valid_selection else None,
                all_pair_accuracy=all_correct / all_count if valid_pairs else None,
                non_tie_pair_accuracy=non_tie_correct / non_tie_count if valid_pairs and non_tie_count else None,
                relative_error_geomean=geomean(errors) if valid_scale else None,
                relative_error_p90=p90(errors) if valid_scale else None))
    return dict(policy=POLICY, summary=summaries, relative=details, pairs=pairs, gaps=gaps)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scores', type=Path)
    parser.add_argument('--measurements', type=Path, default=ROOT / 'inputs/measured.csv')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    with args.measurements.open() as f:
        result = evaluate(json.loads(args.scores.read_text()), list(csv.DictReader(f)))
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / 'evaluation.json').write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    for name in ('summary', 'relative', 'pairs', 'gaps'):
        rows = result[name]
        if not rows:
            continue
        with (args.out / (name + '.csv')).open('w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)


if __name__ == '__main__':
    main()
