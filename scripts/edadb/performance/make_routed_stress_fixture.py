#!/usr/bin/env python3
"""Create a routed-memory stress DEF from an existing routed DEF.

The generator copies the longest real NETS record's routing body under unique
root net names.  It intentionally does not invent new routing syntax: nested
wire/segment/point/via order and geometry come from a parser-proven record.
"""

from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} INPUT_DEF OUTPUT_DEF EXTRA_NETS", file=sys.stderr)
        return 2

    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    extra_nets = int(sys.argv[3])
    if extra_nets <= 0:
        raise ValueError("EXTRA_NETS must be positive")

    lines = source.read_text(encoding="utf-8").splitlines(keepends=True)
    section_start = next(index for index, line in enumerate(lines) if line.startswith("NETS "))
    section_end = next(
        index
        for index in range(section_start + 1, len(lines))
        if lines[index].startswith("END NETS")
    )

    records: list[list[str]] = []
    record: list[str] = []
    for line in lines[section_start + 1 : section_end]:
        if line.startswith("- "):
            if record:
                records.append(record)
            record = [line]
        elif record:
            record.append(line)
    if record:
        records.append(record)
    if not records:
        raise ValueError("INPUT_DEF has an empty NETS section")

    template = max(records, key=len)
    route_body = template[1:]
    synthetic_records: list[str] = []
    for index in range(extra_nets):
        synthetic_records.append(f"- __p5_route_stress_{index:06d}\n")
        synthetic_records.extend(route_body)

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        "".join(
            lines[:section_start]
            + [f"NETS {len(records) + extra_nets} ;\n"]
            + [line for original in records for line in original]
            + synthetic_records
            + lines[section_end:]
        ),
        encoding="utf-8",
    )
    print(
        f"template_lines={len(template)} original_nets={len(records)} "
        f"extra_nets={extra_nets} output={output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
