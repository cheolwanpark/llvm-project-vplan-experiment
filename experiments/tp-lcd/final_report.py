#!/usr/bin/env python3
"""Create report-only evaluation tables from preserved real compiler records."""
import csv
import difflib
import json
from pathlib import Path

from evaluate import evaluate, GROUPS, VFS

ROOT = Path(__file__).resolve().parent


def read(path):
    return json.loads(path.read_text())


def write_csv(path, rows):
    if not rows:
        return
    keys = list(dict.fromkeys(k for row in rows for k in row))
    with path.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=keys)
        writer.writeheader()
        for row in rows:
            writer.writerow({k: json.dumps(v, sort_keys=True) if isinstance(v, (dict, list)) else v
                             for k, v in row.items()})


def main():
    out = ROOT / 'final-report'
    out.mkdir(exist_ok=False)
    measured = list(csv.DictReader((ROOT / 'inputs/measured.csv').open()))
    measurements = {(r['kernel'], int(r['vf'])): int(r['raw_roi_cycles']) for r in measured}
    groups = {k: group for group, ks in GROUPS.items() for k in ks}
    current = read(ROOT / 'frozen-xiangshan/scores.json')
    result = evaluate(current, measured)
    (out / 'evaluation.json').write_text(json.dumps(result, indent=2) + '\n')
    for key in ['summary', 'relative', 'pairs', 'gaps']:
        write_csv(out / f'{key}.csv', result[key])
    baseline = read(ROOT / 'baseline-v2/summary/evaluation-expanded.json')
    write_csv(out / 'baseline-summary.csv', baseline['summary'])

    rounds = ['round-00-coverage', 'round-01-coverage', 'round-02-initial-full',
              'round-03-memory256', 'round-04-fma', 'round-04-no-fma',
              'round-05-slide-best-full', 'round-06-latency-best-full',
              'round-07-indexed-startup', 'round-08-data-index',
              'round-08-no-data-index', 'round-09-full-evl',
              'round-09b-full-evl', 'round-10-strided-pressure',
              'round-10-no-strided-pressure', 'round-11-scalar-evl']
    rounds += sorted(p.name for p in ROOT.glob('final-ablation-*') if p.is_dir())
    rounds += ['frozen-xiangshan']
    round_rows = []
    for name in rounds:
        directory = ROOT / name
        if not (directory / 'scores.json').exists():
            continue
        data = read(directory / 'scores.json')
        ev = evaluate(data, measured)
        for row in ev['summary']:
            if row['mode'] in ['max_tp_lcd', 'automatic']:
                round_rows.append(dict(round=name, failures=read(directory / 'failures.json'), **row))
        if name.startswith('final-ablation-'):
            (directory / 'evaluation.json').write_text(json.dumps(ev, indent=2) + '\n')
    write_csv(out / 'rounds-and-ablations.csv', round_rows)

    candidates, selections, shapes, auto_candidates, coverage = [], [], [], [], []
    diffs = out / 'assembly-differences'
    diffs.mkdir()
    for name in ['frozen-xiangshan', 'frozen-saturn', 'frozen-none-observe']:
        directory = ROOT / name
        data = read(directory / 'scores.json')
        records = read(directory / 'records.json')
        by_key = {(r['kernel'], r['mode']): r for r in records}
        profile = read(directory / 'provenance.json')['profile']
        assert len(records) == 45 and len(by_key) == 45
        assert not read(directory / 'failures.json')
        expected = {(k, m) for k in groups for m in [*VFS, 'expanded']}
        assert set(by_key) == expected
        visits_ok = True
        for r in records:
            loops = r['shape']['vector_loops']
            frames = r['register_allocation_frame_lines']
            shape = dict(collection=name, profile=profile, kernel=r['kernel'], mode=r['mode'],
                         vf=r['vf'], scalable=r['scalable'],
                         e32_lmuls=r['shape']['e32_lmuls'],
                         vset_per_loop=[l['opcodes'].get('vsetvli', 0) + l['opcodes'].get('vsetivli', 0) for l in loops],
                         loop_opcodes=[l['opcodes'] for l in loops],
                         register_allocation_frame_lines=frames,
                         allocation_dump_present=r['allocation_dump_present'],
                         same_full_assembly_as_pristine=r['same_full_assembly_as_pristine'],
                         same_body_as_pristine=r['same_selected_body_as_pristine'],
                         historical_binary_measured=False)
            shapes.append(shape)
            if r['mode'] == 'expanded':
                cs = [d for d in r['diagnostics'] if d['kind'] == 'candidate']
                visits = sorted({d['vf'] for d in cs if d['scalable']})
                visits_ok &= set(VFS) <= set(visits)
                auto_candidates.extend(dict(collection=name, profile=profile, kernel=r['kernel'], **d) for d in cs)
                forced = by_key.get((r['kernel'], r['vf'])) if r['scalable'] else None
                same = forced is not None and r['shape']['body_sha256'] == forced['shape']['body_sha256']
                same_ops = forced is not None and [l['opcodes'] for l in loops] == [l['opcodes'] for l in forced['shape']['vector_loops']]
                # Saturn is deliberately never joined to XiangShan measurements.
                regret = (measurements[r['kernel'], r['vf']] / min(measurements[r['kernel'], v] for v in VFS)
                          if profile != 'saturn' and r['scalable'] and r['vf'] in VFS else None)
                selections.append(dict(collection=name, profile=profile, group=groups[r['kernel']],
                                       kernel=r['kernel'], actual_vf=r['vf'], scalable=r['scalable'],
                                       visited_scalable_vfs=visits, historical_selection_regret=regret,
                                       same_body_as_same_vf_forced=same, same_loop_opcodes_as_forced=same_ops,
                                       allocation_frame_lines=frames, binary_measured=False))
                if forced and not same:
                    a = (directory / r['kernel'] / str(r['vf']) / 'unpatched.s').read_text().splitlines(True)
                    b = (directory / r['kernel'] / 'expanded/unpatched.s').read_text().splitlines(True)
                    (diffs / f'{name}-{r["kernel"]}.diff').write_text(''.join(difflib.unified_diff(a, b, fromfile='same-VF forced', tofile='actual automatic')))
        assert visits_ok
        coverage.append(dict(collection=name, profile=profile, forced=36, actual_automatic=9,
                             all_four_vfs_visited=visits_ok,
                             supported_forced=sum(not c.get('fallback_reason') for c in data['candidates']),
                             full_assembly_pristine_identical=sum(r['same_full_assembly_as_pristine'] for r in records),
                             forced_assembly_pristine_identical=sum(r['same_full_assembly_as_pristine'] for r in records if r['mode'] in VFS),
                             all_allocation_dumps=all(r['allocation_dump_present'] for r in records),
                             failures=read(directory / 'failures.json'),
                             measured=False))
        if name == 'frozen-xiangshan':
            for c in data['candidates']:
                r = by_key[c['kernel'], c['vf']]
                candidates.append(dict(group=groups[c['kernel']], **c,
                                       measured_cycles=measurements[c['kernel'], c['vf']],
                                       measured_relative_vf2=measurements[c['kernel'], c['vf']] / measurements[c['kernel'], 2],
                                       allocation_frame_lines=r['register_allocation_frame_lines']))
    write_csv(out / 'candidates.csv', candidates)
    write_csv(out / 'automatic-candidates.csv', auto_candidates)
    write_csv(out / 'selections.csv', selections)
    write_csv(out / 'code-shape.csv', shapes)
    write_csv(out / 'coverage.csv', coverage)
    (out / 'coverage.json').write_text(json.dumps(coverage, indent=2) + '\n')
    old = read(ROOT / 'round-11-scalar-evl/scores.json')
    fields = ['vf', 'runtime_vf', 'legacy_cost', 'memory_tp', 'compute_tp', 'other_tp', 'tp', 'lcd', 'final_score']
    frozen_equal = [{k: c[k] for k in fields} for c in old['candidates']] == [{k: c[k] for k in fields} for c in current['candidates']]
    assert frozen_equal and current['automatic'] == old['automatic']
    checks = {}
    for group in GROUPS:
        row = next(r for r in result['summary'] if r['group'] == group and r['mode'] == 'max_tp_lcd')
        auto = next(r for r in result['summary'] if r['group'] == group and r['mode'] == 'automatic')
        checks[group] = dict(forced_regret_gm=row['regret_geomean'] <= 1.05,
                             forced_regret_worst=row['regret_worst'] <= 1.15,
                             automatic_regret_gm=auto['regret_geomean'] <= 1.05,
                             automatic_regret_worst=auto['regret_worst'] <= 1.15,
                             non_tie_accuracy=row['non_tie_pair_accuracy'] >= .90,
                             relative_gm=row['relative_error_geomean'] <= 1.15,
                             relative_p90=row['relative_error_p90'] <= 1.30)
    audit = dict(frozen_scores_equal_round11=frozen_equal, thresholds=checks,
                 all_report_calibration_thresholds_met=all(all(c.values()) for c in checks.values()),
                 current_binary_measurements=False, saturn_measurements=False,
                 historical_binary_identity_proven=False, validation_informed=True)
    assert audit['all_report_calibration_thresholds_met']
    (out / 'metric-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
    print(json.dumps(audit, indent=2))


if __name__ == '__main__':
    main()
