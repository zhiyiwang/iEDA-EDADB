/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#pragma once

#include "../data/design/IdbDesign.h"
#include "../data/design/db_layout/IdbUnits.h"
#include "../data/design/IdbEnum.h"
#include "../data/design/db_layout/IdbTerm.h"

#include "../../third_party/edadb/include/edadb.h"
#include "shadow.h"


//////// global init function ////////////////////////////////////////
namespace edadb {

void initPrimKeys(void);

} // edadb



//////// macro for table and class mapping ////////////////////////////////////////

TABLE4EXTERNALCLASS(idb::IdbUnits, "iUnitsSD", (_nanoseconds_sd, _picofarads_sd, _ohms_sd, _milliwatts_sd, _milliamps_sd, _volts_sd, _micron_dbu_sd, _megahertz_sd));

TABLE4EXTERNALCLASS(idb::IdbBusBitChars, "iBusBitCharsSD", (_left_delim_sd, _right_delim_sd));

TABLE4EXTERNALCLASS(idb::IdbDesign, "iDesignSD", (_design_name_sd, _version_sd, _units_sd, _bus_bit_chars_sd));


TABLE4EXTERNALCLASS(idb::IdbCoordinate<int32_t>, "iCoordSD", (_x_sd, _y_sd));

//TABLE4CLASS(edadb::IdbDieShadow, "iDieSD", (_x_sd, _y_sd));

    




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


