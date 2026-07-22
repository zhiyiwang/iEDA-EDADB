/**
 * @file shadow_idb_layer_shape.h
 * @brief This file contains shadow class definition for IdbLayerShape
 * @author Zhiyi Wang
 */

#pragma once

#include <stdint.h>
#include "edadb.h"
#include "database/basic/geometry/IdbLayerShape.h"
#include "../edadb_idb_helper.h"
#include "shadow_idb_geometry.h"


namespace edadb {
template<>
class Shadow<idb::IdbLayerShape> {
public:
    Shadow<idb::IdbLayerShape>(void): primary_key(next_primary_key++) {}

    Shadow<idb::IdbLayerShape>(const Shadow& other): primary_key(next_primary_key++) {
        _vec_idx = other._vec_idx;
        _layer_name_sd = other._layer_name_sd;
        _type_sd = other._type_sd;
        // deep copy
        for (auto& rect : other._rect_list_sd) {
            _rect_list_sd.push_back(new idb::IdbRect(rect));
        }
    } // copy ctor

    Shadow<idb::IdbLayerShape>& operator=(const Shadow& other) {
        if (this != &other) {
            _vec_idx = other._vec_idx;
            _layer_name_sd = other._layer_name_sd;
            _type_sd = other._type_sd;
            // clean up existing rect list
            for (auto rect : _rect_list_sd) {
                delete rect; rect = nullptr;
            }
            _rect_list_sd.clear();
            // deep copy
            for (auto& rect : other._rect_list_sd) {
                _rect_list_sd.push_back(new idb::IdbRect(rect));
            }
        }
        return *this;
    } // copy assignment operator

    ~Shadow<idb::IdbLayerShape>(void) {
        // clean up rect list
        for (auto rect : _rect_list_sd) {
            delete rect; rect = nullptr;
        }
        _rect_list_sd.clear();
    }

public:
    bool toShadow(idb::IdbLayerShape* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || obj->get_layer() == nullptr) {
            return false;
        }
        if (idx_ptr != nullptr) {
            _vec_idx = *idx_ptr;
        }
        _layer_name_sd = obj->get_layer()->get_name();
        _type_sd = obj->get_type();

        // deep copy
        assert(_rect_list_sd.empty());
        for (auto& rect : obj->get_rect_list()) {
            if (rect == nullptr) {
                return false;
            }
            // use ctor: IdbRect(IdbRect* rect) 
            _rect_list_sd.push_back(new idb::IdbRect(rect));
        }

        return true;
    } // toShadow

    bool fromShadow(idb::IdbLayerShape* obj, uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }
        if (idx_ptr != nullptr) {
            *idx_ptr = _vec_idx;
        }
        idb::IdbLayer* layer = idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName(_layer_name_sd);
        if (layer == nullptr) {
            std::cerr << "edadb::Shadow<idb::IdbLayerShape>::fromShadow error: cannot find layer for layer shape: " << _layer_name_sd << std::endl;
            return false;
        }
        obj->set_layer(layer); 

        obj->_type = _type_sd;

        // Maybe multi used by EDADB API:
        // deep copy
        auto& rect_list = obj->get_rect_list();
        assert(rect_list.empty());
        for (auto& rect_sd : _rect_list_sd) {
            rect_list.push_back(new idb::IdbRect(rect_sd));
        }
        return true;
    } // fromShadow

public:
    uint64_t primary_key = 0;
    uint32_t _vec_idx = 0;
    std::string _layer_name_sd;
    idb::IdbLayerShapeType _type_sd;
    std::vector<idb::IdbRect*> _rect_list_sd;

private:
    static inline uint64_t next_primary_key = 1;
}; // idb::IdbLayerShape

} // namespace edadb
