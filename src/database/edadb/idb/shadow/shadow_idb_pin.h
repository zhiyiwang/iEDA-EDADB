/**
 * @file shadow_idb_pin.h
 * @brief This file contains shadow class definition for IdbPin
 * @author Zhiyi Wang
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>

#include "edadb.h"
#include "database/data/design/db_design/IdbPins.h"
#include "shadow/shadow_idb_term.h"

namespace edadb {
template<>
class Shadow<idb::IdbPin> {
public:
    Shadow<idb::IdbPin> () = default;
    ~Shadow<idb::IdbPin>() {
        delete _io_term_sd; _io_term_sd = nullptr;
    }

    Shadow<idb::IdbPin>(const Shadow& other) = delete;
    Shadow<idb::IdbPin>& operator=(const Shadow& other) = delete;

public:
    bool toShadow(idb::IdbPin* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || idx_ptr == nullptr) {
            return false;
        }
        _pin_name_sd = obj->get_pin_name();
        _order_sd = *idx_ptr;
        _net_name_sd = obj->get_net_name();

        idb::IdbTerm* term = obj->get_term();
        if (term == nullptr) {
            return false;
        }

        const bool writer_uses_port_branch =
                term->is_port_exist() || obj->is_special_net_pin();
        const bool writer_is_special =
                term->is_special_net() || obj->is_special_net_pin();

        _io_term_sd = new edadb::Shadow<idb::IdbTerm>();
        _io_term_sd->setWriterUsesPortBranch(writer_uses_port_branch);
        _io_term_sd->setWriterIsSpecial(writer_is_special);
        if (!_io_term_sd->toShadow(term)) {
            return false;
        }

        if (!writer_uses_port_branch && term->is_placed()) {
            _no_port_placement_status_sd = term->get_placement_status();
            _no_port_location_sd = *(obj->get_location());
            _no_port_orient_sd = obj->get_orient();
        }

        return true;
    } // toShadow

    bool fromShadow(idb::IdbPin* obj, uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }
        if (idx_ptr != nullptr) {
            *idx_ptr = static_cast<uint32_t>(_order_sd);
        }

        obj->set_pin_name(_pin_name_sd);
        obj->set_net_name(_net_name_sd);
        obj->set_orient(_no_port_orient_sd);
        obj->set_as_io();

        if (_io_term_sd == nullptr) {
            return false;
        }

        idb::IdbTerm* term = obj->set_term(nullptr);
        term->set_name(obj->get_pin_name());
        if (!_io_term_sd->fromShadow(term)) {
            return false;
        }

        if (term->is_port_exist()) {
            obj->set_port_layer_shape();
            return true;
        }

        int32_t bounding_box_ll_x = INT_MAX;
        int32_t bounding_box_ll_y = INT_MAX;
        int32_t bounding_box_ur_x = INT_MIN;
        int32_t bounding_box_ur_y = INT_MIN;
        int32_t coordinate_x = 0;
        int32_t coordinate_y = 0;
        int32_t layer_num = 0;

        for (idb::IdbPort* port : term->get_port_list()) {
            for (idb::IdbLayerShape* layer_shape : port->get_layer_shape()) {
                for (idb::IdbRect* rect : layer_shape->get_rect_list()) {
                    bounding_box_ll_x = std::min(bounding_box_ll_x, rect->get_low_x());
                    bounding_box_ll_y = std::min(bounding_box_ll_y, rect->get_low_y());
                    bounding_box_ur_x = std::max(bounding_box_ur_x, rect->get_high_x());
                    bounding_box_ur_y = std::max(bounding_box_ur_y, rect->get_high_y());
                    coordinate_x += rect->get_low_x() + rect->get_high_x();
                    coordinate_y += rect->get_low_y() + rect->get_high_y();
                    ++layer_num;
                }
            }
        }

        if (layer_num > 0) {
            term->set_average_position(coordinate_x / (layer_num * 2),
                                       coordinate_y / (layer_num * 2));
            term->set_bounding_box(bounding_box_ll_x, bounding_box_ll_y,
                                   bounding_box_ur_x, bounding_box_ur_y);

            // DefRead::parse_pin() performs these calculations only inside
            // the no-PORT hasPlacement branch.
            if (_no_port_placement_status_sd != idb::IdbPlacementStatus::kNone) {
                term->set_placement_status(_no_port_placement_status_sd);
                obj->set_location(_no_port_location_sd.get_x(), _no_port_location_sd.get_y());
                obj->set_average_coordinate(_no_port_location_sd.get_x() + term->get_average_position().get_x(),
                                            _no_port_location_sd.get_y() + term->get_average_position().get_y());
                obj->set_bounding_box();
            }
        }

        return true;
    } // fromShadow

public:
    // columns
    std::string _pin_name_sd;
    uint64_t _order_sd = 0;
    std::string _net_name_sd;
    edadb::Shadow<idb::IdbTerm> *_io_term_sd = nullptr; 

    idb::IdbCoordinate<int32_t> _no_port_location_sd;
    idb::IdbOrient _no_port_orient_sd = idb::IdbOrient::kN_R0;
    idb::IdbPlacementStatus _no_port_placement_status_sd = idb::IdbPlacementStatus::kNone;

/**
 * NOTE: no need to store idb::IdbPin member:
 *     std::vector<IdbLayerShape*> _layer_shape_list;
 *   using 
 *     vector<edadb::Shadow<idb::IdbLayerShape>* > _layer_shape_list_sd;
 * 
 * Since idb::IdbPin::_layer_shape_list stores rects using absolute coordinates,
 * the adapter stores the relative geometry from
 *   IdbPin::_io_term->_port_list->_layer_shape_list 
 * and rebuilds the absolute geometry in fromShadow().
 */
}; // Shadow<idb::IdbPin>

} // namespace edadb
