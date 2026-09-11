#!/usr/bin/env python3
"""Patch e32 floating-point reduction extraction for XiangShan."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import tempfile


LMULS = ("m1", "m2", "m4", "m8")
WORKAROUND_NAME = "xiangshan-f32-reduction-extract-via-gpr-e64-v1"
MARKER = f"# {WORKAROUND_NAME}"
SUPPORTED_REDUCTIONS = {
    "vfredosum.vs",
    "vfredusum.vs",
    "vfredmin.vs",
    "vfredmax.vs",
}
WIDENING_REDUCTIONS = {"vfwredosum.vs", "vfwredusum.vs"}

FUNCTION_RE = re.compile(
    r"(?ms)^[ \t]*\.type[ \t]+(?P<name>[A-Za-z_.$][\w.$]*),[ \t]*@function[^\n]*\n"
    r"(?P<body>.*?)"
    r"(?=^[ \t]*\.size[ \t]+(?P=name)[ \t]*,)"
)
INSTRUCTION_RE = re.compile(
    r"(?m)^(?P<indent>[ \t]*)(?P<opcode>[A-Za-z][A-Za-z0-9_.]*)"
    r"(?:[ \t]+(?P<operands>[^#\n]*?))?[ \t]*(?:#.*)?(?P<newline>\n|$)"
)
IGNORABLE_RE = re.compile(r"(?m)^[ \t]*(?:#[^\n]*)?(?:\n|$)")
VECTOR_CONFIGS = {"vsetvli", "vsetivli", "vsetvl"}


class WorkaroundError(ValueError):
    """Assembly does not match a safe workaround shape."""


def _operands(instruction: re.Match[str]) -> list[str]:
    return [operand.strip() for operand in (instruction["operands"] or "").split(",")]


def _only_space_or_comments(text: str) -> bool:
    return not IGNORABLE_RE.sub("", text)


def _active_sew(instructions: list[re.Match[str]], reduction_index: int) -> str | None:
    for instruction in reversed(instructions[:reduction_index]):
        if instruction["opcode"].lower() not in VECTOR_CONFIGS:
            continue
        match = re.search(r"(?:^|,)\s*(e(?:8|16|32|64))\b", instruction["operands"] or "")
        return match.group(1) if match else None
    return None


def _active_vtype(instructions: list[re.Match[str]], reduction_index: int) -> str:
    """Recover the immediate vtype; never restore through XiangShan's vsetvl path."""
    for instruction in reversed(instructions[:reduction_index]):
        opcode = instruction["opcode"].lower()
        if opcode not in VECTOR_CONFIGS:
            continue
        if opcode != "vsetvl":
            suffix = ", ".join(_operands(instruction)[2:])
            if re.fullmatch(r"e32, m(?:f[248]|[1248]), t[au], m[au]", suffix):
                return suffix
        break
    raise WorkaroundError("state-preserving extraction requires a known immediate e32 vtype")


def _is_vector_instruction(instruction: re.Match[str]) -> bool:
    return instruction["opcode"].lower().startswith("v")


def _paired_reductions(body: str) -> list[tuple[list[re.Match[str]], int]]:
    instructions = list(INSTRUCTION_RE.finditer(body))
    pairs: list[tuple[list[re.Match[str]], int]] = []
    reductions = SUPPORTED_REDUCTIONS | WIDENING_REDUCTIONS
    for index, instruction in enumerate(instructions):
        if instruction["opcode"].lower() not in reductions or index + 1 == len(instructions):
            continue
        extraction = instructions[index + 1]
        between = body[instruction.end() : extraction.start()]
        if (
            extraction["opcode"].lower() == "vfmv.f.s"
            and _only_space_or_comments(between)
        ):
            pairs.append((instructions, index))
    return pairs


def remaining_extraction_count(text: str) -> int:
    """Return terminal or unsafe reduction/extraction pairs still present."""

    return sum(len(_paired_reductions(match["body"])) for match in FUNCTION_RE.finditer(text))


def _rewrite_function(body: str, lmul: str, preserve_state: bool) -> tuple[str, int]:
    replacements: list[tuple[int, int, str]] = []
    for instructions, reduction_index in _paired_reductions(body):
        reduction = instructions[reduction_index]
        extraction = instructions[reduction_index + 1]
        opcode = reduction["opcode"].lower()
        if opcode in WIDENING_REDUCTIONS:
            raise WorkaroundError("widening floating-point reduction extraction is unsupported")

        reduction_operands = _operands(reduction)
        extraction_operands = _operands(extraction)
        if len(reduction_operands) < 2 or not re.fullmatch(r"v\d+", reduction_operands[0]):
            raise WorkaroundError("floating-point reduction has an invalid destination register")
        if len(extraction_operands) != 2:
            raise WorkaroundError("vfmv.f.s extraction has invalid operands")
        fp_destination = extraction_operands[0]
        if not re.fullmatch(r"(?:f(?:[0-9]|[12][0-9]|3[01])|fa[0-7]|ft(?:[0-9]|1[01])|fs(?:[0-9]|1[01]))", fp_destination):
            raise WorkaroundError("vfmv.f.s extraction has an invalid floating-point destination")
        if not preserve_state and fp_destination != "fa0":
            raise WorkaroundError("floating-point reduction extraction destination is not fa0")
        vd = reduction_operands[0]
        if extraction_operands[1] != vd:
            raise WorkaroundError(
                f"floating-point reduction destination {vd} does not match extraction source "
                f"{extraction_operands[1]}"
            )
        sew = _active_sew(instructions, reduction_index)
        if sew != "e32":
            raise WorkaroundError(
                "floating-point reduction extraction is not known to use e32 "
                f"(active SEW: {sew or 'unknown'})"
            )
        if not preserve_state and any(_is_vector_instruction(item) for item in instructions[reduction_index + 2 :]):
            raise WorkaroundError("vector instruction follows floating-point reduction extraction")

        indent = reduction["indent"]
        reduction_line = body[reduction.start() : reduction.end()].rstrip("\n")
        between = body[reduction.end() : extraction.start()]
        newline = extraction["newline"] or "\n"
        prefix = [f"{indent}{MARKER}"]
        if preserve_state:
            # No liveness assumptions: keep the ABI-aligned stack and all scratch
            # GPRs intact. Save runtime VL, but recover vtype from the assembly:
            # register-form vsetvl can stall the deployed XiangShan RTL after
            # committing, even though the extraction itself completed correctly.
            restore_vtype = _active_vtype(instructions, reduction_index)
            prefix += [
                f"{indent}addi\tsp, sp, -16",
                f"{indent}sd\tt0, 0(sp)",
                f"{indent}sd\tt1, 8(sp)",
                f"{indent}csrr\tt1, vl",
            ]
        prefix += [
            f"{indent}vsetvli\tzero, zero, e32, {lmul}, tu, ma",
            reduction_line,
        ]
        replacement = "\n".join(prefix)
        replacement += "\n" + between
        scratch = "t0" if preserve_state else "a0"
        suffix = [
            f"{indent}vsetivli\tzero, 1, e64, m1, ta, ma",
            f"{indent}vmv.x.s\t{scratch}, {vd}",
            f"{indent}fmv.w.x\t{fp_destination}, {scratch}",
        ]
        if preserve_state:
            suffix += [
                f"{indent}vsetvli\tzero, t1, {restore_vtype}",
                f"{indent}ld\tt0, 0(sp)",
                f"{indent}ld\tt1, 8(sp)",
                f"{indent}addi\tsp, sp, 16",
            ]
        replacement += "\n".join(suffix)
        replacement += newline
        replacements.append((reduction.start(), extraction.end(), replacement))

    for start, end, replacement in reversed(replacements):
        body = body[:start] + replacement + body[end:]
    return body, len(replacements)


def rewrite_assembly(text: str, lmul: str, *, preserve_state: bool = False) -> tuple[str, int]:
    """Rewrite safe reduction extractions and return text plus applied count."""

    if lmul not in LMULS:
        raise ValueError(f"unsupported LMUL: {lmul}")

    replacements: list[tuple[int, int, str]] = []
    count = 0
    for function in FUNCTION_RE.finditer(text):
        body, applied = _rewrite_function(function["body"], lmul, preserve_state)
        if applied:
            replacements.append((function.start("body"), function.end("body"), body))
            count += applied
    for start, end, replacement in reversed(replacements):
        text = text[:start] + replacement + text[end:]
    return text, count


def patch_file(path: str | Path, lmul: str, *, preserve_state: bool = False) -> int:
    """Atomically rewrite an assembly file and return applied count."""

    assembly = Path(path)
    original = assembly.read_text()
    rewritten, count = rewrite_assembly(original, lmul, preserve_state=preserve_state)
    if rewritten == original:
        return count

    mode = assembly.stat().st_mode
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{assembly.name}.", suffix=".tmp", dir=assembly.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w") as output:
            output.write(rewritten)
            output.flush()
            os.fsync(output.fileno())
        os.chmod(temporary, mode)
        os.replace(temporary, assembly)
    finally:
        if temporary.exists():
            temporary.unlink()
    return count


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("assembly", type=Path, metavar="ASM")
    parser.add_argument("--lmul", choices=LMULS, required=True)
    parser.add_argument("--preserve-state", action="store_true",
                        help="preserve scratch GPRs, runtime vl and immediate vtype; allow any FP destination and later vector operations")
    args = parser.parse_args(argv)
    try:
        count = patch_file(args.assembly, args.lmul, preserve_state=args.preserve_state)
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")
    print(f"applied {count} {WORKAROUND_NAME} workaround(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
