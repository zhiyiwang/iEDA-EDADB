/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#pragma once


#include "../data/design/IdbEnum.h"
#include "../data/design/IdbDesign.h"
#include "../data/design/db_layout/IdbUnits.h"
#include "../data/design/db_layout/IdbTerm.h"

#include "../../third_party/edadb/include/edadb.h"
#include "shadow.h"


//////// global init function ////////////////////////////////////////
namespace edadb {

/**
 * @brief Initialize the edadb database to read data.
 * @return 0 success; <0 fail
 */
int init2read(const char* edadb_path);

/**
 * @brief Initialize the edadb database to write data.
 * @return 0 success; <0 fail
 */
int init2write(const char* edadb_path);


/**
 * @brief Initialize primary key settings for iEDA classes in edadb.
 */
void initPrimKeys(void);

/**
 * @brief Create a database table for the specified iEDA class type.
 * @return 0 success; <0 fail
 */
template <typename T>
int createTable(void);

/**
 * @brief Create a database table for the specified iEDA class type.
 * @return 0 success; <0 fail
 */
int createAllTables(void);

} // namespace edadb



//////// utility classes for table mapping ////////////////////////////////////////
namespace edadb {

class CppStrings {
public:
    std::string str;
};
} // namespace edadb

TABLE4CLASS(edadb::CppStrings, "CppStr", (str));



//////// macro for table and class mapping ////////////////////////////////////////

TABLE4CLASS(idb::IdbUnits, "iUnits", (_nanoseconds, _picofarads, _ohms, _milliwatts, _milliamps, _volts, _micron_dbu, _megahertz));
TABLE4CLASS(idb::IdbBusBitChars, "iBusBitChars", (_left_delimiter, _right_delimiter));
TABLE4CLASS(idb::IdbDesign, "iDesign", (_design_name, _version, _units, _bus_bit_chars));

TABLE4CLASS(idb::IdbCoordinate<int32_t>, "iCoord", (_x, _y));
TABLE4CLASS(idb::IdbRect, "IdbRect", (_lx, _ly, _hx, _hy));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbDie>, "iDieSD", (primary_key), (points_sd));

#include "../data/design/db_layout/IdbGCellGrid.h"
TABLE4CLASS(idb::IdbGCellGrid, "iGCellGrid", (_direction, _start, _num, _space));


#include "../data/design/db_layout/IdbSite.h"
//TABLE4CLASS(idb::IdbSite, "iSite", (_name, _width, _heigtht, _b_overlap, _site_class, _symmetry, _orient, _type));
TABLE4CLASS(idb::IdbSite, "iSite", (_name, _orient));

#include "../data/design/db_layout/IdbRow.h"
TABLE4CLASS(idb::IdbRow, "iRow", (_name, _site, _original_coordinate, _row_num_x, _row_num_y, _step_x, _step_y));


#include "../data/design/db_design/IdbRegion.h"
TABLE4CLASS_WVEC(idb::IdbRegion, "iRegion", (_name, _type), (_boudary_list));

#include "../data/design/db_design/IdbSlot.h"
TABLE4CLASS_WVEC(idb::IdbSlot, "iSlot", (_layer_name),(_rect_list));


#include "../basic/geometry/IdbLayerShape.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbLayerShape>, "iLayerShapeSD", (_layer_name_sd, _type_sd), (_rect_list_sd));

//--EDADB_IGNORE: no need to define as a table: store the underlayer member directly
//--TABLE4CLASS(edadb::Shadow<idb::IdbViaMasterFixed>, "iViaMasterFixedSD", (primary_key, _layer_shape_sd));

TABLE4EXTERNALCLASS(idb::IdbViaMasterGenerate, "iViaMasterGenerateSD", (_rule_name_sd,  _cut_size_x_sd, _cut_size_y_sd, _cut_spacing_x_sd, _cut_spacing_y_sd, _enclosure_bottom_x_sd, _enclosure_bottom_y_sd, _enclosure_top_x_sd, _enclosure_top_y_sd, _num_cut_rows_sd, _num_cut_cols_sd, _original_offset_x_sd, _original_offset_y_sd, _offset_bottom_x_sd, _offset_bottom_y_sd, _offset_top_x_sd, _offset_top_y_sd, _layer_bottom_name_sd, _layer_cut_name_sd, _layer_top_name_sd, _pattern_name_sd));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbViaMaster>, "iViaMasterSD", (_name_sd, _type_sd, _master_generate_sd), (fixed_layer_shape_list_sd));

TABLE4CLASS(edadb::Shadow<idb::IdbVia>, "iViaSD", (_name_sd, _master_instance_sd));


#if 0
#include "../data/design/db_design/IdbSpecialNet.h"
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWireSegment>, "iSpecWireSegSD", (_layer_name_sd, _via_name_sd, _shape_type_sd, _route_width_sd, _is_via_sd, _is_rect_sd, _delta_rect_sd), (_point_list_sd));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWire>, "iSpecWireSD", (_wire_name_sd), (_segment_list_sd));

TABLE4CLASS(idb::IdbInstance, "iInstance", (_name));



//  IdbTerm* _io_term; -> string _name;
TABLE4CLASS(idb::IdbPin, "iPin", (_pin_name, _instance));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialNet>, "iSpecNetSD", (_net_name_sd, _connection_type_sd), (_pin_name_list_sd, _pin_list_sd, _instance_list_sd, _wire_list));

#endif 