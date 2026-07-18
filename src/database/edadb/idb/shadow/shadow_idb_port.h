/**
 * @file shadow_idb_port.h
 * @brief This file contains shadow class definition for IdbPort
 * @author Zhiyi Wang
 */

#pragma once

#include <algorithm>

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
    Shadow<idb::IdbPort>(const Shadow& other): primary_key(next_primary_key++) {
        _vec_idx = other._vec_idx;
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
            _vec_idx = other._vec_idx;
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
    void setWriterUsesPortBranch(bool value) { _writer_uses_port_branch = value; }

    bool toShadow(idb::IdbPort* obj, const uint32_t* idx_ptr = nullptr) {
        if (idx_ptr != nullptr) {
            _vec_idx = *idx_ptr;
        }
        if (_writer_uses_port_branch && obj->is_placed()) {
            _orient_sd = obj->get_orient();
            _placement_status_sd = obj->get_placement_status();
            _coordinate_sd = *(obj->get_coordinate());
        }

        // layer shape list
        assert(_layer_shape_list_sd.empty());
        const auto& layer_shape_list = obj->get_layer_shape();
        for (uint32_t layer_shape_idx = 0; layer_shape_idx < layer_shape_list.size(); ++layer_shape_idx) {
            idb::IdbLayerShape* layer_shape = layer_shape_list[layer_shape_idx];
            if (layer_shape == nullptr) {
                return false;
            }
            edadb::Shadow<idb::IdbLayerShape>* layer_shape_sd =
                    new edadb::Shadow<idb::IdbLayerShape>();
            if (!layer_shape_sd->toShadow(layer_shape, &layer_shape_idx)) {
                delete layer_shape_sd;
                return false;
            }
            _layer_shape_list_sd.push_back(layer_shape_sd);
        }

        return true;
    } // toShadow

    bool fromShadow(idb::IdbPort* obj, uint32_t* idx_ptr = nullptr) {
        if (idx_ptr != nullptr) {
            *idx_ptr = _vec_idx;
        }

        // DefRead::parse_pin() restores LAYER/RECT before placement.
        assert(obj->get_layer_shape().empty());
        for (auto* layer_shape_sd : _layer_shape_list_sd) {
            if (layer_shape_sd == nullptr) {
                return false;
            }
        }
        std::stable_sort(_layer_shape_list_sd.begin(), _layer_shape_list_sd.end(),
                         [](const auto* lhs, const auto* rhs) {
                             return lhs->_vec_idx < rhs->_vec_idx;
                         });
        for (auto* layer_shape_sd : _layer_shape_list_sd) {
            idb::IdbLayerShape* layer_shape = obj->add_layer_shape();
            if (!layer_shape_sd->fromShadow(layer_shape)) {
                return false;
            }
            layer_shape->set_type_rect();
        }

        if (_placement_status_sd != idb::IdbPlacementStatus::kNone) {
            obj->set_orient(_orient_sd);
            obj->set_placement_status(_placement_status_sd);
            obj->set_coordinate(_coordinate_sd.get_x(), _coordinate_sd.get_y());
        }

        return true;
    } // fromShadow

public:
    uint64_t primary_key = 0;
    uint32_t _vec_idx = 0;

    // columns
    idb::IdbOrient _orient_sd = idb::IdbOrient::kN_R0;
    idb::IdbPlacementStatus _placement_status_sd = idb::IdbPlacementStatus::kNone;
    idb::IdbCoordinate<int32_t> _coordinate_sd; // alway created in IdbPort

    vector< edadb::Shadow<idb::IdbLayerShape>* > _layer_shape_list_sd;

private:
    bool _writer_uses_port_branch = false;
    static inline uint64_t next_primary_key = 1;
};

} // namespace edadb
