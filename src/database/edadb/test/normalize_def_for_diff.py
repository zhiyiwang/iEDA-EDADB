#!/usr/bin/env python3
"""Normalize DEF files for EDADB regression semantic diff.

All A/B/C/D root records may be reordered for the no-sort experiment branch.
Nested record contents are kept exactly in their original order.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Callable, Iterable, List, Sequence, Tuple


ROOT_BLOCK_SECTIONS = {
    "COMPONENTS",
    "PINS",
    "NETS",
    "VIAS",
    "REGIONS",
    "BLOCKAGES",
    "SLOTS",
    "FILLS",
    "GROUPS",
    "SPECIALNETS",
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
    if section in {"COMPONENTS", "PINS", "NETS", "VIAS", "REGIONS", "GROUPS", "SPECIALNETS"}:
        return key_named
    if section == "BLOCKAGES":
        return key_blockage
    if section == "SLOTS":
        return key_slot
    if section == "FILLS":
        return key_fill
    return lambda record: (compact_text(record),)


def statement_key(kind: str) -> Callable[[Sequence[str]], Tuple[str, ...]]:
    if kind == "ROW":
        return key_named
    if kind == "TRACKS":
        return key_track
    if kind == "GCELLGRID":
        return key_gcellgrid
    return lambda record: (compact_text(record),)


def section_name_from_header(line: str) -> str | None:
    stripped = line.strip()
    match = re.match(r"^([A-Z][A-Z0-9_]*)\b", stripped)
    if not match:
        return None
    name = match.group(1)
    if name in ROOT_BLOCK_SECTIONS:
        return name
    return None


def top_statement_name(line: str) -> str | None:
    stripped = line.strip()
    match = re.match(r"^(ROW|TRACKS|GCELLGRID)\b", stripped)
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


def flush_statement_run(output: List[str], run_kind: str | None, run_lines: List[str]) -> None:
    if not run_lines:
        return
    if run_kind is None:
        output.extend(run_lines)
        run_lines.clear()
        return
    sorted_lines = sorted(([line] for line in run_lines), key=statement_key(run_kind))
    for record in sorted_lines:
        output.extend(record)
    run_lines.clear()


def normalize_lines(lines: Sequence[str]) -> List[str]:
    output: List[str] = []
    index = 0
    statement_run_kind: str | None = None
    statement_run_lines: List[str] = []

    while index < len(lines):
        line = lines[index]
        statement_kind = top_statement_name(line)
        if statement_kind is not None:
            if statement_run_kind not in (None, statement_kind):
                flush_statement_run(output, statement_run_kind, statement_run_lines)
            statement_run_kind = statement_kind
            statement_run_lines.append(line)
            index += 1
            continue

        flush_statement_run(output, statement_run_kind, statement_run_lines)
        statement_run_kind = None

        section = section_name_from_header(line)
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

    flush_statement_run(output, statement_run_kind, statement_run_lines)
    return output


def normalize_file(path: Path) -> str:
    text = path.read_text()
    lines = text.splitlines(keepends=True)
    normalized = normalize_lines(lines)
    return "".join(normalized)


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser(description="Normalize DEF for no-sort ABCD root-order diff")
    parser.add_argument("def_file", type=Path)
    args = parser.parse_args(list(argv))
    sys.stdout.write(normalize_file(args.def_file))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
