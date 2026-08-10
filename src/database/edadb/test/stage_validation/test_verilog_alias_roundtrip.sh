#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"
IEDA_BIN="${IEDA_BIN:-$REPO_ROOT/bin/iEDA}"
DESIGN_DIR="$REPO_ROOT/scripts/design/ihp130_gcd"
OUT_DIR="${OUT_DIR:-/tmp/ieda_verilog_alias_roundtrip}"
INPUT_VERILOG="$SCRIPT_DIR/fixtures/verilog/io_port_alias.v"
OUTPUT_VERILOG="$OUT_DIR/io_port_alias_roundtrip.v"
OUTPUT_DEF="$OUT_DIR/io_port_alias.def"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

RESULT_DIR="$OUT_DIR" \
FOUNDRY_DIR="$REPO_ROOT/scripts/foundry/ihp130" \
IEDA_CONFIG_DIR="$DESIGN_DIR/iEDA_config" \
IEDA_TCL_SCRIPT_DIR="$DESIGN_DIR/script" \
SDC_FILE="$DESIGN_DIR/default.sdc" \
INPUT_VERILOG="$INPUT_VERILOG" \
OUTPUT_VERILOG="$OUTPUT_VERILOG" \
OUTPUT_DEF="$OUTPUT_DEF" \
  "$IEDA_BIN" -script "$SCRIPT_DIR/tcl/run_verilog_alias_roundtrip.tcl" >"$OUT_DIR/run.log" 2>&1

grep -Fxq 'assign shared = in ;' "$OUTPUT_VERILOG"
grep -Fxq 'assign out0 = shared ;' "$OUTPUT_VERILOG"
grep -Fxq 'assign out1 = shared ;' "$OUTPUT_VERILOG"
grep -Fxq 'assign out2 = out1 ;' "$OUTPUT_VERILOG"

if grep -Eq '^assign shared = (out0|out1) ;$' "$OUTPUT_VERILOG"; then
  echo "FAIL: output-port assignment direction was reversed" >&2
  exit 1
fi

for pin in in out0 out1 out2; do
  membership_count="$(grep -o "( PIN $pin )" "$OUTPUT_DEF" | wc -l)"
  if [[ "$membership_count" -ne 1 ]]; then
    echo "FAIL: $pin appears in $membership_count DEF root-net memberships" >&2
    exit 1
  fi
done

echo "PASS: Verilog IO aliases preserve direction and single root-net membership"
