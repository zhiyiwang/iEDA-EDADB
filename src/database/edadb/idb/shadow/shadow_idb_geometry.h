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
    Shadow(): _vec_idx(0) {}

public:
    bool toShadow(idb::IdbCoordinate<int32_t>* obj, const uint32_t* idx_ptr = nullptr) {
        _x_sd = obj->get_x();
        _y_sd = obj->get_y();
        if (idx_ptr != nullptr) {
            _vec_idx = *idx_ptr;
        }
        return true;
    } // toShadow
    bool fromShadow(idb::IdbCoordinate<int32_t>* obj, uint32_t* idx_ptr = nullptr) {
        obj->set_x(_x_sd);
        obj->set_y(_y_sd);
        if (idx_ptr != nullptr) {
            *idx_ptr = _vec_idx;
        }
        return true;
    } // fromShadow

public:
    uint64_t _vec_idx = 0; // vector index
    int32_t _x_sd = 0;
    int32_t _y_sd = 0;
};  // idb::IdbCoordinate

} // namespace edadb
