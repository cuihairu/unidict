# Unidict Project CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(Unidict VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find Qt ---
# This will find the Qt installation on your system.
# You might need to set CMAKE_PREFIX_PATH for CMake to find it, e.g.:
# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/your/Qt/version/platform
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# --- Project Options ---
# Automatically run moc, uic, and rcc when needed.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# --- Add Subdirectories ---
# This tells CMake to look into our subdirectories for more build targets.
add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(cli)
