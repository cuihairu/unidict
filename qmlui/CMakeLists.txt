# qmlui/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# 如果不是作为子目录被包含，则设置项目
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(UnidictQML VERSION 1.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick Qml QuickControls2 TextToSpeech Network Multimedia QuickDialogs2)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

# 移动平台检测
set(IS_MOBILE FALSE)
if(ANDROID OR IOS)
    set(IS_MOBILE TRUE)
endif()

# 可执行文件创建
if(ANDROID)
    # Android应用
    qt_add_executable(unidict_qml
        main.cpp
        lookup_adapter.cpp
        lookup_adapter.h
        clipboard_monitor.cpp
        clipboard_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        mobile_utils.cpp
        mobile_utils.h
        learning_manager.cpp
        learning_manager.h
        ../adapters/qt/sync_service_qt.cpp
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
        clipboard_monitor.cpp
        clipboard_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        mobile_utils.cpp
        mobile_utils.h
        learning_manager.cpp
        learning_manager.h
        ../adapters/qt/sync_service_qt.cpp
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
        clipboard_monitor.cpp
        clipboard_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        mobile_utils.cpp
        mobile_utils.h
        learning_manager.cpp
        learning_manager.h
        ../adapters/qt/sync_service_qt.cpp
        resources.qrc
    )
endif()

# macOS 产 .app bundle（与 gui 同理）：macdeployqt 依赖 bundle 结构收
# frameworks，每日构建的 macOS 打包按 .app 假设（BUGS.md BUG-002：
# 分发 GUI 主程序换成 QML 应用）；其他平台此属性无效果
if(APPLE)
    set_target_properties(unidict_qml PROPERTIES MACOSX_BUNDLE TRUE)
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

# 离屏 UI 截图沙盒（默认 OFF，先例 falcon dev/ui_sandbox.cpp）：无显示环境
# 下按 视图×亮暗×双尺寸 截图，做 BEFORE/AFTER 留档与背景像素命中校验。
option(UNIDICT_BUILD_UI_SANDBOX "Build offscreen UI screenshot sandbox" OFF)
if(UNIDICT_BUILD_UI_SANDBOX AND NOT ANDROID AND NOT IOS)
    add_executable(unidict_ui_sandbox
        dev/ui_sandbox.cpp
        theme.cpp
        theme.h
        lookup_adapter.cpp
        lookup_adapter.h
        clipboard_monitor.cpp
        clipboard_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        mobile_utils.cpp
        mobile_utils.h
        learning_manager.cpp
        learning_manager.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        ../adapters/qt/sync_service_qt.cpp
        resources.qrc
    )
    target_link_libraries(unidict_ui_sandbox PRIVATE
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
# QNetworkAccessManager + QMediaPlayer，桌面与 Android 同一份 C++ 代码）
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
