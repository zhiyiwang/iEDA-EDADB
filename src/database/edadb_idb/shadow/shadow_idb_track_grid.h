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
    void toShadow(idb::IdbTrackGrid* obj, const uint32_t* idx_ptr = nullptr) {
        assert(obj != nullptr);
        assert(idx_ptr == nullptr);

        _track_num_sd = obj->get_track_num();
        // assign to write, no need to deep copy
        _track_sd = *(obj->get_track());
        _layer_name_vec_sd.clear();
        for ( auto& layer : obj->get_layer_list() ) {
            edadb::CppStrings layer_name_sd;
            layer_name_sd.str = layer->get_name();
            _layer_name_vec_sd.emplace_back( layer_name_sd );
        }
    }

    void fromShadow(idb::IdbTrackGrid* obj, uint32_t* idx_ptr = nullptr) {
        assert(obj != nullptr);
        assert(idx_ptr == nullptr);

        obj->set_track_number( _track_num_sd );
        *(obj->get_track()) = _track_sd;
        assert( obj->get_layer_list().empty() );

        // use layer name to lookup layer during def read
    }

public:
    uint64_t primary_key = 0;
    uint32_t _track_num_sd = 0;
    idb::IdbTrack _track_sd;
    vector<edadb::CppStrings> _layer_name_vec_sd;

private:
    static inline uint64_t next_primary_key = 1;
}; // shadow IdbTrackGrid

} // namespace edadb



