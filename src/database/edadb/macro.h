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

//TABLE4CLASS(idb::IdbTerm, "IdbTerm", (_name, _direction, _type, _shape, _placement_status, _has_port, _is_special_net, _is_instance));

TABLE4CLASS_WVEC(idb::IdbTerm, "IdbTerm", (_name, _direction, _type, _shape, _placement_status, _has_port, _is_special_net, _is_instance), (_port_list));


TABLE4CLASS(idb::IdbDesign, "IdbDesign", (_version, _design_name, _units));




//////// global object to test edadb read/write //////////////////////////////////
namespace test_edadb {


//////// init iEDA object funcs /////////////////////////////////
template <typename T>
void init (T* t) = delete;

void init(idb::IdbUnits *u);
void init(idb::IdbPort  *p);
void init(idb::IdbTerm  *t);
void init(idb::IdbDesign *d);

//////// verify equal funcs /////////////////////////////////
template<typename T>
bool verifyEqual(T* org, T* got) = delete;

bool verifyEqual(idb::IdbUnits* org, idb::IdbUnits* got);
bool verifyEqual(idb::IdbPort* org, idb::IdbPort* got);
bool verifyEqual(idb::IdbTerm* org, idb::IdbTerm* got);
bool verifyEqual(idb::IdbDesign* org, idb::IdbDesign *got);

}  // namespace test_edadb