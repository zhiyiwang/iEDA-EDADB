/**
 * @file edadb_schema.h
 * @brief This file contains the schema definitions for iEDA classes in edadb database.
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb_idb_shadow.h"

namespace __probe {
  using X = ::boost::fusion::traits::tag_of<int>; 
}

namespace __probe2 {
  using Y = boost::fusion::traits::tag_of<int>; 
}

// Active DEF object-family schema for the C adapter line. Keep every enabled
// schema group synchronized with the matching readIdbXXX/writeIdbXXX path and
// the disabled DEF parser callback in DefReadEdadb::createDbByDef().
#include "database/data/design/db_layout/IdbUnits.h"
TABLE4CLASS(idb::IdbUnits, "iUnits", (_micron_dbu));

#include "database/data/design/db_design/IdbBusBitChars.h"
TABLE4CLASS(idb::IdbBusBitChars, "iBusBitChars", (_left_delimiter, _right_delimiter));

#include "database/data/design/IdbDesign.h"
TABLE4CLASS(idb::IdbDesign, "iDesign", (_design_name, _version, _units, _bus_bit_chars));


#include "shadow/shadow_idb_geometry.h"
TABLE4SHADOW(idb::IdbCoordinate<int32_t>);
TABLE4CLASS (edadb::Shadow<idb::IdbCoordinate<int32_t>>, "iCoordSD", (_vec_idx, _x_sd, _y_sd));

#include "shadow/shadow_idb_die.h"
TABLE4SHADOW_WVEC(idb::IdbDie);
TABLE4CLASS_WVEC (edadb::Shadow<idb::IdbDie>, "iDieSD", (primary_key), (points_sd));


// EDADB_TODO: Row adapter stores only site name/orient and rebuilds row-local
// IdbSite from LEF layout sites. Re-enable this only when a real EDADB iSite
// reader/writer is added.
#if 0
#include "database/data/design/db_layout/IdbSite.h"
TABLE4CLASS(idb::IdbSite, "iSite", (_name, _width, _heigtht, _b_overlap, _site_class, _symmetry, _orient, _type));
#endif

#include "shadow/shadow_idb_row.h"
TABLE4SHADOW(idb::IdbRow);
TABLE4CLASS(edadb::Shadow<idb::IdbRow>, "iRow", (_name_sd, _order_sd, _site_name_sd, _site_orient_sd, _origin_x_sd, _origin_y_sd, _row_num_x_sd, _row_num_y_sd, _step_x_sd, _step_y_sd));

#include "database/data/design/db_design/IdbTrackGrid.h"
TABLE4CLASS(idb::IdbTrack, "iTrack", (_start, _direction, _pitch));

#include "shadow/shadow_idb_track_grid.h"
TABLE4SHADOW_WVEC(idb::IdbTrackGrid);
TABLE4CLASS_WVEC (edadb::Shadow<idb::IdbTrackGrid>, "iTrackGridSD", (primary_key, _track_num_sd, _track_sd), (_layer_name_vec_sd));


#include "database/data/design/db_layout/IdbGCellGrid.h"
TABLE4CLASS(idb::IdbGCellGrid, "iGCellGrid", (_direction, _start, _num, _space));


#include "shadow/shadow_idb_via_master.h"
TABLE4CLASS(edadb::Shadow<idb::IdbViaMasterGenerate>, "iViaMasterGenerateSD", (_rule_name_sd,  _cut_size_x_sd, _cut_size_y_sd, _cut_spacing_x_sd, _cut_spacing_y_sd, _enclosure_bottom_x_sd, _enclosure_bottom_y_sd, _enclosure_top_x_sd, _enclosure_top_y_sd, _num_cut_rows_sd, _num_cut_cols_sd, _original_offset_x_sd, _original_offset_y_sd, _offset_bottom_x_sd, _offset_bottom_y_sd, _offset_top_x_sd, _offset_top_y_sd, _layer_bottom_name_sd, _layer_cut_name_sd, _layer_top_name_sd, _pattern_name_sd));

#include "database/basic/geometry/IdbGeometry.h"
TABLE4SHADOW(idb::IdbRect);
TABLE4CLASS(edadb::Shadow<idb::IdbRect>, "IdbRectSD", (_vec_idx, _lx_sd, _ly_sd, _hx_sd, _hy_sd));

#include "shadow/shadow_idb_layer_shape.h"
TABLE4SHADOW_WVEC(idb::IdbLayerShape);
TABLE4CLASS_WVEC (edadb::Shadow<idb::IdbLayerShape>, "iLayerShapeSD", (primary_key, _vec_idx, _layer_name_sd, _type_sd), (_rect_list_sd));

#include "shadow/shadow_idb_via_master.h"
TABLE4SHADOW_WVEC(idb::IdbViaMaster);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbViaMaster>, "iViaMasterSD", (_name_sd, _type_sd, _master_generate_sd), (fixed_layer_shape_list_sd));

#include "database/data/design/db_design/IdbVias.h"
TABLE4CLASS(idb::IdbVia, "iVia", (_name, _master_instance));


#include "database/data/design/db_design/IdbHalo.h"
TABLE4CLASS(idb::IdbHalo, "iHalo", (_extend_left, _extend_right, _extend_top, _extend_bottom, _is_soft));

#include "shadow/shadow_idb_halo.h"
TABLE4CLASS(edadb::Shadow<idb::IdbRouteHalo>, "iRouteHaloSD", (_route_distance_sd, _layer_bottom_name_sd, _layer_top_name_sd));

#include "shadow/shadow_idb_instance.h"
TABLE4CLASS(edadb::Shadow<idb::IdbInstance>, "iInstSD", (_name_sd, _order_sd, _type_sd, _status_sd, _orient_sd, _weight_sd, _cell_master_name_sd, _coordinate_sd, _halo_sd, _route_halo_sd, _region_name_sd));

#include "shadow/shadow_idb_port.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbPort>, "iPortSD", (primary_key, _vec_idx, _orient_sd, _placement_status_sd, _coordinate_sd), (_layer_shape_list_sd));

#include "shadow/shadow_idb_term.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbTerm>, "iTermSD", (_direction_sd, _type_sd, _has_port_sd, _is_special_net_sd), (_port_list_sd));

#include "shadow/shadow_idb_pin.h"
TABLE4CLASS(edadb::Shadow<idb::IdbPin>, "iPinSD", (_pin_name_sd, _order_sd, _net_name_sd, _io_term_sd, _no_port_location_sd, _no_port_orient_sd, _no_port_placement_status_sd));

#include "shadow/shadow_idb_blockage.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbBlockage>, "iBlockageSD", (primary_key, _instance_name_sd, _is_pushdown_sd, _type_sd, _layer_name_sd, _is_except_pgnet_sd), (_rect_list_sd));

#include "database/data/design/db_design/IdbRegion.h"
TABLE4CLASS_WVEC(idb::IdbRegion, "iRegion", (_name, _type), (_boudary_list));

#include "shadow/shadow_idb_slot.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSlot>, "iSlotSD", (primary_key, _order_sd, _layer_name_sd), (_rect_list_sd));

#include "shadow/shadow_idb_group.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbGroup>, "iGroupSD", (_group_name_sd, _region_name_sd), (_instance_name_vec_sd));
