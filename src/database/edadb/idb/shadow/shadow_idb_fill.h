/**
 * @file shadow_idb_fill.h
 * @brief This file contains shadow class definition for IdbFill
 * @author Zhiyi Wang 
 */

#pragma once

#include "edadb.h"
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
    void toShadow(idb::IdbFill* obj) {
        _type_sd = obj->get_type();
        if ( _type_sd == idb::IdbFill::IdbFillType::kLayer && obj->get_layer() != nullptr ) {
            _layer_name_sd = obj->get_layer()->get_layer() ? obj->get_layer()->get_layer()->get_name() : "";
            for ( auto& rect : obj->get_layer()->get_rect_list() ) {
                idb::IdbRect* rect_sd = new idb::IdbRect(*rect);
                _rect_list_sd.emplace_back( rect_sd );
            }
        } else if ( _type_sd == idb::IdbFill::IdbFillType::kVia && obj->get_via() != nullptr ) {
            _via_name_sd = obj->get_via()->get_via() ? obj->get_via()->get_via()->get_name() : "";
            for ( auto& coordinate : obj->get_via()->get_coordinate_list() ) {
                idb::IdbCoordinate<int32_t>* coordinate_sd = new idb::IdbCoordinate<int32_t>(*coordinate);
                _coordinate_list_sd.emplace_back( coordinate_sd );
            }
        }
    } // toShadow
    void fromShadow(idb::IdbFill* obj) {
        obj->set_type( _type_sd );

    } // fromShadow
public:
    uint64_t primary_key;   
    idb::IdbFill::IdbFillType _type_sd;
    std::string _layer_name_sd;
    std::vector<idb::IdbRect*> _rect_list_sd;
    std::string _via_name_sd;
    std::vector<idb::IdbCoordinate<int32_t>*> _coordinate_list_sd;
private:
    static inline uint64_t next_primary_key;
}; // Shadow IdbFill


} // edadb
