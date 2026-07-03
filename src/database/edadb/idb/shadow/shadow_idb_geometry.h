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

template<>
class Shadow<idb::IdbRect> {
public:
    Shadow(): _vec_idx(0) {}

public:
    bool toShadow(idb::IdbRect* obj, const uint32_t* idx_ptr = nullptr) {
        if (idx_ptr != nullptr) {
            _vec_idx = *idx_ptr;
        }

        _lx_sd = obj->get_low_x();
        _ly_sd = obj->get_low_y();
        _hx_sd = obj->get_high_x();
        _hy_sd = obj->get_high_y();
        return true;
    } // toShadow

    bool fromShadow(idb::IdbRect* obj, uint32_t* idx_ptr = nullptr) {
        if (idx_ptr != nullptr) {
            *idx_ptr = _vec_idx;
        }

        obj->set_low_x(_lx_sd);
        obj->set_low_y(_ly_sd);
        obj->set_high_x(_hx_sd);
        obj->set_high_y(_hy_sd);
        return true;
    } // fromShadow

public:
    uint64_t _vec_idx = 0;
    int32_t _lx_sd = 0;
    int32_t _ly_sd = 0;
    int32_t _hx_sd = 0;
    int32_t _hy_sd = 0;
};  // idb::IdbRect

} // namespace edadb
