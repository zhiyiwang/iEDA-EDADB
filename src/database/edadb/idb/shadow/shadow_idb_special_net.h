/**
 * @file shadow_idb_special_net.h
 * @brief This file contains shadow class definitions for IdbSpecialNet
 * @author Zhiyi Wang
 */

#pragma once

#include <algorithm>

#include "edadb.h"
#include "../edadb_idb_helper.h"
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
        _is_via_sd = obj->is_via();
        _is_rect_sd = obj->is_rect();
        _shape_type_sd = obj->get_shape_type();

        if (_is_via_sd) {
            // Match DefWrite::write_specialnet_wire_segment_via():
            // layer + route width + shape + point list + via name.
            _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : "";
            _route_width_sd = obj->get_route_width();
            _via_name_sd = obj->get_via() ? obj->get_via()->get_name() : "";

            // DEF read may set STYLE; current DefWrite special-net writer does not emit it.
            _style_sd = obj->get_style();

            assert(_point_list_sd.empty());
            for (auto point : obj->get_point_list()) {
                _point_list_sd.emplace_back(new idb::IdbCoordinate<int32_t>(*point));
            }
        } else if (_is_rect_sd) {
            // Match DefWrite::write_specialnet_wire_segment_rect():
            // shape + layer + delta rect.
            _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : "";
            if (obj->get_delta_rect() != nullptr) {
                _delta_rect_sd = new idb::IdbRect(*obj->get_delta_rect());
            }
        } else {
            // Match DefWrite::write_specialnet_wire_segment_points():
            // layer + route width + shape + point list.
            _layer_name_sd = obj->get_layer() ? obj->get_layer()->get_name() : "";
            _route_width_sd = obj->get_route_width();

            // DEF read may set STYLE; current DefWrite special-net writer does not emit it.
            _style_sd = obj->get_style();

            assert(_point_list_sd.empty());
            for (auto point : obj->get_point_list()) {
                _point_list_sd.emplace_back(new idb::IdbCoordinate<int32_t>(*point));
            }
        }
    }

    bool fromShadow(idb::IdbSpecialWireSegment* obj) {
        obj->set_route_width(_route_width_sd);
        obj->set_style(_style_sd);
        obj->set_shape_type(_shape_type_sd);
        obj->set_is_via(_is_via_sd);
        obj->set_is_rect(_is_rect_sd);

        for (auto point_sd : _point_list_sd) {
            obj->add_point(point_sd->get_x(), point_sd->get_y());
        }

        if (_delta_rect_sd != nullptr) {
            obj->set_delta_rect(_delta_rect_sd->get_low_x(), _delta_rect_sd->get_low_y(),
                                _delta_rect_sd->get_high_x(), _delta_rect_sd->get_high_y());
        }

        if (!_layer_name_sd.empty()) {
            idb::IdbLayer* layer = idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName(_layer_name_sd);
            if (layer == nullptr) {
                std::cerr << "edadb::Shadow<idb::IdbSpecialWireSegment>::fromShadow failed to find layer: "
                          << _layer_name_sd << std::endl;
                return false;
            }
            obj->set_layer(layer);
        }

        if (_is_via_sd) {
            idb::IdbVias* via_list_def = idb::edadb_adapter::EdadbIdbHelper::getIdbDefVias();
            idb::IdbVias* via_list_lef = idb::edadb_adapter::EdadbIdbHelper::getIdbLefVias();
            if (via_list_def == nullptr || via_list_lef == nullptr) {
                std::cerr << "edadb::Shadow<idb::IdbSpecialWireSegment>::fromShadow failed to get via lists" << std::endl;
                return false;
            }
            idb::IdbVia* via = via_list_def->find_via(_via_name_sd);
            if (via == nullptr) {
                via = via_list_lef->find_via(_via_name_sd);
            }
            if (via == nullptr) {
                std::cerr << "edadb::Shadow<idb::IdbSpecialWireSegment>::fromShadow failed to find via: "
                          << _via_name_sd << std::endl;
                return false;
            }

            idb::IdbVia* via_new = obj->copy_via(via);
            if (via_new != nullptr) {
                via_new->set_coordinate(obj->get_point_start());
            }
        }

        obj->set_bounding_box();
        return true;
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

    bool fromShadow(idb::IdbSpecialWire* obj) {
        obj->set_wire_state(_wire_state_sd);
        obj->set_shield_name(_shield_name_sd);
        obj->init(_segment_list_sd.size());

        for (auto segment_sd : _segment_list_sd) {
            idb::IdbSpecialWireSegment* segment = obj->add_segment(nullptr);
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
            _pin_string_list_sd.emplace_back(pin_name);
        }

        if (_pin_string_list_sd.empty()) {
            for (auto pin : obj->get_io_pin_list()->get_pin_list()) {
                _io_pin_name_list_sd.emplace_back(pin->get_pin_name());
            }

            uint64_t pin_order = 0;
            for (auto pin : obj->get_instance_pin_list()->get_pin_list()) {
                idb::edadb_adapter::SpecialNetPinRef pin_ref_sd;
                pin_ref_sd._order_sd = pin_order++;
                pin_ref_sd.instance_name = pin->get_instance() ? pin->get_instance()->get_name() : "";
                pin_ref_sd.pin_name = pin->get_pin_name();
                _instance_pin_list_sd.emplace_back(pin_ref_sd);
            }
        }

        assert(_wire_list_sd.empty());
        for (auto wire : obj->get_wire_list()->get_wire_list()) {
            auto* wire_sd = new Shadow<idb::IdbSpecialWire>();
            wire_sd->toShadow(wire);
            _wire_list_sd.emplace_back(wire_sd);
        }
    }

    bool fromShadow(idb::IdbSpecialNet* obj) {
        // Match DefRead::parse_pdn(): restore SPECIALNETS header fields.
        obj->set_original_net_name(_original_net_name_sd);
        obj->set_connect_type(_connect_type_sd);
        obj->set_source_type(_source_type_sd);
        obj->set_weight(_weight_sd);

        // Match DefRead::parse_pdn(): restore connection records.
        for (auto& pin_name_sd : _pin_string_list_sd) {
            restorePinStringConnection(obj, pin_name_sd);
        }

        if (!obj->get_pin_string_list().empty()) {
            // Match parse_pdn() after "( * pin )": derive instance pins from pin strings.
            idb::IdbInstanceList* instance_list = idb::edadb_adapter::EdadbIdbHelper::getIdbInstanceList();
            if (instance_list == nullptr) {
                std::cerr << "edadb::Shadow<idb::IdbSpecialNet>::fromShadow failed to get instance list" << std::endl;
                return false;
            }
            instance_list->get_pin_list_by_names(obj->get_pin_string_list(), obj->get_instance_pin_list(), obj->get_instance_list());
        } else {
            // Match parse_pdn() branch: io_name == "PIN".
            for (auto& pin_name_sd : _io_pin_name_list_sd) {
                if (!restoreIoPinConnection(obj, pin_name_sd)) {
                    return false;
                }
            }

            // Match parse_pdn() branch: io_name is an instance name.
            std::sort(_instance_pin_list_sd.begin(), _instance_pin_list_sd.end(),
                      [](const auto& lhs, const auto& rhs) { return lhs._order_sd < rhs._order_sd; });
            for (auto& pin_ref_sd : _instance_pin_list_sd) {
                if (!restoreInstancePinConnection(obj, pin_ref_sd)) {
                    return false;
                }
            }
        }

        // Match DefRead::parse_pdn_wire() and parse_pdn_rects(): rebuild wires,
        // segments, layer/via references, points, rects, and computed bounding boxes.
        // Non-PDN SPECIALNETS that are dispatched by DefRead::parse_special_net()
        // into parse_net() are handled by Shadow<IdbNet>, not by this SPECIALNETS view.
        idb::IdbSpecialWireList* wire_list = obj->get_wire_list();
        for (auto wire_sd : _wire_list_sd) {
            idb::IdbSpecialWire* wire = wire_list->add_wire(nullptr);
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

private:
    void restorePinStringConnection(idb::IdbSpecialNet* obj, const std::string& pin_name_sd) {
        obj->add_pin_string(pin_name_sd);
    }

    bool restoreIoPinConnection(idb::IdbSpecialNet* obj, const std::string& pin_name_sd) {
        idb::IdbPins* io_pin_list = idb::edadb_adapter::EdadbIdbHelper::getIdbIoPins();
        if (io_pin_list == nullptr) {
            std::cerr << "edadb::Shadow<idb::IdbSpecialNet>::fromShadow failed to get IO pin list" << std::endl;
            return false;
        }

        idb::IdbPin* pin = io_pin_list->find_pin(pin_name_sd);
        if (pin != nullptr) {
            obj->add_io_pin(pin);
            pin->set_special_net(obj);
        }
        return true;
    }

    bool restoreInstancePinConnection(idb::IdbSpecialNet* obj,
                                      const idb::edadb_adapter::SpecialNetPinRef& pin_ref_sd) {
        idb::IdbInstanceList* instance_list = idb::edadb_adapter::EdadbIdbHelper::getIdbInstanceList();
        if (instance_list == nullptr) {
            std::cerr << "edadb::Shadow<idb::IdbSpecialNet>::fromShadow failed to get instance list" << std::endl;
            return false;
        }

        idb::IdbInstance* instance = instance_list->find_instance(pin_ref_sd.instance_name);
        if (instance != nullptr) {
            obj->add_instance(instance);
            idb::IdbPin* pin = instance->get_pin_by_term(pin_ref_sd.pin_name);
            if (pin != nullptr) {
                obj->add_instance_pin(pin);
                pin->set_special_net(obj);
            }
        }
        return true;
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
