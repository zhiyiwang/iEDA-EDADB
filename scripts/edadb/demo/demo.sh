#!/usr/bin/env bash

## only read lef/edadb/def and save def 
## run this script in bin dir, such as:
## `bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/write_def.sh`

set -e


# get script dir, using absolute path
# SCRIPT_DIR=scripts/edadb/demo
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EDADB_TCL_SCRIPT_DIR="${SCRIPT_DIR}/tcl"
DESIGN_TCL_SCRIPT_DIR="${SCRIPT_DIR}/../../design/sky130_gcd/script"

# make it visible to Tcl as ::env(TCL_SCRIPT_DIR)
export EDADB_TCL_SCRIPT_DIR
export DESIGN_TCL_SCRIPT_DIR
echo "EDADB_TCL_SCRIPT_DIR: $EDADB_TCL_SCRIPT_DIR"
echo "DESIGN_TCL_SCRIPT_DIR: $DESIGN_TCL_SCRIPT_DIR"

# (fixed) iEDA setting
# WORKSPACE=scripts/design/sky130_gcd
export WORKSPACE="$SCRIPT_DIR/../../design/sky130_gcd"
export CONFIG_DIR=$WORKSPACE/iEDA_config
export FOUNDRY_DIR=$WORKSPACE/../../foundry/sky130
export RESULT_DIR=$SCRIPT_DIR/result
export TCL_SCRIPT_DIR=$WORKSPACE/script
mkdir -p "$RESULT_DIR"

# design files
export DESIGN_TOP=gcd
export NETLIST_FILE=$WORKSPACE/result/verilog/gcd.v
export SDC_FILE=$FOUNDRY_DIR/sdc/gcd.sdc
export SPEF_FILE=$FOUNDRY_DIR/spef/gcd.spef



# If the script has parameters, use the 1st as INPUT_DEF
if [ $# -ge 1 ]; then
    # output usage and exit
    echo "Usage: $0 [input_def]"
    echo "  input_def: input def file, default: scripts/edadb/demo/design.def"
    export INPUT_DEF="$1"
else
    export INPUT_DEF="$DESIGN_TCL_SCRIPT_DIR/../result/iPL_result.def"
    # export INPUT_DEF "$DESIGN_TCL_SCRIPT_DIR/../result/iFP_result.def"
    # export INPUT_DEF "$DESIGN_TCL_SCRIPT_DIR/../result/iCTS_result.def"
fi
echo "####[demo.sh] INPUT_DEF: $INPUT_DEF"


# edadb for write/read def
export EDADB_DB_PATH="$RESULT_DIR/edadb.db"
rm -f $EDADB_DB_PATH

# edadb def file postfix
export EDADB_DEF_POST="_edadb"

# tcl run parameters
export READ_DEF=1
export WRITE_EDADB=1 # 0: write edadb; 1: write def
export READ_EDADB=1 # 0: read edadb; 1: read def

##gdb ./iEDA
./iEDA -script $EDADB_TCL_SCRIPT_DIR/def2edadb.tcl

##gdb ./iEDA
./iEDA -script $EDADB_TCL_SCRIPT_DIR/edadb2def.tcl

# compare input def and output def, output diff if different
echo "[demo.sh] compare input def and output def:"
echo "input def: $INPUT_DEF"
echo "output def: ${INPUT_DEF%.*}_edadb.def"
diff -q "$INPUT_DEF" "${INPUT_DEF%.*}${EDADB_DEF_POST}.def" > diff_output.txt || true
if [ ! -s diff_output.txt ]; then
    echo "Input def and output def are the same."
    rm diff_output.txt
else
    echo "Input def and output def are different. See diff below:"
    cat diff_output.txt
    rm diff_output.txt
    echo "vimdiff $INPUT_DEF ${INPUT_DEF%.*}${EDADB_DEF_POST}.def"
    exit 1
fi 