/**
 * @file shadow_idb_coordinate.h
 * @brief This file contains shadow class definition for IdbCoordinate
 * @author Zhiyi Wang 
 */

#pragma once

#include <stdint.h>
#include "edadb.h"
#include "database/basic/geometry/IdbGeometry.h"


namespace edadb {
template<>
class Shadow<idb::IdbCoordinate<int32_t>> {
public:
    Shadow(): _vidx(0) {}
public:
    void toShadow(uint64_t idx, idb::IdbCoordinate<int32_t>* obj) {
        _vidx = idx;
        _x_sd = obj->get_x();
        _y_sd = obj->get_y();
    }
    void fromShadow(uint64_t& idx, idb::IdbCoordinate<int32_t>* obj) {
        idx = _vidx;
        obj->set_x(_x_sd);
        obj->set_y(_y_sd);
    }
public:
    uint64_t _vidx = 0; // vector index
    int32_t _x_sd = 0;
    int32_t _y_sd = 0;
};  // idb::IdbCoordinate

} // namespace edadb
