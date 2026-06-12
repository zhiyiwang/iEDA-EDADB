/*
 * @file edadb_idb_helper.h
 * @brief This file contains helper functions for Idb and EDADB database operations.
 * @author Zhiyi Wang
 */

#pragma once

#include "def_service.h"
#include "IdbViaRule.h"

namespace idb::edadb_adapter {

class EdadbIdbHelper {
private:
    EdadbIdbHelper (void) = default;
    ~EdadbIdbHelper(void) = default;

public:
    // helper function to build def from edadb database
    static bool setIdbDefService(idb::IdbDefService* def_service) {
        if (def_service == nullptr) {
            std::cout << "EdabIdbHelper Error: setIdbDefService input def_service is nullptr";
            std::cout << std::endl;
            return false;
        }

        if (s_def_service != nullptr) {
            std::cout << "EdabIdbHelper Error: setIdbDefService s_def_service is already set";
            std::cout << std::endl;
            return false;
        }

        s_def_service = def_service;
        return true;
    }

    static idb::IdbDefService* getIdbDefService() {
        return s_def_service;
    }


    static idb::IdbViaRuleList* getIdbViaRuleList() {
        if (s_def_service == nullptr) {
            std::cout << "EdabIdbHelper Error: getIdbViaRuleList s_def_service is nullptr";
            std::cout << std::endl;
            assert(s_def_service != nullptr);
            return nullptr;
        }
        return s_def_service->get_layout()->get_via_rule_list();
    }

    static idb::IdbViaRuleGenerate* findIdbViaRuleGenerateByName(const std::string& rule_name) {
        idb::IdbViaRuleList* via_rule_list = getIdbViaRuleList();
        if (via_rule_list == nullptr) {
            std::cout << "EdabIdbHelper Error: findIdbViaRuleGenerateByName failed to get via_rule_list";
            std::cout << std::endl;
            return nullptr;
        }
        return via_rule_list->find_via_rule_generate(rule_name);
    }


    static idb::IdbLayers* getIdbLayers() {
        if (s_def_service == nullptr) {
            std::cout << "EdabIdbHelper Error: getIdbLayers s_def_service is nullptr";
            std::cout << std::endl;
            assert(s_def_service != nullptr);
            return nullptr;
        }
        return s_def_service->get_layout()->get_layers();
    }

    static idb::IdbLayer* findIdbLayerByName(const std::string& layer_name) {
        idb::IdbLayers* layers = getIdbLayers();
        if (layers == nullptr) {
            std::cout << "EdabIdbHelper Error: findIdbLayerByName failed to get layers";
            std::cout << std::endl;
            return nullptr;
        }
        return layers->find_layer(layer_name);
    }


public:
    // helper function to build def from edadb database
    static idb::IdbDefService* s_def_service;
}; // class EdadbIdbHelper


} // namespace idb::edadb_adapter
