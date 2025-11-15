# Unidict Project CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(Unidict VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Build options ---
option(UNIDICT_BUILD_ADAPTER_QT "Build Qt adapters" ON)
option(UNIDICT_BUILD_QT_APPS "Build Qt apps (gui, qmlui, cli)" ON)
option(UNIDICT_BUILD_QT_CORE "Build Qt-based core target (legacy)" OFF)
option(UNIDICT_BUILD_QT_TESTS "Build Qt-based tests" ON)
option(UNIDICT_BUILD_STD_CLI "Build std-only CLI (no Qt)" ON)

# --- Qt detection (only when needed) ---
set(UNIDICT_WANTS_QT OFF)
if(UNIDICT_BUILD_ADAPTER_QT OR UNIDICT_BUILD_QT_APPS OR UNIDICT_BUILD_QT_CORE OR UNIDICT_BUILD_QT_TESTS)
  set(UNIDICT_WANTS_QT ON)
  find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick Qml QuickControls2 Test TextToSpeech)
endif()

# --- Autogen tools only when Qt is used ---
if(UNIDICT_WANTS_QT)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTOUIC ON)
  set(CMAKE_AUTORCC ON)
endif()

enable_testing()

# --- Add Subdirectories ---
add_subdirectory(core)
if(UNIDICT_BUILD_ADAPTER_QT)
  add_subdirectory(adapters)
endif()
if(UNIDICT_BUILD_QT_APPS AND UNIDICT_BUILD_QT_CORE)
  add_subdirectory(gui)
  add_subdirectory(cli)
  add_subdirectory(qmlui)
endif()
if(UNIDICT_BUILD_STD_CLI)
  add_subdirectory(cli-std)
endif()
add_subdirectory(tests)
