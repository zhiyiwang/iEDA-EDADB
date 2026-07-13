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

cat >"$TMP_DIR/components_a.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
COMPONENTS 2 ;
    - inst_b cell_b + PLACED ( 20 20 ) N ;
    - inst_a cell_a + PLACED ( 10 10 ) N ;
END COMPONENTS
END DESIGN
DEF

cat >"$TMP_DIR/components_b.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
COMPONENTS 2 ;
    - inst_a cell_a + PLACED ( 10 10 ) N ;
    - inst_b cell_b + PLACED ( 20 20 ) N ;
END COMPONENTS
END DESIGN
DEF

cat >"$TMP_DIR/pins_a.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
PINS 2 ;
    - pin_b + NET net_b + DIRECTION INPUT ;
    - pin_a + NET net_a + DIRECTION OUTPUT ;
END PINS
END DESIGN
DEF

cat >"$TMP_DIR/pins_b.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
PINS 2 ;
    - pin_a + NET net_a + DIRECTION OUTPUT ;
    - pin_b + NET net_b + DIRECTION INPUT ;
END PINS
END DESIGN
DEF

cat >"$TMP_DIR/nets_a.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
NETS 2 ;
    - net_b ( inst_b A )
      + ROUTED met1 ( 0 0 ) ( 10 0 )
    ;
    - net_a ( inst_a A )
      + ROUTED met2 ( 20 0 ) ( 30 0 )
    ;
END NETS
END DESIGN
DEF

cat >"$TMP_DIR/nets_b.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
NETS 2 ;
    - net_a ( inst_a A )
      + ROUTED met2 ( 20 0 ) ( 30 0 )
    ;
    - net_b ( inst_b A )
      + ROUTED met1 ( 0 0 ) ( 10 0 )
    ;
END NETS
END DESIGN
DEF

cat >"$TMP_DIR/net_nested_changed.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
NETS 2 ;
    - net_b ( inst_b A )
      + ROUTED met1 ( 10 0 ) ( 0 0 )
    ;
    - net_a ( inst_a A )
      + ROUTED met2 ( 20 0 ) ( 30 0 )
    ;
END NETS
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

cat >"$TMP_DIR/fills_a.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
FILLS 2 ;
    - LAYER met2 RECT ( 20 20 ) ( 30 30 ) ;
    - LAYER met1 RECT ( 0 0 ) ( 10 10 ) ;
END FILLS
END DESIGN
DEF

cat >"$TMP_DIR/fills_b.def" <<'DEF'
VERSION 5.8 ;
DESIGN demo ;
FILLS 2 ;
    - LAYER met1 RECT ( 0 0 ) ( 10 10 ) ;
    - LAYER met2 RECT ( 20 20 ) ( 30 30 ) ;
END FILLS
END DESIGN
DEF

assert_norm_same "$TMP_DIR/regions_a.def" "$TMP_DIR/regions_b.def" "D REGIONS root order"
assert_norm_same "$TMP_DIR/rows_a.def" "$TMP_DIR/rows_b.def" "B ROWS root order"
assert_norm_same "$TMP_DIR/components_a.def" "$TMP_DIR/components_b.def" "C COMPONENTS root order"
assert_norm_different "$TMP_DIR/pins_a.def" "$TMP_DIR/pins_b.def" "DEF-fallback PINS root order"
assert_norm_different "$TMP_DIR/nets_a.def" "$TMP_DIR/nets_b.def" "DEF-fallback NETS root order"
assert_norm_different "$TMP_DIR/nets_a.def" "$TMP_DIR/net_nested_changed.def" "NETS nested route order"
assert_norm_different "$TMP_DIR/special_a.def" "$TMP_DIR/special_b.def" "DEF-fallback SPECIALNETS root order"
assert_norm_different "$TMP_DIR/special_a.def" "$TMP_DIR/special_nested_changed.def" "SPECIALNETS nested order"
assert_norm_different "$TMP_DIR/fills_a.def" "$TMP_DIR/fills_b.def" "DEF-fallback FILLS root order"
assert_norm_same "$TMP_DIR/vias_a.def" "$TMP_DIR/vias_b.def" "D VIAS root block order"
assert_norm_different "$TMP_DIR/vias_a.def" "$TMP_DIR/via_nested_changed.def" "VIAS nested geometry order"

echo "All normalize_def_for_diff tests passed."
