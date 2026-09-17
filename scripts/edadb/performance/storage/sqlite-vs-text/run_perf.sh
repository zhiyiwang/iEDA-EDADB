#!/usr/bin/env bash
# Run AFTER timing samples finish. Only launched benchmark processes are sampled.
# bash run_perf.sh /tmp/iedadb_sqlite_text_build/profile_benchmark /tmp/iedadb_sqlite_text_perf
# Requires installed perf and sudo permission. No global sysctl changes.
set -euo pipefail
binary="$(realpath "$1")"
out="$2"
test ! -e "$out"
mkdir -p "$out"
out="$(realpath "$out")"
cp "$binary" "$out/profile_benchmark"
cp "$(dirname "$0")/benchmark.cpp" "$out/benchmark.cpp"
cp "$(dirname "$0")/run_perf.sh" "$out/run_perf.sh"
cpu="${PERF_CPU:-0}"
perf version > "$out/perf_version.txt"
sha256sum "$binary" > "$out/binary.sha256"
sudo -n true
for config in A B-batch; do
    for route in sqlite text; do
        for phase in write read; do
            for sample in 1 2 3; do
                stem="$out/$config-$route-$phase-$sample"
                # Same profile binary and privilege context without perf, so
                # sampling overhead is not compared against a different executable.
                sudo -n taskset -c "$cpu" "$binary" "$route" "$config" 1000000 "$stem.control.db" timing "$sample" 0 \
                    > "$stem.control.log" 2>&1
                if test -f "$stem.control.db"; then sudo -n rm "$stem.control.db"; fi
                mkfifo "$stem.control" "$stem.ack"
                # -D -1 starts disabled. The benchmark enables only the selected
                # data phase and waits for acknowledgement outside its data timer.
                timeout 90 sudo -n taskset -c "$cpu" perf record -q -e cycles:u -F 499 \
                    --call-graph dwarf,8192 -D -1 \
                    --control "fifo:$stem.control,$stem.ack" \
                    -o "$stem.perf.data" -- env \
                    SQLITE_TEXT_PERF_PHASE="$phase" \
                    SQLITE_TEXT_PERF_CONTROL="$stem.control" \
                    SQLITE_TEXT_PERF_ACK="$stem.ack" \
                    "$binary" "$route" "$config" 1000000 "$stem.db" timing "$sample" 0 \
                    > "$stem.log" 2>&1
                sudo -n perf report -i "$stem.perf.data" --stdio --children \
                    --sort symbol --percent-limit 0.1 --no-call-graph \
                    > "$stem.report.txt" 2> "$stem.report.err"
                # Allow the user to inspect/copy their own generated perf data.
                sudo -n chown "$(id -u):$(id -g)" "$stem.perf.data"
                if test -f "$stem.db"; then sudo -n rm "$stem.db"; fi
                rm "$stem.control" "$stem.ack"
                echo "$(date '+%H:%M:%S') $config $route $phase $sample PERF COMPLETE"
            done
        done
    done
done
