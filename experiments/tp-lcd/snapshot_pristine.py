#!/usr/bin/env python3
"""Preserve pristine tools and resources before any modified compiler rebuild."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    record = json.loads((ROOT / 'pristine-build.json').read_text())
    build = REPO / 'build-tp-lcd-main'
    if record['status'] != 'complete' or sha(build / 'bin/clang') != record['compiler_sha256']:
        raise SystemExit('Pristine build is not complete or compiler hash changed')
    dest = ROOT / 'toolchain/pristine'
    dest.mkdir(parents=True, exist_ok=False)
    (dest / 'bin').mkdir()
    names = ['clang', 'opt', 'llc', 'FileCheck', 'llvm-as', 'llvm-dis',
             'llvm-link', 'llvm-config', 'llvm-readobj', 'count', 'not']
    for name in names:
        shutil.copy2(build / 'bin' / name, dest / 'bin' / name)
    (dest / 'bin/clang++').symlink_to('clang')
    resource = Path(subprocess.check_output([str(build / 'bin/clang'), '-print-resource-dir'], text=True).strip())
    resource_dest = dest / 'lib/clang' / resource.name
    shutil.copytree(resource, resource_dest)
    hashes = {str(path.relative_to(dest)): sha(path)
              for path in sorted(dest.rglob('*')) if path.is_file()}
    if hashes['bin/clang'] != record['compiler_sha256']:
        raise SystemExit('Snapshot compiler mismatch')
    actual_resource = Path(subprocess.check_output([str(dest / 'bin/clang'), '-print-resource-dir'], text=True).strip())
    if actual_resource.resolve() != resource_dest.resolve():
        raise SystemExit('Snapshot compiler does not find preserved resource directory')
    snapshot = dict(start_sha=record['start_sha'], source_build=str(build),
                    destination=str(dest), resource_directory=str(resource_dest),
                    files=hashes, compiler_version=subprocess.check_output(
                        [str(dest / 'bin/clang'), '--version'], text=True))
    (ROOT / 'pristine-snapshot.json').write_text(json.dumps(snapshot, indent=2) + '\n')
    for path in dest.rglob('*'):
        if path.is_file() and not path.is_symlink():
            path.chmod(path.stat().st_mode & ~0o222)
    print('Preserved', len(hashes), 'tools/resource files at', dest)


if __name__ == '__main__':
    main()
