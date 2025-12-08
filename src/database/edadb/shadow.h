/**
 * @file shadow.h
 * @brief This file contains the definition of the IdbShadow class for representing shadow areas in the design.
 * @author Zhiyi Wang 
 */

#pragma once

#include <stdint.h>
#include <vector>

#include "../../third_party/edadb/include/edadb.h"


#include "../data/design/db_layout/IdbDie.h"
namespace edadb {
template<>
class Shadow<idb::IdbDie> {
public:
    Shadow(): primary_key(next_primary_key++) {}
public:
    void fromShadow(idb::IdbDie* obj) {
        auto& points = obj->get_points();
        assert(points.empty());
        points_sd.swap(points);
    } 
    void toShadow(idb::IdbDie* obj) {
        // assign to write, no need to deep copy
        points_sd = obj->get_points();
    }
public:
    uint64_t primary_key = 0;
    std::vector< idb::IdbCoordinate<int32_t>* > points_sd;
private:
    static inline uint64_t next_primary_key = 1;
};  // idb::IdbDie
} // namespace edadb



#include "../basic/geometry/IdbLayerShape.h"
namespace edadb {
template<>
class Shadow<idb::IdbLayerShape> {
public:
    void fromShadow(idb::IdbLayerShape* obj) {
        // layer name is the primary key to get layer instance,
        // use name to get layer instance during def read
        _layer_name_sd.assign(obj->get_layer()->get_name());
        obj->_type = _type_sd;
        auto& rect_list = obj->get_rect_list();
        assert(rect_list.empty());
        _rect_list_sd.swap(rect_list);
        
    } 
    void toShadow(idb::IdbLayerShape* obj) {
        _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : "";
        _type_sd = obj->get_type();
        // assign to write, no need to deep copy
        _rect_list_sd = obj->get_rect_list(); 
    }

public:
    std::string _layer_name_sd;
    idb::IdbLayerShapeType _type_sd;
    std::vector<idb::IdbRect*> _rect_list_sd;
}; // idb::IdbLayerShape
} // namespace edadb



#include "../data/design/db_layout/IdbViaMaster.h"
namespace edadb {


template<>
class Shadow<idb::IdbViaMasterGenerate> {
public:
    /**
     * @brief toShadow: convert ALL data from ieda instance to shadow instance
     * @param obj: the ieda instance
    */
    void toShadow(idb::IdbViaMasterGenerate* obj) {
        _rule_name_sd = obj->get_rule_name();
        _cut_size_x_sd = obj->get_cut_size_x();
        _cut_size_y_sd = obj->get_cut_size_y();
        _cut_spacing_x_sd = obj->get_cut_spcing_x ();
        _cut_spacing_y_sd = obj->get_cut_spcing_y();
        _enclosure_bottom_x_sd = obj->get_enclosure_bottom_x();
        _enclosure_bottom_y_sd = obj->get_enclosure_bottom_y();
        _enclosure_top_x_sd = obj->get_enclosure_top_x();
        _enclosure_top_y_sd = obj->get_enclosure_top_y();
        _num_cut_rows_sd = obj->get_cut_rows();
        _num_cut_cols_sd = obj->get_cut_cols();
        _original_offset_x_sd = obj->get_original_offset_x();
        _original_offset_y_sd = obj->get_original_offset_y();
        _offset_bottom_x_sd = obj->get_offset_bottom_x();
        _offset_bottom_y_sd = obj->get_offset_bottom_y();
        _offset_top_x_sd = obj->get_offset_top_x();
        _offset_top_y_sd = obj->get_offset_top_y();

        _layer_bottom_name_sd = obj->get_layer_bottom() ? obj->get_layer_bottom()->get_name() : "";
        _layer_cut_name_sd = obj->get_layer_cut() ? obj->get_layer_cut()->get_name() : "";
        _layer_top_name_sd = obj->get_layer_top() ? obj->get_layer_top()->get_name() : "";
        _pattern_name_sd = obj->get_patttern() ? obj->get_patttern()->get_pattern_string() : "";
    }

    /**
     * @brief fromShadow: convert basic type data from shadow instance to ieda instance
     * @param obj: the ieda instance
     */
    void fromShadow(idb::IdbViaMasterGenerate* obj){
        obj->set_rule_name(_rule_name_sd);
        obj->set_cut_size(_cut_size_x_sd, _cut_size_y_sd);
        obj->set_cut_spacing(_cut_spacing_x_sd, _cut_spacing_y_sd);
        obj->set_enclosure_bottom(_enclosure_bottom_x_sd, _enclosure_bottom_y_sd);
        obj->set_enclosure_top(_enclosure_top_x_sd, _enclosure_top_y_sd);
        obj->set_cut_row_col(_num_cut_rows_sd, _num_cut_cols_sd);
        obj->set_original(_original_offset_x_sd, _original_offset_y_sd);
        obj->set_offset_bottom(_offset_bottom_x_sd, _offset_bottom_y_sd);
        obj->set_offset_top(_offset_top_x_sd, _offset_top_y_sd);

        // NOTE: need to use name string to get layer and pattern instance during def read  
    }

public: // shadow data members
    std::string _rule_name_sd;
    int32_t _cut_size_x_sd = 0;
    int32_t _cut_size_y_sd = 0;
    int32_t _cut_spacing_x_sd = 0;
    int32_t _cut_spacing_y_sd = 0;
    int32_t _enclosure_bottom_x_sd = 0;
    int32_t _enclosure_bottom_y_sd = 0;
    int32_t _enclosure_top_x_sd = 0;
    int32_t _enclosure_top_y_sd = 0;
    int32_t _num_cut_rows_sd = 0;
    int32_t _num_cut_cols_sd = 0;
    int32_t _original_offset_x_sd = 0;
    int32_t _original_offset_y_sd = 0;
    int32_t _offset_bottom_x_sd = 0;
    int32_t _offset_bottom_y_sd = 0;
    int32_t _offset_top_x_sd = 0;
    int32_t _offset_top_y_sd = 0;

public: // name string to get layer and pattern instance in ieda
    std::string _layer_bottom_name_sd;
    std::string _layer_cut_name_sd;
    std::string _layer_top_name_sd;
    std::string _pattern_name_sd;
}; 


template<>
class Shadow<idb::IdbViaMaster> {
public:
    ~Shadow<idb::IdbViaMaster>() {
        for (auto& fixed : fixed_layer_shape_list_sd) {
            delete fixed;
        }
        fixed_layer_shape_list_sd.clear();
    }
public:
    void toShadow(idb::IdbViaMaster* obj){
        _name_sd = obj->get_name();
        _type_sd = obj->get_type();
        _master_generate_sd.toShadow( obj->get_master_generate() );

        for ( auto& fixed : obj->get_master_fixed_list() ) {
            Shadow<idb::IdbLayerShape> *sd = new Shadow<idb::IdbLayerShape>();
            sd->toShadow( fixed->get_layer_shape() );
            fixed_layer_shape_list_sd.emplace_back(sd);
        }
    }
    void fromShadow(idb::IdbViaMaster* obj){
        obj->set_name(_name_sd);
        obj->set_type(_type_sd);
        _master_generate_sd.fromShadow( obj->get_master_generate() );

        auto& fixed_list = obj->get_master_fixed_list();
        assert( fixed_list.empty() );
        for ( auto& fixed_sd : fixed_layer_shape_list_sd ) {
            fixed_list.emplace_back( new idb::IdbViaMasterFixed() );
            fixed_sd->fromShadow( fixed_list.back()->get_layer_shape() );   
        }
    }
public:
    std::string _name_sd;
    idb::IdbViaMaster::IdbViaMasterType _type_sd;
    Shadow<idb::IdbViaMasterGenerate> _master_generate_sd;

    // direct use the layer shape shadow to avoid higher level shadow include
    std::vector< Shadow<idb::IdbLayerShape >* > fixed_layer_shape_list_sd;
}; // IdbViaMaster

} // namespace edadb


#include "../data/design/db_design/IdbVias.h"
namespace edadb {
template<>
class Shadow<idb::IdbVia> {
public:
    void toShadow(idb::IdbVia* obj){
        _name_sd = obj->get_name();
        _master_instance_sd.toShadow( obj->get_instance() );
    }
    void fromShadow(idb::IdbVia* obj){
        obj->set_name(_name_sd);
        _master_instance_sd.fromShadow( obj->get_instance() );
    }
public:
    std::string _name_sd;
    Shadow<idb::IdbViaMaster> _master_instance_sd;
}; // idb::IdbVia



} // namespace edadb