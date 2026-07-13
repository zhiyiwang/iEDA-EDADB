/**
 * @file shadow_idb_term.h
 * @brief This file contains shadow class definition for IdbTerm
 * @author Zhiyi Wang
 */

#pragma once

#include <cassert>
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
    void toShadow(idb::IdbPin* obj) {
        _pin_name_sd = obj->get_pin_name();
        _net_name_sd = obj->get_net_name();

        idb::IdbTerm* term = obj->get_term();
        if (term != nullptr) {
            _io_term_sd = new edadb::Shadow<idb::IdbTerm>();
            _io_term_sd->toShadow(term);
        }
        _average_coordinate_sd = *(obj->get_average_coordinate());
        _location_sd = *(obj->get_location());

        _orient_sd = obj->get_orient();
        _is_io_pin_sd = obj->is_io_pin();
        _is_special_net_sd = obj->is_special_net_pin();
        _layer_num_sd = obj->get_port_box_list().size();
    } // toShadow

    void fromShadow(idb::IdbPin* obj) {
        obj->set_pin_name(_pin_name_sd);
        obj->set_net_name(_net_name_sd);

        if (_io_term_sd != nullptr) {
            // obj create IdbTerm instance 
            idb::IdbTerm* term = obj->set_term(nullptr);
            _io_term_sd->fromShadow(term);
        }

        obj->set_orient(_orient_sd);
        if (_is_io_pin_sd) 
            obj->set_as_io();

        obj->set_location(_location_sd.get_x(), _location_sd.get_y());
        obj->set_average_coordinate(_average_coordinate_sd.get_x(), _average_coordinate_sd.get_y());

    } // fromShadow

public:
    // columns
    std::string _pin_name_sd;
    std::string _net_name_sd;
    edadb::Shadow<idb::IdbTerm> *_io_term_sd = nullptr; 

    idb::IdbCoordinate<int32_t> _average_coordinate_sd;
    idb::IdbCoordinate<int32_t> _location_sd;

    idb::IdbOrient _orient_sd;
    bool _is_io_pin_sd;
    bool _is_special_net_sd; // pointer _special_net == nullptr 
    uint32_t _layer_num_sd; // == (_layer_shape_list.size())

/**
 * NOTE: no need to store idb::IdbPin member:
 *     std::vector<IdbLayerShape*> _layer_shape_list;
 *   using 
 *     vector<edadb::Shadow<idb::IdbLayerShape>* > _layer_shape_list_sd;
 * 
 * Since idb::IdbPin::_layer_shape_list store the rect using absolute coordinate,
 * however, during def read and write, the builder use relative coordinate.
 * So we only need to store relative coordinate from
 *   IdbPin::_io_term->_port_list->_layer_shape_list 
 */
}; // Shadow<idb::IdbPin>

} // namespace edadb
