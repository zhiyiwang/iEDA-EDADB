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

//-- // debugging
//-- TABLE4CLASS(idb::IdbTerm, "IdbTerm", (_name, _direction, _type, _shape, _placement_status, _has_port, _is_special_net, _is_instance));
//-- 
//-- // TODO:
//-- //TABLE4CLASS_WVEC(idb::IdbTerm, "IdbTerm", (_name, _direction, _type, _shape, _placement_status, _has_port, _is_special_net, _is_instance), (_port_list));
//-- 

TABLE4CLASS(idb::IdbDesign, "IdbDesign", (_version, _design_name, _units));




//////// global object to test edadb read/write ////////////////////////////////////////
namespace test_edadb {


//////// global object to test edadb read/write ////////////////////////////////////////

inline void initPort(idb::IdbPort* p)
{
    // define global object and set values for test
    p->_class = idb::IdbPortClass::kCore;
    p->_coordinate->set_xy(100, 200);
    p->_io_average_coordinate->set_xy(150, 250);
    p->_io_bounding_box->set_rect(50, 150, 250, 350);
    p->_orient = idb::IdbOrient::kN_R0;
    p->_placement_status = idb::IdbPlacementStatus::kFixed;
} // initPort

inline void initTerm(idb::IdbTerm *t)
{
    // define global object and set values for test
    t->_name = "TEST_TERM";
    t->_direction = idb::IdbConnectDirection::kInput;
    t->_type = idb::IdbConnectType::kSignal;
    t->_shape = idb::IdbTermShape::kAbutment;
    t->_placement_status = idb::IdbPlacementStatus::kFixed;
    t->_has_port = true;
    t->_is_special_net = false;
    t->_is_instance = false;
    idb::IdbPort* port = new idb::IdbPort();
    initPort(port);
    t->add_port(port);
} // initTerm



}  // namespace test_edadb