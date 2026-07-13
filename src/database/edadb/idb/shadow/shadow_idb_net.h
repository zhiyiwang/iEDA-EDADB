/**
 * @file shadow_idb_net.h
 * @brief This file contains shadow class definitions for IdbNet
 * @author Zhiyi Wang
 */

#pragma once

#include <algorithm>

#include "edadb.h"
#include "../edadb_idb_helper.h"
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
        _is_via_sd = obj->is_via();
        _is_rect_sd = obj->is_rect();

        // Match DefWrite::write_net_wire_segment(): rect -> via -> points.
        // Preserve both parser flags if a path contains both RECT and VIA tokens.
        if (_is_rect_sd && obj->get_delta_rect() != nullptr) {
            _delta_rect_sd = new idb::IdbRect(*obj->get_delta_rect());
        }
        if (_is_via_sd && !obj->get_via_list().empty()) {
            _via_name_sd = obj->get_via_list().front()->get_name();
        }

        if (obj->get_point_number() > 1) {
            _is_second_point_virtual_sd = obj->is_virtual(obj->get_point_second());
        }

        assert(_point_list_sd.empty());
        for (auto point : obj->get_point_list()) {
            _point_list_sd.emplace_back(new idb::IdbCoordinate<int32_t>(*point));
        }
    }

    bool fromShadow(idb::IdbRegularWireSegment* obj) {
        obj->set_is_via(_is_via_sd);
        obj->set_is_rect(_is_rect_sd);

        // Match DefRead::parse_net() DEFIPATH_LAYER.
        if (!_layer_name_sd.empty()) {
            obj->set_layer_name(_layer_name_sd);
            idb::IdbLayer* layer = idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName(_layer_name_sd);
            obj->set_layer(layer);
        }

        // Match DEFIPATH_POINT / DEFIPATH_VIRTUALPOINT.
        for (size_t point_idx = 0; point_idx < _point_list_sd.size(); ++point_idx) {
            auto point_sd = _point_list_sd.at(point_idx);
            if (point_idx == _POINT_SECOND_ && _is_second_point_virtual_sd) {
                obj->add_virtual_point(point_sd->get_x(), point_sd->get_y());
            } else {
                obj->add_point(point_sd->get_x(), point_sd->get_y());
            }
        }

        // Match DEFIPATH_RECT.
        if (_delta_rect_sd != nullptr) {
            obj->set_delta_rect(_delta_rect_sd->get_low_x(), _delta_rect_sd->get_low_y(),
                                _delta_rect_sd->get_high_x(), _delta_rect_sd->get_high_y());
        }

        // Match DEFIPATH_VIA: DEF via lookup first, then LEF via lookup.
        if (_is_via_sd) {
            idb::IdbVias* via_list_def = idb::edadb_adapter::EdadbIdbHelper::getIdbDefVias();
            idb::IdbVias* via_list_lef = idb::edadb_adapter::EdadbIdbHelper::getIdbLefVias();
            if (via_list_def == nullptr || via_list_lef == nullptr) {
                return false;
            }

            idb::IdbVia* via = via_list_def->find_via(_via_name_sd);
            if (via == nullptr) {
                via = via_list_lef->find_via(_via_name_sd);
            }
            if (via == nullptr) {
                std::cout << "Error : can not find the via = " << _via_name_sd << std::endl;
                return true;
            }

            idb::IdbVia* via_new = obj->copy_via(via);
            if (via_new != nullptr) {
                via_new->set_coordinate(obj->get_point_end());
            }
        }

        return true;
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

    bool fromShadow(idb::IdbRegularWire* obj) {
        obj->set_wire_state(_wire_state_sd);
        obj->set_shield_name(_shield_name_sd);
        obj->init(_segment_list_sd.size());

        for (auto segment_sd : _segment_list_sd) {
            idb::IdbRegularWireSegment* segment = obj->add_segment(nullptr);
            if (!segment_sd->fromShadow(segment)) {
                return false;
            }
        }

        return true;
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

        // Match DefWrite::write_net(): IO connections, then instance connections.
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

        // Match DefWrite::write_net() optional header fields.
        _connect_type_sd = obj->get_connect_type();
        _source_type_sd = obj->get_source_type();
        _original_net_name_sd = obj->get_original_net_name();
        _weight_sd = obj->get_weight();
        _xtalk_sd = obj->get_xtalk();
        _fix_bump_sd = obj->is_fix_bump();
        _frequency_sd = obj->get_frequency();

        // Match DefWrite::write_net_wire().
        assert(_wire_list_sd.empty());
        for (auto wire : obj->get_wire_list()->get_wire_list()) {
            auto* wire_sd = new Shadow<idb::IdbRegularWire>();
            wire_sd->toShadow(wire);
            _wire_list_sd.emplace_back(wire_sd);
        }
    }

    bool fromShadow(idb::IdbNet* obj) {
        // Match DefRead::parse_net() optional header fields.
        obj->set_connect_type(_connect_type_sd);
        obj->set_source_type(_source_type_sd);
        obj->set_weight(_weight_sd);
        obj->set_xtalk(_xtalk_sd);
        obj->set_fix_bump(_fix_bump_sd);
        obj->set_frequency(_frequency_sd);
        obj->set_original_net_name(_original_net_name_sd);

        idb::IdbPins* io_pin_list = idb::edadb_adapter::EdadbIdbHelper::getIdbIoPins();
        idb::IdbInstanceList* instance_list = idb::edadb_adapter::EdadbIdbHelper::getIdbInstanceList();
        if (io_pin_list == nullptr || instance_list == nullptr) {
            return false;
        }

        // Match DefRead::parse_net() connection loop and setPinNet policy.
        const int32_t num_connections = _io_pin_name_list_sd.size() + _instance_pin_list_sd.size();
        auto set_pin_net = [obj, num_connections](idb::IdbPin* pin) {
            if (num_connections < 2) {
                if (pin->get_net() == nullptr) {
                    pin->set_net(obj);
                }
            } else {
                pin->set_net(obj);
            }
        };

        for (auto& pin_name_sd : _io_pin_name_list_sd) {
            idb::IdbPin* pin = io_pin_list->find_pin(pin_name_sd);
            if (pin == nullptr) {
                std::cout << "Can not find Pin in Pin list ... pin name = " << pin_name_sd << std::endl;
            } else {
                obj->add_io_pin(pin);
                set_pin_net(pin);
            }
        }

        std::sort(_instance_pin_list_sd.begin(), _instance_pin_list_sd.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs._order_sd < rhs._order_sd; });
        for (auto& pin_ref_sd : _instance_pin_list_sd) {
            idb::IdbInstance* instance = instance_list->find_instance(pin_ref_sd.instance_name);
            if (instance == nullptr) {
                std::cout << "Can not find instance in instance list ... instance name = "
                          << pin_ref_sd.instance_name << std::endl;
                continue;
            }

            obj->get_instance_list()->add_instance(instance);
            idb::IdbPin* pin = instance->get_pin_by_term(pin_ref_sd.pin_name);
            if (pin == nullptr) {
                std::cout << "Can not find Pin in Pin list ... pin name = " << pin_ref_sd.pin_name << std::endl;
            } else {
                obj->add_instance_pin(pin);
                set_pin_net(pin);
            }
        }

        // Match DefRead::parse_net() regular-wire reconstruction.
        idb::IdbRegularWireList* wire_list = obj->get_wire_list();
        for (auto wire_sd : _wire_list_sd) {
            idb::IdbRegularWire* wire = wire_list->add_wire(nullptr);
            if (!wire_sd->fromShadow(wire)) {
                return false;
            }
        }

        return true;
    }

    int32_t getSegmentCount(void) const {
        int32_t segment_count = 0;
        for (auto wire_sd : _wire_list_sd) {
            if (wire_sd != nullptr) {
                segment_count += wire_sd->_segment_list_sd.size();
            }
        }
        return segment_count;
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
