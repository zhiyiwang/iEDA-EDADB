/**
 * @file shadow_idb_group.h
 * @brief This file contains shadow class definition for IdbGroup
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "../edadb_idb_helper.h"
#include "database/data/design/db_design/IdbRegion.h"
#include "database/data/design/db_design/IdbGroup.h"

namespace edadb {
template<>
class Shadow<idb::IdbGroup> {
public:
    Shadow<idb::IdbGroup>(void) = default;
    ~Shadow<idb::IdbGroup>(void) = default;

public:
    bool toShadow(idb::IdbGroup* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || obj->get_region() == nullptr || obj->get_instance_list() == nullptr) {
            return false;
        }

        _group_name_sd = obj->get_group_name();
        _region_name_sd = obj->get_region()->get_name();
        _instance_name_vec_sd.clear();
        for (auto& instance : obj->get_instance_list()->get_instance_list()) {
            if (instance == nullptr) {
                return false;
            }
            _instance_name_vec_sd.emplace_back(instance->get_name());
        }
        return true;
    }

    bool fromShadow(idb::IdbGroup* obj, uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }

        obj->set_group_name(_group_name_sd);
        idb::IdbRegion* region = idb::edadb_adapter::EdadbIdbHelper::findIdbRegionByName(_region_name_sd);
        if (region == nullptr) {
            return false;
        }
        obj->set_region(region);

        for (const std::string& instance_name : _instance_name_vec_sd) {
            idb::IdbInstance* instance = idb::edadb_adapter::EdadbIdbHelper::findIdbInstanceByName(instance_name);
            if (instance == nullptr) {
                return false;
            }
            if (!hasInstance(obj, instance)) {
                obj->add_instance(instance);
            }
        }
        return true;
    }

private:
    static bool hasInstance(idb::IdbGroup* group, idb::IdbInstance* instance) {
        for (idb::IdbInstance* existing : group->get_instance_list()->get_instance_list()) {
            if (existing == instance) {
                return true;
            }
        }
        return false;
    }

public:
    std::string _group_name_sd;
    std::string _region_name_sd;
    std::vector<std::string> _instance_name_vec_sd;
}; // Shadow IdbGroup

} // namespace edadb
