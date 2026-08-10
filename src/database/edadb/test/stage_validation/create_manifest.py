#!/usr/bin/env python3
"""Write reproducibility metadata for one stage-validation process."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_output(repo: Path, *args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=repo, text=True).strip()


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--stage", required=True)
    parser.add_argument("--mode", required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--command", required=True)
    parser.add_argument("--status", type=int, required=True)
    parser.add_argument("--config", type=Path, action="append", default=[])
    args = parser.parse_args(list(argv))

    manifest = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "git_branch": git_output(args.repo, "branch", "--show-current"),
        "git_commit": git_output(args.repo, "rev-parse", "HEAD"),
        "git_dirty": bool(git_output(args.repo, "status", "--short")),
        "dataset": args.dataset,
        "stage": args.stage,
        "mode": args.mode,
        "input": str(args.input.resolve()),
        "input_sha256": sha256(args.input),
        "configs": {str(path.resolve()): sha256(path) for path in args.config},
        "command": args.command,
        "exit_status": args.status,
        "host": platform.node(),
        "platform": platform.platform(),
        "logical_cpus": os.cpu_count(),
        "mem_available_kib": next(
            int(line.split()[1])
            for line in Path("/proc/meminfo").read_text(encoding="utf-8").splitlines()
            if line.startswith("MemAvailable:")
        ),
        "rt_thread_number": os.environ.get("RT_THREAD_NUMBER", "64"),
        "stage_run_jobs": os.environ.get("STAGE_RUN_JOBS", "auto"),
        "fixture_run_jobs": os.environ.get("FIXTURE_RUN_JOBS", "unset"),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
