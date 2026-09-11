#!/usr/bin/env python3
"""Replay preserved full-TU commands with the modified compiler and JSON model.

No measurement is run. The original cycles remain historical calibration data.
"""
import argparse
import json
import re
from pathlib import Path
import subprocess
import sys

from collect_baseline import ROOT, REPO, sha, selected_log, assembly_shape, report
from workaround_adapter import rewrite_assembly


def diagnostics(log):
    return [json.loads(line[len('VPLAN-TP-LCD '):]) for line in log.splitlines()
            if line.startswith('VPLAN-TP-LCD ')]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--cc', type=Path, default=REPO / 'build-tp-lcd-main/bin/clang')
    p.add_argument('--profile', choices=['none', 'xiangshan', 'saturn'], default='xiangshan')
    p.add_argument('--model', choices=['off', 'observe', 'rank'], default='rank')
    p.add_argument('--scores-only', action='store_true')
    p.add_argument('--extra-llvm', action='append', default=[])
    args = p.parse_args()
    out, cc = args.out.resolve(), args.cc.resolve()
    baseline = ROOT / 'baseline-v2'
    out.mkdir(parents=True, exist_ok=False)
    diff = subprocess.check_output(['git', 'diff', 'HEAD'], cwd=REPO)
    (out / 'tracked.patch').write_bytes(diff)
    new_sources = [REPO / 'llvm/lib/Transforms/Vectorize/VPlanCostModel.h',
                   REPO / 'llvm/lib/Transforms/Vectorize/VPlanCostModel.cpp']
    provenance = dict(compiler=str(cc), compiler_sha256=sha(cc),
                      compiler_version=subprocess.check_output([str(cc), '--version'], text=True),
                      start_sha=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
                      diff_sha256=sha(out / 'tracked.patch'),
                      new_source_sha256={str(f.relative_to(REPO)): sha(f) for f in new_sources},
                      profile=args.profile, model=args.model, extra_llvm=args.extra_llvm,
                      scores_only=args.scores_only, validation_informed=True,
                      collector_sha256=sha(Path(__file__)),
                      evaluation_policy_sha256=sha(ROOT / 'evaluation-policy.json'),
                      measurements_sha256=sha(ROOT / 'inputs/measured.csv'),
                      baseline_provenance_sha256=sha(baseline / 'provenance.json'),
                      measurement=('none; Saturn software coverage only' if args.profile == 'saturn'
                                   else 'none; historical XiangShan calibration only'),
                      saturn='software profile only; hardware and measurements unknown')
    (out / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    for f in new_sources:
        (out / f.name).write_bytes(f.read_bytes())
    candidates, automatic, records, failures = [], {}, [], []

    for kernel, _, _ in report.CASES:
        for mode in [2, 4, 8, 16, 'expanded']:
            source_dir, dest = baseline / kernel / str(mode), out / kernel / str(mode)
            dest.mkdir(parents=True)

            def run(stage):
                command = json.loads((source_dir / f'{stage}.command.json').read_text())
                command = [str(cc)] + [v.replace(str(source_dir), str(dest)) for v in command[1:]]
                if args.profile == 'saturn':
                    # No Saturn CPU/hardware configuration has been supplied.
                    # Exercise its explicit profile on a generic software CPU.
                    command = ['-mcpu=generic-rv64' if v.startswith('-mcpu=') else v
                               for v in command]
                if stage != 'assemble':
                    command[1:1] = ['-mllvm', f'-riscv-tp-lcd-profile={args.profile}',
                                     '-mllvm', f'-vplan-tp-lcd={args.model}']
                    for flag in args.extra_llvm:
                        command[1:1] = ['-mllvm', flag]
                (dest / f'{stage}.command.json').write_text(json.dumps(command, indent=2) + '\n')
                result = subprocess.run(command, text=True, capture_output=True)
                (dest / f'{stage}.stdout.txt').write_text(result.stdout)
                (dest / f'{stage}.stderr.txt').write_text(result.stderr)
                if result.returncode:
                    raise RuntimeError(f'{stage} exit {result.returncode}')
                return result.stderr

            try:
                log = run('assembly')
                selected = selected_log(log)
                (dest / 'selected-debug.txt').write_text(selected)
                ds = [d for d in diagnostics(log) if d.get('function') == 'selected_kernel']
                (dest / 'diagnostics.json').write_text(json.dumps(ds, indent=2) + '\n')
                cs = [d for d in ds if d['kind'] == 'candidate']
                if isinstance(mode, int):
                    matching = [d for d in cs if d['vf'] == mode and d['scalable']]
                    if len(matching) != 1:
                        raise ValueError(f'expected one candidate, found {len(matching)}')
                    candidate = dict(matching[0], kernel=kernel, profile=args.profile)
                    candidates.append(candidate)
                    vf, scalable = mode, True
                else:
                    ss = [d for d in ds if d['kind'] == 'selection']
                    if len(ss) != 1:
                        raise ValueError(f'expected one selection, found {len(ss)}')
                    vf, scalable = ss[0]['vf'], ss[0]['scalable']
                    automatic[kernel] = vf if scalable else f'fixed:{vf}'
                    visits = {d['vf'] for d in cs if d['scalable']}
                    if not {2, 4, 8, 16} <= visits:
                        raise ValueError(f'expanded search missed candidates: {visits}')
                assembly = (dest / 'unpatched.s').read_text()
                baseline_assembly = (source_dir / 'unpatched.s').read_text()
                shape = assembly_shape(assembly)
                rec = dict(kernel=kernel, mode=mode, vf=vf, scalable=scalable, shape=shape,
                           same_full_assembly_as_pristine=assembly == baseline_assembly,
                           same_selected_body_as_pristine=shape['body_sha256'] == assembly_shape(baseline_assembly)['body_sha256'],
                           historical_binary_measured=False,
                           diagnostics=ds)
                if not args.scores_only:
                    patched, count, moved = assembly, 0, 0
                    if scalable and kernel in ('s311', 'gesummv-l00', 'bicg-l01'):
                        patched, count, moved, adjacent = rewrite_assembly(assembly, f'm{vf // 2}',
                                                                        preserve_state=kernel != 's311')
                        (dest / 'workaround-input.s').write_text(adjacent)
                        if count != {'s311': 1, 'gesummv-l00': 2, 'bicg-l01': 0}[kernel]:
                            raise ValueError(f'changed extraction count {count}')
                    (dest / 'workaround.s').write_text(patched)
                    run('assemble')
                    run('ir')
                    alloc = run('without-debug')
                    if assembly != (dest / 'without-debug.s').read_text():
                        raise ValueError('debug observation changed assembly')
                    rec.update(workaround_applied=count, scheduled_extractions_moved=moved,
                               allocation_dump_present='After Virtual Register Rewriter' in alloc,
                               register_allocation_frame_lines=[
                                   line for line in alloc.splitlines()
                                   if re.search(r'(fi#-?\d+|%stack\.|%fixed-stack\.)', line)])
                records.append(rec)
                (dest / 'record.json').write_text(json.dumps(rec, indent=2) + '\n')
                print(kernel, mode, 'ok', flush=True)
            except (RuntimeError, ValueError) as e:
                failures.append(dict(kernel=kernel, mode=mode, error=str(e)))
                print(kernel, mode, str(e), flush=True)
            (out / 'records.json').write_text(json.dumps(records, indent=2) + '\n')
            (out / 'scores.json').write_text(json.dumps(dict(candidates=candidates, automatic=automatic), indent=2) + '\n')
            (out / 'failures.json').write_text(json.dumps(failures, indent=2) + '\n')
    if sha(cc) != provenance['compiler_sha256']:
        raise RuntimeError('compiler changed during collection')
    if failures:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
