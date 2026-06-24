/**
 * @file shadow_idb_net.h
 * @brief This file contains shadow class definitions for IdbNet
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_design/IdbNet.h"

namespace idb::edadb_adapter {

class NetPinRef {
public:
    uint64_t _order_sd = 0;
    std::string instance_name;
    std::string pin_name;
};

} // namespace idb::edadb_adapter

namespace edadb {

template <>
class Shadow<idb::IdbRegularWireSegment> {
public:
    Shadow<idb::IdbRegularWireSegment>(void) : primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbRegularWireSegment>() {
        delete _delta_rect_sd;
        _delta_rect_sd = nullptr;
        for (auto& point : _point_list_sd) {
            delete point;
            point = nullptr;
        }
        _point_list_sd.clear();
    }

    Shadow<idb::IdbRegularWireSegment>(const Shadow& other) = delete;
    Shadow<idb::IdbRegularWireSegment>& operator=(const Shadow& other) = delete;

public:
    void toShadow(idb::IdbRegularWireSegment* obj) {
        _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : obj->get_layer_name();
        _via_name_sd = obj->get_via_list().empty() ? "" : obj->get_via_list().front()->get_name();
        _is_via_sd = obj->is_via();
        _is_rect_sd = obj->is_rect();

        if (obj->get_point_number() > 1) {
            _is_second_point_virtual_sd = obj->is_virtual(obj->get_point_second());
        }

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
    bool _is_via_sd = false;
    bool _is_rect_sd = false;
    bool _is_second_point_virtual_sd = false;
    idb::IdbRect* _delta_rect_sd = nullptr;
    std::vector<idb::IdbCoordinate<int32_t>*> _point_list_sd;

private:
    static inline uint64_t next_primary_key = 1;
}; // Shadow<idb::IdbRegularWireSegment>

template <>
class Shadow<idb::IdbRegularWire> {
public:
    Shadow<idb::IdbRegularWire>(void) : primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbRegularWire>() {
        for (auto& segment : _segment_list_sd) {
            delete segment;
            segment = nullptr;
        }
        _segment_list_sd.clear();
    }

    Shadow<idb::IdbRegularWire>(const Shadow& other) = delete;
    Shadow<idb::IdbRegularWire>& operator=(const Shadow& other) = delete;

public:
    void toShadow(idb::IdbRegularWire* obj) {
        _wire_state_sd = obj->get_wire_statement();
        _shield_name_sd = obj->get_shiled_name();

        assert(_segment_list_sd.empty());
        for (auto segment : obj->get_segment_list()) {
            auto* segment_sd = new Shadow<idb::IdbRegularWireSegment>();
            segment_sd->toShadow(segment);
            _segment_list_sd.emplace_back(segment_sd);
        }
    }

public:
    uint64_t primary_key = 0;
    idb::IdbWiringStatement _wire_state_sd = idb::IdbWiringStatement::kNone;
    std::string _shield_name_sd;
    std::vector<Shadow<idb::IdbRegularWireSegment>*> _segment_list_sd;

private:
    static inline uint64_t next_primary_key = 1;
}; // Shadow<idb::IdbRegularWire>

template <>
class Shadow<idb::IdbNet> {
public:
    Shadow<idb::IdbNet>(void) = default;
    ~Shadow<idb::IdbNet>() {
        for (auto& wire : _wire_list_sd) {
            delete wire;
            wire = nullptr;
        }
        _wire_list_sd.clear();
    }

    Shadow<idb::IdbNet>(const Shadow& other) = delete;
    Shadow<idb::IdbNet>& operator=(const Shadow& other) = delete;

public:
    void toShadow(idb::IdbNet* obj) {
        _net_name_sd = obj->get_net_name();
        _original_net_name_sd = obj->get_original_net_name();
        _connect_type_sd = obj->get_connect_type();
        _source_type_sd = obj->get_source_type();
        _weight_sd = obj->get_weight();
        _xtalk_sd = obj->get_xtalk();
        _fix_bump_sd = obj->is_fix_bump();
        _frequency_sd = obj->get_frequency();

        for (auto pin : obj->get_io_pins()->get_pin_list()) {
            _io_pin_name_list_sd.emplace_back(pin->get_pin_name());
        }

        uint64_t pin_order = 0;
        for (auto pin : obj->get_instance_pin_list()->get_pin_list()) {
            idb::edadb_adapter::NetPinRef pin_ref_sd;
            pin_ref_sd._order_sd = pin_order++;
            pin_ref_sd.instance_name = pin->get_instance() ? pin->get_instance()->get_name() : "";
            pin_ref_sd.pin_name = pin->get_pin_name();
            _instance_pin_list_sd.emplace_back(pin_ref_sd);
        }

        assert(_wire_list_sd.empty());
        for (auto wire : obj->get_wire_list()->get_wire_list()) {
            auto* wire_sd = new Shadow<idb::IdbRegularWire>();
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
    int32_t _xtalk_sd = 0;
    bool _fix_bump_sd = false;
    double _frequency_sd = 0.0;
    std::vector<std::string> _io_pin_name_list_sd;
    std::vector<idb::edadb_adapter::NetPinRef> _instance_pin_list_sd;
    std::vector<Shadow<idb::IdbRegularWire>*> _wire_list_sd;
}; // Shadow<idb::IdbNet>

} // namespace edadb
