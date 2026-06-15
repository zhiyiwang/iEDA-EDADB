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
    void toShadow(idb::IdbSlot* obj) {
        _layer_name_sd = obj->get_layer_name();

        assert(_rect_list_sd.empty());
        for (auto rect : obj->get_rect_list()) {
            _rect_list_sd.push_back(*rect);
        }
    } // toShadow

    void fromShadow(idb::IdbSlot* obj) {
        obj->set_layer_name(_layer_name_sd);

        assert(obj->get_rect_list().empty());
        for (auto& rect_sd : _rect_list_sd) {
            idb::IdbRect* rect = obj->add_rect();
            *rect = rect_sd;
        }
    } // fromShadow

public:
    uint64_t primary_key = 0;
    std::string _layer_name_sd;
    std::vector<idb::IdbRect> _rect_list_sd;

private:
    static inline uint64_t next_primary_key = 1;
}; // Shadow<idb::IdbSlot>

} // namespace edadb
