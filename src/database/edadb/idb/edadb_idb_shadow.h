/**
 * @file edadb_shadow.h
 * @brief This file contains shadow class definitions
 * @author Zhiyi Wang 
 */

#pragma once

#include "edadb.h"

namespace idb::edadb_adapter {

class CppStrings {
public:
    std::string str;
};

} // namespace idb::edadb_adapter

TABLE4CLASS(idb::edadb_adapter::CppStrings, "CppStr", (str));

#include "shadow/shadow_idb_geometry.h"
#include "shadow/shadow_idb_die.h"
#include "shadow/shadow_idb_track_grid.h"
#include "shadow/shadow_idb_layer_shape.h"
#include "shadow/shadow_idb_via_master.h"
#include "shadow/shadow_idb_halo.h"
#include "shadow/shadow_idb_instance.h"
#include "shadow/shadow_idb_port.h"
#include "shadow/shadow_idb_term.h"
#include "shadow/shadow_idb_pin.h"
#include "shadow/shadow_idb_blockage.h"
#include "shadow/shadow_idb_slot.h"
#include "shadow/shadow_idb_group.h"
#include "shadow/shadow_idb_fill.h"
#include "shadow/shadow_idb_special_net.h"


//--#include "shadow/shadow_idb_via.h"
// #include <stdint.h>
// #include <vector>
// 
// #include "../../third_party/edadb/include/edadb.h"
// 
// #include "../data/design/db_layout/IdbLayer.h"
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
//             idb::edadb_adapter::CppStrings* pin_name_sd = new idb::edadb_adapter::CppStrings();
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
//    std::vector< idb::edadb_adapter::CppStrings* > _pin_name_list_sd;
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
