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
    Shadow<idb::IdbDie>(void): primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbDie>(void) {
        for (auto& point_shadow : points_sd) {
            delete point_shadow;
        }
        points_sd.clear();
    }

    Shadow<idb::IdbDie>(const Shadow& other) = delete;
    Shadow<idb::IdbDie>& operator=(const Shadow& other) = delete;
    
public:
    void fromShadow(idb::IdbDie* obj) {
        auto& points = obj->get_points();
        assert(points.empty());

        uint64_t num = points_sd.size();
        points.resize(num, nullptr);

        for (uint64_t cnt = 0, idx = 0; cnt < num; ++cnt) {
            idb::IdbCoordinate<int32_t>* point = new idb::IdbCoordinate<int32_t>();
            points_sd[cnt]->fromShadow(idx, point);
            assert(points[idx] == nullptr);
            points[idx] = point;
        } // for

        for (auto& point_shadow : points_sd) {
            delete point_shadow;
        }
        points_sd.clear();
    } // fromShadow 

    void toShadow(idb::IdbDie* obj) {
        assert( points_sd.empty() );

        auto &points = obj->get_points();
        for (uint64_t idx = 0; idx < points.size(); ++idx) {
            Shadow<idb::IdbCoordinate<int32_t>>* point_shadow = 
                    new Shadow<idb::IdbCoordinate<int32_t>>();
            point_shadow->toShadow(idx, points[idx]);
            points_sd.emplace_back( point_shadow );
        } // for
    } // toShadow

public:
    uint64_t primary_key = 0;
    std::vector< Shadow<idb::IdbCoordinate<int32_t>>* > points_sd;
private:
    static inline uint64_t next_primary_key = 1;
};  // idb::IdbDie

} // namespace edadb
