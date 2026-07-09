/**
 * @file shadow_idb_track_grid.h
 * @brief This file contains shadow class definition for IdbTrackGrid
 * @author Zhiyi Wang
*/

#pragma once

#include "edadb.h"
#include "database/data/design/db_layout/IdbLayer.h"
#include "database/data/design/db_design/IdbTrackGrid.h"


namespace edadb {
template<>
class Shadow<idb::IdbTrackGrid> {
public:
    Shadow(): primary_key(next_primary_key++) {}
public:
    bool toShadow(idb::IdbTrackGrid* obj) {
        assert(obj != nullptr);

        _track_num_sd = obj->get_track_num();
        // assign to write, no need to deep copy
        _track_sd = *(obj->get_track());
        _layer_name_vec_sd.clear();
        for ( auto& layer : obj->get_layer_list() ) {
            _layer_name_vec_sd.emplace_back(layer->get_name());
        }
        return true;
    } // toShadow

    bool fromShadow(idb::IdbTrackGrid* obj) {
        assert(obj != nullptr);

        obj->set_track_number( _track_num_sd );
        *(obj->get_track()) = _track_sd;
        assert( obj->get_layer_list().empty() );

        // use layer name to lookup layer during def read
        return true;
    } // fromShadow

public:
    uint64_t primary_key = 0;
    uint32_t _track_num_sd = 0;
    idb::IdbTrack _track_sd;
    std::vector<std::string> _layer_name_vec_sd;

private:
    static inline uint64_t next_primary_key = 1;
}; // shadow IdbTrackGrid

} // namespace edadb
