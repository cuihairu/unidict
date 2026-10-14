# qmlui/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# 如果不是作为子目录被包含，则设置项目
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(UnidictQML VERSION 1.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick Qml QuickControls2 TextToSpeech Network Multimedia QuickDialogs2)
    # Widgets 顶层已全量 find；独立构建此行补托盘依赖（TrayController 的
    # QSystemTrayIcon/QMenu 是 Widgets 系）
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    # 独立配置时顶层 CMakeLists 不参与：OBJCXX 在此首启（信息模块在本
    # 目录加载，规则变量就地生效）。集成构建走顶层启用，此处不执行。
    if(APPLE AND NOT IOS)
        enable_language(OBJCXX)
    endif()
endif()

# 移动平台检测
set(IS_MOBILE FALSE)
if(ANDROID OR IOS)
    set(IS_MOBILE TRUE)
endif()

# F10 桌面全局热键平台面（global_hotkeys.cpp 的 native 层）：Linux 找
# X11 头/库（UNIDICT_HAS_X11；Wayland 会话注册时运行时明示失败，不假装）；
# macOS 编 Carbon 桥 global_hotkeys_mac.mm（OBJCXX 由顶层启用——规则
# 变量是目录作用域，子目录各自 enable 第二次不生效，见顶层注释）。
# 移动分支不参与（global_hotkeys.cpp 编译为 stub）。
if(UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(X11 QUIET)
endif()

# 编 global_hotkeys.cpp 的桌面 target 统一走本函数补平台依赖
# （ApplicationServices 同在此挂：F13b mac_selection_reader 的 AX 无障碍
# 接口在 ApplicationServices 框架内，桌面三 target 共用本函数）
function(unidict_hotkey_platform tgt)
    if(APPLE AND NOT IOS)
        target_sources(${tgt} PRIVATE global_hotkeys_mac.mm)
        target_link_libraries(${tgt} PRIVATE "-framework Carbon")
        target_link_libraries(${tgt} PRIVATE "-framework ApplicationServices")
    endif()
    if(UNIX AND NOT APPLE AND NOT ANDROID AND X11_FOUND)
        target_compile_definitions(${tgt} PRIVATE UNIDICT_HAS_X11)
        target_include_directories(${tgt} PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(${tgt} PRIVATE ${X11_LIBRARIES})
    endif()
endfunction()

# 可执行文件创建
if(ANDROID)
    # Android应用
    qt_add_executable(unidict_qml
        main.cpp
        lookup_adapter.cpp
        lookup_adapter.h
        platform_adapter.cpp
        win_selection_reader.cpp
        win_selection_reader.h
        mac_selection_reader.cpp
        mac_selection_reader.h
        voice_adapter.cpp
        vocab_store_adapter.cpp
        platform_adapter.h
        voice_adapter.h
        vocab_store_adapter.h
        clipboard_monitor.cpp
        clipboard_monitor.h
        selection_monitor.cpp
        selection_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        mobile_utils.cpp
        mobile_utils.h
        ../adapters/qt/sync_service_qt.cpp
        ../adapters/qt/sync_manager_qt.cpp
        ../adapters/qt/sync_http_transport_qt.cpp
        ../adapters/qt/sync_http_transport_qt.h
        ../server/sync_relay/cpp/relay_http.cpp
        ../server/sync_relay/cpp/relay_http.h
        ../server/sync_relay/cpp/relay_routes.h
        ../server/sync_relay/cpp/lan_host_std.cpp
        ../server/sync_relay/cpp/lan_host_std.h
        resources.qrc
    )
    set_target_properties(unidict_qml PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mobile/android
    )
    target_sources(unidict_qml PRIVATE theme.cpp theme.h)
elseif(IOS)
    # iOS应用
    qt_add_executable(unidict_qml
        main.cpp
        lookup_adapter.cpp
        lookup_adapter.h
        platform_adapter.cpp
        win_selection_reader.cpp
        win_selection_reader.h
        mac_selection_reader.cpp
        mac_selection_reader.h
        voice_adapter.cpp
        vocab_store_adapter.cpp
        platform_adapter.h
        voice_adapter.h
        vocab_store_adapter.h
        clipboard_monitor.cpp
        clipboard_monitor.h
        selection_monitor.cpp
        selection_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        mobile_utils.cpp
        mobile_utils.h
        ../adapters/qt/sync_service_qt.cpp
        ../adapters/qt/sync_manager_qt.cpp
        ../adapters/qt/sync_http_transport_qt.cpp
        ../adapters/qt/sync_http_transport_qt.h
        ../server/sync_relay/cpp/relay_http.cpp
        ../server/sync_relay/cpp/relay_http.h
        ../server/sync_relay/cpp/relay_routes.h
        ../server/sync_relay/cpp/lan_host_std.cpp
        ../server/sync_relay/cpp/lan_host_std.h
        resources.qrc
    )
    set_target_properties(unidict_qml PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/mobile/ios/Info.plist
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "Your Team ID"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.yourcompany.unidict"
    )
    target_sources(unidict_qml PRIVATE theme.cpp theme.h)
else()
    # 桌面应用。WIN32：GUI 子系统，Windows 下双击不弹控制台（BUGS.md
    # BUG-001，Qt6 自动提供 WinMain 入口）；其他平台无效果
    add_executable(unidict_qml WIN32
        main.cpp
        lookup_adapter.cpp
        lookup_adapter.h
        platform_adapter.cpp
        win_selection_reader.cpp
        win_selection_reader.h
        mac_selection_reader.cpp
        mac_selection_reader.h
        voice_adapter.cpp
        vocab_store_adapter.cpp
        platform_adapter.h
        voice_adapter.h
        vocab_store_adapter.h
        clipboard_monitor.cpp
        clipboard_monitor.h
        selection_monitor.cpp
        selection_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        startup_launcher.cpp
        startup_launcher.h
        tray_controller.cpp
        tray_controller.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        mobile_utils.cpp
        mobile_utils.h
        ../adapters/qt/sync_service_qt.cpp
        ../adapters/qt/sync_manager_qt.cpp
        ../adapters/qt/sync_http_transport_qt.cpp
        ../adapters/qt/sync_http_transport_qt.h
        ../server/sync_relay/cpp/relay_http.cpp
        ../server/sync_relay/cpp/relay_http.h
        ../server/sync_relay/cpp/relay_routes.h
        ../server/sync_relay/cpp/lan_host_std.cpp
        ../server/sync_relay/cpp/lan_host_std.h
        resources.qrc
    )
endif()

# 桌面全局热键平台面（unidict_qml 三平台分支在上层 if/elseif/else 已建）
unidict_hotkey_platform(unidict_qml)

# macOS 产 .app bundle（与 gui 同理）：macdeployqt 依赖 bundle 结构收
# frameworks，每日构建的 macOS 打包按 .app 假设（BUGS.md BUG-002：
# 分发 GUI 主程序换成 QML 应用）；其他平台此属性无效果
if(APPLE)
    set_target_properties(unidict_qml PROPERTIES MACOSX_BUNDLE TRUE)
    # bundle 身份：CMake 默认 Info.plist 在未设对应属性时把 CFBundle*
    # 填成空串（BUGS.md BUG-007，nightly 产物实测），Launch Services 无法
    # 归一应用身份，后续签名/公证也要用。
    #
    # 关键：模板 Modules/MacOSXBundleInfo.plist.in 里 CFBundleIdentifier
    # 代入的是 **MACOSX_BUNDLE_GUI_IDENTIFIER**（CMake 4.2 仍是这个旧
    # 变量名），**不是** MACOSX_BUNDLE_IDENTIFIER —— 后者只喂 CPack 的
    # DMG 标识，设在 target 上对生成的 Info.plist 毫无作用（首版修复就栽
    # 在这里：CFBundleName 生效了、Identifier 仍为空）。取值与
    # CPACK_BUNDLE_IDENTIFIER（cmake/BuildOptions.cmake）保持一致。
    # 版本串同属「必填键不能空」：否则 Finder 版本位空白、签名公证校验不过。
    set_target_properties(unidict_qml PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.unidict.unidict"
        MACOSX_BUNDLE_BUNDLE_NAME "Unidict"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${UNIDICT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${UNIDICT_VERSION}")
endif()

# 品牌图标（二进制图标接线）：Windows .rc 编进 PE 资源段；macOS icns
# 落 bundle 的 Contents/Resources（CMake 默认 Info.plist 由
# MACOSX_BUNDLE_ICON_FILE 注入 CFBundleIconFile）。SVG 源在 docs/logo.svg，
# 多尺寸资产经 ImageMagick/手工 ICNS 容器打包（assets/icons/）
if(WIN32)
    target_sources(unidict_qml PRIVATE unidict.rc)
endif()
if(APPLE)
    set_target_properties(unidict_qml PROPERTIES MACOSX_BUNDLE_ICON_FILE unidict.icns)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/assets/icons/unidict.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    target_sources(unidict_qml PRIVATE ${CMAKE_SOURCE_DIR}/assets/icons/unidict.icns)
endif()

# install 面（Linux）：桌面集成三件套之一的本体——顶层 CMakeLists 落
# .desktop + hicolor 图标，这里补 bin/unidict_qml，make install 后
# Exec=unidict_qml 直接可用；macOS install 落 .app（日常分发仍走
# macdeployqt staging 口径）。INSTALL_RPATH 清空：Qt6 link 线带来的
# build rpath 与 install 前缀组合会触发 CMake RPATH_CHANGE 校验错误
# （Qt target 的 $ORIGIN 变换出尾冒号路径），安装后 rpath 由分发方/
# 系统包管理决定
set_target_properties(unidict_qml PROPERTIES INSTALL_RPATH "")
install(TARGETS unidict_qml
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin)

# Crashpad 崩溃采集（docs/crash_report_plan.md §3/§5/§6）：桌面壳接入
# 面。选项在 Android/iOS 下被 CMakeDependentOption 强制 OFF，走不到这。
# 源码只在 unidict_crash 库里编一份（不 target_sources 进壳——MSVC 侧
# NOMINMAX 等修复按源文件收口在库那侧，双编会让壳侧副本吃不到）
if(UNIDICT_BUILD_CRASH)
    target_compile_definitions(unidict_qml PRIVATE UNIDICT_ENABLE_CRASH)
    target_link_libraries(unidict_qml PRIVATE unidict_crash)
    # 栈还原需要符号（§6）：非 MSVC 给接入目标补 -g（产物内嵌调试信息；
    # 分发口径另走 scripts/symbols_split.sh 分离）
    if(NOT MSVC)
        target_compile_options(unidict_qml PRIVATE -g)
    endif()
    # handler 随产物（§5）：构建期拷到主程序同目录，安装期进 bin——
    # 独立进程，主进程崩溃不影响写 dump
    add_dependencies(unidict_qml crashpad_handler)
    add_custom_command(TARGET unidict_qml POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:crashpad_handler>
            $<TARGET_FILE_DIR:unidict_qml>
        COMMENT "Copying crashpad_handler alongside unidict_qml")
    install(TARGETS crashpad_handler RUNTIME DESTINATION bin)
endif()

# 离屏 UI 截图沙盒（默认 OFF，先例 falcon dev/ui_sandbox.cpp）：无显示环境
# 下按 视图×亮暗×双尺寸 截图，做 BEFORE/AFTER 留档与背景像素命中校验。
option(UNIDICT_BUILD_UI_SANDBOX "Build offscreen UI screenshot sandbox" OFF)
if(UNIDICT_BUILD_UI_SANDBOX AND NOT ANDROID AND NOT IOS)
    add_executable(unidict_ui_sandbox
        dev/ui_sandbox.cpp
        startup_launcher.cpp
        startup_launcher.h
        tray_controller.cpp
        tray_controller.h
        theme.cpp
        theme.h
        lookup_adapter.cpp
        lookup_adapter.h
        platform_adapter.cpp
        win_selection_reader.cpp
        win_selection_reader.h
        mac_selection_reader.cpp
        mac_selection_reader.h
        voice_adapter.cpp
        vocab_store_adapter.cpp
        platform_adapter.h
        voice_adapter.h
        vocab_store_adapter.h
        clipboard_monitor.cpp
        clipboard_monitor.h
        selection_monitor.cpp
        selection_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        mobile_utils.cpp
        mobile_utils.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        ../adapters/qt/sync_service_qt.cpp
        ../adapters/qt/sync_manager_qt.cpp
        ../adapters/qt/sync_http_transport_qt.cpp
        ../adapters/qt/sync_http_transport_qt.h
        ../server/sync_relay/cpp/relay_http.cpp
        ../server/sync_relay/cpp/relay_http.h
        ../server/sync_relay/cpp/relay_routes.h
        ../server/sync_relay/cpp/lan_host_std.cpp
        ../server/sync_relay/cpp/lan_host_std.h
        resources.qrc
    )
    target_link_libraries(unidict_ui_sandbox PRIVATE
        unidict_core_qt
        unidict_fulltext_qt
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::TextToSpeech
        Qt6::Network
        Qt6::Multimedia
    )
    target_include_directories(unidict_ui_sandbox PRIVATE
        "${CMAKE_SOURCE_DIR}/server/sync_relay/cpp"
    )
    unidict_hotkey_platform(unidict_ui_sandbox)

    # TD-131：离屏截图探活并入 ctest（CI 的 qt job 开 UNIDICT_BUILD_UI_SANDBOX
    # 即生效）。退出码语义：0 = 20 张截图全部落盘；非 0 = 结构断言失败
    # （QML 装载失败 / grabWindow 空图 / 保存失败）。截图为结构留档（非像素
    # 断言）——亮暗主题可区分性由 CI 侧灰度均值抽检兜底。
    # 预算口径：macOS 共享 runner 实测 271.5 秒（run 38392000631），300
    # 预算余量仅 10%，同码 300.06 秒被杀（run 38411130592）——APPLE 侧
    # 单独放宽到 900 留 3× 余量；其余平台维持 300。
    if(APPLE)
        set(UI_TEST_TIMEOUT 900)
    else()
        set(UI_TEST_TIMEOUT 300)
    endif()
    add_test(NAME ui_sandbox_screenshots
        COMMAND unidict_ui_sandbox
            "${CMAKE_BINARY_DIR}/ui_sandbox_ci"
        )
    set_tests_properties(ui_sandbox_screenshots PROPERTIES
        TIMEOUT ${UI_TEST_TIMEOUT}
        # 原型基线用真词典落图（docs/ui「实现即原型」口径）：内置
        # CC-CEDICT 12.5 万词，截图即用户开箱效果；fixture 词见
        # ui_sandbox.cpp（你好）
        ENVIRONMENT "UNIDICT_DICTS=${CMAKE_SOURCE_DIR}/dictionaries/ccedict-zh-en.json"
        )
endif()

# 离屏 UI 真点审计（BUG-010：可点元素全接线，点不动的不许留）。与截图
# 沙盒同门同开关：对每个可点元素发真实 QMouseEvent（press+release 进
# QQuickWindow 交付链），断言点击前后可观测状态差，逐项 PASS/FAIL 证据
# 落盘，任一 FAIL 退出码非 0。词典用 click_audit 专用 fixture——保证
# 例句/词组/近义联想/全文四个内容 tab 都有数据可点。
if(UNIDICT_BUILD_UI_SANDBOX AND NOT ANDROID AND NOT IOS)
    add_executable(unidict_ui_click_audit
        dev/ui_click_audit.cpp
        startup_launcher.cpp
        startup_launcher.h
        tray_controller.cpp
        tray_controller.h
        theme.cpp
        theme.h
        lookup_adapter.cpp
        lookup_adapter.h
        platform_adapter.cpp
        win_selection_reader.cpp
        win_selection_reader.h
        mac_selection_reader.cpp
        mac_selection_reader.h
        voice_adapter.cpp
        vocab_store_adapter.cpp
        platform_adapter.h
        voice_adapter.h
        vocab_store_adapter.h
        clipboard_monitor.cpp
        clipboard_monitor.h
        selection_monitor.cpp
        selection_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        mobile_utils.cpp
        mobile_utils.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        ../adapters/qt/sync_service_qt.cpp
        ../adapters/qt/sync_manager_qt.cpp
        ../adapters/qt/sync_http_transport_qt.cpp
        ../adapters/qt/sync_http_transport_qt.h
        ../server/sync_relay/cpp/relay_http.cpp
        ../server/sync_relay/cpp/relay_http.h
        ../server/sync_relay/cpp/relay_routes.h
        ../server/sync_relay/cpp/lan_host_std.cpp
        ../server/sync_relay/cpp/lan_host_std.h
        resources.qrc
    )
    target_link_libraries(unidict_ui_click_audit PRIVATE
        unidict_core_qt
        unidict_fulltext_qt
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::TextToSpeech
        Qt6::Network
        Qt6::Multimedia
    )
    target_include_directories(unidict_ui_click_audit PRIVATE
        "${CMAKE_SOURCE_DIR}/server/sync_relay/cpp"
    )
    unidict_hotkey_platform(unidict_ui_click_audit)

    add_test(NAME ui_click_audit
        COMMAND unidict_ui_click_audit
            "${CMAKE_BINARY_DIR}/ui_click_audit_ci"
        )
    # UNIDICT_DICTS 多路径分隔符与平台 PATH 惯例一致（lookup_adapter
    # splitEnvDictPaths 同口径：Windows ';'、POSIX ':'）
    if(WIN32)
        set(ENV_PATH_SEP ";")
    else()
        set(ENV_PATH_SEP ":")
    endif()
    set(AUDIT_DICTS
        "${CMAKE_CURRENT_SOURCE_DIR}/dev/fixtures/click_audit_dict.json${ENV_PATH_SEP}${CMAKE_CURRENT_SOURCE_DIR}/dev/fixtures/click_audit_dict2.json")
    if(WIN32)
        # CMake 把 ENVIRONMENT 值按 ';' 做列表切分——Windows 路径分隔符
        # 必须转义成 \; 才能原样进环境变量（否则 dict2 丢失、S22 五查全挂）
        string(REPLACE ";" "\\;" AUDIT_DICTS "${AUDIT_DICTS}")
    endif()
    set_tests_properties(ui_click_audit PROPERTIES
        TIMEOUT ${UI_TEST_TIMEOUT}
        # 两本 fixture：命中词典 chip 概览与多词典分组卡需要 >1 词典；
        # dict2 的 "help" 词头与 dict1 双双精确命中（S9 的 "hel" 探针依赖
        # 无精确命中的前缀层，dict2 不收 "hel"）
        ENVIRONMENT "UNIDICT_DICTS=${AUDIT_DICTS}"
        )
endif()

# 桌面目标也要带上主题单例（Android/iOS 分支已各自并入）
if(NOT ANDROID AND NOT IOS)
    target_sources(unidict_qml PRIVATE theme.cpp theme.h)
    # 随包词典落位（BUGS.md BUG-004）：main.cpp 兜底按 exe 同目录找
    # ccedict-zh-en.json / dict.json——本地构建同样双击即有词典可查；
    # 分发包由 daily-build 打包步骤从同一来源取文件
    # 用 CURRENT_SOURCE_DIR/.. 定位仓库根：主工程子目录与独立构建两态都对
    add_custom_command(TARGET unidict_qml POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../dictionaries/ccedict-zh-en.json"
            "$<TARGET_FILE_DIR:unidict_qml>/ccedict-zh-en.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../examples/dict.json"
            "$<TARGET_FILE_DIR:unidict_qml>/dict.json"
    )
endif()

# 平台特定的链接库（Network/Multimedia：lookup_adapter 的在线发音——
# QNetworkAccessManager + QMediaPlayer，桌面与 Android 同一份 C++ 代码；
# Widgets 仅桌面托盘 TrayController 用，移动分支不链）
if(NOT ANDROID AND NOT IOS)
    target_link_libraries(unidict_qml PRIVATE Qt6::Widgets)
endif()
target_link_libraries(unidict_qml PRIVATE
    unidict_core_qt
    unidict_fulltext_qt
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::TextToSpeech
    Qt6::Network
    Qt6::Multimedia
)

# Android特定配置
if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS AndroidExtras)
    target_link_libraries(unidict_qml PRIVATE Qt6::AndroidExtras)

    # Android权限和特性
    set_target_properties(unidict_qml PROPERTIES
        QT_ANDROID_MINIMUM_SDK_VERSION 23
        QT_ANDROID_TARGET_SDK_VERSION 31
    )
endif()

# iOS特定配置
if(IOS)
    target_link_libraries(unidict_qml PRIVATE
        "-framework Foundation"
        "-framework UIKit"
        "-framework AVFoundation"
    )
endif()

target_include_directories(unidict_qml PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/server/sync_relay/cpp"
)

# QML模块注册
if(IS_MOBILE)
    qt_add_qml_module(unidict_qml
        URI UniDict.Mobile
        VERSION 1.0
        QML_FILES
            mobile/common/ResponsiveLayout.qml
            mobile/common/MobileFileDialog.qml
            mobile/common/DefinitionContent.qml
    )
endif()
