/**
 * @file shadow_idb_instance.h
 * @brief This file contains shadow class definition IdbInstance 
 * @author Zhiyi Wang
 */

#pragma once

#include <cassert>
#include <cstdint>

#include "edadb.h"
#include "shadow/shadow_idb_halo.h"
#include "database/data/design/db_design/IdbHalo.h"
#include "database/data/design/db_design/IdbInstance.h"
#include "../edadb_idb_helper.h"

namespace edadb {
template<>
class Shadow<idb::IdbInstance>  {
public:
    ~Shadow<idb::IdbInstance>(void) {
        if (_coordinate_sd_owner) {
            delete _coordinate_sd;
            _coordinate_sd = nullptr;
        }

        // _halo_sd ownership get from / moved to IdbInstance 

        if (_route_halo_sd != nullptr) {
            delete _route_halo_sd;
            _route_halo_sd = nullptr;
        }
    }


public:
    bool toShadow(idb::IdbInstance* obj, const uint32_t* = nullptr) {
        if (obj == nullptr || obj->get_cell_master() == nullptr
            || obj->get_coordinate() == nullptr) {
            return false;
        }

        _name_sd = obj->get_name();
        _cell_master_name_sd = obj->get_cell_master() ? obj->get_cell_master()->get_name() : "";

        _type_sd = obj->get_type();
        _status_sd = obj->get_status();
        _orient_sd = obj->get_orient();
        _weight_sd = obj->get_weight();

        // assign to write, no need to deep copy
        _coordinate_sd = obj->get_coordinate();
        _coordinate_sd_owner = false;

        if ( obj->has_halo() ) {
            _halo_sd = obj->get_halo();
        }
        if ( obj->has_route_halo() ) {
            _route_halo_sd = new Shadow<idb::IdbRouteHalo>();
            if (!_route_halo_sd->toShadow(obj->get_route_halo())) {
                return false;
            }
        }
        _region_name_sd = obj->get_region() ? obj->get_region()->get_name() : "";
        return true;
    }

    bool fromShadow(idb::IdbInstance* obj, uint32_t* = nullptr) {
        if (obj == nullptr || _coordinate_sd == nullptr) {
            return false;
        }

        idb::IdbCellMaster* cell_master =
            idb::edadb_adapter::EdadbIdbHelper::findIdbCellMasterByName(_cell_master_name_sd);
        if (cell_master == nullptr) {
            std::cerr << "edadb::Shadow<idb::IdbInstance>::fromShadow error: cannot find cell master: "
                      << _cell_master_name_sd << std::endl;
            return false;
        }

        obj->set_name(_name_sd);
        obj->set_cell_master(cell_master);
        obj->set_status(_status_sd);
        obj->set_orient(_orient_sd, false);
        obj->set_type(_type_sd);
        obj->set_weight(_weight_sd);

        if (!_region_name_sd.empty()) {
            idb::IdbRegion* region =
                idb::edadb_adapter::EdadbIdbHelper::findIdbRegionByName(_region_name_sd);
            if (region != nullptr) {
                obj->set_region(region);
                region->add_instance(obj);
            }
        }

        if (_halo_sd != nullptr) {
            obj->set_halo(_halo_sd);
            _halo_sd = nullptr;
        }

        if (_route_halo_sd != nullptr) {
            idb::IdbRouteHalo* route_halo = obj->set_route_halo();
            if (!_route_halo_sd->fromShadow(route_halo)) {
                return false;
            }
        }

        obj->set_coodinate(*_coordinate_sd);
        _coordinate_sd_owner = true;
        return true;
    }

public:
    std::string _name_sd;
    std::string _cell_master_name_sd;

    idb::IdbInstanceType _type_sd;
    idb::IdbPlacementStatus _status_sd;
    idb::IdbOrient _orient_sd;
    int32_t _weight_sd;
    idb::IdbCoordinate<int32_t>* _coordinate_sd = nullptr; // allocated by EDADB read or borrowed from iDB write
    bool _coordinate_sd_owner = true;

    idb::IdbHalo *_halo_sd = nullptr;
    /**
     * Define Shadow<idb::IdbRouteHalo>* instead of Shadow<idb::IdbRouteHalo>:
     *   EDADB backend will check if _route_halo_sd is nullptr to decide whether
     *   to create a new idb::IdbRouteHalo object during fromShadow() 
     */
    Shadow<idb::IdbRouteHalo>* _route_halo_sd = nullptr;

    std::string _region_name_sd;
}; // Shadow IdbInstance
} // namespace edadb
