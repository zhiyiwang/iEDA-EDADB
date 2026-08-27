#!/usr/bin/env bash

set -euo pipefail

# Run the widest existing Sky130 GCD roundtrip coverage (default: filler):
#   cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/bin
#   bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh \
#     2>&1 | tee run.out
#
# Run one named physical-design stage or list all available stages:
#   bash scripts/edadb/demo/demo.sh irt
#   bash scripts/edadb/demo/demo.sh --list
#
# Override the generated-artifact directory when needed:
#   RUN_DIR=/tmp/iedadb_filler bash scripts/edadb/demo/demo.sh filler
#
# The demo performs native DEF roundtrip, EDADB write, EDADB read, and then
# compares the native and EDADB-restored DEF outputs.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
RESULT_ROOT="$REPO_ROOT/scripts/design/sky130_gcd/result"

if [[ "$#" -gt 1 ]]; then
    echo "Usage: $0 [stage|input_def]" >&2
    exit 2
fi

list_stages() {
    cat <<EOF
Sky130 GCD stages:
  ifp             iFP_result.def
  fix-fanout      iTO_fix_fanout_result.def
  ipl             iPL_result.def
  icts            iCTS_result.def
  ito-drv         iTO_drv_result.def
  ito-hold        iTO_hold_result.def
  ipl-lg          iPL_lg_result.def
  irt             iRT_result.def
  filler          iPL_filler_result.def (default; widest existing GCD coverage)

An existing DEF path may be supplied instead of a stage name.
EOF
}

# No argument selects the largest existing Sky130 GCD DEF result so the default
# command covers as many implemented adapter root families and DEF tags as possible.
selector="${1:-filler}"
case "${selector,,}" in
    -h|--help)
        echo "Usage: $0 [stage|input_def]"
        list_stages
        exit 0
        ;;
    -l|--list)
        list_stages
        exit 0
        ;;
    ifp)
        stage_name=ifp
        INPUT_DEF="$RESULT_ROOT/iFP_result.def"
        ;;
    fix-fanout|fix_fanout|ito-fix-fanout|ito_fix_fanout)
        stage_name=fix-fanout
        INPUT_DEF="$RESULT_ROOT/iTO_fix_fanout_result.def"
        ;;
    ipl)
        stage_name=ipl
        INPUT_DEF="$RESULT_ROOT/iPL_result.def"
        ;;
    icts|cts)
        stage_name=icts
        INPUT_DEF="$RESULT_ROOT/iCTS_result.def"
        ;;
    ito-drv|ito_drv)
        stage_name=ito-drv
        INPUT_DEF="$RESULT_ROOT/iTO_drv_result.def"
        ;;
    ito-hold|ito_hold)
        stage_name=ito-hold
        INPUT_DEF="$RESULT_ROOT/iTO_hold_result.def"
        ;;
    ipl-lg|ipl_lg)
        stage_name=ipl-lg
        INPUT_DEF="$RESULT_ROOT/iPL_lg_result.def"
        ;;
    irt|rt)
        stage_name=irt
        INPUT_DEF="$RESULT_ROOT/iRT_result.def"
        ;;
    filler|ipl-filler|ipl_filler)
        stage_name=filler
        INPUT_DEF="$RESULT_ROOT/iPL_filler_result.def"
        ;;
    *)
        if [[ ! -f "$selector" ]]; then
            echo "ERROR: unknown stage or missing DEF: $selector" >&2
            list_stages >&2
            exit 2
        fi
        INPUT_DEF="$(realpath "$selector")"
        stage_name="$(basename "${INPUT_DEF%.def}")"
        ;;
esac

if [[ ! -f "$INPUT_DEF" ]]; then
    echo "ERROR: stage DEF is missing: $INPUT_DEF" >&2
    exit 1
fi

if [[ -z "${RUN_DIR:-}" ]]; then
    if [[ "$#" -eq 0 ]]; then
        RUN_DIR="$SCRIPT_DIR/result"
    else
        RUN_DIR="$SCRIPT_DIR/result/$stage_name"
    fi
fi

echo "Selected stage: $stage_name"
echo "Input DEF:      $INPUT_DEF"

# Keep the established demo command as a stable wrapper. The general runner uses
# native DEF -> DEF as its executable baseline before comparing EDADB restoration.
DESIGN_PROFILE_DIR="${DESIGN_PROFILE_DIR:-$REPO_ROOT/scripts/design/sky130_gcd}" \
FOUNDRY_DIR="${FOUNDRY_DIR:-$REPO_ROOT/scripts/foundry/sky130}" \
RUN_DIR="$RUN_DIR" \
    bash "$SCRIPT_DIR/../roundtrip/run.sh" "$INPUT_DEF"
