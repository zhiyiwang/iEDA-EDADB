/**
 * @file shadow_idb_term.h
 * @brief This file contains shadow class definition for IdbTerm
 * @author Zhiyi Wang
 */

#pragma once

#include <algorithm>

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
    void setWriterUsesPortBranch(bool value) { _writer_uses_port_branch = value; }

    bool toShadow(idb::IdbTerm* obj, const uint32_t* idx_ptr = nullptr) {
        (void) idx_ptr;
        _direction_sd = obj->_direction;
        _type_sd = obj->_type;
        _has_port_sd = _writer_uses_port_branch;
        _is_special_net_sd = obj->_is_special_net;

        assert(_port_list_sd.empty());
        const auto& port_list = obj->get_port_list();
        for (uint32_t port_idx = 0; port_idx < port_list.size(); ++port_idx) {
            idb::IdbPort* port = port_list[port_idx];
            if (port == nullptr) {
                return false;
            }
            edadb::Shadow<idb::IdbPort>* port_sd = new edadb::Shadow<idb::IdbPort>();
            port_sd->setWriterUsesPortBranch(_writer_uses_port_branch);
            if (!port_sd->toShadow(port, &port_idx)) {
                delete port_sd;
                return false;
            }
            _port_list_sd.push_back(port_sd);
        }

        return true;
    } // toShadow

    bool fromShadow(idb::IdbTerm* obj, uint32_t* idx_ptr = nullptr) {
        (void) idx_ptr;
        obj->_direction = _direction_sd;
        obj->_type = _type_sd;
        obj->_has_port = _has_port_sd;
        obj->_is_special_net = _is_special_net_sd;

        assert(obj->_port_list.empty());
        for (auto* port_sd : _port_list_sd) {
            if (port_sd == nullptr) {
                return false;
            }
        }
        std::stable_sort(_port_list_sd.begin(), _port_list_sd.end(),
                         [](const auto* lhs, const auto* rhs) {
                             return lhs->_vec_idx < rhs->_vec_idx;
                         });
        for (auto* port_sd : _port_list_sd) {
            idb::IdbPort* port = obj->add_port();
            if (!port_sd->fromShadow(port)) {
                return false;
            }
        }

        // DefRead::parse_pin() copies the first explicit port placement status
        // to IdbTerm inside the i == 0 branch.
        if (_has_port_sd && !obj->_port_list.empty()) {
            idb::IdbPort* first_port = obj->_port_list.front();
            if (first_port->get_placement_status() != idb::IdbPlacementStatus::kNone) {
                obj->_placement_status = first_port->get_placement_status();
            }
        }

        return true;
    } // fromShadow


public:
    // columns
    idb::IdbConnectDirection _direction_sd = idb::IdbConnectDirection::kNone;
    idb::IdbConnectType _type_sd = idb::IdbConnectType::kNone;
    bool _has_port_sd = false;
    bool _is_special_net_sd = false;

    vector< edadb::Shadow<idb::IdbPort>* > _port_list_sd;

private:
    bool _writer_uses_port_branch = false;
}; // class Shadow<idb::IdbTerm>

} // namespace edadb
