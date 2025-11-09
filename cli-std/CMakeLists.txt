add_executable(unidict_cli_std
    main.cpp
)

target_link_libraries(unidict_cli_std PRIVATE
    unidict_index_std
    unidict_std_core
)

target_include_directories(unidict_cli_std PRIVATE
    "${CMAKE_SOURCE_DIR}/core"
    "${CMAKE_SOURCE_DIR}/core/std"
)

