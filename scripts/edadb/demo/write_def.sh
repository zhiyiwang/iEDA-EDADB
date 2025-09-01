#!/usr/bin/env bash

## only read lef/edadb/def and save def 
## run this script in bin dir, such as:
## `bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/write_def.sh``

set -e

# If the script has parameters, use the 1st as INPUT_DEF
if [ $# -ge 1 ]; then
    export INPUT_DEF="$1"
fi

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
#export WORKSPACE=$(cd "$(dirname "$0")";pwd)
export CONFIG_DIR=$WORKSPACE/iEDA_config
export FOUNDRY_DIR=$WORKSPACE/../../foundry/sky130
export RESULT_DIR=$WORKSPACE/result
export TCL_SCRIPT_DIR=$WORKSPACE/script
mkdir -p "$RESULT_DIR"

# design files
export DESIGN_TOP=gcd
export NETLIST_FILE=$WORKSPACE/result/verilog/gcd.v
export SDC_FILE=$FOUNDRY_DIR/sdc/gcd.sdc
export SPEF_FILE=$FOUNDRY_DIR/spef/gcd.spef


# read lef, write def
export EDADB_DB_PATH="$WORKSPACE/result/edadb.db"

export WRITE_EDADB=1 # 0: write edadb; 1: write def
export READ_EDADB=1 # 0: read edadb; 1: read def
./iEDA -script $EDADB_TCL_SCRIPT_DIR/edadb.tcl


# basic flow check
if [[ -f "${RESULT_DIR}/.flow_ok" ]]; then
  echo "OK: sentinel found"
else
  echo "ERROR: sentinel missing: ${RESULT_DIR}/.flow_ok" >&2
  exit 1
fi

echo "All minimal checks passed"


## # 读 DEF，写 DEF
## export READ_LEF=1
## export READ_DEF=1
## export READ_EDADB=0
## export WRITE_DEF=1
## export WRITE_EDADB=0
## ./iEDA -script $TCL_SCRIPT_DIR/DB_script/db_read_edadb.tcl
## 
## # 读 edadb，写 DEF
## export READ_LEF=1
## export READ_DEF=0
## export READ_EDADB=1
## export WRITE_DEF=1
## export WRITE_EDADB=0
## ./iEDA -script $TCL_SCRIPT_DIR/DB_script/db_read_edadb.tcl
## 
## # 读 edadb，写 edadb
## export READ_LEF=1
## export READ_DEF=0
## export READ_EDADB=1
## export WRITE_DEF=0
## export WRITE_EDADB=1
## ./iEDA -script $TCL_SCRIPT_DIR/DB_script/db_read_edadb.tcl
##
## # 读取 edadb（可选）
## if {[info exists ::env(READ_EDADB)] && $::env(READ_EDADB)} {
##     ./iEDA -script $::env(TCL_SCRIPT_DIR)/DB_script/db_read_edadb.tcl
## }