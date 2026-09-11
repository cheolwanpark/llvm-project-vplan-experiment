#!/usr/bin/env python3
"""Build the requested pristine revision and preserve compiler provenance.

Run from any directory. This refuses tracked source modifications. The build
directory is reserved for the pristine compiler until baseline collection and
an immutable compiler/resource snapshot have completed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
EXPERIMENT = ROOT / 'experiments/tp-lcd'
SHA = 'ca7933e47d3a3451d81e72ac174dcb5aa28b59d1'
TARGETS = ['clang', 'opt', 'llc', 'FileCheck', 'llvm-as', 'llvm-dis',
           'llvm-link', 'llvm-config', 'llvm-readobj', 'count', 'not']


def output(*command):
    return subprocess.check_output(command, cwd=ROOT, text=True).strip()


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=3)
    args = parser.parse_args()
    if output('git', 'branch', '--show-current') != 'main':
        raise SystemExit('Expected local main')
    if output('git', 'rev-parse', 'HEAD') != SHA or output('git', 'rev-parse', 'refs/heads/main') != SHA:
        raise SystemExit('Unexpected start revision')
    if output('git', 'diff', 'HEAD', '--stat'):
        raise SystemExit('Pristine build requires no tracked source changes')
    build = ROOT / 'build-tp-lcd-main'
    commands = [
        ['cmake', '-S', str(ROOT / 'llvm'), '-B', str(build), '-G', 'Ninja',
         '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=/usr/bin/clang',
         '-DCMAKE_CXX_COMPILER=/usr/bin/clang++',
         '-DCMAKE_C_FLAGS_RELEASE=-O2 -DNDEBUG', '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG',
         '-DLLVM_ENABLE_ASSERTIONS=ON', '-DLLVM_ENABLE_DUMP=ON',
         '-DLLVM_TARGETS_TO_BUILD=RISCV', '-DLLVM_ENABLE_PROJECTS=clang',
         '-DLLVM_INCLUDE_BENCHMARKS=OFF', '-DLLVM_INCLUDE_EXAMPLES=OFF',
         '-DLLVM_ENABLE_ZSTD=OFF', '-DLLVM_ENABLE_ZLIB=OFF',
         '-DLLVM_PARALLEL_LINK_JOBS=1'],
        ['cmake', '--build', str(build), '--target', *TARGETS, '--parallel', str(args.jobs)],
    ]
    record = dict(start_sha=SHA, branch='main', status='running', started=time.time(),
                  commands=commands, host_compiler=output('/usr/bin/clang', '--version'),
                  cmake=output('cmake', '--version'), ninja=output('ninja', '--version'))
    path = EXPERIMENT / 'pristine-build.json'
    path.write_text(json.dumps(record, indent=2) + '\n')
    for command in commands:
        print('Running:', command, flush=True)
        result = subprocess.run(command, cwd=ROOT)
        if result.returncode:
            record.update(status='failed', returncode=result.returncode, finished=time.time())
            path.write_text(json.dumps(record, indent=2) + '\n')
            raise SystemExit(result.returncode)
    record.update(status='complete', finished=time.time(),
                  compiler_version=output(str(build / 'bin/clang'), '--version'),
                  compiler_sha256=file_sha256(build / 'bin/clang'))
    path.write_text(json.dumps(record, indent=2) + '\n')


if __name__ == '__main__':
    main()
