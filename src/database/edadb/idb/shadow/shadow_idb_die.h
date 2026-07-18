/**
 * @file shadow_idb_die.h
 * @brief This file contains shadow class definition for IdbDie
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_layout/IdbDie.h"
#include "shadow_idb_geometry.h"


namespace edadb {
template<>
class Shadow<idb::IdbDie> {
public:
    Shadow <idb::IdbDie>(void) = default;
    ~Shadow<idb::IdbDie>(void) { points_sd.clear(); }
    
public:
    bool toShadow(idb::IdbDie* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }
        for (auto* point : obj->get_points()) {
            if (point == nullptr) {
                return false;
            }
        }

        // assign to write, no need to deep copy
        points_sd.clear();
        points_sd = obj->get_points();

        // DbmapWriter will write the vector element one by one from points_sd.
        // Each element will store the vector index in the database automatically,
        // so no need to handle the vector index here.
        return true;
    } // toShadow

    bool fromShadow(idb::IdbDie* obj, uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || !obj->get_points().empty()) {
            return false;
        }
        for (auto* point : points_sd) {
            if (point == nullptr) {
                return false;
            }
        }

        for (auto* point : points_sd) {
            obj->add_point(point);
        }
        points_sd.clear();
        return true;
    } // fromShadow 

public:
    uint64_t primary_key = 1;
    std::vector< idb::IdbCoordinate<int32_t>* > points_sd;
};  // idb::IdbDie

} // namespace edadb
