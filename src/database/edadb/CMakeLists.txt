file(GLOB_RECURSE EDADB_SOURCES
    "*.cpp"
)
if(BUILD_STATIC_LIB) 
    add_library(db_edadb STATIC ${EDADB_SOURCES})
else()
    add_library(db_edadb SHARED ${EDADB_SOURCES})
endif()

target_include_directories(db_edadb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)