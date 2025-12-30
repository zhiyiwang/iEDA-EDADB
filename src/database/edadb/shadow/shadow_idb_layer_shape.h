/**
 * @file shadow_idb_layer_shape.h
 * @brief This file contains shadow class definition for IdbLayerShape
 * @author Zhiyi Wang
 */

#pragma once

#include <stdint.h>
#include "edadb.h"
#include "database/basic/geometry/IdbLayerShape.h"
#include "shadow_idb_geometry.h"


namespace edadb {
template<>
class Shadow<idb::IdbLayerShape> {
public:
    void toShadow(idb::IdbLayerShape* obj) {
        _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : "";
        _type_sd = obj->get_type();
        // assign to write, no need to deep copy
        _rect_list_sd = obj->get_rect_list(); 
    }

    void fromShadow(idb::IdbLayerShape* obj) {
        obj->_type = _type_sd;

        auto& rect_list = obj->get_rect_list();
        assert(rect_list.empty());
        rect_list.swap(_rect_list_sd);
    }

public:
    std::string _layer_name_sd;
    idb::IdbLayerShapeType _type_sd;
    std::vector<idb::IdbRect*> _rect_list_sd;
}; // idb::IdbLayerShape
} // namespace edadb

