# Qt adapter wrappers that bridge Qt types to std-only core modules.

add_library(unidict_index_qt STATIC index_engine_qt.cpp)
target_link_libraries(unidict_index_qt PUBLIC unidict_index_std Qt6::Core)
target_include_directories(unidict_index_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core" "${CMAKE_SOURCE_DIR}")

add_library(unidict_utils_qt STATIC path_utils_qt.cpp)
target_link_libraries(unidict_utils_qt PUBLIC Qt6::Core unidict_std_core)
target_include_directories(unidict_utils_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core" "${CMAKE_SOURCE_DIR}")

add_library(unidict_plugins_qt STATIC plugin_manager_qt.cpp json_parser_qt.cpp stardict_parser_qt.cpp mdict_parser_qt.cpp)
target_link_libraries(unidict_plugins_qt PUBLIC Qt6::Core)
target_include_directories(unidict_plugins_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core" "${CMAKE_SOURCE_DIR}/core/std" "${CMAKE_SOURCE_DIR}")
target_link_libraries(unidict_plugins_qt PUBLIC unidict_std_core)

add_library(unidict_data_qt STATIC data_store_qt.cpp)
target_link_libraries(unidict_data_qt PUBLIC Qt6::Core unidict_std_core)
target_include_directories(unidict_data_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core" "${CMAKE_SOURCE_DIR}")

# Legacy Qt core target: builds Qt-based APIs from core/ under the adapters layer.
if(UNIDICT_BUILD_QT_CORE)
  find_package(ZLIB REQUIRED)
  add_library(unidict_core_qt STATIC
    ${CMAKE_SOURCE_DIR}/core/unidict_core.cpp
    ${CMAKE_SOURCE_DIR}/core/unidict_core.h
    ${CMAKE_SOURCE_DIR}/core/stardict_parser.cpp
    ${CMAKE_SOURCE_DIR}/core/stardict_parser.h
    ${CMAKE_SOURCE_DIR}/core/mdict_parser.cpp
    ${CMAKE_SOURCE_DIR}/core/mdict_parser.h
    ${CMAKE_SOURCE_DIR}/core/index_engine.cpp
    ${CMAKE_SOURCE_DIR}/core/index_engine.h
    ${CMAKE_SOURCE_DIR}/core/data_store.cpp
    ${CMAKE_SOURCE_DIR}/core/data_store.h
    ${CMAKE_SOURCE_DIR}/core/json_parser.cpp
    ${CMAKE_SOURCE_DIR}/core/json_parser.h
    ${CMAKE_SOURCE_DIR}/core/plugin_manager.cpp
    ${CMAKE_SOURCE_DIR}/core/plugin_manager.h
    ${CMAKE_SOURCE_DIR}/core/lookup_service.cpp
    ${CMAKE_SOURCE_DIR}/core/lookup_service.h
    ${CMAKE_SOURCE_DIR}/core/path_utils.cpp
    ${CMAKE_SOURCE_DIR}/core/path_utils.h
  )
  target_link_libraries(unidict_core_qt PRIVATE Qt6::Core unidict_index_qt unidict_utils_qt unidict_plugins_qt unidict_data_qt ZLIB::ZLIB)
  target_include_directories(unidict_core_qt PUBLIC "${CMAKE_SOURCE_DIR}/core" "${CMAKE_SOURCE_DIR}/adapters/qt" "${CMAKE_SOURCE_DIR}")
endif()
