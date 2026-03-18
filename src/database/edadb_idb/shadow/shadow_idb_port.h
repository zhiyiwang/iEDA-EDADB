/**
 * @file shadow_idb_port.h
 * @brief This file contains shadow class definition for IdbPort
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_layout/IdbTerm.h"

namespace edadb {
template<>
class Shadow<idb::IdbPort> {
public:
    ~Shadow<idb::IdbPort>() {
        for (auto& layer_shape_sd : _layer_shape_list_sd) {
            delete layer_shape_sd; layer_shape_sd = nullptr;
        }
        _layer_shape_list_sd.clear();
    }

    Shadow<idb::IdbPort>(): primary_key(next_primary_key++) {}
    Shadow<idb::IdbPort>(const Shadow& other) {
        _class_sd = other._class_sd;
        _orient_sd = other._orient_sd;
        _placement_status_sd = other._placement_status_sd;
        _coordinate_sd = other._coordinate_sd;
        // deep copy
        for (auto& layer_shape_sd : other._layer_shape_list_sd) {
            edadb::Shadow<idb::IdbLayerShape>* layer_shape_copy =
                    new edadb::Shadow<idb::IdbLayerShape>();
            *layer_shape_copy = *layer_shape_sd;
            _layer_shape_list_sd.push_back(layer_shape_copy);
        }
    }

    Shadow<idb::IdbPort>& operator=(const Shadow& other) {
        if (this != &other) {
            _class_sd = other._class_sd;
            _orient_sd = other._orient_sd;
            _placement_status_sd = other._placement_status_sd;
            _coordinate_sd = other._coordinate_sd;
            // clean up existing layer shape list
            for (auto layer_shape_sd : _layer_shape_list_sd) {
                delete layer_shape_sd; layer_shape_sd = nullptr;
            }
            _layer_shape_list_sd.clear();
            // deep copy
            for (auto& layer_shape_sd : other._layer_shape_list_sd) {
                edadb::Shadow<idb::IdbLayerShape>* layer_shape_copy =
                        new edadb::Shadow<idb::IdbLayerShape>();
                *layer_shape_copy = *layer_shape_sd;
                _layer_shape_list_sd.push_back(layer_shape_copy);
            }
        }
        return *this;
    }

public:
    void toShadow(idb::IdbPort* obj) {
        _class_sd = obj->get_port_class();
        _orient_sd = obj->get_orient();
        _placement_status_sd = obj->get_placement_status();

        // assign to write, no need to deep copy
        _coordinate_sd = *(obj->get_coordinate());

        // layer shape list
        assert(_layer_shape_list_sd.empty());
        for (auto& layer_shape : obj->get_layer_shape()) {
            edadb::Shadow<idb::IdbLayerShape>* layer_shape_sd =
                    new edadb::Shadow<idb::IdbLayerShape>();
            layer_shape_sd->toShadow(layer_shape);
            _layer_shape_list_sd.push_back(layer_shape_sd);
        }
    } // toShadow

    void fromShadow(idb::IdbPort* obj) {
        obj->set_port_class(_class_sd);
        obj->set_orient(_orient_sd);
        obj->set_placement_status(_placement_status_sd);

        // assign to write, no need to deep copy
        obj->set_coordinate(_coordinate_sd.get_x(), _coordinate_sd.get_y());

        // _layer_shape_list todo:
        // 1. need to handle during def read
        // 2. need to call obj->set_io_bounding_box() after each layer shape has rect
        assert(obj->get_layer_shape().empty());
    } // fromShadow

public:
    uint64_t primary_key = 0;

    // columns
    idb::IdbPortClass _class_sd;
    idb::IdbOrient _orient_sd;
    idb::IdbPlacementStatus _placement_status_sd;
    idb::IdbCoordinate<int32_t> _coordinate_sd; // alway created in IdbPort

    vector< edadb::Shadow<idb::IdbLayerShape>* > _layer_shape_list_sd;

private:
    static inline uint64_t next_primary_key = 1;
};

} // namespace edadb