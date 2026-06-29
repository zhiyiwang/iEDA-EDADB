/**
 * @file edadb_schema.h
 * @brief iDB schema definitions enabled in the demo branch.
 */

#pragma once

#include "edadb_idb_shadow.h"

// Demo branch scope:
// Design, Die, Row, TrackGrid, GCellGrid, Region, Slot.

#include "database/data/design/db_layout/IdbUnits.h"
TABLE4CLASS(idb::IdbUnits, "iUnits", (_nanoseconds, _picofarads, _ohms, _milliwatts, _milliamps, _volts, _micron_dbu, _megahertz));

#include "database/data/design/db_design/IdbBusBitChars.h"
TABLE4CLASS(idb::IdbBusBitChars, "iBusBitChars", (_left_delimiter, _right_delimiter));

#include "database/data/design/IdbDesign.h"
TABLE4CLASS(idb::IdbDesign, "iDesign", (_design_name, _version, _units, _bus_bit_chars));

#include "shadow/shadow_idb_geometry.h"
TABLE4SHADOW(idb::IdbCoordinate<int32_t>);
TABLE4CLASS(edadb::Shadow<idb::IdbCoordinate<int32_t>>, "iCoordSD", (_vec_idx, _x_sd, _y_sd));

#include "shadow/shadow_idb_die.h"
TABLE4SHADOW_WVEC(idb::IdbDie);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbDie>, "iDieSD", (primary_key), (points_sd));

#include "database/data/design/db_layout/IdbSite.h"
TABLE4CLASS(idb::IdbSite, "iSite", (_name, _width, _heigtht, _b_overlap, _site_class, _symmetry, _orient, _type));

#include "shadow/shadow_idb_row.h"
TABLE4SHADOW(idb::IdbRow);
TABLE4CLASS(edadb::Shadow<idb::IdbRow>, "iRow",
            (_name_sd, _order_sd, _site_name_sd, _site_orient_sd,
             _origin_x_sd, _origin_y_sd, _row_num_x_sd, _row_num_y_sd,
             _step_x_sd, _step_y_sd));

#include "database/data/design/db_design/IdbTrackGrid.h"
TABLE4CLASS(idb::IdbTrack, "iTrack", (_start, _direction, _pitch));

#include "shadow/shadow_idb_track_grid.h"
TABLE4SHADOW_WVEC(idb::IdbTrackGrid);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbTrackGrid>, "iTrackGridSD",
                 (primary_key, _order_sd, _track_num_sd, _track_sd),
                 (_layer_name_vec_sd));

#include "database/data/design/db_layout/IdbGCellGrid.h"
#include "shadow/shadow_idb_gcell_grid.h"
TABLE4SHADOW(idb::IdbGCellGrid);
TABLE4CLASS(edadb::Shadow<idb::IdbGCellGrid>, "iGCellGrid",
            (primary_key, _order_sd, _direction_sd, _start_sd, _num_sd, _space_sd));

#include "database/basic/geometry/IdbGeometry.h"
TABLE4CLASS(idb::IdbRect, "IdbRect", (_lx, _ly, _hx, _hy));

#include "database/data/design/db_design/IdbRegion.h"
#include "shadow/shadow_idb_region.h"
TABLE4SHADOW_WVEC(idb::IdbRegion);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbRegion>, "iRegion",
                 (_name_sd, _order_sd, _type_sd),
                 (_boundary_list_sd));

#include "shadow/shadow_idb_slot.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSlot>, "iSlotSD",
                 (primary_key, _order_sd, _layer_name_sd),
                 (_rect_list_sd));
