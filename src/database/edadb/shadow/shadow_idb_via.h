/**
 * @file shadow_idb_via_master.h
 * @brief This file contains shadow class definition for IdbViaMasterGenerate and IdbViaMaster
 * @author Zhiyi Wang
 */

#pragma once

#include <stdint.h>
#include "edadb.h"
#include "database/data/design/db_design/IdbVias.h"
#include "shadow/shadow_idb_via_master.h"

namespace edadb {
template<>
class Shadow<idb::IdbVia> {
public:
    void toShadow(idb::IdbVia* obj){
        _name_sd = obj->get_name();
        _master_instance_sd.toShadow( obj->get_instance() );
    }
    void fromShadow(idb::IdbVia* obj){
        obj->set_name(_name_sd);
        _master_instance_sd.fromShadow( obj->get_instance() );
    }
public:
    std::string _name_sd;
    Shadow<idb::IdbViaMaster> _master_instance_sd;
}; // idb::IdbVia
} // namespace edadb



