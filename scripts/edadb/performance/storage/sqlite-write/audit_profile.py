import argparse
import json
from pathlib import Path
import re
import statistics


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    root = args.directory
    summary = json.loads((root / "summary.json").read_text())
    reports = []
    for source in sorted(root.glob("*.report.txt")):
        content = source.read_text()
        lost = int(re.search(r"Total Lost Samples:\s*(\d+)", content).group(1))
        samples = int(re.search(r"^# Samples:\s*([\d,]+)", content, re.MULTILINE).group(1).replace(",", ""))
        assert lost == 0 and samples > 0
        symbols = {}
        for line in content.splitlines():
            match = re.match(r"\s*([\d.]+)%\s+([\d.]+)%\s+\[.\]\s+(.+?)\s+-", line)
            if match:
                symbols[match.group(3).strip()] = dict(children=float(match.group(1)), self=float(match.group(2)))
        reports.append(dict(name=source.name, lost=lost, samples=samples, symbols=symbols))
    assert len(reports) == 12 and len(summary["observations"]) == 12
    hotspots = []
    for group in summary["summary"]:
        route = group["route"]
        selected = [report for report in reports if report["name"].startswith(route + "-")]
        output = dict(route=route, paired_delta_percent=group["paired_delta_percent"])
        for symbol in ("sqlite3_step", "bind_record<false>", "sqlite3_reset", "sqlite3VdbeHalt", "sqlite3BtreeInsert"):
            values = [report["symbols"].get(symbol, {}).get("children") for report in selected]
            output[symbol] = statistics.median(values) if all(value is not None for value in values) else None
        hotspots.append(output)
    result = dict(status="PASS", captures=12, controls=12, lost_samples=0,
                  overhead_within_5_percent=all(abs(row["paired_delta_percent"]) <= 5 for row in hotspots),
                  hotspots=hotspots, reports=reports,
                  scope="memory write CPU samples; child percentages overlap; omitted symbols are unknown, not zero")
    (root / "audit.json").write_text(json.dumps(result, indent=2))
    print(json.dumps({key:value for key,value in result.items() if key != "reports"}, indent=2))


if __name__ == "__main__":
    main()
