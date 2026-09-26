cmake_minimum_required(VERSION 3.16)
project(Unidict VERSION 1.0 LANGUAGES CXX)

# 先定标准再 include BuildOptions：其 status 打印与选项快照才反映实际值
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
include(cmake/BuildOptions.cmake)

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

if(UNIDICT_BUILD_QT_CORE OR UNIDICT_BUILD_ADAPTER_QT OR UNIDICT_BUILD_QT_APPS OR UNIDICT_BUILD_QT_TESTS OR UNIDICT_BUILD_PRON)
    add_subdirectory(adapters)
endif()

if(UNIDICT_BUILD_STD_CLI)
    add_subdirectory(cli-std)
endif()

if(UNIDICT_BUILD_QT_APPS)
    add_subdirectory(cli)
endif()

if(UNIDICT_BUILD_QT_GUI)
    add_subdirectory(gui)
endif()

if(UNIDICT_BUILD_QT_QMLUI)
    add_subdirectory(qmlui)
endif()

if(BUILD_TESTING AND (UNIDICT_BUILD_STD_TESTS OR UNIDICT_BUILD_QT_TESTS))
    add_subdirectory(tests)
endif()
