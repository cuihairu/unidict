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

# 发音评分子命令（--pron-score）：链接 onnxruntime 适配器。OFF 时子命令
# 报"未构建"而不是链接失败
if(UNIDICT_BUILD_PRON)
    target_link_libraries(unidict_cli_std PRIVATE unidict_pron)
    target_compile_definitions(unidict_cli_std PRIVATE UNIDICT_HAVE_PRON=1)
    target_include_directories(unidict_cli_std PRIVATE
        "${CMAKE_SOURCE_DIR}/adapters/pron")
endif()

