# Unidict Project CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(Unidict VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find Qt ---
# This will find the Qt installation on your system.
# You might need to set CMAKE_PREFIX_PATH for CMake to find it, e.g.:
# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/your/Qt/version/platform
# Build toggles (toward a Qt-free core; Qt stays in adapters/apps)
option(UNIDICT_BUILD_ADAPTER_QT "Build Qt adapters" ON)
option(UNIDICT_BUILD_QT_APPS "Build Qt apps (gui, qmlui, cli)" ON)
option(UNIDICT_BUILD_QT_CORE "Build Qt-based core target (legacy)" ON)
option(UNIDICT_BUILD_QT_TESTS "Build Qt-based tests" ON)
option(UNIDICT_BUILD_STD_CLI "Build std-only CLI (no Qt)" ON)

# Find Qt only if required by any enabled component
if(UNIDICT_BUILD_ADAPTER_QT OR UNIDICT_BUILD_QT_APPS OR UNIDICT_BUILD_QT_CORE OR UNIDICT_BUILD_QT_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick Qml QuickControls2 Test)
endif()

# Build toggles (toward a Qt-free core; Qt stays in adapters/apps)
option(UNIDICT_BUILD_ADAPTER_QT "Build Qt adapters" ON)
option(UNIDICT_BUILD_QT_APPS "Build Qt apps (gui, qmlui, cli)" ON)

# --- Project Options ---
# Automatically run moc, uic, and rcc when needed.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
enable_testing()

# --- Add Subdirectories ---
add_subdirectory(core)
if(UNIDICT_BUILD_ADAPTER_QT)
  add_subdirectory(adapters)
endif()
if(UNIDICT_BUILD_QT_APPS)
  add_subdirectory(gui)
  add_subdirectory(cli)
  add_subdirectory(qmlui)
endif()
if(UNIDICT_BUILD_STD_CLI)
  add_subdirectory(cli-std)
endif()
add_subdirectory(tests)
