add_executable(unidict_core_tests
    core_lookup_tests.cpp
)

target_link_libraries(unidict_core_tests PRIVATE
    unidict_core
    Qt6::Core
    Qt6::Test
)

add_test(NAME unidict_core_tests COMMAND unidict_core_tests)
