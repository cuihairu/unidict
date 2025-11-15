# core/CMakeLists.txt

# Legacy Qt core target moved to adapters/qt (target: unidict_core_qt).

# Experimental: Qt-free index engine (std-only). This is a separate target for now
# and is intended to replace the Qt-based IndexEngine gradually.
add_library(unidict_index_std STATIC
    std/index_engine_std.cpp
)

target_include_directories(unidict_index_std PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/std"
)

# Std-only core components (parsers, utils, datastore)
find_package(ZLIB REQUIRED)
add_library(unidict_std_core STATIC
    std/path_utils_std.cpp
    std/data_store_std.cpp
    std/json_parser_std.cpp
    std/stardict_parser_std.cpp
    std/dictionary_manager_std.cpp
    std/dictionary_manager_std.h
    std/fulltext_index_std.cpp
    std/fulltext_index_std.h
    std/mdict_parser_std.cpp
    std/dsl_parser_std.cpp
    std/csv_parser_std.cpp
)

target_include_directories(unidict_std_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/std"
)

target_link_libraries(unidict_std_core PRIVATE ZLIB::ZLIB)
target_link_libraries(unidict_std_core PUBLIC unidict_index_std)
