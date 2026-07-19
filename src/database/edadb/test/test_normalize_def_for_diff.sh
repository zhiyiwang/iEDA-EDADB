#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NORMALIZER="$SCRIPT_DIR/normalize_def_for_diff.py"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

normalize() {
    python3 "$NORMALIZER" "$1"
}

assert_norm_same() {
    local left="$1"
    local right="$2"
    local label="$3"
    normalize "$left" >"$TMP_DIR/left.norm"
    normalize "$right" >"$TMP_DIR/right.norm"
    if ! diff -u "$TMP_DIR/left.norm" "$TMP_DIR/right.norm" >"$TMP_DIR/diff.out"; then
        echo "FAIL: $label should normalize to the same DEF" >&2
        cat "$TMP_DIR/diff.out" >&2
        exit 1
    fi
    echo "PASS: $label"
}

assert_norm_different() {
    local left="$1"
    local right="$2"
    local label="$3"
    normalize "$left" >"$TMP_DIR/left.norm"
    normalize "$right" >"$TMP_DIR/right.norm"
    if diff -u "$TMP_DIR/left.norm" "$TMP_DIR/right.norm" >"$TMP_DIR/diff.out"; then
        echo "FAIL: $label should remain different after normalization" >&2
        exit 1
    fi
    echo "PASS: $label"
}

cat >"$TMP_DIR/regions_a.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
REGIONS 2 ;
    - z_region ( 10 10 ) ( 20 20 ) + TYPE FENCE ;
    - a_region ( 30 30 ) ( 40 40 ) + TYPE GUIDE ;
END REGIONS
END DESIGN
DEF

cat >"$TMP_DIR/regions_b.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
REGIONS 2 ;
    - a_region ( 30 30 ) ( 40 40 ) + TYPE GUIDE ;
    - z_region ( 10 10 ) ( 20 20 ) + TYPE FENCE ;
END REGIONS
END DESIGN
DEF

cat >"$TMP_DIR/rows_a.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
ROW ROW_1 unit 0 0 N DO 1 BY 1 STEP 10 0 ;
ROW ROW_2 unit 0 10 FS DO 1 BY 1 STEP 10 0 ;
END DESIGN
DEF

cat >"$TMP_DIR/rows_b.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
ROW ROW_2 unit 0 10 FS DO 1 BY 1 STEP 10 0 ;
ROW ROW_1 unit 0 0 N DO 1 BY 1 STEP 10 0 ;
END DESIGN
DEF

cat >"$TMP_DIR/grids_a.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
TRACKS X 0 DO 10 STEP 20 LAYER met1 met2 ;

TRACKS Y 5 DO 8 STEP 40 LAYER met3 ;

GCELLGRID X 0 DO 4 STEP 100 ;
GCELLGRID Y 10 DO 3 STEP 200 ;
END DESIGN
DEF

cat >"$TMP_DIR/grids_b.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
TRACKS Y 5 DO 8 STEP 40 LAYER met3 ;

TRACKS X 0 DO 10 STEP 20 LAYER met1 met2 ;

GCELLGRID Y 10 DO 3 STEP 200 ;
GCELLGRID X 0 DO 4 STEP 100 ;
END DESIGN
DEF

cat >"$TMP_DIR/grids_nested_changed.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
TRACKS X 0 DO 10 STEP 20 LAYER met2 met1 ;

TRACKS Y 5 DO 8 STEP 40 LAYER met3 ;

GCELLGRID X 0 DO 4 STEP 100 ;
GCELLGRID Y 10 DO 3 STEP 200 ;
END DESIGN
DEF

cat >"$TMP_DIR/special_a.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
SPECIALNETS 2 ;
    - VSS
      + ROUTED met1 ( 0 0 ) ( 10 0 )
      + ROUTED met2 ( 10 0 ) ( 10 10 )
    ;
    - VDD
      + ROUTED met1 ( 20 0 ) ( 30 0 )
    ;
END SPECIALNETS
END DESIGN
DEF

cat >"$TMP_DIR/special_b.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
SPECIALNETS 2 ;
    - VDD
      + ROUTED met1 ( 20 0 ) ( 30 0 )
    ;
    - VSS
      + ROUTED met1 ( 0 0 ) ( 10 0 )
      + ROUTED met2 ( 10 0 ) ( 10 10 )
    ;
END SPECIALNETS
END DESIGN
DEF

cat >"$TMP_DIR/special_nested_changed.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
SPECIALNETS 2 ;
    - VSS
      + ROUTED met2 ( 10 0 ) ( 10 10 )
      + ROUTED met1 ( 0 0 ) ( 10 0 )
    ;
    - VDD
      + ROUTED met1 ( 20 0 ) ( 30 0 )
    ;
END SPECIALNETS
END DESIGN
DEF

cat >"$TMP_DIR/vias_a.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
VIAS 2 ;
    - via_b
      + RECT met1 ( 0 0 ) ( 1 1 )
      + RECT met2 ( 2 2 ) ( 3 3 )
    ;
    - via_a
      + RECT met1 ( 4 4 ) ( 5 5 )
    ;
END VIAS
END DESIGN
DEF

cat >"$TMP_DIR/vias_b.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
VIAS 2 ;
    - via_a
      + RECT met1 ( 4 4 ) ( 5 5 )
    ;
    - via_b
      + RECT met1 ( 0 0 ) ( 1 1 )
      + RECT met2 ( 2 2 ) ( 3 3 )
    ;
END VIAS
END DESIGN
DEF

cat >"$TMP_DIR/via_nested_changed.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
VIAS 2 ;
    - via_b
      + RECT met2 ( 2 2 ) ( 3 3 )
      + RECT met1 ( 0 0 ) ( 1 1 )
    ;
    - via_a
      + RECT met1 ( 4 4 ) ( 5 5 )
    ;
END VIAS
END DESIGN
DEF

assert_norm_same "$TMP_DIR/regions_a.def" "$TMP_DIR/regions_b.def" "D REGIONS root order"
assert_norm_different "$TMP_DIR/rows_a.def" "$TMP_DIR/rows_b.def" "B ROWS root order"
assert_norm_same "$TMP_DIR/grids_a.def" "$TMP_DIR/grids_b.def" "D TRACKS and GCELLGRID root order"
assert_norm_different "$TMP_DIR/grids_a.def" "$TMP_DIR/grids_nested_changed.def" "TRACKS nested layer order"
assert_norm_same "$TMP_DIR/special_a.def" "$TMP_DIR/special_b.def" "D SPECIALNETS root block order"
assert_norm_different "$TMP_DIR/special_a.def" "$TMP_DIR/special_nested_changed.def" "SPECIALNETS nested order"
assert_norm_same "$TMP_DIR/vias_a.def" "$TMP_DIR/vias_b.def" "D VIAS root block order"
assert_norm_different "$TMP_DIR/vias_a.def" "$TMP_DIR/via_nested_changed.def" "VIAS nested geometry order"

echo "All normalize_def_for_diff tests passed."
