file(GLOB_RECURSE EDADB_SOURCES
    "*.cpp"
)
if(BUILD_STATIC_LIB) 
    add_library(db_edadb STATIC ${EDADB_SOURCES})
else()
    add_library(db_edadb SHARED ${EDADB_SOURCES})
endif()

target_include_directories(db_edadb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} # src/database/edadb
    ${PROJECT_SOURCE_DIR}/src # src
    ${HOME_DATABASE}/edadb_idb # src/database/edadb_idb
    ${HOME_THIRDPARTY}/edadb/include 

    ## edadb_idb_helper
    ${HOME_DATABASE}/manager/builder
    ${HOME_DATABASE}/manager/builder/def_builder
    ${HOME_DATABASE}/manager/builder/lef_builder
    ${HOME_DATABASE}/manager/builder/verilog_builder
    ${HOME_DATABASE}/manager/builder/json_builder
    ${HOME_DATABASE}/manager/builder/gds_builder
    ${HOME_DATABASE}/data/design
    ${HOME_DATABASE}/data/design/db_design
    ${HOME_DATABASE}/data/design/db_layout
    ${HOME_DATABASE}/manager/service/def_service
    ${HOME_DATABASE}/manager/service/lef_service
) 