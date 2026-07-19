/**
 * @file shadow_idb_halo.h
 * @brief This file contains shadow class definition for IdbHalo
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_design/IdbHalo.h"
#include "../edadb_idb_helper.h"

namespace edadb {
template<>
class Shadow<idb::IdbRouteHalo> {
public:
    bool toShadow(idb::IdbRouteHalo* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }

        _route_distance_sd = obj->get_route_distance();

        _layer_bottom_name_sd = obj->get_layer_bottom() ?
                obj->get_layer_bottom()->get_name() : "";
        _layer_top_name_sd = obj->get_layer_top() ?
                obj->get_layer_top()->get_name() : "";
        return true;
    }

    bool fromShadow(idb::IdbRouteHalo* obj, uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }

        obj->set_route_distance( _route_distance_sd );
        obj->set_layer_bottom(idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName(_layer_bottom_name_sd));
        obj->set_layer_top(idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName(_layer_top_name_sd));
        return true;
    }

public:
    int32_t _route_distance_sd = 0;
    std::string _layer_bottom_name_sd = "";
    std::string _layer_top_name_sd = "";
}; // shadow IdbRouteHalo
} // namespace edadb
