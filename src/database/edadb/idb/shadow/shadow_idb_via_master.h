/**
 * @file shadow_idb_via_master.h
 * @brief This file contains shadow class definition for IdbViaMasterGenerate and IdbViaMaster
 * @author Zhiyi Wang
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <stdint.h>
#include "database/data/design/db_layout/IdbViaMaster.h"

#include "edadb.h"
#include "shadow_idb_layer_shape.h"
#include "../edadb_idb_helper.h"


namespace edadb {
template<>
class Shadow<idb::IdbViaMasterGenerate> {
public:
    /**
     * @brief toShadow: convert ALL data from ieda instance to shadow instance
     * @param obj: the ieda instance
    */
    bool toShadow(idb::IdbViaMasterGenerate* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }

        _rule_name_sd = obj->get_rule_name();
        _cut_size_x_sd = obj->get_cut_size_x();
        _cut_size_y_sd = obj->get_cut_size_y();
        _cut_spacing_x_sd = obj->get_cut_spcing_x ();
        _cut_spacing_y_sd = obj->get_cut_spcing_y();
        _enclosure_bottom_x_sd = obj->get_enclosure_bottom_x();
        _enclosure_bottom_y_sd = obj->get_enclosure_bottom_y();
        _enclosure_top_x_sd = obj->get_enclosure_top_x();
        _enclosure_top_y_sd = obj->get_enclosure_top_y();
        _num_cut_rows_sd = obj->get_cut_rows();
        _num_cut_cols_sd = obj->get_cut_cols();
        _original_offset_x_sd = obj->get_original_offset_x();
        _original_offset_y_sd = obj->get_original_offset_y();
        _offset_bottom_x_sd = obj->get_offset_bottom_x();
        _offset_bottom_y_sd = obj->get_offset_bottom_y();
        _offset_top_x_sd = obj->get_offset_top_x();
        _offset_top_y_sd = obj->get_offset_top_y();

        _layer_bottom_name_sd = obj->get_layer_bottom() ? obj->get_layer_bottom()->get_name() : "";
        _layer_cut_name_sd = obj->get_layer_cut() ? obj->get_layer_cut()->get_name() : "";
        _layer_top_name_sd = obj->get_layer_top() ? obj->get_layer_top()->get_name() : "";
        _pattern_name_sd = obj->get_patttern() ? obj->get_patttern()->get_pattern_string() : "";

        return true;
    } // toShadow

    /**
     * @brief fromShadow: convert basic type data from shadow instance to ieda instance
     * @param obj: the ieda instance
     */
    bool fromShadow(idb::IdbViaMasterGenerate* obj, uint32_t* idx_ptr = nullptr) {
        obj->set_rule_name(_rule_name_sd);
        idb::IdbViaRuleGenerate* via_rule = idb::edadb_adapter::EdadbIdbHelper::findIdbViaRuleGenerateByName(_rule_name_sd);
        if (via_rule == nullptr) {
            std::cout << "edadb::Shadow<idb::IdbViaMasterGenerate>::fromShadow error: cannot find via rule for via master generate: " << _rule_name_sd << std::endl;
            return false;
        }
        obj->set_rule_generate(via_rule);

        obj->set_cut_size(_cut_size_x_sd, _cut_size_y_sd);
        obj->set_cut_spacing(_cut_spacing_x_sd, _cut_spacing_y_sd);
        obj->set_enclosure_bottom(_enclosure_bottom_x_sd, _enclosure_bottom_y_sd);
        obj->set_enclosure_top(_enclosure_top_x_sd, _enclosure_top_y_sd);
        obj->set_cut_row_col(_num_cut_rows_sd, _num_cut_cols_sd);
        obj->set_original(_original_offset_x_sd, _original_offset_y_sd);
        obj->set_offset_bottom(_offset_bottom_x_sd, _offset_bottom_y_sd);
        obj->set_offset_top(_offset_top_x_sd, _offset_top_y_sd);

        // NOTE: set layer and pattern instance by looking up with name in shadow
        idb::IdbLayer* layer_bottom = idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName(_layer_bottom_name_sd);
        if (layer_bottom == nullptr) {
            std::cout << "edadb::Shadow<idb::IdbViaMasterGenerate>::fromShadow error: cannot find layer for via master generate: " << _layer_bottom_name_sd << std::endl;
            return false;
        }
        obj->set_layer_bottom(dynamic_cast<idb::IdbLayerRouting*>(layer_bottom));

        idb::IdbLayerCut* layer_cut = dynamic_cast<idb::IdbLayerCut*>(idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName(_layer_cut_name_sd));
        if (layer_cut == nullptr) {
            std::cout << "edadb::Shadow<idb::IdbViaMasterGenerate>::fromShadow error: cannot find layer for via master generate: " << _layer_cut_name_sd << std::endl;
            return false;
        }
        layer_cut->set_via_rule(via_rule); 
        obj->set_layer_cut(layer_cut);

        idb::IdbLayer* layer_top = idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName(_layer_top_name_sd);
        if (layer_top == nullptr) {
            std::cout << "edadb::Shadow<idb::IdbViaMasterGenerate>::fromShadow error: cannot find layer for via master generate: " << _layer_top_name_sd << std::endl;
            return false;
        }
        obj->set_layer_top(dynamic_cast<idb::IdbLayerRouting*>(layer_top));

        // use pattern name string to create and set pattern instance 
        obj->set_patttern(_pattern_name_sd);

        // DefRead::parse_via() expands ROWCOL on a newly-created via master and calls
        // set_via_shape() once. TABLE4SHADOW_WVEC SELECT instead creates two temporary
        // Shadow<IdbViaMaster> instances that both restore the same IdbViaMaster target.
        // The scalar-phase shadow is populated from parent-row columns and calls
        // fromShadow(). The vector-phase shadow is first populated by toShadow(target),
        // then receives child-table rows and calls fromShadow(target) again. This does
        // not read the parent row twice; it is a two-phase object reconstruction.
        // Generated vias have no fixed-via child rows, but the second application still
        // replays their inline generate fields copied by toShadow().
        // add_cut_rect() owns and appends heap rectangles, so retaining the first
        // pass makes N source cuts become 2N. The next set_via_shape() would append
        // all 2N cuts after the N derived cuts from the first pass, producing 3N.
        // The generated-via fixture exposed this as 19 native cuts versus 57 EDADB
        // cuts (27 versus 65 total iRT obstacles). Rebuild the cut array by replacement.
        std::vector<idb::IdbRect*>& cut_rect_list = obj->get_cut_rect_list();
        for (idb::IdbRect*& cut_rect : cut_rect_list) {
            delete cut_rect;
            cut_rect = nullptr;
        }
        cut_rect_list.clear();

        int32_t cut_width_total = _num_cut_cols_sd * _cut_size_x_sd + (_num_cut_cols_sd - 1) * _cut_spacing_x_sd;
        int32_t cut_height_total = _num_cut_rows_sd * _cut_size_y_sd + (_num_cut_rows_sd - 1) * _cut_spacing_y_sd;

        int32_t ll_x_min = (-cut_width_total / 2) + _original_offset_x_sd;
        int32_t ll_y_min = (-cut_height_total / 2) + _original_offset_y_sd;
        for (int32_t i = 0; i < _num_cut_rows_sd; ++i) {
            for (int32_t j = 0; j < _num_cut_cols_sd; j++) {
                /// if pattern exist, cut shape must obey the pattern rule
                if (nullptr != obj->get_patttern() && !obj->is_pattern_cut_exist(i, j)) {
                    continue;
                }
                int32_t ll_x = ll_x_min + j * (_cut_size_x_sd + _cut_spacing_x_sd);
                int32_t ll_y = ll_y_min + i * (_cut_size_y_sd + _cut_spacing_y_sd);
                int32_t ur_x = ll_x + _cut_size_x_sd;
                int32_t ur_y = ll_y + _cut_size_y_sd;
                obj->add_cut_rect(ll_x, ll_y, ur_x, ur_y);
            }
        }
        obj->set_cut_bouding_rect(ll_x_min, ll_y_min, ll_x_min + cut_width_total, ll_y_min + cut_height_total);

        return true;
    } // fromShadow

public: // shadow data members
    std::string _rule_name_sd;
    int32_t _cut_size_x_sd = 0;
    int32_t _cut_size_y_sd = 0;
    int32_t _cut_spacing_x_sd = 0;
    int32_t _cut_spacing_y_sd = 0;
    int32_t _enclosure_bottom_x_sd = 0;
    int32_t _enclosure_bottom_y_sd = 0;
    int32_t _enclosure_top_x_sd = 0;
    int32_t _enclosure_top_y_sd = 0;
    int32_t _num_cut_rows_sd = 0;
    int32_t _num_cut_cols_sd = 0;
    int32_t _original_offset_x_sd = 0;
    int32_t _original_offset_y_sd = 0;
    int32_t _offset_bottom_x_sd = 0;
    int32_t _offset_bottom_y_sd = 0;
    int32_t _offset_top_x_sd = 0;
    int32_t _offset_top_y_sd = 0;

public: // name string to get layer and pattern instance in ieda
    std::string _layer_bottom_name_sd;
    std::string _layer_cut_name_sd;
    std::string _layer_top_name_sd;
    std::string _pattern_name_sd;
}; // edadb::Shadow<idb::IdbViaMasterGenerate> 



template<>
class Shadow<idb::IdbViaMaster> {
public:
    Shadow <idb::IdbViaMaster>(void) = default;
    ~Shadow<idb::IdbViaMaster>(void) { fixed_layer_shape_list_sd.clear(); }

public:
    bool toShadow(idb::IdbViaMaster* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }

        _name_sd = obj->get_name();
        _type_sd = obj->get_type();

        if (obj->is_generate()) {
            if (!_master_generate_sd.toShadow(obj->get_master_generate())) {
                return false;
            }
        } else if (obj->is_fix()) {
            for (idb::IdbViaMasterFixed* &fixed : obj->get_master_fixed_list()) {
                if (fixed == nullptr || fixed->get_layer_shape() == nullptr) {
                    return false;
                }
                fixed_layer_shape_list_sd.emplace_back(fixed->get_layer_shape());
            }
        }
        return true;
    } // toShadow

    bool fromShadow(idb::IdbViaMaster* obj, uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }

        obj->set_name(_name_sd);
        obj->set_type(_type_sd);

        if (obj->is_generate()) {
            if (!_master_generate_sd.fromShadow(obj->get_master_generate())) {
                return false;
            }
            // Both EDADB restore phases reach this branch on the same target object.
            // IdbViaMaster::set_via_shape() uses IdbLayerShape::add_rect(), so it
            // appends rather than replaces bottom, cut, and top geometry. Clear all
            // derived shapes before recomputing them to make every fromShadow()
            // application equivalent to the single call in DefRead::parse_via().
            // iRT later merges duplicate routing enclosures but preserves duplicate
            // cut obstacles; downstream merging must not be used to hide stale iDB state.
            obj->get_bottom_layer_shape()->clear();
            obj->get_cut_layer_shape()->clear();
            obj->get_top_layer_shape()->clear();
            obj->set_via_shape();
            return true;
        }

        auto& fixed_list = obj->get_master_fixed_list();
        if (!fixed_list.empty()) {
            return false;
        }

        // Fixed via
        int32_t min_x = INT_MAX;
        int32_t min_y = INT_MAX;
        int32_t max_x = INT_MIN;
        int32_t max_y = INT_MIN;
 
        for (idb::IdbLayerShape* &fixed_layer_shape : fixed_layer_shape_list_sd) {
            idb::IdbLayer* layer = fixed_layer_shape->get_layer();
            if (layer == nullptr) {
                std::cerr << "edadb::Shadow<idb::IdbViaMaster>::fromShadow error: fixed via layer is nullptr" << std::endl;
                return false;
            }

            const std::string layer_name = layer->get_name();
            idb::IdbViaMasterFixed* master_fixed = obj->add_fixed(layer_name);
            master_fixed->set_layer(layer);

            std::vector<idb::IdbRect*>& rect_list = fixed_layer_shape->get_rect_list();
            for (idb::IdbRect* rect : rect_list) {
                int32_t ll_x = rect->get_low_x();
                int32_t ll_y = rect->get_low_y();
                int32_t ur_x = rect->get_high_x();
                int32_t ur_y = rect->get_high_y();
                master_fixed->add_rect(ll_x, ll_y, ur_x, ur_y);

                if (layer->get_type() == idb::IdbLayerType::kLayerCut) {
                    min_x = std::min(min_x, ll_x);
                    min_y = std::min(min_y, ll_y);
                    max_x = std::max(max_x, ur_x);
                    max_y = std::max(max_y, ur_y);
                }
            }
        } // for 

        if (!fixed_layer_shape_list_sd.empty()) {
            obj->set_cut_rect(min_x, min_y, max_x, max_y);
            obj->set_via_shape();
        }

        return true;
    } // fromShadow

public:
    std::string _name_sd;
    idb::IdbViaMaster::IdbViaMasterType _type_sd;

    // Since we always need to use Shadow<idb::IdbViaMaster> to omit idb::IdbLayerShape,
    // we directly use Shadow<idb::IdbViaMasterGenerate> instead of IdbViaMasterGenerate
    Shadow<idb::IdbViaMasterGenerate> _master_generate_sd;

    // class IdbViaMaster member: 
    //      vector<IdbViaMasterFixed*> _master_fixed_list;
    // Direct use the IdbLayerShape to avoid define extra table for IdbViaMasterFixed,
    // since IdbViaMasterFixed only contains one member: IdbLayerShape* _layer_shape;
    std::vector< idb::IdbLayerShape* > fixed_layer_shape_list_sd;
}; // IdbViaMaster

} // namespace edadb
