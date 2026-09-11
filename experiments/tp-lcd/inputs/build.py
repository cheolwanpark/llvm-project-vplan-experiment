#!/usr/bin/env python3
"""Build the nine original kernels at IC=1, SWP=off, scalable VF=2/4/8/16."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parent
CASES = [
    ("s000", "tsvc_kernels.c", 0),
    ("s1111", "tsvc_kernels.c", 1),
    ("s125", "tsvc_kernels.c", 2),
    ("s311", "tsvc_kernels.c", 3),
    ("s4112", "tsvc_kernels.c", 4),
    ("jacobi-2d-imper-l00", "application_kernels.c", 0),
    ("gesummv-l00", "application_kernels.c", 1),
    ("bicg-l01", "application_kernels.c", 2),
    ("gemver-l00", "application_kernels.c", 4),
]
TARGET = [
    "--target=riscv64-unknown-elf",
    "-march=rv64gcv_zba_zbb_zbc_zbs_zicbom_zicboz_zvl128b",
    "-mcpu=xiangshan-kunminghu", "-mabi=lp64d", "-mcmodel=medany",
    "-mno-relax", "-mllvm", "-riscv-v-vector-bits-min=128",
]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="/opt/llvm-main/bin/clang")
    parser.add_argument("--cxx", default="/opt/llvm-main/bin/clang++")
    parser.add_argument("--gcc", default="riscv64-unknown-elf-gcc")
    parser.add_argument("--picolibc", type=Path,
                        default=Path("/usr/lib/picolibc/riscv64-unknown-elf"))
    parser.add_argument("--out", type=Path, default=ROOT / "out")
    parser.add_argument("--assembly-only", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    out = args.out.resolve()

    def run(command, capture=False):
        print(shlex.join(map(str, command)), flush=True)
        if args.dry_run:
            return None
        return subprocess.run(list(map(str, command)), check=True,
                              text=True, capture_output=capture)

    if not args.dry_run:
        out.mkdir(parents=True, exist_ok=True)
        (out / "compiler-version.txt").write_text(
            run([args.cc, "--version"], True).stdout)
    headers = ["-idirafter", str(args.picolibc / "include")]
    runtime_objects = []
    gcc_lib = "<resolved-by-riscv64-unknown-elf-gcc>"
    if not args.assembly_only:
        runtime = out / "runtime"
        if not args.dry_run:
            runtime.mkdir(exist_ok=True)
        runtime_flags = TARGET + [
            "-O2", "-ffreestanding", "-fno-builtin", "-ffunction-sections",
            "-fdata-sections", "-D_DEFAULT_SOURCE=1", "-D_POSIX_C_SOURCE=200112L",
        ] + headers
        for name, source, language in [
            ("startup", "xiangshan/startup.S", "assembler-with-cpp"),
            ("start", "common/start.c", "c"),
            ("syscalls", "common/syscalls.c", "c"),
            ("stdio", "common/stdio.c", "c"),
            ("cxx", "common/cxx.cpp", "c++"),
            ("platform", "xiangshan/platform.c", "c"),
        ]:
            obj = runtime / (name + ".o")
            flags = list(runtime_flags)
            if language == "c++":
                flags += ["-std=c++20", "-stdlib=libc++", "-fno-exceptions", "-fno-rtti"]
            run([args.cxx if language == "c++" else args.cc] + flags +
                ["-x", language, "-c", ROOT / "runtime" / source, "-o", obj])
            runtime_objects.append(str(obj))
        result = run([args.gcc, "-march=rv64iafd", "-mabi=lp64d",
                      "-print-libgcc-file-name"], True)
        if result:
            gcc_lib = result.stdout.strip()
            if not Path(gcc_lib).is_file():
                raise RuntimeError("libgcc.a was not found")

    if not args.dry_run:
        from xiangshan_reduction_workaround import rewrite_assembly, remaining_extraction_count
    manifest = []
    for kernel, source, case in CASES:
        for vf, lmul in [(2, "m1"), (4, "m2"), (8, "m4"), (16, "m8")]:
            directory = out / kernel / f"vf{vf}-ic1-swp-off"
            asm = directory / "xiangshan.s"
            obj = directory / "xiangshan.o"
            elf = directory / "xiangshan.elf"
            defines = ([f"-DTSVC_CASE={case}"] if source == "tsvc_kernels.c" else
                       [f"-DAPP_CASE={case}", "-DAPP_LOOP_SIZE=4096", "-DAPP_REPEATS=32"])
            flags = TARGET + [
                "-std=c11", "-ffreestanding", "-fno-builtin", "-ffunction-sections",
                "-fdata-sections", "-O2", "-ffast-math", "-fno-unroll-loops",
                "-mllvm", "-scalable-vectorization=on",
                "-mllvm", f"-force-vector-width={vf}",
                "-mllvm", "-force-vector-interleave=1",
                "-mllvm", "-riscv-enable-pipeliner=false", "-Rpass=loop-vectorize",
                "-I", str(ROOT / "include"),
            ] + headers + defines
            compile_command = [args.cc] + flags + ["-S", str(ROOT / source), "-o", str(asm)]
            if not args.dry_run:
                directory.mkdir(parents=True, exist_ok=True)
            result = run(compile_command, True)
            applied = 0
            if not args.dry_run:
                (directory / "xiangshan.remarks.txt").write_text(result.stderr)
                expected = f"vectorization width: vscale x {vf}, interleaved count: 1"
                if expected not in result.stderr:
                    raise RuntimeError(f"{kernel}/{vf}: missing forced VF/IC remark")
                original = asm.read_text()
                (directory / "xiangshan.unpatched.s").write_text(original)
                stateful = kernel in ("gesummv-l00", "bicg-l01")
                if kernel == "s311" or stateful:
                    patched, applied = rewrite_assembly(original, lmul, preserve_state=stateful)
                    expected_count = 1 if kernel == "s311" else 2 if kernel == "gesummv-l00" else 0
                    if applied != expected_count or remaining_extraction_count(patched):
                        raise RuntimeError(f"{kernel}/{vf}: unexpected extraction shape/count {applied}")
                    asm.write_text(patched)
                body = re.search(r"(?ms)^selected_kernel:.*?^\s*\.size\s+selected_kernel,", asm.read_text())
                if not body or not re.search(rf"e32,\s*{lmul}\b", body.group()):
                    raise RuntimeError(f"{kernel}/{vf}: missing target-loop LMUL")
            assemble_command = [args.cc] + TARGET + ["-c", str(asm), "-o", str(obj)]
            link_command = [args.cc] + TARGET + [
                "-nostdlib", "-nostartfiles", "-static", "-fuse-ld=lld", "-Wl,--gc-sections",
                "-Wl,-T," + str(ROOT / "runtime/xiangshan/link.ld"),
            ] + runtime_objects + [str(obj), "-L", str(args.picolibc / "lib/rv64iafd/lp64d"),
                                  "-Wl,--start-group", "-lc", "-lm", gcc_lib,
                                  "-Wl,--end-group", "-o", str(elf)]
            if not args.assembly_only:
                run(assemble_command)
                run(link_command)
            if not args.dry_run:
                record = dict(kernel=kernel, vf=vf, lmul=lmul, interleave_count=1,
                              software_pipelining="off", useful_elements=4096 * 32,
                              source_sha256=sha(ROOT / source), assembly_sha256=sha(asm),
                              workaround_applied=applied, compile=compile_command,
                              assemble=assemble_command, link=link_command)
                if not args.assembly_only:
                    record.update(elf=str(elf), elf_sha256=sha(elf))
                (directory / "command.json").write_text(json.dumps(record, indent=2) + "\n")
                manifest.append(record)
    if not args.dry_run:
        (out / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

if __name__ == "__main__":
    main()
