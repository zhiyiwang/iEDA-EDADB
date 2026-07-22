/**
 * @file shadow_idb_slot.h
 * @brief This file contains shadow class definition for IdbSlot
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_design/IdbSlot.h"

namespace edadb {

template <>
class Shadow<idb::IdbSlot> {
public:
    Shadow<idb::IdbSlot>(void) : primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbSlot>() = default;

    Shadow<idb::IdbSlot>(const Shadow& other) = delete;
    Shadow<idb::IdbSlot>& operator=(const Shadow& other) = delete;

public:
    bool toShadow(idb::IdbSlot* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || idx_ptr == nullptr) {
            return false;
        }

        _order_sd = *idx_ptr;
        _layer_name_sd = obj->get_layer_name();

        _rect_list_sd.clear();
        for (auto rect : obj->get_rect_list()) {
            if (rect == nullptr) {
                return false;
            }
            _rect_list_sd.push_back(*rect);
        }
        return true;
    } // toShadow

    bool fromShadow(idb::IdbSlot* obj, uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || !obj->get_rect_list().empty()) {
            return false;
        }
        if (idx_ptr != nullptr) {
            *idx_ptr = static_cast<uint32_t>(_order_sd);
        }

        obj->set_layer_name(_layer_name_sd);

        for (auto& rect_sd : _rect_list_sd) {
            idb::IdbRect* rect = obj->add_rect();
            *rect = rect_sd;
        }
        return true;
    } // fromShadow

public:
    uint64_t primary_key = 0;
    uint64_t _order_sd = 0;
    std::string _layer_name_sd;
    std::vector<idb::IdbRect> _rect_list_sd;

private:
    static inline uint64_t next_primary_key = 1;
}; // Shadow<idb::IdbSlot>

} // namespace edadb
