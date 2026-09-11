#!/usr/bin/env python3
"""Download the recorded Picolibc package and extract only RISC-V headers."""
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parent
PACKAGE = 'picolibc-riscv64-unknown-elf_1.8.6-2_all.deb'
URL = 'https://archive.ubuntu.com/ubuntu/pool/universe/p/picolibc/' + PACKAGE
SHA256 = '9563bbe39bbdf4eda1970ced8c54e5df47a112d91427fc4fc38100428fd23a9b'


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    target = ROOT / 'toolchain'
    target.mkdir(exist_ok=True)
    package = target / PACKAGE
    if not package.exists():
        partial = package.with_suffix('.partial')
        urllib.request.urlretrieve(URL, partial)
        if file_sha256(partial) != SHA256:
            raise SystemExit('Package SHA-256 mismatch')
        partial.rename(package)
    if file_sha256(package) != SHA256:
        raise SystemExit('Package SHA-256 mismatch')
    dest = target / 'picolibc'
    dest.mkdir(exist_ok=False)
    relative = 'usr/lib/picolibc/riscv64-unknown-elf/include'
    archive = subprocess.Popen(['ar', '-p', str(package), 'data.tar.zst'], stdout=subprocess.PIPE)
    try:
        subprocess.run(['tar', '-xf', '-', '-C', str(dest), './' + relative],
                       stdin=archive.stdout, check=True)
    finally:
        archive.stdout.close()
    if archive.wait():
        raise SystemExit('ar extraction failed')
    headers = dest / relative
    manifest = {
        'package': 'picolibc-riscv64-unknown-elf', 'version': '1.8.6-2',
        'url': URL, 'package_sha256': SHA256,
        'purpose': 'Assembly/scoring headers only; no runtime or simulator execution',
        'historical_package_identity': 'not established',
        'headers': {str(p.relative_to(headers)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in sorted(headers.rglob('*')) if p.is_file()},
    }
    (ROOT / 'header-provenance.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
