/**
 * @file shadow_idb_track_grid.h
 * @brief This file contains shadow class definition for IdbTrackGrid
 * @author Zhiyi Wang
*/

#pragma once

#include "edadb.h"
#include "database/data/design/db_layout/IdbLayer.h"
#include "database/data/design/db_design/IdbTrackGrid.h"
#include "../edadb_idb_helper.h"


namespace edadb {
template<>
class Shadow<idb::IdbTrackGrid> {
public:
    Shadow(): primary_key(next_primary_key++) {}
public:
    bool toShadow(idb::IdbTrackGrid* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || obj->get_track() == nullptr) {
            return false;
        }

        _track_num_sd = obj->get_track_num();
        _track_sd.set_direction(obj->get_track()->get_direction());
        _track_sd.set_start(obj->get_track()->get_start());
        _track_sd.set_pitch(obj->get_track()->get_pitch());
        _layer_name_vec_sd.clear();
        for (auto* layer : obj->get_layer_list()) {
            if (layer == nullptr) {
                return false;
            }
            _layer_name_vec_sd.emplace_back(layer->get_name());
        }
        return true;
    } // toShadow

    bool fromShadow(idb::IdbTrackGrid* obj, uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || obj->get_track() == nullptr || !obj->get_layer_list().empty()) {
            return false;
        }

        idb::IdbLayers* layers = idb::edadb_adapter::EdadbIdbHelper::getIdbLayers();
        if (layers == nullptr) {
            return false;
        }

        obj->set_track_number(_track_num_sd);
        obj->get_track()->set_direction(_track_sd.get_direction());
        obj->get_track()->set_start(_track_sd.get_start());
        obj->get_track()->set_pitch(_track_sd.get_pitch());

        for (const std::string& layer_name : _layer_name_vec_sd) {
            idb::IdbLayer* layer = layers->find_layer(layer_name);
            if (layer == nullptr) {
                std::cout << "Track Grid Error : no layer exist..." << std::endl;
                continue;
            }

            obj->add_layer_list(layer);
            if (layer->is_routing()) {
                auto* routing_layer = dynamic_cast<idb::IdbLayerRouting*>(layer);
                if (routing_layer == nullptr) {
                    return false;
                }
                routing_layer->add_track_grid(obj);
            }
        }

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
