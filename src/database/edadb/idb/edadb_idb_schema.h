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

//EDADB_TODO: C currently keeps EDADB as an empty framework. Re-enable each
// schema group only when the corresponding readIdbXXX/writeIdbXXX implementation
// has been ported to the EDADB DbTableOp API.
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


#include "database/data/design/db_layout/IdbSite.h"
TABLE4CLASS(idb::IdbSite, "iSite", (_name, _width, _heigtht, _b_overlap, _site_class, _symmetry, _orient, _type));

#include "database/data/design/db_layout/IdbRow.h"
TABLE4CLASS(idb::IdbRow, "iRow", (_name, _site, _original_coordinate, _row_num_x, _row_num_y, _step_x, _step_y));


//EDADB_TODO: restore via schema groups after their
// readIdbXXX/writeIdbXXX implementations are ported to the EDADB DbTableOp API.
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
TABLE4CLASS(idb::IdbRect, "IdbRect", (_lx, _ly, _hx, _hy));

#include "shadow/shadow_idb_layer_shape.h"
TABLE4SHADOW_WVEC(idb::IdbLayerShape);
TABLE4CLASS_WVEC (edadb::Shadow<idb::IdbLayerShape>, "iLayerShapeSD", (_layer_name_sd, _type_sd), (_rect_list_sd));

//--EDADB_IGNORE: no need to define as a table: store the underlayer member directly
//--TABLE4CLASS(edadb::Shadow<idb::IdbViaMasterFixed>, "iViaMasterFixedSD", (primary_key, _layer_shape_sd));

#include "shadow/shadow_idb_via_master.h"
TABLE4SHADOW_WVEC(idb::IdbViaMaster);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbViaMaster>, "iViaMasterSD", (_name_sd, _type_sd, _master_generate_sd), (fixed_layer_shape_list_sd));

//TABLE4SHADOW(idb::IdbViaMaster);
//TABLE4CLASS(edadb::Shadow<idb::IdbViaMaster>, "iViaMasterSD", (_name_sd, _type_sd, _master_generate_sd));

#include "database/data/design/db_design/IdbVias.h"
TABLE4CLASS(idb::IdbVia, "iVia", (_name, _master_instance));


#include "database/data/design/db_design/IdbHalo.h"
TABLE4CLASS(idb::IdbHalo, "iHalo", (_extend_left, _extend_right, _extend_top, _extend_bottom, _is_soft));

#include "shadow/shadow_idb_halo.h"
TABLE4CLASS(edadb::Shadow<idb::IdbRouteHalo>, "iRouteHaloSD", (_route_distance_sd, _layer_bottom_name_sd, _layer_top_name_sd));

#include "shadow/shadow_idb_instance.h"
TABLE4CLASS(edadb::Shadow<idb::IdbInstance>, "iInstSD", (_name_sd, _type_sd, _status_sd, _orient_sd, _weight_sd, _cell_master_name_sd, _coordinate_sd, _halo_sd, _route_halo_sd, _region_name_sd));

#include "shadow/shadow_idb_port.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbPort>, "iPortSD", (primary_key, _class_sd, _orient_sd, _placement_status_sd, _coordinate_sd), (_layer_shape_list_sd));

#include "shadow/shadow_idb_term.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbTerm>, "iTermSD", (_name_sd, _direction_sd, _type_sd, _shape_sd, _placement_status_sd, _has_port_sd, _is_special_net_sd, _is_instance_sd), (_port_list_sd));

#include "shadow/shadow_idb_pin.h"
TABLE4CLASS(edadb::Shadow<idb::IdbPin>, "iPinSD", (_pin_name_sd, _net_name_sd, _io_term_sd, _average_coordinate_sd, _location_sd, _orient_sd, _is_io_pin_sd, _is_special_net_sd, _layer_num_sd));

#include "shadow/shadow_idb_blockage.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbBlockage>, "iBlockageSD", (primary_key, _instance_name_sd, _is_pushdown_sd, _type_sd, _layer_name_sd, _is_except_pgnet_sd), (_rect_list_sd));

#include "database/data/design/db_design/IdbRegion.h"
TABLE4CLASS_WVEC(idb::IdbRegion, "iRegion", (_name, _type), (_boudary_list));

#include "shadow/shadow_idb_slot.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSlot>, "iSlotSD", (primary_key, _layer_name_sd), (_rect_list_sd));

#include "shadow/shadow_idb_group.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbGroup>, "iGroupSD", (_group_name_sd, _region_name_sd), (_instance_name_vec_sd));

#include "shadow/shadow_idb_fill.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbFillLayer>, "iFillLayerSD", (primary_key, _layer_name_sd), (_rect_list_sd));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbFillVia>, "iFillViaSD", (primary_key, _via_name_sd), (_coordinate_list_sd));

TABLE4CLASS(edadb::Shadow<idb::IdbFill>, "iFillSD", (primary_key, _type_sd, _layer_sd, _via_sd));

#include "shadow/shadow_idb_special_net.h"
TABLE4CLASS(idb::edadb_adapter::SpecialNetPinRef, "iSpecPinRef", (_order_sd, instance_name, pin_name));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWireSegment>, "iSpecWireSegSD",
                 (primary_key, _layer_name_sd, _via_name_sd, _route_width_sd, _style_sd, _shape_type_sd, _is_via_sd, _is_rect_sd, _delta_rect_sd),
                 (_point_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWire>, "iSpecWireSD",
                 (primary_key, _wire_state_sd, _shield_name_sd), (_segment_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialNet>, "iSpecNetSD",
                 (_net_name_sd, _original_net_name_sd, _connect_type_sd, _source_type_sd, _weight_sd),
                 (_pin_string_list_sd, _io_pin_name_list_sd, _instance_pin_list_sd, _wire_list_sd));

#include "shadow/shadow_idb_net.h"
TABLE4CLASS(idb::edadb_adapter::NetPinRef, "iNetPinRef", (_order_sd, instance_name, pin_name));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbRegularWireSegment>, "iRegWireSegSD",
                 (primary_key, _layer_name_sd, _via_name_sd, _is_via_sd, _is_rect_sd, _is_second_point_virtual_sd, _delta_rect_sd),
                 (_point_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbRegularWire>, "iRegWireSD",
                 (primary_key, _wire_state_sd, _shield_name_sd), (_segment_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbNet>, "iNetSD",
                 (_net_name_sd, _original_net_name_sd, _connect_type_sd, _source_type_sd, _weight_sd, _xtalk_sd, _fix_bump_sd, _frequency_sd),
                 (_io_pin_name_list_sd, _instance_pin_list_sd, _wire_list_sd));
