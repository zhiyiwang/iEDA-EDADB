/**
 * @file shadow_idb_special_net.h
 * @brief This file contains shadow class definitions for IdbSpecialNet
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_design/IdbSpecialNet.h"

namespace idb::edadb_adapter {

class SpecialNetPinRef {
public:
    uint64_t _order_sd = 0;
    std::string instance_name;
    std::string pin_name;
};

} // namespace idb::edadb_adapter

namespace edadb {

template <>
class Shadow<idb::IdbSpecialWireSegment> {
public:
    Shadow<idb::IdbSpecialWireSegment>(void) : primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbSpecialWireSegment>() {
        delete _delta_rect_sd;
        _delta_rect_sd = nullptr;
        for (auto& point : _point_list_sd) {
            delete point;
            point = nullptr;
        }
        _point_list_sd.clear();
    }

    Shadow<idb::IdbSpecialWireSegment>(const Shadow& other) = delete;
    Shadow<idb::IdbSpecialWireSegment>& operator=(const Shadow& other) = delete;

public:
    void toShadow(idb::IdbSpecialWireSegment* obj) {
        _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : "";
        _via_name_sd = obj->get_via() ? obj->get_via()->get_name() : "";
        _route_width_sd = obj->get_route_width();
        _style_sd = obj->get_style();
        _shape_type_sd = obj->get_shape_type();
        _is_via_sd = obj->is_via();
        _is_rect_sd = obj->is_rect();

        if (obj->get_delta_rect() != nullptr) {
            _delta_rect_sd = new idb::IdbRect(*obj->get_delta_rect());
        }

        assert(_point_list_sd.empty());
        for (auto point : obj->get_point_list()) {
            _point_list_sd.emplace_back(new idb::IdbCoordinate<int32_t>(*point));
        }
    }

public:
    uint64_t primary_key = 0;
    std::string _layer_name_sd;
    std::string _via_name_sd;
    int32_t _route_width_sd = -1;
    int32_t _style_sd = -1;
    idb::IdbWireShapeType _shape_type_sd = idb::IdbWireShapeType::kNone;
    bool _is_via_sd = false;
    bool _is_rect_sd = false;
    idb::IdbRect* _delta_rect_sd = nullptr;
    std::vector<idb::IdbCoordinate<int32_t>*> _point_list_sd;

private:
    static inline uint64_t next_primary_key = 1;
}; // Shadow<idb::IdbSpecialWireSegment>

template <>
class Shadow<idb::IdbSpecialWire> {
public:
    Shadow<idb::IdbSpecialWire>(void) : primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbSpecialWire>() {
        for (auto& segment : _segment_list_sd) {
            delete segment;
            segment = nullptr;
        }
        _segment_list_sd.clear();
    }

    Shadow<idb::IdbSpecialWire>(const Shadow& other) = delete;
    Shadow<idb::IdbSpecialWire>& operator=(const Shadow& other) = delete;

public:
    void toShadow(idb::IdbSpecialWire* obj) {
        _wire_state_sd = obj->get_wire_state();
        _shield_name_sd = obj->get_shiled_name();

        assert(_segment_list_sd.empty());
        for (auto segment : obj->get_segment_list()) {
            auto* segment_sd = new Shadow<idb::IdbSpecialWireSegment>();
            segment_sd->toShadow(segment);
            _segment_list_sd.emplace_back(segment_sd);
        }
    }

public:
    uint64_t primary_key = 0;
    idb::IdbWiringStatement _wire_state_sd = idb::IdbWiringStatement::kNone;
    std::string _shield_name_sd;
    std::vector<Shadow<idb::IdbSpecialWireSegment>*> _segment_list_sd;

private:
    static inline uint64_t next_primary_key = 1;
}; // Shadow<idb::IdbSpecialWire>

template <>
class Shadow<idb::IdbSpecialNet> {
public:
    Shadow<idb::IdbSpecialNet>(void) = default;
    ~Shadow<idb::IdbSpecialNet>() {
        for (auto& wire : _wire_list_sd) {
            delete wire;
            wire = nullptr;
        }
        _wire_list_sd.clear();
    }

    Shadow<idb::IdbSpecialNet>(const Shadow& other) = delete;
    Shadow<idb::IdbSpecialNet>& operator=(const Shadow& other) = delete;

public:
    void toShadow(idb::IdbSpecialNet* obj) {
        _net_name_sd = obj->get_net_name();
        _original_net_name_sd = obj->get_original_net_name();
        _connect_type_sd = obj->get_connect_type();
        _source_type_sd = obj->get_source_type();
        _weight_sd = obj->get_weight();

        for (auto& pin_name : obj->get_pin_string_list()) {
            idb::edadb_adapter::CppStrings pin_name_sd;
            pin_name_sd.str = pin_name;
            _pin_string_list_sd.emplace_back(pin_name_sd);
        }

        for (auto pin : obj->get_io_pin_list()->get_pin_list()) {
            idb::edadb_adapter::CppStrings pin_name_sd;
            pin_name_sd.str = pin->get_pin_name();
            _io_pin_name_list_sd.emplace_back(pin_name_sd);
        }

        uint64_t pin_order = 0;
        for (auto pin : obj->get_instance_pin_list()->get_pin_list()) {
            idb::edadb_adapter::SpecialNetPinRef pin_ref_sd;
            pin_ref_sd._order_sd = pin_order++;
            pin_ref_sd.instance_name = pin->get_instance() ? pin->get_instance()->get_name() : "";
            pin_ref_sd.pin_name = pin->get_pin_name();
            _instance_pin_list_sd.emplace_back(pin_ref_sd);
        }

        assert(_wire_list_sd.empty());
        for (auto wire : obj->get_wire_list()->get_wire_list()) {
            auto* wire_sd = new Shadow<idb::IdbSpecialWire>();
            wire_sd->toShadow(wire);
            _wire_list_sd.emplace_back(wire_sd);
        }
    }

public:
    std::string _net_name_sd;
    std::string _original_net_name_sd;
    idb::IdbConnectType _connect_type_sd = idb::IdbConnectType::kNone;
    idb::IdbInstanceType _source_type_sd = idb::IdbInstanceType::kNone;
    int32_t _weight_sd = 0;
    std::vector<idb::edadb_adapter::CppStrings> _pin_string_list_sd;
    std::vector<idb::edadb_adapter::CppStrings> _io_pin_name_list_sd;
    std::vector<idb::edadb_adapter::SpecialNetPinRef> _instance_pin_list_sd;
    std::vector<Shadow<idb::IdbSpecialWire>*> _wire_list_sd;
}; // Shadow<idb::IdbSpecialNet>

} // namespace edadb
