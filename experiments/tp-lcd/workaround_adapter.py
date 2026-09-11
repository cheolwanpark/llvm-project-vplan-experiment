"""Safely make scheduled reduction/extraction pairs adjacent for report tooling.

The immutable report postprocessor accepts adjacent pairs. LLVM 22.1.8 can
schedule GESUMMV's two reductions before their scalar extractions. Move an
extraction earlier only across instructions that neither touch its FP result
nor its source vector register, with no control-flow or vector-state changes.
Then apply the original state-preserving workaround unchanged.
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / 'inputs'))
import xiangshan_reduction_workaround as original


def adjacent_extractions(text):
    replacements = []
    moved = 0
    for function in original.FUNCTION_RE.finditer(text):
        body = function['body']
        instructions = list(original.INSTRUCTION_RE.finditer(body))
        edits = []
        for index, extract in enumerate(instructions):
            if extract['opcode'].lower() != 'vfmv.f.s':
                continue
            operands = original._operands(extract)
            if len(operands) != 2:
                raise ValueError('Malformed extraction')
            destination, source = operands
            reduction_index = None
            for prev in range(index - 1, -1, -1):
                inst = instructions[prev]
                ops = original._operands(inst)
                if source not in ops:
                    continue
                if inst['opcode'].lower() in original.SUPPORTED_REDUCTIONS and ops[0] == source:
                    reduction_index = prev
                break
            if reduction_index is None:
                raise ValueError('Extraction has no supported reaching reduction')
            if reduction_index == index - 1:
                continue
            reduction = instructions[reduction_index]
            for prev in range(reduction_index, index):
                inst = instructions[prev]
                following = instructions[prev + 1]
                if not original._only_space_or_comments(body[inst.end():following.start()]):
                    raise ValueError('Extraction crosses a label or directive')
            for inst in instructions[reduction_index + 1:index]:
                opcode = inst['opcode'].lower()
                ops = original._operands(inst)
                if (destination in ops or source in ops or opcode in original.VECTOR_CONFIGS
                        or (opcode.startswith('v') and opcode not in original.SUPPORTED_REDUCTIONS
                            and opcode != 'vfmv.f.s')
                        or opcode.startswith('b') or opcode in ('j', 'jr', 'jal', 'jalr', 'call', 'tail', 'ret')):
                    raise ValueError('Extraction cannot safely cross: ' + inst.group().strip())
            if original._active_sew(instructions, reduction_index) != 'e32':
                raise ValueError('Expected e32 reduction before extraction')
            edits += [(extract.start(), extract.end(), ''),
                      (reduction.end(), reduction.end(), extract.group())]
            moved += 1
        for start, end, replacement in sorted(edits, reverse=True):
            body = body[:start] + replacement + body[end:]
        if edits:
            replacements.append((function.start('body'), function.end('body'), body))
    for start, end, replacement in reversed(replacements):
        text = text[:start] + replacement + text[end:]
    return text, moved


def rewrite_assembly(text, lmul, *, preserve_state=False):
    adjacent, moved = adjacent_extractions(text)
    patched, applied = original.rewrite_assembly(adjacent, lmul, preserve_state=preserve_state)
    # Original remaining_extraction_count only sees adjacent pairs. Explicitly
    # check every extraction so scheduled unmatched ones cannot slip through.
    if re.search(r'^\s*vfmv\.f\.s\s', patched, re.M):
        raise ValueError('Unpatched floating-point vector extraction remains')
    return patched, applied, moved, adjacent
