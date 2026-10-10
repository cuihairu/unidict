cmake_minimum_required(VERSION 3.16)
project(Unidict VERSION 1.0 LANGUAGES CXX)

# 先定标准再 include BuildOptions：其 status 打印与选项快照才反映实际值
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 仓源码统一 UTF-8：MSVC 无此旗标按系统码页（CI runner=CP1252）读源，
# 非 ASCII 的 char16_t 字面量（u"牛津" 等）转出错误码元，窄字面量两侧
# 不对称时等值断言翻车（tests/mdict_utf16_header_std_test 实证）；
# /utf-8 同时定源与执行字符集，与 GCC 行为对齐
if(MSVC)
    add_compile_options(/utf-8)
endif()

include(CTest)
include(cmake/BuildOptions.cmake)

# Linux 系统包（deb/rpm）：std CLI 面向发行版包管理器的分发面，
# std 构建下从构建目录 cpack -G "DEB;RPM" 直接出包
if(UNIX AND NOT APPLE AND UNIDICT_BUILD_STD_CLI)
    include(cmake/packaging_linux.cmake)
endif()

# Enable Qt automoc/uic/rcc only when Qt-backed targets are part of the build.
set(_unidict_need_qt OFF)
if(UNIDICT_BUILD_QT_CORE OR UNIDICT_BUILD_ADAPTER_QT OR UNIDICT_BUILD_QT_APPS OR UNIDICT_BUILD_QT_TESTS)
    set(_unidict_need_qt ON)
endif()

if(_unidict_need_qt)
    # 基础组件：任何 Qt 目标都需要
    find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Test
    )

    # lookup_adapter（qmlui 与 Qt 测试共用）与发音练习面板（gui）使用 TextToSpeech
    if(UNIDICT_BUILD_QT_QMLUI OR UNIDICT_BUILD_QT_TESTS OR UNIDICT_BUILD_QT_GUI)
        find_package(Qt6 REQUIRED COMPONENTS TextToSpeech)
    endif()

    # lookup_adapter 的在线发音：QNetworkAccessManager 拉 dictionaryapi.dev、
    # QMediaPlayer 播音频（qmlui 与编译它的 Qt 测试都需要）
    if(UNIDICT_BUILD_QT_QMLUI OR UNIDICT_BUILD_QT_TESTS)
        find_package(Qt6 REQUIRED COMPONENTS Network Multimedia)
    endif()

    # 仅 QML 壳需要 Quick 系列组件（Widgets GUI 不需要）
    if(UNIDICT_BUILD_QT_QMLUI)
        find_package(Qt6 REQUIRED COMPONENTS
            Quick
            Qml
            QuickControls2
        )
    endif()

    # 发音练习面板（gui，M1 录音基建）需要 Multimedia
    if(UNIDICT_BUILD_QT_GUI)
        find_package(Qt6 REQUIRED COMPONENTS Multimedia)
    endif()

    # 发音评分接线（M4，gui）：QtConcurrent 把模型推理挪出 UI 线程
    if(UNIDICT_BUILD_QT_GUI AND UNIDICT_BUILD_PRON)
        find_package(Qt6 REQUIRED COMPONENTS Concurrent)
    endif()

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

if(UNIDICT_BUILD_STD_CORE)
    add_subdirectory(core)
endif()

# §7 B5：C++ 版自带中转 unidict-relay（Qt-free 单二进制，依赖 std core）
if(UNIDICT_BUILD_STD_CORE)
    add_subdirectory(server/sync_relay/cpp)
endif()

if(UNIDICT_BUILD_QT_CORE OR UNIDICT_BUILD_ADAPTER_QT OR UNIDICT_BUILD_QT_APPS OR UNIDICT_BUILD_QT_TESTS OR UNIDICT_BUILD_PRON)
    add_subdirectory(adapters)
endif()

# Crashpad 崩溃采集（docs/crash_report_plan.md）：vendored CMake 封装
# （cmake/crashpad/，源 TheAssemblyArmada/crashpad-cmake，Apache-2.0，
# 已按宿主编译器适配——C++17/去 -Werror/强制预包含 cstdint）。
# EXCLUDE_FROM_ALL：不进默认全量构建，由依赖它的目标（qmlui 拷
# handler、adapters/crash、测试）按需拉起
if(UNIDICT_BUILD_CRASH)
    add_subdirectory(cmake/crashpad crashpad EXCLUDE_FROM_ALL)
endif()

if(UNIDICT_BUILD_STD_CLI)
    add_subdirectory(cli-std)
endif()

if(UNIDICT_BUILD_QT_GUI)
    add_subdirectory(gui)
endif()

if(UNIDICT_BUILD_QT_QMLUI)
    add_subdirectory(qmlui)
endif()

# Linux 桌面集成（二进制图标接线）：.desktop 入口 + hicolor 图标进
# install 树（share/applications、share/icons/hicolor）。PNG 源自
# docs/logo.svg 多尺寸导出（assets/icons/）；Exec 指向分发 GUI 主程序
if(UNIX AND NOT APPLE AND UNIDICT_BUILD_QT_APPS)
    install(FILES assets/linux/unidict.desktop
            DESTINATION share/applications)
    install(FILES assets/icons/unidict_256.png RENAME unidict.png
            DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES assets/icons/unidict_512.png RENAME unidict.png
            DESTINATION share/icons/hicolor/512x512/apps)
endif()

if(BUILD_TESTING AND (UNIDICT_BUILD_STD_TESTS OR UNIDICT_BUILD_QT_TESTS))
    add_subdirectory(tests)
endif()
