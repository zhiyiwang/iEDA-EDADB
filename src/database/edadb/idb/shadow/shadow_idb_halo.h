/**
 * @file shadow_idb_halo.h
 * @brief This file contains shadow class definition for IdbHalo
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_design/IdbHalo.h"

namespace edadb {
template<>
class Shadow<idb::IdbRouteHalo> {
public:
    void toShadow(idb::IdbRouteHalo* obj) {
        _route_distance_sd = obj->get_route_distance();

        _layer_bottom_name_sd = obj->get_layer_bottom() ?
                obj->get_layer_bottom()->get_name() : "";
        _layer_top_name_sd = obj->get_layer_top() ?
                obj->get_layer_top()->get_name() : "";
    }
    void fromShadow(idb::IdbRouteHalo* obj) {
        obj->set_route_distance( _route_distance_sd );
        // use layer name to lookup layer during def read
    }

public:
    int32_t _route_distance_sd = 0;
    std::string _layer_bottom_name_sd = "";
    std::string _layer_top_name_sd = "";
}; // shadow IdbRouteHalo
} // namespace edadb

