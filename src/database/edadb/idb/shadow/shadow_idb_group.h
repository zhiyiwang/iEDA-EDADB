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
    void toShadow(idb::IdbGroup* obj) {
        assert(obj != nullptr);

        _group_name_sd = obj->get_group_name();
        if (obj->get_region() != nullptr) {
            _region_name_sd = obj->get_region()->get_name();
        }
        _instance_name_vec_sd.clear();
        for (auto& instance : obj->get_instance_list()->get_instance_list()) {
            _instance_name_vec_sd.emplace_back(instance->get_name());
        }
    }

    void fromShadow(idb::IdbGroup* obj) {
        assert(obj != nullptr);

        obj->set_group_name(_group_name_sd);
        // Region and instance references are rebuilt by DefReadEdadb::readIdbGroup().
    }

public:
    std::string _group_name_sd;
    std::string _region_name_sd;
    std::vector<std::string> _instance_name_vec_sd;
}; // Shadow IdbGroup

} // namespace edadb
