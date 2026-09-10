cmake_minimum_required(VERSION 3.16)
project(sqlite_def_baseline LANGUAGES CXX)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "Configure with -DCMAKE_BUILD_TYPE=Release")
endif()
find_package(SQLite3 REQUIRED)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(IEDA_RELEASE_BUILD "${REPO_ROOT}/build-release" CACHE PATH "Verified iEDA Release dependencies")
set(NATIVE_INCLUDES "")
foreach(relative IN ITEMS
    src/database/manager/builder/def_builder/CMakeFiles/def_builder.dir/flags.make
    src/database/manager/builder/lef_builder/CMakeFiles/lef_builder.dir/flags.make
    src/database/data/design/CMakeFiles/idb.dir/flags.make)
    file(READ "${IEDA_RELEASE_BUILD}/${relative}" flags)
    if(NOT flags MATCHES "CXX_FLAGS = [^\n]*-O3")
        message(FATAL_ERROR "Not an O3 native dependency: ${relative}")
    endif()
    string(REGEX MATCH "CXX_INCLUDES = ([^\n]*)" unused "${flags}")
    string(REPLACE "-isystem " "-I" include_flags "${CMAKE_MATCH_1}")
    separate_arguments(includes UNIX_COMMAND "${include_flags}")
    list(APPEND NATIVE_INCLUDES ${includes})
endforeach()
set(NATIVE_LIBRARIES "")
foreach(library IN ITEMS lef_builder def_service lef_service idb geometry_db solver_geometry_boost str log
    absl_strings absl_strings_internal absl_base absl_spinlock_wait absl_int128 absl_throw_delegate absl_raw_logging_internal absl_log_severity)
    list(APPEND NATIVE_LIBRARIES "${IEDA_RELEASE_BUILD}/lib/lib${library}.a")
endforeach()
add_executable(stream_benchmark stream_benchmark.cpp
    "${REPO_ROOT}/src/database/manager/builder/def_builder/def_read.cpp"
    "${REPO_ROOT}/src/database/manager/builder/def_builder/def_write.cpp"
    "${REPO_ROOT}/src/database/manager/builder/def_builder/def_read_edadb.cpp"
    "${REPO_ROOT}/src/database/manager/builder/def_builder/def_write_edadb.cpp"
    "${REPO_ROOT}/src/database/edadb/idb/edadb_idb_init.cpp"
    "${REPO_ROOT}/src/database/edadb/idb/edadb_idb_helper.cpp")
target_compile_features(stream_benchmark PRIVATE cxx_std_20)
target_compile_options(stream_benchmark PRIVATE ${NATIVE_INCLUDES} -O3 -march=native -mtune=native -fopenmp -ffunction-sections -fdata-sections)
target_include_directories(stream_benchmark PRIVATE "${REPO_ROOT}/src" "${REPO_ROOT}/src/database/edadb/idb" "${REPO_ROOT}/src/database/edadb/core/include")
target_compile_definitions(stream_benchmark PRIVATE EDADB_ENABLE=1 EDADB_OUTPUT_DEBUG=0 _EDADB_DEBUG_TRACE_SQL_STMT_=0 BOOST_ALLOW_DEPRECATED_HEADERS BOOST_BIND_GLOBAL_PLACEHOLDERS GLOG_USE_GLOG_EXPORT)
target_link_options(stream_benchmark PRIVATE -fopenmp -flto=40 -fno-ipa-icf -Wl,--gc-sections)
target_link_libraries(stream_benchmark PRIVATE -Wl,--start-group ${NATIVE_LIBRARIES}
    "${REPO_ROOT}/src/third_party/lefdef/lef/lib/liblef.a"
    "${REPO_ROOT}/src/third_party/lefdef/def/lib/libdef.a"
    "${REPO_ROOT}/src/third_party/lefdef/def/lib/libdefzlib.a"
    -Wl,--end-group SQLite::SQLite3 z glog gflags unwind pthread dl)
