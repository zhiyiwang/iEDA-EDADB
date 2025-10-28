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

void initPrimKeys(void);

} // edadb



//////// macro for table and class mapping ////////////////////////////////////////
TABLE4CLASS(idb::IdbUnits, "iUnits", (_nanoseconds, _picofarads, _ohms, _milliwatts, _milliamps, _volts, _micron_dbu, _megahertz));

TABLE4CLASS(idb::IdbBusBitChars, "iBusBitChars", (_left_delimiter, _right_delimiter));

TABLE4CLASS(idb::IdbDesign, "iDesign", (_design_name, _version, _units, _bus_bit_chars));


TABLE4CLASS(idb::IdbCoordinate<int32_t>, "iCoord", (_x, _y));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbDie>, "iDieSD", (primary_key), (points_sd));


#include "../data/design/db_layout/IdbGCellGrid.h"
TABLE4CLASS(idb::IdbGCellGrid, "iGCellGrid", (_direction, _start, _num, _space));
    




//// no primary key 
//TABLE4CLASS(idb::IdbRect, "IdbRect", (_lx, _ly, _hx, _hy));
//
//// no primary key 
//TABLE4CLASS(idb::IdbCoordinate<int32_t>, "IdbCoordinate", (_x, _y));
//
//TABLE4CLASS(idb::IdbPort, "IdbPort", (_class, _coordinate, _io_average_coordinate, _io_bounding_box, _orient, _placement_status));
//
//TABLE4CLASS_WVEC(idb::IdbTerm, "IdbTerm", (_name, _direction, _type, _shape, _placement_status, _has_port, _is_special_net, _is_instance), (_port_list));
//
//
//TABLE4CLASS(idb::IdbLayer, "IdbLayer", (_name, _type, _layer_id, _layer_order))

//// no primary key
//TABLE4CLASS_WVEC(idb::IdbLayerShape, "IdbLayerShape", (_type, _layer), (_rect_list))


// no primary key
//TABLE4CLASS(idb::IdbPin, "IdbPin", (_pin_name, _net_name, _io_term, 

//TABLE4CLASS(idb::IdbNet, "IdbNet", (_net_name, _connect_type, _io_pin_list, _instance_pin_list, _wire_list));


