#!/usr/bin/env python3
"""Extract and verify the report's immutable inputs; refuse overwrites."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def extract(report, destination):
    destination = destination.resolve()
    files = {}
    for name, expected, body in re.findall(
        r'<!-- file: ([^\n]+) sha256: ([0-9a-f]{64}) -->\n```[^\n]*\n(.*?)\n```',
        report.read_text(), re.S
    ):
        path = (destination / name).resolve()
        data = (body + '\n').encode()
        if not path.is_relative_to(destination) or name in files:
            raise ValueError('Invalid or duplicate path: ' + name)
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError('Hash mismatch: ' + name)
        files[name] = (data, expected)
    if not files:
        raise ValueError('No embedded files found')
    manifest_path = destination.parent / 'input-manifest.json'
    if manifest_path.exists():
        raise FileExistsError(manifest_path)
    destination.mkdir(parents=True, exist_ok=False)
    for name, (data, _) in files.items():
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    manifest_path.write_text(json.dumps({
        'report_sha256': hashlib.sha256(report.read_bytes()).hexdigest(),
        'files': {name: digest for name, (_, digest) in files.items()},
    }, indent=2) + '\n')
    print('Verified and extracted', len(files), 'files')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    extract(args.report, args.destination)
