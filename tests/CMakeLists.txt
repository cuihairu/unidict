enable_testing()

find_package(ZLIB REQUIRED)

if(UNIDICT_BUILD_QT_TESTS AND UNIDICT_BUILD_QT_CORE)
  find_package(Qt6 REQUIRED COMPONENTS Test)

  add_executable(test_index_engine index_engine_test.cpp)
  target_link_libraries(test_index_engine PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_index_engine COMMAND test_index_engine)

  add_executable(test_data_store data_store_test.cpp)
  target_link_libraries(test_data_store PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_data_store COMMAND test_data_store)

  add_executable(test_lookup_service lookup_service_test.cpp)
  target_link_libraries(test_lookup_service PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_lookup_service COMMAND test_lookup_service)
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

add_executable(test_mdict_std
    mdict_std_test.cpp
)

target_link_libraries(test_mdict_std PRIVATE unidict_std_core)

add_test(NAME test_mdict_std COMMAND test_mdict_std)

add_executable(test_stardict_dz_std
    stardict_dz_std_test.cpp
)

target_link_libraries(test_stardict_dz_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_stardict_dz_std COMMAND test_stardict_dz_std)

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

add_executable(test_mdict_mdxkr_std
    mdict_mdxkr_std_test.cpp
)

target_link_libraries(test_mdict_mdxkr_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_mdxkr_std COMMAND test_mdict_mdxkr_std)

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

target_link_libraries(test_dictionary_manager_std PRIVATE unidict_std_core)

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
