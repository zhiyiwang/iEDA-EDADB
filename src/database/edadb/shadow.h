/**
 * @file shadow.h
 * @brief This file contains the definition of the IdbShadow class for representing shadow areas in the design.
 * @author Zhiyi Wang 
 */

#pragma once

#include <stdint.h>
#include <vector>

#include "../../third_party/edadb/include/edadb.h"


#include "../data/design/db_layout/IdbDie.h"

namespace edadb {
template<>
class Shadow<idb::IdbDie> {
public:
    Shadow(): primary_key(next_primary_key++) {}
    ~Shadow() = default;
public:
    void fromShadow(idb::IdbDie* obj) {
        auto& points = obj->get_points();
        assert(points.empty());
        points_sd.swap(points);
    } 
    void toShadow(idb::IdbDie* obj) {
        // directly assign the vector to write 
        points_sd = obj->get_points();
    }
public:
    uint64_t primary_key = 0;
    std::vector< idb::IdbCoordinate<int32_t>* > points_sd;
private:
    static inline uint64_t next_primary_key = 1;
}; 
} // namespace edadb
