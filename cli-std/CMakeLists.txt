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

# Linux 系统包安装规则（deb/rpm，cmake/packaging_linux.cmake 驱动）：
# std CLI 零 Qt 依赖，包内只带 CLI + 示例词典 + 文档，不引入 Qt 依赖
# 地狱（GUI 分发走每日构建的 unidict_qml 包，见 BUGS.md BUG-002）
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS unidict_cli_std
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT cli
    )
    install(FILES "${CMAKE_SOURCE_DIR}/examples/dict.json"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/unidict
        COMPONENT cli
    )
    install(FILES "${CMAKE_SOURCE_DIR}/README.md" "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/unidict/doc
        COMPONENT cli
    )
endif()

# 发音评分子命令（--pron-score）：链接 onnxruntime 适配器。OFF 时子命令
# 报"未构建"而不是链接失败
if(UNIDICT_BUILD_PRON)
    target_link_libraries(unidict_cli_std PRIVATE unidict_pron)
    target_compile_definitions(unidict_cli_std PRIVATE UNIDICT_HAVE_PRON=1)
    target_include_directories(unidict_cli_std PRIVATE
        "${CMAKE_SOURCE_DIR}/adapters/pron")
endif()

