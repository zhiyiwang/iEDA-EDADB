/**
 * @file shadow_idb_group.h
 * @brief This file contains shadow class definition for IdbGroup
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_design/IdbRegion.h"
#include "database/data/design/db_design/IdbGroup.h"

namespace edadb {
template<>
class Shadow<idb::IdbGroup> {
public:
    Shadow<idb::IdbGroup>(void) = default;
    ~Shadow<idb::IdbGroup>(void) = default;

public:
    void toShadow(idb::IdbGroup* obj, const uint32_t* idx_ptr = nullptr) {
        assert(idx_ptr != nullptr);

        _group_name_sd = obj->get_group_name();
        _order_sd = *idx_ptr;
        if (obj->get_region() != nullptr) {
            _region_name_sd = obj->get_region()->get_name();
        }
        _instance_name_vec_sd.clear();
        for (auto& instance : obj->get_instance_list()->get_instance_list()) {
            _instance_name_vec_sd.emplace_back(instance->get_name());
        }
    }

    void fromShadow(idb::IdbGroup* obj, uint32_t* idx_ptr = nullptr) {
        if (idx_ptr != nullptr) {
            *idx_ptr = static_cast<uint32_t>(_order_sd);
        }

        obj->set_group_name( _group_name_sd );
        // use region name to lookup region during def read
        // use instance names to lookup instances during def read
    }

public:
    std::string _group_name_sd;
    uint64_t _order_sd = 0;
    std::string _region_name_sd;
    std::vector<std::string> _instance_name_vec_sd;
}; // Shadow IdbGroup

} // namespace edadb
