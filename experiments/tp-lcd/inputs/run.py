#!/usr/bin/env python3
"""Measure rebuilt ELFs using an externally supplied XiangShan emulator."""
import argparse
import csv
import json
from pathlib import Path
import re
import subprocess

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, default=Path("out"))
    p.add_argument("--results", type=Path, default=Path("rerun"))
    p.add_argument("--emu", type=Path, required=True)
    p.add_argument("--ref", type=Path, required=True)
    p.add_argument("--cwd", type=Path, required=True,
                   help="Original emulator working directory, including its init/config inputs")
    args = p.parse_args()
    # A fresh directory prevents accidental mixing with the historical results.
    args.results.mkdir(parents=True, exist_ok=False)
    rows = json.loads((args.build / "build-manifest.json").read_text())
    fields = ["kernel", "vf", "lmul", "interleave_count", "software_pipelining",
              "raw_roi_cycles", "cycles_per_element", "rtl_revision", "seed"]
    with (args.results / "summary.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            directory = args.results / row["kernel"] / f"vf{row['vf']}"
            directory.mkdir(parents=True)
            elf = args.build / row["kernel"] / f"vf{row['vf']}-ic1-swp-off" / "xiangshan.elf"
            command = [str(args.emu.resolve()), "-i", str(elf.resolve()),
                       "--diff", str(args.ref.resolve()), "--max-cycles", "5000000", "--seed", "1"]
            (directory / "command.json").write_text(json.dumps(command, indent=2) + "\n")
            with (directory / "stdout.log").open("w") as stdout, (directory / "stderr.log").open("w") as stderr:
                result = subprocess.run(command, cwd=args.cwd, stdout=stdout, stderr=stderr)
            log = (directory / "stdout.log").read_text()
            markers = re.findall(r"MB_ROI=([0-9a-fA-F]+)", log)
            if result.returncode or len(markers) != 1 or "HIT GOOD TRAP" not in log:
                raise RuntimeError(f"failed run: {directory}")
            cycles = int(markers[0], 16)
            if cycles <= 0:
                raise RuntimeError(f"invalid ROI cycles: {directory}")
            revision = re.search(r"Commit SHA is: ([0-9a-f]+)", log)
            output = {key: row[key] for key in fields[:5]}
            output.update(raw_roi_cycles=cycles, cycles_per_element=cycles / 131072,
                          rtl_revision=revision[1] if revision else "unknown", seed=1)
            writer.writerow(output)
            f.flush()
            print(output, flush=True)

if __name__ == "__main__":
    main()
