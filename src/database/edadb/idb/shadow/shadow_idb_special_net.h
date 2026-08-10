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
        resetStorage();
    }

    Shadow<idb::IdbSpecialWireSegment>(const Shadow& other) = delete;
    Shadow<idb::IdbSpecialWireSegment>& operator=(const Shadow& other) = delete;

public:
    bool toShadow(idb::IdbSpecialWireSegment* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || idx_ptr == nullptr || obj->get_layer() == nullptr) {
            return false;
        }

        resetStorage();
        _vec_idx = *idx_ptr;
        _is_via_sd = obj->is_via();
        _is_rect_sd = obj->is_rect();
        if (_is_via_sd && _is_rect_sd) {
            return false;
        }
        _shape_type_sd = obj->get_shape_type();
        _layer_name_sd = obj->get_layer()->get_name();

        if (_is_via_sd) {
            // Match DefWrite::write_specialnet_wire_segment_via():
            // layer + route width + shape + point list + via name;
            // Keep parser-only STYLE in the EDADB write view.
            _route_width_sd = obj->get_route_width();
            _style_sd = obj->get_style();
            if (obj->get_via() == nullptr || obj->get_point_list().empty()) {
                return false;
            }
            _via_name_sd = obj->get_via()->get_name();

            for (auto point : obj->get_point_list()) {
                if (point == nullptr) {
                    return false;
                }
                _point_list_sd.emplace_back(new idb::IdbCoordinate<int32_t>(*point));
            }
        } else if (_is_rect_sd) {
            // Match DefWrite::write_specialnet_wire_segment_rect():
            // shape + layer + delta rect.
            if (obj->get_delta_rect() == nullptr) {
                return false;
            }
            _delta_rect_sd = new idb::IdbRect(*obj->get_delta_rect());
        } else {
            // Match DefWrite::write_specialnet_wire_segment_points():
            // layer + route width + shape + point list;
            // Keep parser-only STYLE in the EDADB write view.
            _route_width_sd = obj->get_route_width();
            _style_sd = obj->get_style();
            if (obj->get_point_list().size() < _POINT_MAX_) {
                return false;
            }
            for (auto point : obj->get_point_list()) {
                if (point == nullptr) {
                    return false;
                }
                _point_list_sd.emplace_back(new idb::IdbCoordinate<int32_t>(*point));
            }
        }
        return true;
    }

private:
    void resetStorage() {
        _layer_name_sd.clear();
        _via_name_sd.clear();
        _route_width_sd = -1;
        _shape_type_sd = idb::IdbWireShapeType::kNone;
        _style_sd = -1;
        _is_via_sd = false;
        _is_rect_sd = false;

        delete _delta_rect_sd;
        _delta_rect_sd = nullptr;
        for (auto& point : _point_list_sd) {
            delete point;
            point = nullptr;
        }
        _point_list_sd.clear();
    }

public:
    uint64_t primary_key = 0;
    uint64_t _vec_idx = 0;
    std::string _layer_name_sd;
    std::string _via_name_sd;
    int32_t _route_width_sd = -1;
    idb::IdbWireShapeType _shape_type_sd = idb::IdbWireShapeType::kNone;
    int32_t _style_sd = -1;
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
        resetStorage();
    }

    Shadow<idb::IdbSpecialWire>(const Shadow& other) = delete;
    Shadow<idb::IdbSpecialWire>& operator=(const Shadow& other) = delete;

public:
    bool toShadow(idb::IdbSpecialWire* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || idx_ptr == nullptr) {
            return false;
        }

        resetStorage();
        _vec_idx = *idx_ptr;
        _wire_state_sd = obj->get_wire_state();
        if (_wire_state_sd == idb::IdbWiringStatement::kShield) {
            // DefRead::parse_pdn_wire()/parse_pdn_rects() retain SHIELD state;
            // the native writer currently omits it, but EDADB must not lose it.
            _shield_name_sd = obj->get_shiled_name();
        }

        uint32_t segment_idx = 0;
        for (auto segment : obj->get_segment_list()) {
            auto* segment_sd = new Shadow<idb::IdbSpecialWireSegment>();
            if (!segment_sd->toShadow(segment, &segment_idx)) {
                delete segment_sd;
                return false;
            }
            _segment_list_sd.emplace_back(segment_sd);
            ++segment_idx;
        }
        return true;
    }

private:
    void resetStorage() {
        _wire_state_sd = idb::IdbWiringStatement::kNone;
        _shield_name_sd.clear();
        for (auto& segment : _segment_list_sd) {
            delete segment;
            segment = nullptr;
        }
        _segment_list_sd.clear();
    }

public:
    uint64_t primary_key = 0;
    uint64_t _vec_idx = 0;
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
        resetStorage();
    }

    Shadow<idb::IdbSpecialNet>(const Shadow& other) = delete;
    Shadow<idb::IdbSpecialNet>& operator=(const Shadow& other) = delete;

public:
    bool toShadow(idb::IdbSpecialNet* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || obj->get_io_pin_list() == nullptr || obj->get_instance_pin_list() == nullptr
            || obj->get_wire_list() == nullptr || obj->get_net_name().empty()) {
            return false;
        }

        resetStorage();
        _net_name_sd = obj->get_net_name();
        _original_net_name_sd = obj->get_original_net_name();
        _connect_type_sd = obj->get_connect_type();
        _source_type_sd = obj->get_source_type();
        _weight_sd = obj->get_weight();

        for (auto& pin_name : obj->get_pin_string_list()) {
            _pin_string_list_sd.emplace_back(pin_name);
        }

        if (_pin_string_list_sd.empty()) {
            for (auto pin : obj->get_io_pin_list()->get_pin_list()) {
                if (pin == nullptr) {
                    return false;
                }
                _io_pin_name_list_sd.emplace_back(pin->get_pin_name());
            }

            uint64_t pin_order = 0;
            for (auto pin : obj->get_instance_pin_list()->get_pin_list()) {
                if (pin == nullptr || pin->get_instance() == nullptr) {
                    return false;
                }
                idb::edadb_adapter::SpecialNetPinRef pin_ref_sd;
                pin_ref_sd._order_sd = pin_order++;
                pin_ref_sd.instance_name = pin->get_instance() ? pin->get_instance()->get_name() : "";
                pin_ref_sd.pin_name = pin->get_pin_name();
                _instance_pin_list_sd.emplace_back(pin_ref_sd);
            }
        }

        uint32_t wire_idx = 0;
        for (auto wire : obj->get_wire_list()->get_wire_list()) {
            auto* wire_sd = new Shadow<idb::IdbSpecialWire>();
            if (!wire_sd->toShadow(wire, &wire_idx)) {
                delete wire_sd;
                return false;
            }
            _wire_list_sd.emplace_back(wire_sd);
            ++wire_idx;
        }
        return true;
    }

private:
    void resetStorage() {
        _net_name_sd.clear();
        _original_net_name_sd.clear();
        _connect_type_sd = idb::IdbConnectType::kNone;
        _source_type_sd = idb::IdbInstanceType::kNone;
        _weight_sd = 0;
        _pin_string_list_sd.clear();
        _io_pin_name_list_sd.clear();
        _instance_pin_list_sd.clear();
        for (auto& wire : _wire_list_sd) {
            delete wire;
            wire = nullptr;
        }
        _wire_list_sd.clear();
    }

public:
    std::string _net_name_sd;
    std::string _original_net_name_sd;
    idb::IdbConnectType _connect_type_sd = idb::IdbConnectType::kNone;
    idb::IdbInstanceType _source_type_sd = idb::IdbInstanceType::kNone;
    int32_t _weight_sd = 0;
    std::vector<std::string> _pin_string_list_sd;
    std::vector<std::string> _io_pin_name_list_sd;
    std::vector<idb::edadb_adapter::SpecialNetPinRef> _instance_pin_list_sd;
    std::vector<Shadow<idb::IdbSpecialWire>*> _wire_list_sd;
}; // Shadow<idb::IdbSpecialNet>

} // namespace edadb
