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
    find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Quick
        Qml
        QuickControls2
        TextToSpeech
        QuickDialogs2
        Test
    )

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
