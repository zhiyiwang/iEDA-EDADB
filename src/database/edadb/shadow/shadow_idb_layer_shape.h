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
    Shadow<idb::IdbLayerShape>() = default;
    ~Shadow<idb::IdbLayerShape>() {
        // clean up rect list
        for (auto rect : _rect_list_sd) {
            delete rect; rect = nullptr;
        }
        _rect_list_sd.clear();
    }
public:
    void toShadow(idb::IdbLayerShape* obj) {
        _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : "";
        _type_sd = obj->get_type();

        // deep copy
        for (auto& rect : obj->get_rect_list()) {
            idb::IdbRect* rect_sd = new idb::IdbRect();
            rect_sd->set_low_x(rect->get_low_x());
            rect_sd->set_low_y(rect->get_low_y());
            rect_sd->set_high_x(rect->get_high_x());
            rect_sd->set_high_y(rect->get_high_y());
            _rect_list_sd.push_back(rect_sd);
        }
    }

    void fromShadow(idb::IdbLayerShape* obj) {
        obj->_type = _type_sd;

        auto& rect_list = obj->get_rect_list();
        assert(rect_list.empty());

        // deep copy
        for (auto& rect_sd : _rect_list_sd) {
            idb::IdbRect* rect = new idb::IdbRect();
            rect->set_low_x(rect_sd->get_low_x());
            rect->set_low_y(rect_sd->get_low_y());
            rect->set_high_x(rect_sd->get_high_x());
            rect->set_high_y(rect_sd->get_high_y());
            rect_list.push_back(rect);
        }
    }

    void print(void) {
        std::cout << "Shadow<idb::IdbLayerShape> : layer_name = " << _layer_name_sd << ", type = " << static_cast<uint32_t>(_type_sd)
                  << ", rect_list size = " << _rect_list_sd.size() << std::endl;
        for (auto& rect : _rect_list_sd) {
            std::cout << "  Rect: (" << rect->get_low_x() << ", " << rect->get_low_y() << ") - ("
                      << rect->get_high_x() << ", " << rect->get_high_y() << ")" << std::endl;
        }
    } // print

public:
    std::string _layer_name_sd;
    idb::IdbLayerShapeType _type_sd;
    std::vector<idb::IdbRect*> _rect_list_sd;
}; // idb::IdbLayerShape
} // namespace edadb

