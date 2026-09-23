cmake_minimum_required(VERSION 3.16)
project(Unidict VERSION 1.0 LANGUAGES CXX)

include(CTest)
include(cmake/BuildOptions.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

    # lookup_adapter（qmlui 与 Qt 测试共用）使用 QTextToSpeech
    if(UNIDICT_BUILD_QT_APPS OR UNIDICT_BUILD_QT_TESTS)
        find_package(Qt6 REQUIRED COMPONENTS TextToSpeech)
    endif()

    # 仅 QML 应用需要 Quick 系列组件
    if(UNIDICT_BUILD_QT_APPS)
        find_package(Qt6 REQUIRED COMPONENTS
            Quick
            Qml
            QuickControls2
        )
    endif()

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

if(UNIDICT_BUILD_STD_CORE)
    add_subdirectory(core)
endif()

if(UNIDICT_BUILD_QT_CORE OR UNIDICT_BUILD_ADAPTER_QT OR UNIDICT_BUILD_QT_APPS OR UNIDICT_BUILD_QT_TESTS)
    add_subdirectory(adapters)
endif()

if(UNIDICT_BUILD_STD_CLI)
    add_subdirectory(cli-std)
endif()

if(UNIDICT_BUILD_QT_APPS)
    add_subdirectory(cli)
    add_subdirectory(gui)
    add_subdirectory(qmlui)
endif()

if(BUILD_TESTING AND (UNIDICT_BUILD_STD_TESTS OR UNIDICT_BUILD_QT_TESTS))
    add_subdirectory(tests)
endif()
