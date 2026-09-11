#!/usr/bin/env python3
"""Compile all report candidates and actual default/expanded automatic searches.

Preserves full translation units, command logs, untouched and workaround
assembly, optimized IR, VPlan/debug output and compiler hashes. This collector
uses the pristine revision's existing diagnostics; it changes no compiler code.
"""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
INPUTS = ROOT / 'inputs'
sys.path.insert(0, str(INPUTS))
from xiangshan_reduction_workaround import remaining_extraction_count
from workaround_adapter import rewrite_assembly

spec = importlib.util.spec_from_file_location('report_build', INPUTS / 'build.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def selected_log(log):
    blocks = re.split(r"(?=\nLV: Checking a loop in ')", log)
    selected = [b for b in blocks if b.startswith("\nLV: Checking a loop in 'selected_kernel'")]
    if len(selected) != 1:
        raise ValueError(f'Expected one selected_kernel loop, found {len(selected)}')
    return selected[0]


def forced_cost(log, vf):
    # selectUserVectorizationFactor calls expectedCost once before this marker.
    # For vector candidates expectedCost sums instruction costs without scalar
    # block-probability division. Refuse duplicate IR entries rather than hide a
    # changed diagnostic flow by summing multiple evaluations.
    prefix = log.split('LV: Using user VF')[0]
    entries = re.findall(rf'^LV: Found an estimated cost of (\S+) for VF vscale x {vf} For instruction: (.*)$',
                         prefix, re.M)
    if not entries:
        raise ValueError('Missing forced legacy instruction costs')
    if len({ir for _, ir in entries}) != len(entries):
        raise ValueError('Repeated forced cost evaluation; inspect debug flow')
    if any(not value.isdecimal() for value, _ in entries):
        return None
    return sum(int(value) for value, _ in entries)


def assembly_shape(assembly):
    match = re.search(r'(?ms)^selected_kernel:.*?^\s*\.size\s+selected_kernel,.*?$', assembly)
    if not match:
        raise ValueError('selected_kernel assembly missing')
    body = match.group()
    lines = body.splitlines()
    labels = {m.group(1): i for i, line in enumerate(lines)
              if (m := re.match(r'(\.LBB\w+):', line))}
    loops = []
    for i, line in enumerate(lines):
        branch = re.match(r'\s*(?:b\w+|j)\s+.*?(\.LBB\w+)\s*(?:#.*)?$', line)
        if not branch or branch.group(1) not in labels or labels[branch.group(1)] >= i:
            continue
        region = lines[labels[branch.group(1)]:i + 1]
        instructions = [m.group(1) for l in region if (m := re.match(r'\s+([a-z][a-z0-9_.]+)\s', l))]
        if any(op.startswith('v') for op in instructions):
            loops.append(dict(label=branch.group(1), opcodes=dict(Counter(instructions)),
                              text='\n'.join(region) + '\n'))
    return dict(body_sha256=hashlib.sha256(body.encode()).hexdigest(), vector_loops=loops,
                e32_lmuls=sorted(set(re.findall(r'e32,\s*(m[f]?\d+)', body))),
                vector_stack_accesses=[line.strip() for line in lines
                                       if re.search(r'^\s+v[ls]\S*\s+.*\(sp\)', line)],
                spill_audit='stack-base vector accesses are a diagnostic, not a complete spill proof')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    cc, out = args.cc.resolve(), args.out.resolve()
    build_record = json.loads((ROOT / 'pristine-build.json').read_text())
    if build_record['status'] != 'complete' or sha(cc) != build_record['compiler_sha256']:
        raise SystemExit('Use the successfully built pristine compiler (or its identical snapshot)')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip()
    diff = subprocess.check_output(['git', 'diff', 'HEAD'], cwd=REPO, text=True)
    if revision != build_record['start_sha'] or diff:
        raise SystemExit('Collect pristine baseline before modifying compiler sources')
    headers = ROOT / 'toolchain/picolibc/usr/lib/picolibc/riscv64-unknown-elf/include'
    if not (headers / 'stdio.h').is_file():
        raise SystemExit('Prepare Picolibc headers first')
    out.mkdir(parents=True, exist_ok=False)
    provenance = dict(
        start_sha=revision, tracked_diff=diff,
        compiler=str(cc), compiler_sha256=sha(cc),
        compiler_version=subprocess.check_output([str(cc), '--version'], text=True),
        input_manifest=json.loads((ROOT / 'input-manifest.json').read_text()),
        header_provenance_sha256=sha(ROOT / 'header-provenance.json'),
        historical_binary_identity='not established; full historical assembly/ELF absent',
        conditions=dict(vlen_min=128, actual_measured_vlen=128, vscale=2, ic=1,
                        software_pipelining='off', useful_iterations=131072),
    )
    (out / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    candidates, automatic, expanded, records = [], {}, {}, []

    def run(command, path):
        path.with_suffix('.command.json').write_text(json.dumps(command, indent=2) + '\n')
        result = subprocess.run(command, text=True, capture_output=True)
        path.with_suffix('.stdout.txt').write_text(result.stdout)
        path.with_suffix('.stderr.txt').write_text(result.stderr)
        if result.returncode:
            raise RuntimeError(f'{path}: compiler exit {result.returncode}; inspect stderr')
        return result.stderr

    for kernel, source, case in report.CASES:
        defines = ([f'-DTSVC_CASE={case}'] if source == 'tsvc_kernels.c' else
                   [f'-DAPP_CASE={case}', '-DAPP_LOOP_SIZE=4096', '-DAPP_REPEATS=32'])
        for mode in [2, 4, 8, 16, 'automatic', 'expanded']:
            directory = out / kernel / str(mode)
            directory.mkdir(parents=True)
            flags = report.TARGET + [
                '-std=c11', '-ffreestanding', '-fno-builtin', '-ffunction-sections',
                '-fdata-sections', '-O2', '-ffast-math', '-fno-unroll-loops',
                '-mllvm', '-scalable-vectorization=on',
                '-mllvm', '-force-vector-interleave=1',
                '-mllvm', '-riscv-enable-pipeliner=false', '-Rpass=loop-vectorize',
                '-I', str(INPUTS / 'include'), '-idirafter', str(headers),
            ] + defines
            if isinstance(mode, int):
                flags += ['-mllvm', f'-force-vector-width={mode}']
            if mode == 'expanded':
                flags += ['-mllvm', '-riscv-v-register-bit-width-lmul=8']
            base = [str(cc)] + flags
            assembly = directory / 'unpatched.s'
            log = run(base + ['-mllvm', '-debug-only=loop-vectorize', '-S',
                              str(INPUTS / source), '-o', str(assembly)], directory / 'assembly')
            selected = selected_log(log)
            (directory / 'selected-debug.txt').write_text(selected)
            widths = re.findall(r'LV: Selecting VF: (vscale x )?(\d+)\.', selected)
            if isinstance(mode, int):
                expected = f'vectorization width: vscale x {mode}, interleaved count: 1'
                if expected not in selected:
                    raise ValueError(f'{kernel}/{mode}: forced VF/IC remark missing')
                vf, scalable = mode, True
                candidates.append(dict(kernel=kernel, vf=vf, runtime_vf=2 * vf,
                                       legacy_cost=forced_cost(selected, vf)))
            else:
                if len(widths) != 1:
                    raise ValueError(f'{kernel}/{mode}: ambiguous actual selection {widths}')
                scalable, vf = bool(widths[0][0]), int(widths[0][1])
                # Fixed-width vector selections are outside the measured set.
                (automatic if mode == 'automatic' else expanded)[kernel] = vf if scalable else f'fixed:{vf}'
            original = assembly.read_text()
            shape = assembly_shape(original)
            patched, count, moved = original, 0, 0
            if scalable and kernel in ('s311', 'gesummv-l00', 'bicg-l01'):
                patched, count, moved, adjacent = rewrite_assembly(original, f'm{vf // 2}',
                                                                  preserve_state=kernel != 's311')
                (directory / 'workaround-input.s').write_text(adjacent)
                expected_count = {'s311': 1, 'gesummv-l00': 2, 'bicg-l01': 0}[kernel]
                if count != expected_count or remaining_extraction_count(patched):
                    raise ValueError(f'{kernel}/{mode}: changed reduction extraction shape ({count})')
            (directory / 'workaround.s').write_text(patched)
            run([str(cc)] + report.TARGET + ['-c', str(directory / 'workaround.s'),
                                           '-o', str(directory / 'workaround.o')], directory / 'assemble')
            run(base + ['-S', '-emit-llvm', str(INPUTS / source), '-o', str(directory / 'optimized.ll')],
                directory / 'ir')
            # Verify that enabling debug observation does not change assembly.
            allocation_log = run(base + ['-mllvm', '-print-after=virtregrewriter',
                                        '-mllvm', '-filter-print-funcs=selected_kernel',
                                        '-S', str(INPUTS / source), '-o', str(directory / 'without-debug.s')],
                                 directory / 'without-debug')
            if original != (directory / 'without-debug.s').read_text():
                raise ValueError('Debug observation changed generated assembly')
            visited = re.findall(r'^Cost for VF (vscale x )?(\d+): (\S+)', selected, re.M)
            record = dict(kernel=kernel, mode=mode, actual_vf=vf, scalable=scalable,
                          source_sha256=sha(INPUTS / source), assembly_sha256=sha(assembly),
                          shape=shape, workaround_applied=count, scheduled_extractions_moved=moved,
                          register_allocation_frame_lines=[line for line in allocation_log.splitlines()
                                                           if re.search(r'(fi#-?\d+|%stack\.|%fixed-stack\.)', line)],
                          register_allocation_observed='After Virtual Register Rewriter' in allocation_log,
                          visited=[dict(scalable=bool(s), vf=int(v), cost=c) for s, v, c in visited])
            records.append(record)
            (directory / 'record.json').write_text(json.dumps(record, indent=2) + '\n')
            print(kernel, mode, 'completed', flush=True)
            (out / 'records.json').write_text(json.dumps(records, indent=2) + '\n')
            for name, choices in [('default', automatic), ('expanded', expanded)]:
                (out / f'scores-{name}.json').write_text(json.dumps(
                    dict(candidates=candidates, automatic=choices), indent=2) + '\n')


if __name__ == '__main__':
    main()
