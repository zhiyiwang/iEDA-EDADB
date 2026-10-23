import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    cpu = min(os.sched_getaffinity(0))
    observations = []

    def execute(command, target):
        completed = subprocess.run(command, text=True, capture_output=True, timeout=120)
        target.write_text(completed.stdout + completed.stderr)
        if completed.returncode:
            raise RuntimeError(completed.stderr)
        return completed.stdout

    for route in ("single", "batch100", "switch1", "grouped"):
        for repeat in range(3):
            stem = args.output / f"{route}-{repeat}"
            control = execute(["sudo", "-n", "taskset", "-c", str(cpu), str(args.binary), route, "control"], Path(str(stem) + ".control.log"))
            fifo, ack = str(stem) + ".fifo", str(stem) + ".ack"
            os.mkfifo(fifo)
            os.mkfifo(ack)
            data = str(stem) + ".perf.data"
            sampled = execute(["sudo", "-n", "taskset", "-c", str(cpu), "perf", "record", "-q", "-e", "cycles:u", "-F", "499", "--call-graph", "dwarf,8192", "-D", "-1", "--control", f"fifo:{fifo},{ack}", "-o", data, "--", "env", "SQLITE_TEXT_PERF_PHASE=write", f"SQLITE_TEXT_PERF_CONTROL={fifo}", f"SQLITE_TEXT_PERF_ACK={ack}", str(args.binary), route, "sample"], Path(str(stem) + ".sample.log"))
            execute(["sudo", "-n", "perf", "report", "-i", data, "--stdio", "--children", "--sort", "symbol", "--percent-limit", "0.5", "--no-call-graph"], Path(str(stem) + ".report.txt"))
            execute(["sudo", "-n", "perf", "script", "-i", data, "--show-lost-events"], Path(str(stem) + ".events.txt"))
            os.unlink(fifo)
            os.unlink(ack)
            values = []
            for output in (control, sampled):
                assert "CHECK\tPASS" in output
                values.append(float(next(line.split("\t")[1] for line in output.splitlines() if line.startswith("PROFILE_MS"))))
            observations.append(dict(route=route, repeat=repeat, control_ms=values[0], sampled_ms=values[1]))
            print(f"Profiling heartbeat: {route} {repeat+1}/3", flush=True)
    summary = []
    for route in ("single", "batch100", "switch1", "grouped"):
        group = [row for row in observations if row["route"] == route]
        baseline = statistics.median(row["control_ms"] for row in group)
        delta = statistics.median(row["sampled_ms"]-row["control_ms"] for row in group)
        summary.append(dict(route=route, control_ms=baseline, paired_delta_percent=100*delta/baseline))
    (args.output / "summary.json").write_text(json.dumps(dict(observations=observations, summary=summary, binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(), cpu=cpu, scope="memory/write data only; independent profile binary; no wall-time attribution"), indent=2))


if __name__ == "__main__":
    main()
