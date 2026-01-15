/**
 * @file shadow_idb_term.h
 * @brief This file contains shadow class definition for IdbTerm
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_layout/IdbTerm.h"
#include "shadow/shadow_idb_port.h"

namespace edadb {
template<>
class Shadow<idb::IdbTerm> {
public:
    ~Shadow<idb::IdbTerm>() {
        for (auto& port_sd : _port_list_sd) {
            delete port_sd; port_sd = nullptr;
        }
        _port_list_sd.clear();
    }

public:
    void toShadow(idb::IdbTerm* obj) {
        _name_sd = obj->_name;
        _direction_sd = obj->_direction;
        _type_sd = obj->_type;
        _shape_sd = obj->_shape;
        _placement_status_sd = obj->_placement_status;
        _has_port_sd = obj->_has_port;
        _is_special_net_sd = obj->_is_special_net;
        _is_instance_sd = obj->_is_instance;

        // port list
        assert(_port_list_sd.empty());
        for (auto& port : obj->get_port_list()) {
            edadb::Shadow<idb::IdbPort>* port_sd = new edadb::Shadow<idb::IdbPort>();
            port_sd->toShadow(port);
            _port_list_sd.push_back(port_sd);
        }
    } // toShadow

    void fromShadow(idb::IdbTerm* obj) {
        obj->_name = _name_sd;
        obj->_direction = _direction_sd;
        obj->_type = _type_sd;
        obj->_shape = _shape_sd;
        obj->_placement_status = _placement_status_sd;

        obj->_has_port = _has_port_sd;
        obj->_is_special_net = _is_special_net_sd;
        obj->_is_instance = _is_instance_sd;

        assert(obj->_port_list.empty());
        // DefReadEdadb::readIdbPin: 
        //   will create IdbPort and call fromShadow to get the value
//--        for (auto& port_sd : _port_list_sd) {
//--            idb::IdbPort* port = obj->add_port(nullptr);
//--            port_sd->fromShadow(port);
//--        }
    } // fromShadow


public:
    // columns
    string _name_sd;
    idb::IdbConnectDirection _direction_sd;
    idb::IdbConnectType _type_sd;
    idb::IdbTermShape _shape_sd;
    idb::IdbPlacementStatus _placement_status_sd;
    bool _has_port_sd;
    bool _is_special_net_sd;
    bool _is_instance_sd;

    vector< edadb::Shadow<idb::IdbPort>* > _port_list_sd;
}; // class Shadow<idb::IdbTerm>

} // namespace edadb

