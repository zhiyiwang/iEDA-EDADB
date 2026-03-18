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

#include "database/data/design/db_layout/IdbUnits.h"
TABLE4CLASS(idb::IdbUnits, "iUnits", (_nanoseconds, _picofarads, _ohms, _milliwatts, _milliamps, _volts, _micron_dbu, _megahertz));

#include "database/data/design/db_design/IdbBusBitChars.h"
TABLE4CLASS(idb::IdbBusBitChars, "iBusBitChars", (_left_delimiter, _right_delimiter));

#include "database/data/design/IdbDesign.h"
TABLE4CLASS(idb::IdbDesign, "iDesign", (_design_name, _version, _units, _bus_bit_chars));


#include "shadow/shadow_idb_geometry.h"
TABLE4SHADOW(idb::IdbCoordinate<int32_t>);
TABLE4CLASS (edadb::Shadow<idb::IdbCoordinate<int32_t>>, "iCoordSD", (_vec_idx, _x_sd, _y_sd));
//DIS--TABLE4CLASS(idb::IdbCoordinate<int32_t>, "iCoord", (_x, _y));
//DIS--// vector coordinate points 
//DIS--TABLE4CLASS(edadb::Shadow<idb::IdbCoordinate<int32_t>>, "iCoordSD", (_vidx, _x_sd, _y_sd));


#include "shadow/shadow_idb_die.h"
TABLE4SHADOW_WVEC(idb::IdbDie);
TABLE4CLASS_WVEC (edadb::Shadow<idb::IdbDie>, "iDieSD", (primary_key), (points_sd));


//--#include "database/basic/geometry/IdbGeometry.h"
//--TABLE4CLASS(idb::IdbRect, "IdbRect", (_lx, _ly, _hx, _hy));

#include "database/data/design/db_layout/IdbSite.h"
TABLE4CLASS(idb::IdbSite, "iSite", (_name, _width, _heigtht, _b_overlap, _site_class, _symmetry, _orient, _type));

#include "database/data/design/db_layout/IdbRow.h"
TABLE4CLASS(idb::IdbRow, "iRow", (_name, _site, _original_coordinate, _row_num_x, _row_num_y, _step_x, _step_y));


//--#include "database/data/design/db_design/IdbTrackGrid.h"
//--TABLE4CLASS(idb::IdbTrack, "iTrack", (_start, _direction, _pitch));
//--
//--#include "shadow/shadow_idb_track_grid.h"
//--TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbTrackGrid>, "iTrackGridSD", (primary_key, _track_num_sd, _track_sd), (_layer_name_vec_sd));
//--
//--
//--#include "database/data/design/db_layout/IdbGCellGrid.h"
//--TABLE4CLASS(idb::IdbGCellGrid, "iGCellGrid", (_direction, _start, _num, _space));
//--
//--
//--#include "shadow/shadow_idb_via_master.h"
//--TABLE4EXTERNALCLASS(idb::IdbViaMasterGenerate, "iViaMasterGenerateSD", (_rule_name_sd,  _cut_size_x_sd, _cut_size_y_sd, _cut_spacing_x_sd, _cut_spacing_y_sd, _enclosure_bottom_x_sd, _enclosure_bottom_y_sd, _enclosure_top_x_sd, _enclosure_top_y_sd, _num_cut_rows_sd, _num_cut_cols_sd, _original_offset_x_sd, _original_offset_y_sd, _offset_bottom_x_sd, _offset_bottom_y_sd, _offset_top_x_sd, _offset_top_y_sd, _layer_bottom_name_sd, _layer_cut_name_sd, _layer_top_name_sd, _pattern_name_sd));
//--
//--#include "shadow/shadow_idb_layer_shape.h"
//--TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbLayerShape>, "iLayerShapeSD", (_layer_name_sd, _type_sd), (_rect_list_sd));
//--
//--//--EDADB_IGNORE: no need to define as a table: store the underlayer member directly
//--//--TABLE4CLASS(edadb::Shadow<idb::IdbViaMasterFixed>, "iViaMasterFixedSD", (primary_key, _layer_shape_sd));
//--
//--#include "shadow/shadow_idb_via_master.h"
//--TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbViaMaster>, "iViaMasterSD", (_name_sd, _type_sd, _master_generate_sd), (fixed_layer_shape_list_sd));
//--
//--#include "shadow/shadow_idb_via.h"
//--TABLE4CLASS(edadb::Shadow<idb::IdbVia>, "iViaSD", (_name_sd, _master_instance_sd));
//--
//--
//--#include "database/data/design/db_design/IdbHalo.h"
//--TABLE4CLASS(idb::IdbHalo, "iHalo", (_extend_left, _extend_right, _extend_top, _extend_bottom, _is_soft));
//--
//--#include "shadow/shadow_idb_halo.h"
//--TABLE4CLASS(edadb::Shadow<idb::IdbRouteHalo>, "iRouteHaloSD", (_route_distance_sd, _layer_bottom_name_sd, _layer_top_name_sd));
//--
//--#include "shadow/shadow_idb_instance.h"
//--TABLE4CLASS(edadb::Shadow<idb::IdbInstance>, "iInstSD", (_name_sd, _type_sd, _status_sd, _orient_sd, _weight_sd, _cell_master_name_sd, _coordinate_sd, _halo_sd, _route_halo_sd, _region_name_sd));
//--
//--
//--#include "shadow/shadow_idb_port.h"
//--TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbPort>, "iPortSD", (primary_key, _class_sd, _orient_sd, _placement_status_sd, _coordinate_sd), (_layer_shape_list_sd));
//--
//--#include "shadow/shadow_idb_term.h"
//--TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbTerm>, "iTermSD", (_name_sd, _direction_sd, _type_sd, _shape_sd, _placement_status_sd, _has_port_sd, _is_special_net_sd, _is_instance_sd), (_port_list_sd));
//--
//--#include "shadow/shadow_idb_pin.h"
//--TABLE4CLASS(edadb::Shadow<idb::IdbPin>, "iPinSD", (_pin_name_sd, _net_name_sd, _io_term_sd, _average_coordinate_sd, _location_sd, _orient_sd, _is_io_pin_sd, _is_special_net_sd, _layer_num_sd));
//--
//--
//--#include "shadow/shadow_idb_blockage.h"
//--TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbBlockage>, "iBlockageSD", (primary_key, _instance_name_sd, _is_pushdown_sd, _type_sd, _layer_name_sd, _min_spacing_sd, _effective_width_sd, _is_slots_sd, _is_fills_sd, _is_except_pgnet_sd, _is_soft_sd, _is_partial_sd, _max_density_sd), (_rect_list_sd));
//--
//--
//--#include "database/data/design/db_design/IdbRegion.h"
//--TABLE4CLASS_WVEC(idb::IdbRegion, "iRegion", (_name, _type), (_boudary_list));
//--
//--
//--#include "database/data/design/db_design/IdbSlot.h"
//--TABLE4CLASS_WVEC(idb::IdbSlot, "iSlot", (_layer_name), (_rect_list));
//--
//--
//--#include "shadow/shadow_idb_group.h"
//--TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbGroup>, "iGroupSD", (_group_name_sd, _region_name_sd), (_instance_name_vec_sd));
//--
//--
//--#include "shadow/shadow_idb_fill.h"
//--TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbFillLayer>, "iFillLayerSD", (primary_key, _layer_name_sd), (_rect_list_sd));
//--
//--TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbFillVia>, "iFillViaSD", (primary_key, _via_name_sd), (_coordinate_list_sd));
//--
//--TABLE4CLASS(edadb::Shadow<idb::IdbFill>, "iFillSD", (primary_key, _type_sd, _layer_sd, _via_sd));


//#if 0
//#include "../data/design/db_design/IdbSpecialNet.h"
//TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWireSegment>, "iSpecWireSegSD", (_layer_name_sd, _via_name_sd, _shape_type_sd, _route_width_sd, _is_via_sd, _is_rect_sd, _delta_rect_sd), (_point_list_sd));
//
//TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWire>, "iSpecWireSD", (_wire_name_sd), (_segment_list_sd));
//
//TABLE4CLASS(idb::IdbInstance, "iInstance", (_name));
//
//
//
////  IdbTerm* _io_term; -> string _name;
//TABLE4CLASS(idb::IdbPin, "iPin", (_pin_name, _instance));
//
//TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialNet>, "iSpecNetSD", (_net_name_sd, _connection_type_sd), (_pin_name_list_sd, _pin_list_sd, _instance_list_sd, _wire_list));
//
//#endif 