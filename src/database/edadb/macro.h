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



//////// macro for table and class mapping ////////////////////////////////////////

TABLE4CLASS(idb::IdbUnits, "IdbUnits", (_nanoseconds, _picofarads, _ohms, _milliwatts, _milliamps, _volts, _micron_dbu, _megahertz));


// IdbPort
TABLE4CLASS(idb::IdbRect, "IdbRect", (_lx, _ly, _hx, _hy));

TABLE4CLASS(idb::IdbCoordinate<int32_t>, "IdbCoordinate", (_x, _y));

TABLE4CLASS(idb::IdbPort, "IdbPort", (_class, _coordinate, _io_average_coordinate, _io_bounding_box, _orient, _placement_status));


// not used: _pa_list, _average_position, _bouding_box, _cell_master, 
TABLE4CLASS(idb::IdbTerm, "IdbTerm", (_name, _direction, _type, _shape, _placement_status, _has_port, _is_special_net, _is_instance));
//TABLE4CLASS(idb::IdbTerm, "IdbTerm", (_name, _direction, _type, _shape, _placement_status, _port_list, _has_port, _is_special_net, _is_instance));


TABLE4CLASS(idb::IdbDesign, "IdbDesign", (_version, _design_name, _units));




//////// global object to test edadb read/write ////////////////////////////////////////
namespace test_edadb {


//////// global object to test edadb read/write ////////////////////////////////////////

inline idb::IdbPort gPort{};  

inline void initGlobalPort(void)
{
    // define global object and set values for test
    gPort._class = idb::IdbPortClass::kCore;
    gPort._coordinate->set_xy(100, 200);
    gPort._io_average_coordinate->set_xy(150, 250);
    gPort._io_bounding_box->set_rect(50, 150, 250, 350);
    gPort._orient = idb::IdbOrient::kN_R0;
    gPort._placement_status = idb::IdbPlacementStatus::kFixed;
} // initGlobalPort




}  // namespace test_edadb