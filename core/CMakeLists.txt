# core/CMakeLists.txt

# Create a static library for our core logic.
if(UNIDICT_BUILD_QT_CORE)
add_library(unidict_core STATIC
    unidict_core.cpp
    unidict_core.h
    stardict_parser.cpp
    stardict_parser.h
    mdict_parser.cpp
    mdict_parser.h
    index_engine.cpp
    index_engine.h
    data_store.cpp
    data_store.h
    json_parser.cpp
    json_parser.h
    plugin_manager.cpp
    plugin_manager.h
    lookup_service.cpp
    lookup_service.h
    path_utils.cpp
    path_utils.h
)

# The core library needs access to Qt's Core module.
target_link_libraries(unidict_core PRIVATE Qt6::Core unidict_index_qt unidict_utils_qt unidict_plugins_qt)
target_link_libraries(unidict_core PRIVATE unidict_data_qt)

# Add zlib for MDict decompression
find_package(ZLIB REQUIRED)
target_link_libraries(unidict_core PRIVATE ZLIB::ZLIB)

# This makes sure that other targets that link against this library
# can find its header files.
target_include_directories(unidict_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

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
