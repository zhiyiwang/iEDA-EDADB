#!/usr/bin/env python3
"""Normalize DEF root-record order for EDADB regression semantic diff.

The default mode reorders only D-level roots. The experimental ``abcd`` mode
also reorders A/B/C roots. Nested record contents always keep their order.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Callable, Iterable, List, Sequence, Tuple


D_BLOCK_SECTIONS = {
    "VIAS",
    "REGIONS",
    "BLOCKAGES",
    "SLOTS",
    "FILLS",
    "GROUPS",
    "SPECIALNETS",
}

ABC_BLOCK_SECTIONS = {
    "COMPONENTS",
    "PINS",
    "NETS",
}

def compact_text(lines: Sequence[str]) -> str:
    return " ".join(" ".join(line.strip().split()) for line in lines)


def first_record_name(record: Sequence[str]) -> str:
    if not record:
        return ""
    parts = record[0].strip().split()
    if len(parts) >= 2 and parts[0] == "-":
        return parts[1]
    return compact_text(record)


def value_after_keyword(text: str, keyword: str) -> str:
    match = re.search(rf"\b{re.escape(keyword)}\s+([^\s;]+)", text)
    return match.group(1) if match else ""


def key_track(lines: Sequence[str]) -> Tuple[str, ...]:
    text = compact_text(lines)
    match = re.search(r"\bTRACKS\s+(\S+)\s+(-?\d+)\s+DO\s+(\d+)\s+STEP\s+(-?\d+)", text)
    layer_match = re.search(r"\bLAYER\s+(.+?)\s*;", text)
    layer_text = layer_match.group(1).strip() if layer_match else ""
    if match:
        return (match.group(1), match.group(2), match.group(3), match.group(4), layer_text, text)
    return (text,)


def key_gcellgrid(lines: Sequence[str]) -> Tuple[str, ...]:
    text = compact_text(lines)
    match = re.search(r"\bGCELLGRID\s+(\S+)\s+(-?\d+)\s+DO\s+(\d+)\s+STEP\s+(-?\d+)", text)
    if match:
        return (match.group(1), match.group(2), match.group(3), match.group(4), text)
    return (text,)


def key_named(record: Sequence[str]) -> Tuple[str, ...]:
    return (first_record_name(record), compact_text(record))


def key_blockage(record: Sequence[str]) -> Tuple[str, ...]:
    return (compact_text(record),)


def key_slot(record: Sequence[str]) -> Tuple[str, ...]:
    text = compact_text(record)
    return (value_after_keyword(text, "LAYER"), text)


def key_fill(record: Sequence[str]) -> Tuple[str, ...]:
    text = compact_text(record)
    fill_type = ""
    fill_name = ""
    first_line = record[0] if record else ""
    if re.search(r"^\s*-\s+LAYER\b", first_line):
        fill_type = "LAYER"
        fill_name = value_after_keyword(text, "LAYER")
    elif re.search(r"^\s*-\s+VIA\b", first_line):
        fill_type = "VIA"
        fill_name = value_after_keyword(text, "VIA")
    return (fill_type, fill_name, text)


def block_key(section: str) -> Callable[[Sequence[str]], Tuple[str, ...]]:
    if section in {"VIAS", "REGIONS", "GROUPS", "SPECIALNETS"}:
        return key_named
    if section == "BLOCKAGES":
        return key_blockage
    if section == "SLOTS":
        return key_slot
    if section == "FILLS":
        return key_fill
    return lambda record: (compact_text(record),)


def statement_key(kind: str) -> Callable[[Sequence[str]], Tuple[str, ...]]:
    if kind == "TRACKS":
        return key_track
    if kind == "GCELLGRID":
        return key_gcellgrid
    return lambda record: (compact_text(record),)


def section_name_from_header(line: str, block_sections: set[str]) -> str | None:
    stripped = line.strip()
    match = re.match(r"^([A-Z][A-Z0-9_]*)\b", stripped)
    if not match:
        return None
    name = match.group(1)
    if name in block_sections:
        return name
    return None


def top_statement_name(line: str, statement_kinds: set[str]) -> str | None:
    stripped = line.strip()
    match = re.match(r"^(ROW|TRACKS|GCELLGRID)\b", stripped)
    if match and match.group(1) not in statement_kinds:
        return None
    return match.group(1) if match else None


def split_block_records(body: Sequence[str]) -> Tuple[List[str], List[List[str]], List[str]]:
    prefix: List[str] = []
    records: List[List[str]] = []
    suffix: List[str] = []
    current: List[str] | None = None

    for line in body:
        if line.lstrip().startswith("- "):
            if current is not None:
                records.append(current)
            current = [line]
        else:
            if current is None:
                prefix.append(line)
            else:
                current.append(line)
    if current is not None:
        records.append(current)
    return prefix, records, suffix


def normalize_block(section: str, block: Sequence[str]) -> List[str]:
    if len(block) <= 2:
        return list(block)
    header = [block[0]]
    footer = [block[-1]] if block[-1].strip() == f"END {section}" else []
    body = block[1:-1] if footer else block[1:]
    prefix, records, suffix = split_block_records(body)
    sorted_records = sorted(records, key=block_key(section))
    output: List[str] = []
    output.extend(header)
    output.extend(prefix)
    for record in sorted_records:
        output.extend(record)
    output.extend(suffix)
    output.extend(footer)
    return output


def flush_statement_run(
    output: List[str], run_kind: str | None, run_lines: List[str], statement_kinds: set[str]
) -> None:
    if not run_lines:
        return
    if run_kind is None:
        output.extend(run_lines)
        run_lines.clear()
        return
    separator_lines = [line for line in run_lines if not line.strip()]
    statement_lines = [line for line in run_lines if line.strip()]
    records: List[List[str]] = []
    current: List[str] | None = None
    for line in statement_lines:
        if top_statement_name(line, statement_kinds) == run_kind:
            if current is not None:
                records.append(current)
            current = [line]
        elif current is not None:
            current.append(line)
    if current is not None:
        records.append(current)

    for record in sorted(records, key=statement_key(run_kind)):
        output.extend(record)
    output.extend(separator_lines)
    run_lines.clear()


def normalize_lines(lines: Sequence[str], root_levels: str = "d") -> List[str]:
    output: List[str] = []
    index = 0
    statement_run_kind: str | None = None
    statement_run_lines: List[str] = []

    block_sections = set(D_BLOCK_SECTIONS)
    statement_kinds = {"TRACKS", "GCELLGRID"}
    if root_levels == "abcd":
        block_sections.update(ABC_BLOCK_SECTIONS)
        statement_kinds.add("ROW")

    while index < len(lines):
        line = lines[index]
        statement_kind = top_statement_name(line, statement_kinds)
        if statement_kind is not None:
            if statement_run_kind not in (None, statement_kind):
                flush_statement_run(output, statement_run_kind, statement_run_lines, statement_kinds)
            statement_run_kind = statement_kind
            statement_run_lines.append(line)
            index += 1
            continue

        if statement_run_kind is not None and not line.strip():
            statement_run_lines.append(line)
            index += 1
            continue

        flush_statement_run(output, statement_run_kind, statement_run_lines, statement_kinds)
        statement_run_kind = None

        section = section_name_from_header(line, block_sections)
        if section is None:
            output.append(line)
            index += 1
            continue

        end_marker = f"END {section}"
        block = [line]
        index += 1
        while index < len(lines):
            block.append(lines[index])
            if lines[index].strip() == end_marker:
                index += 1
                break
            index += 1
        output.extend(normalize_block(section, block))

    flush_statement_run(output, statement_run_kind, statement_run_lines, statement_kinds)
    return output


def normalize_file(path: Path, root_levels: str = "d") -> str:
    text = path.read_text()
    lines = text.splitlines(keepends=True)
    normalized = normalize_lines(lines, root_levels)
    return "".join(normalized)


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser(description="Normalize selected DEF root-record order")
    parser.add_argument(
        "--root-levels",
        choices=("d", "abcd"),
        default="d",
        help="root order levels to normalize (default: d)",
    )
    parser.add_argument("def_file", type=Path)
    args = parser.parse_args(list(argv))
    sys.stdout.write(normalize_file(args.def_file, args.root_levels))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
