enable_testing()

find_package(ZLIB REQUIRED)

# std 测试（*_std_test.cpp）用 assert 断言：Release 构建定义 NDEBUG 时
# 断言整体空转，进程 return 0 照样 passed——CI 三个工作流全是 Release，
# 等于这批测试从未真跑过（2026-09 写 epub/zip 测试时发现）。目录级剥掉
# NDEBUG；Qt 测试的 QVERIFY 不依赖 assert，不受影响。
add_compile_options($<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
                    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)

if(UNIDICT_BUILD_QT_TESTS AND UNIDICT_BUILD_QT_CORE)
  find_package(Qt6 REQUIRED COMPONENTS Test)

  add_executable(test_index_engine index_engine_test.cpp)
  target_link_libraries(test_index_engine PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_index_engine COMMAND test_index_engine)

  # core_lookup_tests 曾在 tests/CMakeLists.txt 大重构时被删掉 target，
  # 900 行状态持久化/StarDict/历史用例从此没进过构建——2026-09 挂回。
  add_executable(unidict_core_tests core_lookup_tests.cpp)
  target_link_libraries(unidict_core_tests PRIVATE unidict_core_qt Qt6::Core Qt6::Test ZLIB::ZLIB)
  add_test(NAME unidict_core_tests COMMAND unidict_core_tests)

  add_executable(test_data_store data_store_test.cpp)
  target_link_libraries(test_data_store PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_data_store COMMAND test_data_store)

  add_executable(test_lookup_service lookup_service_test.cpp)
  target_link_libraries(test_lookup_service PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_lookup_service COMMAND test_lookup_service)

  add_executable(test_dictionary_manager_fulltext dictionary_manager_fulltext_test.cpp)
  target_link_libraries(test_dictionary_manager_fulltext PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_dictionary_manager_fulltext COMMAND test_dictionary_manager_fulltext)

  add_executable(test_dictionary_manager_prefix dictionary_manager_prefix_test.cpp)
  target_link_libraries(test_dictionary_manager_prefix PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_dictionary_manager_prefix COMMAND test_dictionary_manager_prefix)

  add_executable(test_global_hotkeys
    global_hotkeys_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.h
  )
  target_include_directories(test_global_hotkeys PRIVATE "${CMAKE_SOURCE_DIR}/qmlui")
  target_link_libraries(test_global_hotkeys PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
  add_test(NAME test_global_hotkeys COMMAND test_global_hotkeys)

  add_executable(test_startup_launcher
    startup_launcher_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/startup_launcher.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/startup_launcher.h
  )
  target_include_directories(test_startup_launcher PRIVATE "${CMAKE_SOURCE_DIR}/qmlui")
  target_link_libraries(test_startup_launcher PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
  add_test(NAME test_startup_launcher COMMAND test_startup_launcher)

  add_executable(test_lookup_adapter
    lookup_adapter_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/lookup_adapter.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/lookup_adapter.h
    ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.h
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.h
  )
  target_include_directories(test_lookup_adapter PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/core"
    "${CMAKE_SOURCE_DIR}/adapters/qt"
    "${CMAKE_SOURCE_DIR}/qmlui"
  )
  target_link_libraries(test_lookup_adapter PRIVATE
    unidict_core_qt
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::TextToSpeech
  )
  add_test(NAME test_lookup_adapter COMMAND test_lookup_adapter)
  # 无头 Linux/macOS 用 offscreen 平台插件；Windows 不强制——CI runner 有
  # 交互桌面，Qt 6.10 的 offscreen 插件路径在 Windows 上静默崩溃（无输出
  # 退出），走默认 qwindows 主路径更稳
  if(NOT WIN32)
    set_tests_properties(test_lookup_adapter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()
endif()

# Std-only index engine test (no Qt dependency)
add_executable(test_index_engine_std
    index_engine_std_test.cpp
)

target_link_libraries(test_index_engine_std PRIVATE
    unidict_index_std
)

add_test(NAME test_index_engine_std COMMAND test_index_engine_std)

add_executable(test_data_store_std
    data_store_std_test.cpp
)

target_link_libraries(test_data_store_std PRIVATE unidict_std_core)

add_test(NAME test_data_store_std COMMAND test_data_store_std)

add_executable(test_json_parser_std
    json_parser_std_test.cpp
)

target_link_libraries(test_json_parser_std PRIVATE unidict_std_core)

add_test(NAME test_json_parser_std COMMAND test_json_parser_std)

add_executable(test_stardict_std
    stardict_std_test.cpp
)

target_link_libraries(test_stardict_std PRIVATE unidict_std_core)

add_test(NAME test_stardict_std COMMAND test_stardict_std)

add_executable(test_stardict_sts_std
    stardict_sts_std_test.cpp
)

target_link_libraries(test_stardict_sts_std PRIVATE unidict_std_core)

add_test(NAME test_stardict_sts_std COMMAND test_stardict_sts_std)

add_executable(test_text_norm_std
    text_norm_std_test.cpp
)

target_link_libraries(test_text_norm_std PRIVATE unidict_index_std)

add_test(NAME test_text_norm_std COMMAND test_text_norm_std)

add_executable(test_mdict_std
    mdict_std_test.cpp
)

target_link_libraries(test_mdict_std PRIVATE unidict_std_core)

add_test(NAME test_mdict_std COMMAND test_mdict_std)

add_executable(test_epub_zip_std
    epub_zip_std_test.cpp
)

target_link_libraries(test_epub_zip_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_epub_zip_std COMMAND test_epub_zip_std)

add_executable(test_stardict_dz_std
    stardict_dz_std_test.cpp
)

target_link_libraries(test_stardict_dz_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_stardict_dz_std COMMAND test_stardict_dz_std)

add_executable(test_stardict_parser_full_std
    stardict_parser_std_full_test.cpp
)

target_link_libraries(test_stardict_parser_full_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_stardict_parser_full_std COMMAND test_stardict_parser_full_std)

add_executable(test_mdict_zlib_std
    mdict_zlib_std_test.cpp
)

target_link_libraries(test_mdict_zlib_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_zlib_std COMMAND test_mdict_zlib_std)

add_executable(test_mdict_utf16_header_std
    mdict_utf16_header_std_test.cpp
)

target_link_libraries(test_mdict_utf16_header_std PRIVATE unidict_std_core)

add_test(NAME test_mdict_utf16_header_std COMMAND test_mdict_utf16_header_std)

add_executable(test_mdict_encrypted_header_std
    mdict_encrypted_header_std_test.cpp
)
target_link_libraries(test_mdict_encrypted_header_std PRIVATE unidict_std_core)
add_test(NAME test_mdict_encrypted_header_std COMMAND test_mdict_encrypted_header_std)

add_executable(test_mdict_password_env_std
    mdict_password_env_std_test.cpp
)
target_link_libraries(test_mdict_password_env_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_password_env_std COMMAND test_mdict_password_env_std)

add_executable(test_mdict_invalid_kidx_oob_std
    mdict_invalid_kidx_oob_std_test.cpp
)
target_link_libraries(test_mdict_invalid_kidx_oob_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_invalid_kidx_oob_std COMMAND test_mdict_invalid_kidx_oob_std)

add_executable(test_mdict_mdxk_ulen_cap_std
    mdict_mdxk_ulen_cap_std_test.cpp
)
target_link_libraries(test_mdict_mdxk_ulen_cap_std PRIVATE unidict_std_core)
add_test(NAME test_mdict_mdxk_ulen_cap_std COMMAND test_mdict_mdxk_ulen_cap_std)

add_executable(test_mdict_tsv_std
    mdict_tsv_std_test.cpp
)

target_link_libraries(test_mdict_tsv_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_tsv_std COMMAND test_mdict_tsv_std)

add_executable(test_mdict_simplekv_std
    mdict_simplekv_std_test.cpp
)

target_link_libraries(test_mdict_simplekv_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_simplekv_std COMMAND test_mdict_simplekv_std)

add_executable(test_mdict_mdd_resources_std
    mdict_mdd_resources_std_test.cpp
)

target_link_libraries(test_mdict_mdd_resources_std PRIVATE unidict_std_core)

add_test(NAME test_mdict_mdd_resources_std COMMAND test_mdict_mdd_resources_std)

add_executable(test_mdict_parser_cover_std
    mdict_parser_std_cover_test.cpp
)
target_link_libraries(test_mdict_parser_cover_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_parser_cover_std COMMAND test_mdict_parser_cover_std)

add_executable(test_mdict_kidx_std
    mdict_kidx_std_test.cpp
)

target_link_libraries(test_mdict_kidx_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_kidx_std COMMAND test_mdict_kidx_std)

add_executable(test_mdict_keyb_std
    mdict_keyb_std_test.cpp
)

target_link_libraries(test_mdict_keyb_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_keyb_std COMMAND test_mdict_keyb_std)

add_executable(test_mdict_keyb_empty_std
    mdict_keyb_empty_std_test.cpp
)
target_link_libraries(test_mdict_keyb_empty_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_keyb_empty_std COMMAND test_mdict_keyb_empty_std)

add_executable(test_mdict_kbix_std
    mdict_kbix_std_test.cpp
)

target_link_libraries(test_mdict_kbix_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_kbix_std COMMAND test_mdict_kbix_std)

add_executable(test_mdict_kbix_multi_std
    mdict_kbix_multi_std_test.cpp
)

target_link_libraries(test_mdict_kbix_multi_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_kbix_multi_std COMMAND test_mdict_kbix_multi_std)

add_executable(test_mdict_kbix_count_mismatch_std
    mdict_kbix_count_mismatch_std_test.cpp
)
target_link_libraries(test_mdict_kbix_count_mismatch_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_kbix_count_mismatch_std COMMAND test_mdict_kbix_count_mismatch_std)

add_executable(test_mdict_mdxkr_std
    mdict_mdxkr_std_test.cpp
)

target_link_libraries(test_mdict_mdxkr_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_mdxkr_std COMMAND test_mdict_mdxkr_std)

add_executable(test_mdict_mdxk_composite_edge_std
    mdict_mdxk_composite_edge_std_test.cpp
)
target_link_libraries(test_mdict_mdxk_composite_edge_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_mdxk_composite_edge_std COMMAND test_mdict_mdxk_composite_edge_std)

add_executable(test_mdict_mdxk_oob_offsets_std
    mdict_mdxk_oob_offsets_std_test.cpp
)
target_link_libraries(test_mdict_mdxk_oob_offsets_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_mdxk_oob_offsets_std COMMAND test_mdict_mdxk_oob_offsets_std)

add_executable(test_mdict_mdxk_garbage_tail_std
    mdict_mdxk_garbage_tail_std_test.cpp
)
target_link_libraries(test_mdict_mdxk_garbage_tail_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_mdxk_garbage_tail_std COMMAND test_mdict_mdxk_garbage_tail_std)

add_executable(test_mdict_realheur_std
    mdict_realheur_std_test.cpp
)

target_link_libraries(test_mdict_realheur_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_realheur_std COMMAND test_mdict_realheur_std)

add_executable(test_dsl_std
    dsl_parser_std_test.cpp
)

target_link_libraries(test_dsl_std PRIVATE unidict_std_core)

add_test(NAME test_dsl_std COMMAND test_dsl_std)

add_executable(test_dictionary_manager_std
    dictionary_manager_std_test.cpp
)

# 测试直接用 gzopen 造 .dict.dz，需要显式链 zlib（MSVC 无传递包含）
target_link_libraries(test_dictionary_manager_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_dictionary_manager_std COMMAND test_dictionary_manager_std)

add_executable(test_index_persistence_std
    index_persistence_std_test.cpp
)

target_link_libraries(test_index_persistence_std PRIVATE unidict_std_core)

add_test(NAME test_index_persistence_std COMMAND test_index_persistence_std)

if(UNIDICT_BUILD_STD_CLI)
  add_test(NAME test_cli_std_smoke
    COMMAND $<TARGET_FILE:unidict_cli_std> hello)
  # Ensure examples dict is available via env var
  set_tests_properties(test_cli_std_smoke PROPERTIES
    ENVIRONMENT "UNIDICT_DICTS=${CMAKE_SOURCE_DIR}/examples/dict.json")

  add_executable(test_cli_std_mdict_password
    cli_std_mdict_password_test.cpp
  )
  target_link_libraries(test_cli_std_mdict_password PRIVATE ZLIB::ZLIB)
  add_test(NAME test_cli_std_mdict_password
    COMMAND test_cli_std_mdict_password $<TARGET_FILE:unidict_cli_std>)
endif()

add_executable(test_cache_prune_std
    cache_prune_std_test.cpp
)

target_link_libraries(test_cache_prune_std PRIVATE unidict_std_core)

add_test(NAME test_cache_prune_std COMMAND test_cache_prune_std)

add_executable(test_fulltext_index_std
    fulltext_index_std_test.cpp
)
target_link_libraries(test_fulltext_index_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_index_std COMMAND test_fulltext_index_std)

add_executable(test_fulltext_index_std_cover
    fulltext_index_std_cover_test.cpp
)
target_link_libraries(test_fulltext_index_std_cover PRIVATE unidict_std_core)
add_test(NAME test_fulltext_index_std_cover COMMAND test_fulltext_index_std_cover)

add_executable(test_fulltext_lazy_decode_stats_std
    fulltext_lazy_decode_stats_std_test.cpp
)
target_link_libraries(test_fulltext_lazy_decode_stats_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_lazy_decode_stats_std COMMAND test_fulltext_lazy_decode_stats_std)

add_executable(test_index_engine_std_ops
    index_engine_std_ops_test.cpp
)
target_link_libraries(test_index_engine_std_ops PRIVATE unidict_index_std)
add_test(NAME test_index_engine_std_ops COMMAND test_index_engine_std_ops)

add_executable(test_data_store_std_csv_escape
    data_store_std_csv_escape_test.cpp
)
target_link_libraries(test_data_store_std_csv_escape PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_csv_escape COMMAND test_data_store_std_csv_escape)

add_executable(test_data_store_std_remove
    data_store_std_remove_test.cpp
)
target_link_libraries(test_data_store_std_remove PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_remove COMMAND test_data_store_std_remove)

add_executable(test_data_store_std_tags
    data_store_std_tags_test.cpp
)
target_link_libraries(test_data_store_std_tags PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_tags COMMAND test_data_store_std_tags)

add_executable(test_data_store_std_notes
    data_store_std_notes_test.cpp
)
target_link_libraries(test_data_store_std_notes PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_notes COMMAND test_data_store_std_notes)

add_executable(test_csv_parser_std
    csv_parser_std_test.cpp
)

target_link_libraries(test_csv_parser_std PRIVATE unidict_std_core)

add_test(NAME test_csv_parser_std COMMAND test_csv_parser_std)

add_executable(test_csv_parser_std_cover
    csv_parser_std_cover_test.cpp
)
target_link_libraries(test_csv_parser_std_cover PRIVATE unidict_std_core)
add_test(NAME test_csv_parser_std_cover COMMAND test_csv_parser_std_cover)

add_executable(test_data_store_std_cover
    data_store_std_cover_test.cpp
)
target_link_libraries(test_data_store_std_cover PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_cover COMMAND test_data_store_std_cover)

add_executable(test_mdict_decryptor_std
    mdict_decryptor_std_test.cpp
)

target_link_libraries(test_mdict_decryptor_std PRIVATE unidict_std_core)

add_test(NAME test_mdict_decryptor_std COMMAND test_mdict_decryptor_std)

add_executable(test_mdict_decryptor_std_cover
    mdict_decryptor_std_cover_test.cpp
)
target_link_libraries(test_mdict_decryptor_std_cover PRIVATE unidict_std_core)
add_test(NAME test_mdict_decryptor_std_cover COMMAND test_mdict_decryptor_std_cover)

add_executable(test_json_parser_std_edge
    json_parser_std_edge_test.cpp
)
target_link_libraries(test_json_parser_std_edge PRIVATE unidict_std_core)
add_test(NAME test_json_parser_std_edge COMMAND test_json_parser_std_edge)

add_executable(test_dictionary_manager_std_remove
    dictionary_manager_std_remove_test.cpp
)
target_link_libraries(test_dictionary_manager_std_remove PRIVATE unidict_std_core)
add_test(NAME test_dictionary_manager_std_remove COMMAND test_dictionary_manager_std_remove)

if(UNIDICT_BUILD_STD_CLI)
  add_test(NAME test_cli_std_fulltext_smoke
    COMMAND $<TARGET_FILE:unidict_cli_std> --mode fulltext greet)
  set_tests_properties(test_cli_std_fulltext_smoke PROPERTIES
    ENVIRONMENT "UNIDICT_DICTS=${CMAKE_SOURCE_DIR}/examples/dict.json")
endif()

add_executable(test_index_engine_std_edge
    index_engine_std_edge_test.cpp
)
target_link_libraries(test_index_engine_std_edge PRIVATE unidict_index_std)
add_test(NAME test_index_engine_std_edge COMMAND test_index_engine_std_edge)

add_executable(test_stardict_malformed_std
    stardict_malformed_std_test.cpp
)
target_link_libraries(test_stardict_malformed_std PRIVATE unidict_std_core)
add_test(NAME test_stardict_malformed_std COMMAND test_stardict_malformed_std)

add_executable(test_fulltext_persistence_std
    fulltext_persistence_std_test.cpp
)
target_link_libraries(test_fulltext_persistence_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_persistence_std COMMAND test_fulltext_persistence_std)

add_executable(test_fulltext_persistence_mismatch_std
    fulltext_persistence_mismatch_std_test.cpp
)
target_link_libraries(test_fulltext_persistence_mismatch_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_persistence_mismatch_std COMMAND test_fulltext_persistence_mismatch_std)

add_executable(test_fulltext_persistence_pathchange_std
    fulltext_persistence_pathchange_std_test.cpp
)
target_link_libraries(test_fulltext_persistence_pathchange_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_persistence_pathchange_std COMMAND test_fulltext_persistence_pathchange_std)

add_executable(test_fulltext_persistence_stardict_companion_change_std
    fulltext_persistence_stardict_companion_change_std_test.cpp
)
target_link_libraries(test_fulltext_persistence_stardict_companion_change_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_persistence_stardict_companion_change_std COMMAND test_fulltext_persistence_stardict_companion_change_std)

add_executable(test_fulltext_udft3_format_std
    fulltext_udft3_format_std_test.cpp
)
target_link_libraries(test_fulltext_udft3_format_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_udft3_format_std COMMAND test_fulltext_udft3_format_std)

add_executable(test_fulltext_std
    fulltext_std_test.cpp
)

target_link_libraries(test_fulltext_std PRIVATE unidict_std_core)

add_test(NAME test_fulltext_std COMMAND test_fulltext_std)

add_executable(test_fulltext_parallel_build_std
    fulltext_parallel_build_std_test.cpp
)
target_link_libraries(test_fulltext_parallel_build_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_parallel_build_std COMMAND test_fulltext_parallel_build_std)

add_executable(test_fulltext_udft12_load_std
    fulltext_udft12_load_std_test.cpp
)
target_link_libraries(test_fulltext_udft12_load_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_udft12_load_std COMMAND test_fulltext_udft12_load_std)

add_executable(test_fulltext_tie_break_std
    fulltext_tie_break_std_test.cpp
)
target_link_libraries(test_fulltext_tie_break_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_tie_break_std COMMAND test_fulltext_tie_break_std)

add_executable(test_path_utils_env_days_std
    path_utils_env_days_std_test.cpp
)
target_link_libraries(test_path_utils_env_days_std PRIVATE unidict_std_core)
add_test(NAME test_path_utils_env_days_std COMMAND test_path_utils_env_days_std)

add_executable(test_dictionary_manager_std_meta
    dictionary_manager_std_meta_std_test.cpp
)
target_link_libraries(test_dictionary_manager_std_meta PRIVATE unidict_std_core)
add_test(NAME test_dictionary_manager_std_meta COMMAND test_dictionary_manager_std_meta)

# P0 Professional Dictionary Feature Tests
add_executable(test_html_renderer_std
    html_renderer_std_test.cpp
)
target_link_libraries(test_html_renderer_std PRIVATE unidict_std_core)
add_test(NAME test_html_renderer_std COMMAND test_html_renderer_std)

add_executable(test_html_renderer_std_edge
    html_renderer_std_edge_test.cpp
)
target_link_libraries(test_html_renderer_std_edge PRIVATE unidict_std_core)
add_test(NAME test_html_renderer_std_edge COMMAND test_html_renderer_std_edge)

add_executable(test_html_renderer_std_cover
    html_renderer_std_cover_test.cpp
)
target_link_libraries(test_html_renderer_std_cover PRIVATE unidict_std_core)
add_test(NAME test_html_renderer_std_cover COMMAND test_html_renderer_std_cover)

add_executable(test_mdd_resource_std
    mdd_resource_std_test.cpp
)
target_link_libraries(test_mdd_resource_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdd_resource_std COMMAND test_mdd_resource_std)

add_executable(test_cross_reference_std
    cross_reference_std_test.cpp
)
target_link_libraries(test_cross_reference_std PRIVATE unidict_std_core)
add_test(NAME test_cross_reference_std COMMAND test_cross_reference_std)

add_executable(test_aggregate_lookup_std
    aggregate_lookup_std_test.cpp
)
target_link_libraries(test_aggregate_lookup_std PRIVATE unidict_std_core)
add_test(NAME test_aggregate_lookup_std COMMAND test_aggregate_lookup_std)

add_executable(test_mdd_resource_std_full
    mdd_resource_std_full_test.cpp
)

target_link_libraries(test_mdd_resource_std_full PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdd_resource_std_full COMMAND test_mdd_resource_std_full)

add_executable(test_mdd_resource_std_cover
    mdd_resource_std_cover_test.cpp
)
target_link_libraries(test_mdd_resource_std_cover PRIVATE unidict_std_core)
add_test(NAME test_mdd_resource_std_cover COMMAND test_mdd_resource_std_cover)

# 发音练习 M1 纯逻辑（波形降采样 + WAV 封装）：header-only，不链任何库。
# pcm_util.h 2026-09-25 下沉到 core/std/（gui/pcm_util.h 只是转发头），
# include 路径给 core/ 与 gui/ 两个兜底
add_executable(test_pcm_util_std
    pcm_util_std_test.cpp
)
target_include_directories(test_pcm_util_std PRIVATE
    ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/gui)
add_test(NAME test_pcm_util_std COMMAND test_pcm_util_std)

# 发音评分内核（M3a）：ARPAbet 表 / NW 对齐 / 词分聚合，链 unidict_std_core
add_executable(test_pronunciation_score_std
    pronunciation_score_std_test.cpp
)
target_link_libraries(test_pronunciation_score_std PRIVATE unidict_std_core)
add_test(NAME test_pronunciation_score_std COMMAND test_pronunciation_score_std)

# M3b 发音评分纯逻辑：espeak 映射 + CTC 强制对齐 + GOP
add_executable(test_espeak_arpabet_std
    espeak_arpabet_std_test.cpp
)
target_link_libraries(test_espeak_arpabet_std PRIVATE unidict_std_core)
add_test(NAME test_espeak_arpabet_std COMMAND test_espeak_arpabet_std)

add_executable(test_ctc_gop_std
    ctc_gop_std_test.cpp
)
target_link_libraries(test_ctc_gop_std PRIVATE unidict_std_core)
add_test(NAME test_ctc_gop_std COMMAND test_ctc_gop_std)

# M3b 适配器前置纯逻辑：模型词表解析 / 波形预处理 / fp16+log_softmax
add_executable(test_pron_vocab_std
    pron_vocab_std_test.cpp
)
target_link_libraries(test_pron_vocab_std PRIVATE unidict_std_core)
add_test(NAME test_pron_vocab_std COMMAND test_pron_vocab_std)

add_executable(test_pron_wave_std
    pron_wave_std_test.cpp
)
target_link_libraries(test_pron_wave_std PRIVATE unidict_std_core)
add_test(NAME test_pron_wave_std COMMAND test_pron_wave_std)

# ctc_logits_std.h header-only：测试自带 TU，链 std_core 只为 include 路径
add_executable(test_ctc_logits_std
    ctc_logits_std_test.cpp
)
target_link_libraries(test_ctc_logits_std PRIVATE unidict_std_core)
add_test(NAME test_ctc_logits_std COMMAND test_ctc_logits_std)

# M4 变体容忍表：GOP 取 max(主键, 变体)
add_executable(test_pron_variants_std
    pron_variants_std_test.cpp
)
target_link_libraries(test_pron_variants_std PRIVATE unidict_std_core)
add_test(NAME test_pron_variants_std COMMAND test_pron_variants_std)

# M3b onnxruntime 推理壳：1.5KB Constant 假模型全链路（加载→推理→
# 张量解析→对齐→GOP）。整个 target 挂 UNIDICT_BUILD_PRON——OFF 时
# ORT 完全不参与构建（docs/pronunciation-plan.md 分层纪律）
if(UNIDICT_BUILD_PRON)
    add_executable(test_onnx_pron_scorer
        onnx_pron_scorer_test.cpp
    )
    target_link_libraries(test_onnx_pron_scorer PRIVATE unidict_pron)
    target_compile_definitions(test_onnx_pron_scorer PRIVATE
        UNIDICT_PRON_FAKE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME test_onnx_pron_scorer COMMAND test_onnx_pron_scorer)
endif()
