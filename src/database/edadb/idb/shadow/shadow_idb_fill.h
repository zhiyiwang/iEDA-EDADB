/**
 * @file shadow_idb_fill.h
 * @brief This file contains shadow class definition for IdbFill
 * @author Zhiyi Wang 
 */

#pragma once

#include "edadb.h"
#include "../edadb_idb_helper.h"
#include "database/data/design/db_design/IdbFill.h"

namespace edadb {

template <>
class Shadow<idb::IdbFill> {
public:
    Shadow<idb::IdbFill> (void): primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbFill>() {
        for ( auto& rect_sd : _rect_list_sd ) {
            delete rect_sd;
            rect_sd = nullptr;
        }
        _rect_list_sd.clear();

        for ( auto& coordinate_sd : _coordinate_list_sd ) {
            delete coordinate_sd;
            coordinate_sd = nullptr;
        }
        _coordinate_list_sd.clear();
    }
    Shadow<idb::IdbFill>(const Shadow& other) = delete;
    Shadow<idb::IdbFill>& operator=(const Shadow& other) = delete;
public:
    bool toShadow(idb::IdbFill* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }

        _type_sd = obj->get_type();
        if ( _type_sd == idb::IdbFill::IdbFillType::kLayer && obj->get_layer() != nullptr ) {
            if (obj->get_layer()->get_layer() == nullptr) {
                return false;
            }
            _layer_name_sd = obj->get_layer()->get_layer()->get_name();
            for ( auto& rect : obj->get_layer()->get_rect_list() ) {
                if (rect == nullptr) {
                    return false;
                }
                idb::IdbRect* rect_sd = new idb::IdbRect(*rect);
                _rect_list_sd.emplace_back( rect_sd );
            }
        } else if ( _type_sd == idb::IdbFill::IdbFillType::kVia && obj->get_via() != nullptr ) {
            if (obj->get_via()->get_via() == nullptr) {
                return false;
            }
            _via_name_sd = obj->get_via()->get_via()->get_name();
            for ( auto& coordinate : obj->get_via()->get_coordinate_list() ) {
                if (coordinate == nullptr) {
                    return false;
                }
                idb::IdbCoordinate<int32_t>* coordinate_sd = new idb::IdbCoordinate<int32_t>(*coordinate);
                _coordinate_list_sd.emplace_back( coordinate_sd );
            }
        } else {
            return false;
        }
        return true;
    } // toShadow

    bool fromShadow(idb::IdbFill* obj, uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }

        obj->set_type(_type_sd);
        if (_type_sd == idb::IdbFill::IdbFillType::kLayer) {
            idb::IdbLayer* layer = idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName(_layer_name_sd);
            if (layer == nullptr) {
                return false;
            }
            obj->get_layer()->set_layer(layer);
            for (idb::IdbRect* rect_sd : _rect_list_sd) {
                if (rect_sd == nullptr) {
                    return false;
                }
                obj->get_layer()->add_rect(rect_sd->get_low_x(), rect_sd->get_low_y(), rect_sd->get_high_x(), rect_sd->get_high_y());
            }
        } else if (_type_sd == idb::IdbFill::IdbFillType::kVia) {
            idb::IdbVia* via = idb::edadb_adapter::EdadbIdbHelper::findIdbViaByName(_via_name_sd);
            if (via == nullptr) {
                return false;
            }
            idb::IdbVia* via_new = via->clone();
            if (via_new == nullptr) {
                return false;
            }
            obj->get_via()->set_via(via_new);
            for (idb::IdbCoordinate<int32_t>* coordinate_sd : _coordinate_list_sd) {
                if (coordinate_sd == nullptr) {
                    return false;
                }
                obj->get_via()->add_coordinate(coordinate_sd->get_x(), coordinate_sd->get_y());
            }
        } else {
            return false;
        }
        return true;
    } // fromShadow
public:
    uint64_t primary_key = 0;
    idb::IdbFill::IdbFillType _type_sd = idb::IdbFill::IdbFillType::kNone;
    std::string _layer_name_sd;
    std::vector<idb::IdbRect*> _rect_list_sd;
    std::string _via_name_sd;
    std::vector<idb::IdbCoordinate<int32_t>*> _coordinate_list_sd;
private:
    static inline uint64_t next_primary_key = 1;
}; // Shadow IdbFill


} // edadb
