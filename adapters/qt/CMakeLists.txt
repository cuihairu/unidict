# Qt adapter wrappers that bridge Qt types to std-only core modules.

add_library(unidict_index_qt STATIC index_engine_qt.cpp)
target_link_libraries(unidict_index_qt PUBLIC unidict_index_std Qt6::Core)
target_include_directories(unidict_index_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core")

add_library(unidict_utils_qt STATIC path_utils_qt.cpp)
target_link_libraries(unidict_utils_qt PUBLIC Qt6::Core unidict_std_core)
target_include_directories(unidict_utils_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core")

add_library(unidict_plugins_qt STATIC plugin_manager_qt.cpp json_parser_qt.cpp stardict_parser_qt.cpp mdict_parser_qt.cpp)
target_link_libraries(unidict_plugins_qt PUBLIC Qt6::Core)
target_include_directories(unidict_plugins_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core" "${CMAKE_SOURCE_DIR}/core/std")
target_link_libraries(unidict_plugins_qt PUBLIC unidict_std_core)

add_library(unidict_data_qt STATIC data_store_qt.cpp)
target_link_libraries(unidict_data_qt PUBLIC Qt6::Core unidict_std_core)
target_include_directories(unidict_data_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core")
