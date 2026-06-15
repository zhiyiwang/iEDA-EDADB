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
class Shadow<idb::IdbFillLayer>  {
public:
    Shadow<idb::IdbFillLayer> (void): primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbFillLayer>(void) {
        for ( auto& rect_sd : _rect_list_sd ) {
            delete rect_sd; rect_sd = nullptr;
        }
        _rect_list_sd.clear();
    }
    Shadow<idb::IdbFillLayer>(const Shadow& other) = delete;
    Shadow<idb::IdbFillLayer>& operator=(const Shadow& other) = delete;
public:
    void toShadow(idb::IdbFillLayer* obj) {
        _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : "";
        _rect_list_sd.clear();
        for ( auto& rect : obj->get_rect_list() ) {
            idb::IdbRect* rect_sd = new idb::IdbRect(*rect);
            _rect_list_sd.emplace_back( rect_sd );
        }
    }
    void fromShadow(idb::IdbFillLayer* obj) {
        // use _layer_name_sd to lookup layer
        for ( auto& rect_sd : _rect_list_sd ) {
            idb::IdbRect* rect = new idb::IdbRect(*rect_sd);
            obj->get_rect_list().emplace_back( rect );
        }
    }
public:
    uint64_t primary_key;   
    std::string _layer_name_sd;
    std::vector<idb::IdbRect*> _rect_list_sd;
private:
    static inline uint64_t next_primary_key;
}; // Shadow IdbFillLayer


template <>
class Shadow<idb::IdbFillVia> {
public:
    Shadow<idb::IdbFillVia> (void): primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbFillVia>() {
        for ( auto& coordinate_sd : _coordinate_list_sd ) {
            delete coordinate_sd; coordinate_sd = nullptr;
        }
        _coordinate_list_sd.clear();
    }
    Shadow<idb::IdbFillVia>(const Shadow& other) = delete;
    Shadow<idb::IdbFillVia>& operator=(const Shadow& other) = delete;
public:
    void toShadow(idb::IdbFillVia* obj) {
        _via_name_sd = obj->get_via() ? obj->get_via()->get_name() : "";
        _coordinate_list_sd.clear();
        for ( auto& coordinate : obj->get_coordinate_list() ) {
            idb::IdbCoordinate<int32_t>* coordinate_sd = new idb::IdbCoordinate<int32_t>(*coordinate);
            _coordinate_list_sd.emplace_back( coordinate_sd );
        }
    }
    void fromShadow(idb::IdbFillVia* obj) {
        // use _via_name_sd to lookup via
        for ( auto& coordinate_sd : _coordinate_list_sd ) {
            idb::IdbCoordinate<int32_t>* coordinate = new idb::IdbCoordinate<int32_t>(*coordinate_sd);
            obj->get_coordinate_list().emplace_back( coordinate );
        }
    }
public:
    uint64_t primary_key;   
    std::string _via_name_sd;
    std::vector<idb::IdbCoordinate<int32_t>*> _coordinate_list_sd;
private:
    static inline uint64_t next_primary_key;
}; // Shadow IdbFillVia


template <>
class Shadow<idb::IdbFill> {
public:
    Shadow<idb::IdbFill> (void): primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbFill>() {
        delete _layer_sd;
        _layer_sd = nullptr;
        delete _via_sd;
        _via_sd = nullptr;
    }
    Shadow<idb::IdbFill>(const Shadow& other) = delete;
    Shadow<idb::IdbFill>& operator=(const Shadow& other) = delete;
public:
    void toShadow(idb::IdbFill* obj) {
        _type_sd = obj->get_type();
        if ( _type_sd == idb::IdbFill::IdbFillType::kLayer ) {
            _layer_sd = new Shadow<idb::IdbFillLayer>();
            _layer_sd->toShadow( obj->get_layer() );
        } else if ( _type_sd == idb::IdbFill::IdbFillType::kVia ) {
            _via_sd = new Shadow<idb::IdbFillVia>();
            _via_sd->toShadow( obj->get_via() );
        }
    } // toShadow
    void fromShadow(idb::IdbFill* obj) {
        obj->set_type( _type_sd );

    } // fromShadow
public:
    uint64_t primary_key;   
    idb::IdbFill::IdbFillType _type_sd;
    Shadow<idb::IdbFillLayer>* _layer_sd = nullptr;
    Shadow<idb::IdbFillVia>* _via_sd = nullptr;
private:
    static inline uint64_t next_primary_key;
}; // Shadow IdbFill


} // edadb
