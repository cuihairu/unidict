# Qt adapter wrappers that bridge Qt types to std-only core modules.
add_library(unidict_utils_qt STATIC path_utils_qt.cpp)
target_link_libraries(unidict_utils_qt PUBLIC Qt6::Core unidict_std_core)
target_include_directories(unidict_utils_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core" "${CMAKE_SOURCE_DIR}")

add_library(unidict_data_qt STATIC data_store_qt.cpp)
# Qt6::Gui：笔记导出 PDF 面（QPdfWriter/QTextDocument）
target_link_libraries(unidict_data_qt PUBLIC Qt6::Core Qt6::Gui unidict_std_core)
target_include_directories(unidict_data_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core" "${CMAKE_SOURCE_DIR}")

# Qt 门面目标：legacy 词典链（DictionaryManager 单例 + 四套 Qt 解析器 +
# 插件注册表）已退役，只余 path_utils 一个转发门面（tests/lookup_adapter_test
# 的 Qt 面）+ unidict_core.h 词本条目 DTO。DataStore 双跳门面已拆（TD-106）：
# 消费方直连 unidict_data_qt 的 DataStoreQt，符号经 static 库传递链接到达。
if(UNIDICT_BUILD_QT_CORE)
  add_library(unidict_core_qt STATIC
    ${CMAKE_SOURCE_DIR}/core/unidict_core.h
    ${CMAKE_SOURCE_DIR}/core/path_utils.cpp
    ${CMAKE_SOURCE_DIR}/core/path_utils.h
  )
  target_link_libraries(unidict_core_qt PRIVATE Qt6::Core unidict_utils_qt unidict_data_qt unidict_std_core)
  target_include_directories(unidict_core_qt PUBLIC "${CMAKE_SOURCE_DIR}/core" "${CMAKE_SOURCE_DIR}/adapters/qt" "${CMAKE_SOURCE_DIR}")
endif()

# Full-text manager (Qt adapter over std-only fulltext)
add_library(unidict_fulltext_qt STATIC fulltext_manager_qt.cpp)
target_link_libraries(unidict_fulltext_qt PUBLIC Qt6::Core unidict_std_core)
target_include_directories(unidict_fulltext_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/core" "${CMAKE_SOURCE_DIR}")
