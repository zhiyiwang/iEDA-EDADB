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
    Shadow <idb::IdbDie>(void) : primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbDie>(void) { points_sd.clear(); }
    
public:
    bool toShadow(idb::IdbDie* obj, const uint32_t* idx_ptr = nullptr) {
        assert( points_sd.empty() );

        // assign to write, no need to deep copy
        points_sd = obj->get_points();

        // DbmapWriter will write the vector element one by one from points_sd.
        // Each element will store the vector index in the database automatically,
        // so no need to handle the vector index here.
        return true;
    } // toShadow

    bool fromShadow(idb::IdbDie* obj, uint32_t* idx_ptr = nullptr) {
        auto& points = obj->get_points();
        assert(points.empty());

        // swap the shadow vector with the object's vector,
        // so that we can avoid deep copy and reuse the shadow vector's memory for write.
        points_sd.swap(points); 
        return true;
    } // fromShadow 

public:
    uint64_t primary_key = 0;
    std::vector< idb::IdbCoordinate<int32_t>* > points_sd;

private:
    static inline uint64_t next_primary_key = 1;
};  // idb::IdbDie

} // namespace edadb
