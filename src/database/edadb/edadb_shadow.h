/**
 * @file edadb_shadow.h
 * @brief This file contains shadow class definitions
 * @author Zhiyi Wang 
 */

#pragma once

#include "shadow/shadow_idb_geometry.h"
#include "shadow/shadow_idb_die.h"
#include "shadow/shadow_idb_track_grid.h"



// #include <stdint.h>
// #include <vector>
// 
// #include "../../third_party/edadb/include/edadb.h"
// 
// 
// 
// #include "../data/design/db_layout/IdbLayer.h"
// #include "../basic/geometry/IdbLayerShape.h"
// namespace edadb {
// template<>
// class Shadow<idb::IdbLayerShape> {
// public:
//     void toShadow(idb::IdbLayerShape* obj) {
//         _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : "";
//         _type_sd = obj->get_type();
//         // assign to write, no need to deep copy
//         _rect_list_sd = obj->get_rect_list(); 
//     }
// 
//     void fromShadow(idb::IdbLayerShape* obj) {
//         obj->_type = _type_sd;
// 
//         auto& rect_list = obj->get_rect_list();
//         assert(rect_list.empty());
//         rect_list.swap(_rect_list_sd);
//     }
// 
// public:
//     std::string _layer_name_sd;
//     idb::IdbLayerShapeType _type_sd;
//     std::vector<idb::IdbRect*> _rect_list_sd;
// }; // idb::IdbLayerShape
// } // namespace edadb
// 
// 
// 
// #include "../data/design/db_layout/IdbViaMaster.h"
// namespace edadb {
// template<>
// class Shadow<idb::IdbViaMasterGenerate> {
// public:
//     /**
//      * @brief toShadow: convert ALL data from ieda instance to shadow instance
//      * @param obj: the ieda instance
//     */
//     void toShadow(idb::IdbViaMasterGenerate* obj) {
//         _rule_name_sd = obj->get_rule_name();
//         _cut_size_x_sd = obj->get_cut_size_x();
//         _cut_size_y_sd = obj->get_cut_size_y();
//         _cut_spacing_x_sd = obj->get_cut_spcing_x ();
//         _cut_spacing_y_sd = obj->get_cut_spcing_y();
//         _enclosure_bottom_x_sd = obj->get_enclosure_bottom_x();
//         _enclosure_bottom_y_sd = obj->get_enclosure_bottom_y();
//         _enclosure_top_x_sd = obj->get_enclosure_top_x();
//         _enclosure_top_y_sd = obj->get_enclosure_top_y();
//         _num_cut_rows_sd = obj->get_cut_rows();
//         _num_cut_cols_sd = obj->get_cut_cols();
//         _original_offset_x_sd = obj->get_original_offset_x();
//         _original_offset_y_sd = obj->get_original_offset_y();
//         _offset_bottom_x_sd = obj->get_offset_bottom_x();
//         _offset_bottom_y_sd = obj->get_offset_bottom_y();
//         _offset_top_x_sd = obj->get_offset_top_x();
//         _offset_top_y_sd = obj->get_offset_top_y();
// 
//         _layer_bottom_name_sd = obj->get_layer_bottom() ? obj->get_layer_bottom()->get_name() : "";
//         _layer_cut_name_sd = obj->get_layer_cut() ? obj->get_layer_cut()->get_name() : "";
//         _layer_top_name_sd = obj->get_layer_top() ? obj->get_layer_top()->get_name() : "";
//         _pattern_name_sd = obj->get_patttern() ? obj->get_patttern()->get_pattern_string() : "";
//     }
// 
//     /**
//      * @brief fromShadow: convert basic type data from shadow instance to ieda instance
//      * @param obj: the ieda instance
//      */
//     void fromShadow(idb::IdbViaMasterGenerate* obj){
//         obj->set_rule_name(_rule_name_sd);
//         obj->set_cut_size(_cut_size_x_sd, _cut_size_y_sd);
//         obj->set_cut_spacing(_cut_spacing_x_sd, _cut_spacing_y_sd);
//         obj->set_enclosure_bottom(_enclosure_bottom_x_sd, _enclosure_bottom_y_sd);
//         obj->set_enclosure_top(_enclosure_top_x_sd, _enclosure_top_y_sd);
//         obj->set_cut_row_col(_num_cut_rows_sd, _num_cut_cols_sd);
//         obj->set_original(_original_offset_x_sd, _original_offset_y_sd);
//         obj->set_offset_bottom(_offset_bottom_x_sd, _offset_bottom_y_sd);
//         obj->set_offset_top(_offset_top_x_sd, _offset_top_y_sd);
// 
//         // NOTE: need to use name string to get layer and pattern instance during def read  
//     }
// 
// public: // shadow data members
//     std::string _rule_name_sd;
//     int32_t _cut_size_x_sd = 0;
//     int32_t _cut_size_y_sd = 0;
//     int32_t _cut_spacing_x_sd = 0;
//     int32_t _cut_spacing_y_sd = 0;
//     int32_t _enclosure_bottom_x_sd = 0;
//     int32_t _enclosure_bottom_y_sd = 0;
//     int32_t _enclosure_top_x_sd = 0;
//     int32_t _enclosure_top_y_sd = 0;
//     int32_t _num_cut_rows_sd = 0;
//     int32_t _num_cut_cols_sd = 0;
//     int32_t _original_offset_x_sd = 0;
//     int32_t _original_offset_y_sd = 0;
//     int32_t _offset_bottom_x_sd = 0;
//     int32_t _offset_bottom_y_sd = 0;
//     int32_t _offset_top_x_sd = 0;
//     int32_t _offset_top_y_sd = 0;
// 
// public: // name string to get layer and pattern instance in ieda
//     std::string _layer_bottom_name_sd;
//     std::string _layer_cut_name_sd;
//     std::string _layer_top_name_sd;
//     std::string _pattern_name_sd;
// }; 
// 
// 
// template<>
// class Shadow<idb::IdbViaMaster> {
// public:
//     ~Shadow<idb::IdbViaMaster>() {
//         for (auto& fixed : fixed_layer_shape_list_sd) {
//             delete fixed;
//         }
//         fixed_layer_shape_list_sd.clear();
//     }
// public:
//     void toShadow(idb::IdbViaMaster* obj){
//         _name_sd = obj->get_name();
//         _type_sd = obj->get_type();
//         _master_generate_sd.toShadow( obj->get_master_generate() );
// 
//         for ( auto& fixed : obj->get_master_fixed_list() ) {
//             Shadow<idb::IdbLayerShape> *sd = new Shadow<idb::IdbLayerShape>();
//             sd->toShadow( fixed->get_layer_shape() );
//             fixed_layer_shape_list_sd.emplace_back(sd);
//         }
//     }
//     void fromShadow(idb::IdbViaMaster* obj){
//         obj->set_name(_name_sd);
//         obj->set_type(_type_sd);
//         _master_generate_sd.fromShadow( obj->get_master_generate() );
// 
//         auto& fixed_list = obj->get_master_fixed_list();
//         assert( fixed_list.empty() );
//         for ( auto& fixed_sd : fixed_layer_shape_list_sd ) {
//             fixed_list.emplace_back( new idb::IdbViaMasterFixed() );
//             fixed_sd->fromShadow( fixed_list.back()->get_layer_shape() );   
//         }
//     }
// public:
//     std::string _name_sd;
//     idb::IdbViaMaster::IdbViaMasterType _type_sd;
//     Shadow<idb::IdbViaMasterGenerate> _master_generate_sd;
// 
//     // direct use the layer shape shadow to avoid higher level shadow include
//     std::vector< Shadow<idb::IdbLayerShape >* > fixed_layer_shape_list_sd;
// }; // IdbViaMaster
// 
// } // namespace edadb
// 
// 
// 
// #include "../data/design/db_design/IdbVias.h"
// namespace edadb {
// template<>
// class Shadow<idb::IdbVia> {
// public:
//     void toShadow(idb::IdbVia* obj){
//         _name_sd = obj->get_name();
//         _master_instance_sd.toShadow( obj->get_instance() );
//     }
//     void fromShadow(idb::IdbVia* obj){
//         obj->set_name(_name_sd);
//         _master_instance_sd.fromShadow( obj->get_instance() );
//     }
// public:
//     std::string _name_sd;
//     Shadow<idb::IdbViaMaster> _master_instance_sd;
// }; // idb::IdbVia
// } // namespace edadb
// 
// 

// 
// #if 0
// #include "../data/design/db_design/IdbHalo.h"
// namespace edadb {
// template<>
// class Shadow<idb::IdbRouteHalo> {
// public:
//     void toShadow(idb::IdbRouteHalo* obj) {
//         _route_distance_sd = obj->get_route_distance();
//         _layer_bottom_name_sd = obj->get_layer_bottom() ? obj->get_layer_bottom()->get_name() : "";
//         _layer_top_name_sd = obj->get_layer_top() ? obj->get_layer_top()->get_name() : "";
//     }
//     void fromShadow(idb::IdbRouteHalo* obj) {
//         obj->set_route_distance( _route_distance_sd );
//         // use layer name to lookup layer during def read
//     }
// 
// public:
//     int32_t _route_distance_sd = 0;
//     std::string _layer_bottom_name_sd;
//     std::string _layer_top_name_sd;
// }; // shadow IdbRouteHalo
// } // namespace edadb
// 
// 
// #include "../data/design/db_design/IdbInstance.h"
// namespace edadb {
// template<>
// class Shadow<idb::IdbInstance>  {
// public:
//     void toShadow(idb::IdbInstance* obj) {
//         _name_sd = obj->get_name();
//         _cell_master_name_sd = obj->get_cell_master() ? obj->get_cell_master()->get_name() : "";
// 
//         // assign to write, no need to deep copy
//         _type_sd = obj->get_type();
//         _status_sd = obj->get_status();
//         _orient_sd = obj->get_orient();
//         _weight_sd = obj->get_weight();
//         _coordinate_sd = obj->get_coordinate();
//         if ( obj->has_halo() ) {
//             _halo_sd = obj->get_halo();
//         }
//         if ( obj->has_route_halo() ) {
//             _route_halo_sd.toShadow( obj->get_route_halo() );
//         }
//         _region_name_sd = obj->get_region() ? obj->get_region()->get_name() : "";
//     }
// 
//     void fromShadow(idb::IdbInstance* obj) {
//         obj->set_name( _name_sd );
//         obj->set_type( _type_sd );
//         obj->set_status( _status_sd );
//         obj->set_orient( _orient_sd );
//         obj->set_weight( _weight_sd );
//         // use cell master name to lookup during def read
//         *(obj->get_coordinate()) = _coordinate_sd;
//         
//         obj->set_halo( _halo_sd );
//         _halo_sd = nullptr; // avoid double free
// 
//         if ( obj->has_route_halo() ) {
//             _route_halo_sd.fromShadow( obj->get_route_halo() );
//         }
//         // use region name to lookup during def read
//     }
// 
// public:
//     std::string _name_sd;
//     std::string _cell_master_name_sd;
//     idb::IdbInstanceType _type_sd;
//     idb::IdbPlacementStatus _status_sd;
//     idb::IdbOrient _orient_sd;
//     int32_t _weight_sd;
// 
//     idb::IdbCoordinate<int32_t>* _coordinate_sd;
//     idb::IdbHalo *_halo_sd = nullptr;
//     Shadow<idb::IdbRouteHalo>* _route_halo_sd = nullptr;
//     std::string _region_name_sd;
// }; // Shadow IdbInstance
// 
// } // namespace edadb
// #endif 
// 
// 
// 
// #if 0
// #include "../data/design/db_design/IdbSpecialNet.h"
// namespace edadb {
// template<>
// class Shadow<idb::IdbSpecialWireSegment> {
// public:
//     ~Shadow<idb::IdbSpecialWireSegment>() {
//         // avoid double free
//         _delta_rect_sd = nullptr;
//         for (auto& point : _point_list_sd) {
//             delete point;
//         }
//         _point_list_sd.clear();
//     }
// public:
//     void toShadow(idb::IdbSpecialWireSegment* obj) {
//         _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : "";
//         _via_name_sd = obj->get_via() ? obj->get_via()->get_name() : "";
//         _shape_type_sd = obj->get_shape_type();
//         _route_width_sd = obj->get_route_width();
//         _is_via_sd = obj->is_via();
//         _is_rect_sd = obj->is_rect();
//         // assign to write, no need to deep copy
//         _delta_rect_sd = obj->get_delta_rect();
//         _point_list_sd = obj->get_point_list();
//     }
// 
//     void fromShadow(idb::IdbSpecialWireSegment* obj) {
//         // use _layer_name_sd and _via_name_sd string to lookup
//         obj->set_shape_type(_shape_type_sd);
//         obj->set_route_width(_route_width_sd);
//         obj->set_is_via(_is_via_sd);
//         obj->set_is_rect(_is_rect_sd);
//         obj->set_delta_rect(_delta_rect_sd);
//         _delta_rect_sd = nullptr; // avoid double free
//         obj->get_point_list().swap(_point_list_sd);
//     }
// public:
//     string _layer_name_sd;
//     string _via_name_sd;
//     IdbWireShapeType _shape_type_sd;
//     int32_t _route_width_sd;
//     int32_t _style_sd;
//     bool _is_via_sd;
//     bool _is_rect_sd;
//     IdbRect* _delta_rect_sd;
// 
//     vector<IdbCoordinate<int32_t>*> _point_list_sd;
// }; // shadow IdbSpecialWireSegment
// 
// 
// template<>
// class Shadow<idb::IdbSpecialWire> {
// public:
//     ~Shadow<idb::IdbSpecialWire>() {
//         for (auto& segment_sd : segment_list_sd) {
//             delete segment_sd;
//         }
//         segment_list_sd.clear();
//     }
// public:
//     void toShadow(idb::IdbSpecialWire* obj) {
//         _wire_state = obj->get_wire_state();
//         segment_list_sd.clear();
//         for ( auto& segment : obj->get_segment_list() ) {
//             Shadow<idb::IdbSpecialWireSegment>* sd = new Shadow<idb::IdbSpecialWireSegment>();
//             sd->toShadow(segment);
//             segment_list_sd.emplace_back(sd);
//         }
//     }
//     void fromShadow(idb::IdbSpecialWire* obj) {
//         obj->set_wire_state(_wire_state);
//         auto& segment_list = obj->get_segment_list();
//         assert( segment_list.empty() );
//         for ( auto& segment_sd : segment_list_sd ) {
//             segment_list.emplace_back( new idb::IdbSpecialWireSegment() );
//             segment_sd->fromShadow( segment_list.back() );
//         }
//     }
// 
// public:
//     IdbWiringStatement _wire_state_sd;
//     std::string _shiled_name_sd;
//     std::vector< Shadow<idb::IdbSpecialWireSegment>* > segment_list_sd;
// 
// }; // Shadow IdbSpecialWire
// 
// 
// template<>
// class Shadow<IdbSpecialNet> {
// public:
//     ~Shadow<IdbSpecialNet>() {
//         for (auto& wire_sd : _wire_list) {
//             delete wire_sd;
//         }
//         _wire_list.clear();
//     }
// public:
//     void toShadow(IdbSpecialNet* obj) {
//         _net_name_sd = obj->get_net_name();
//         _connection_type_sd = obj->get_connection_type();
//         _pin_name_list_sd.clear();
//         _pin_list_sd.clear();
//         _instance_list_sd.clear();
//         for ( auto& pin : obj->get_pin_list() ) {
//             CppStrings* pin_name_sd = new CppStrings();
//             pin_name_sd->str = pin->get_name();
//             _pin_name_list_sd.emplace_back( pin_name_sd );
//             _pin_list_sd.emplace_back( pin );
//             if ( pin->get_instance() ) {
//                 _instance_list_sd.emplace_back( pin->get_instance() );
//             }
//         }
// 
//         _wire_list.clear();
//         for ( auto& wire : obj->get_wire_list() ) {
//             Shadow<idb::IdbSpecialWire>* wire_sd = new Shadow<idb::IdbSpecialWire>();
//             wire_sd->toShadow( wire );
//             _wire_list.emplace_back( wire_sd );
//         }
//     }
//     void fromShadow(IdbSpecialNet* obj) {
//         obj->set_net_name( _net_name_sd );
//         obj->set_connection_type( _connection_type_sd );
//         obj->get_pin_list().clear();
//         for ( size_t i = 0; i < _pin_name_list_sd.size(); ++i ) {
//             idb::IdbPin* pin = new idb::IdbPin();
//             pin->set_name( _pin_name_list_sd[i]->str);
//             if ( i < _instance_list_sd.size() ) {
//                 pin->set_instance( _instance_list_sd[i] );
//             }
//             obj->add_pin( pin );
//         }
// 
//         obj->get_wire_list().clear();
//         for ( auto& wire_sd : _wire_list ) {
//             idb::IdbSpecialWire* wire = new idb::IdbSpecialWire();
//             wire_sd->fromShadow( wire );
//             obj->add_wire( wire );
//         }
//     }
// 
// public:
//    std::string _net_name_sd;
//    std::string _org_net_name_sd;
//    IdbConnectionType _connection_type_sd;
//    IdbInstanceType _source_type_sd;
//    /**
//     * foreach pin_name, use it to lookup:
//     * foreach IdbInstance _instance_list, use pin's   
//     *   _io_term->get_name() to lookup pin instance
//     * then add pin to _instance_pin_list 
//     * and add instance to _instance_list
//     */
//    std::vector< edadb::CppStrings* > _pin_name_list_sd;
//    std::vector< idb::IdbPin* > _io_pin_list_sd;
// 
//    std::vector< idb::IdbPin* > _instance_list_sd; // pins from instances
//    // IdbInstance
//    //       std::string _name;
//    //       IdbPins* _pin_list;  // by idb_term_name
//    IdbInstanceList* _instance_list;
//    IdbSpecialWireList* _wire_list;
// 
//    std::vector< Shadow<idb::IdbSpecialWire>* > _wire_list;
// }; // Shadow IdbSpecialNet
// 
// 
// } // namespace edadb
// #endif 
// 
// 
