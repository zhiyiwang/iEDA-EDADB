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

} // edadb



//////// macro for table and class mapping ////////////////////////////////////////
TABLE4CLASS(idb::IdbCoordinate<int32_t>, "iCoord", (_x, _y));
TABLE4CLASS(idb::IdbRect, "IdbRect", (_lx, _ly, _hx, _hy));

TABLE4CLASS(idb::IdbUnits, "iUnits", (_nanoseconds, _picofarads, _ohms, _milliwatts, _milliamps, _volts, _micron_dbu, _megahertz));
TABLE4CLASS(idb::IdbBusBitChars, "iBusBitChars", (_left_delimiter, _right_delimiter));
TABLE4CLASS(idb::IdbDesign, "iDesign", (_design_name, _version, _units, _bus_bit_chars));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbDie>, "iDieSD", (primary_key), (points_sd));

#include "../data/design/db_layout/IdbGCellGrid.h"
TABLE4CLASS(idb::IdbGCellGrid, "iGCellGrid", (_direction, _start, _num, _space));



#include "../basic/geometry/IdbLayerShape.h"
//TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbLayerShape>, "iLayerShapeSD", (primary_key, _type_sd, _layer_name_sd), (_rect_list_sd));

#include "../data/design/db_layout/IdbViaMaster.h"
//TABLE4CLASS(edadb::Shadow<idb::IdbViaMasterFixed>, "iViaMasterFixedSD", (primary_key, _layer_shape_sd));

TABLE4EXTERNALCLASS(idb::IdbViaMasterGenerate, "iViaMasterGenerateSD", (_rule_name_sd,  _cut_size_x_sd, _cut_size_y_sd, _cut_spacing_x_sd, _cut_spacing_y_sd, _enclosure_bottom_x_sd, _enclosure_bottom_y_sd, _enclosure_top_x_sd, _enclosure_top_y_sd, _num_cut_rows_sd, _num_cut_cols_sd, _original_offset_x_sd, _original_offset_y_sd, _offset_bottom_x_sd, _offset_bottom_y_sd, _offset_top_x_sd, _offset_top_y_sd, _layer_bottom_name_sd, _layer_cut_name_sd, _layer_top_name_sd, _pattern_name_sd));

//TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbViaMaster>, "iViaMasterSD", (_name_sd, _type_sd, _master_generate_sd), (_master_fixed_list_sd));
TABLE4CLASS(edadb::Shadow<idb::IdbViaMaster>, "iViaMasterSD", (_name_sd, _type_sd, _master_generate_sd));

TABLE4CLASS(edadb::Shadow<idb::IdbVia>, "iViaSD", (_name_sd, _master_instance_sd));




//TABLE4CLASS(idb::IdbPort, "IdbPort", (_class, _coordinate, _io_average_coordinate, _io_bounding_box, _orient, _placement_status));
//
//TABLE4CLASS_WVEC(idb::IdbTerm, "IdbTerm", (_name, _direction, _type, _shape, _placement_status, _has_port, _is_special_net, _is_instance), (_port_list));

// no primary key
//TABLE4CLASS(idb::IdbPin, "IdbPin", (_pin_name, _net_name, _io_term, 

//TABLE4CLASS(idb::IdbNet, "IdbNet", (_net_name, _connect_type, _io_pin_list, _instance_pin_list, _wire_list));


