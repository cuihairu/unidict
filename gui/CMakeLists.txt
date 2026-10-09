# gui/CMakeLists.txt

# Create the executable for our GUI application.
# WIN32：GUI 子系统（Qt6 自动提供 WinMain 入口）——否则 Windows 下双击
# 弹 terminal 控制台窗口（BUGS.md BUG-001）；其他平台此属性无效果
add_executable(unidict_gui WIN32
    main.cpp
    # 剪贴板取词监控（纯 QObject，qmlui 侧同源复用，无 QML 依赖）
    ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.cpp
    # 全局热键（native 层 Windows 实现，其余平台 stub，无 QML 依赖）
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.cpp
    # 开机自启（native 层 Windows 注册表 Run 键，其余平台 stub，无 QML 依赖）
    ${CMAKE_SOURCE_DIR}/qmlui/startup_launcher.cpp
    # 发音练习 M1 录音基建（纯逻辑 pcm_util.h 可进 std 测试，其余不进）
    pcm_util.h
    audio_recorder.cpp
    audio_recorder.h
    pcm_playback.cpp
    pcm_playback.h
    waveform_widget.cpp
    waveform_widget.h
    pronunciation_panel.cpp
    pronunciation_panel.h
)

target_include_directories(unidict_gui PRIVATE ${CMAKE_SOURCE_DIR}/qmlui)

# macOS 产出 .app bundle（CMake 自动生成默认 Info.plist）：macdeployqt
# 依赖 bundle 结构收 frameworks，裸二进制在别的机器跑不起来——
# daily-build 的 macOS 打包假设 .app 存在；其他平台此属性无效果
if(APPLE)
    set_target_properties(unidict_gui PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# 品牌图标（与 qmlui 同源资产 assets/icons/）
if(WIN32)
    target_sources(unidict_gui PRIVATE unidict.rc)
endif()
if(APPLE)
    set_target_properties(unidict_gui PROPERTIES MACOSX_BUNDLE_ICON_FILE unidict.icns)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/assets/icons/unidict.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    target_sources(unidict_gui PRIVATE ${CMAKE_SOURCE_DIR}/assets/icons/unidict.icns)
endif()

# Link the GUI application against our core library and Qt's GUI modules.
target_link_libraries(unidict_gui PRIVATE
    unidict_core_qt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::TextToSpeech
)

# 发音评分（M4）：UNIDICT_BUILD_PRON 才链 onnxruntime 适配器并编译
# 评分路径（UNIDICT_GUI_PRON 门控）；OFF 时面板保持 M2 无评分跟读。
# 同 cli-std 的 UNIDICT_HAVE_PRON 纪律：OFF 时"功能不存在"而不是链接失败
if(UNIDICT_BUILD_PRON)
    target_compile_definitions(unidict_gui PRIVATE UNIDICT_GUI_PRON=1)
    target_link_libraries(unidict_gui PRIVATE unidict_pron Qt6::Concurrent Qt6::Network)
    target_include_directories(unidict_gui PRIVATE
        "${CMAKE_SOURCE_DIR}/adapters/pron")
    # M10 模型资产自举下载（面板上「下载发音模型」按钮 + 进度/取消）。
    # 决策全在 core/std pron_model_source_std，这里只有 QNetwork 传输与
    # 文件写盘。同 cli-std 的纪律：OFF 时这个功能整体不存在
    target_sources(unidict_gui PRIVATE
        model_downloader.cpp
        model_downloader.h
    )
endif()

# BUG-010 gui 端真点审计（与 qmlui unidict_ui_click_audit 同口径同门）：
# 可点元素逐个 QTest::mouseClick 真点，断言可观测状态差；模态入口用
# 提前排的 QTimer 在 exec 事件循环里代填/关闭。offscreen 保证无托盘、
# 与 CI 一致。fixture 复用 qmlui click_audit（例句/词组/近义联想/全文全有数据）
option(UNIDICT_BUILD_UI_SANDBOX "Build offscreen UI screenshot sandbox" OFF)
if(UNIDICT_BUILD_UI_SANDBOX AND NOT ANDROID AND NOT IOS)
    # click_audit_test.cpp 直接 include main.cpp（UNIDICT_GUI_AUDIT_NO_MAIN
    # 挡掉原 main），不再单独编 main.cpp
    add_executable(unidict_gui_click_audit
        click_audit_test.cpp
        ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.cpp
        ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.cpp
        ${CMAKE_SOURCE_DIR}/qmlui/startup_launcher.cpp
        audio_recorder.cpp
        audio_recorder.h
        pcm_playback.cpp
        pcm_playback.h
        waveform_widget.cpp
        waveform_widget.h
        pronunciation_panel.cpp
        pronunciation_panel.h
    )
    target_include_directories(unidict_gui_click_audit
        PRIVATE ${CMAKE_SOURCE_DIR}/qmlui)
    target_link_libraries(unidict_gui_click_audit PRIVATE
        unidict_core_qt
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::TextToSpeech
        Qt6::Test
    )

    add_test(NAME gui_click_audit COMMAND unidict_gui_click_audit)
    set_tests_properties(gui_click_audit PROPERTIES
        TIMEOUT 300
        ENVIRONMENT "UNIDICT_DICTS=${CMAKE_SOURCE_DIR}/qmlui/dev/fixtures/click_audit_dict.json;QT_QPA_PLATFORM=offscreen"
        )

    # gui 端离屏截图沙盒（留档用，不进 ctest——探活由 gui_click_audit 覆盖）
    add_executable(unidict_gui_sandbox
        dev/ui_sandbox.cpp
        ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.cpp
        ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.cpp
        ${CMAKE_SOURCE_DIR}/qmlui/startup_launcher.cpp
        audio_recorder.cpp
        audio_recorder.h
        pcm_playback.cpp
        pcm_playback.h
        waveform_widget.cpp
        waveform_widget.h
        pronunciation_panel.cpp
        pronunciation_panel.h
    )
    target_include_directories(unidict_gui_sandbox
        PRIVATE ${CMAKE_SOURCE_DIR}/qmlui)
    target_link_libraries(unidict_gui_sandbox PRIVATE
        unidict_core_qt
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::TextToSpeech
        Qt6::Test
    )
endif()
