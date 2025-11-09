enable_testing()

find_package(ZLIB REQUIRED)

if(UNIDICT_BUILD_QT_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Test)

  add_executable(test_index_engine index_engine_test.cpp)
  target_link_libraries(test_index_engine PRIVATE unidict_core Qt6::Core Qt6::Test)
  add_test(NAME test_index_engine COMMAND test_index_engine)

  add_executable(test_data_store data_store_test.cpp)
  target_link_libraries(test_data_store PRIVATE unidict_core Qt6::Core Qt6::Test)
  add_test(NAME test_data_store COMMAND test_data_store)

  add_executable(test_lookup_service lookup_service_test.cpp)
  target_link_libraries(test_lookup_service PRIVATE unidict_core Qt6::Core Qt6::Test)
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
