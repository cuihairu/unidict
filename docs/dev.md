# Unidict Developer Guide

This guide explains how to set up a development environment for Unidict and how to build the project from source.

## 1. Prerequisites

You will need the following tools installed on your system:

1.  **A C++ Compiler**:
    *   **Windows**: MSVC (via Visual Studio) or MinGW.
    *   **macOS**: Clang (via Xcode Command Line Tools).
    *   **Linux**: GCC or Clang.
2.  **CMake**: Version 3.16 or higher. This is the build system used by the project.
3.  **Qt Framework**: Version 6.2 or higher.

## 2. Environment Setup

The easiest way to get a working C++ and Qt environment is to use the official Qt Online Installer.

1.  **Download Qt**: Go to the [Qt download page](https://www.qt.io/download-qt-installer) and download the online installer.
2.  **Run the Installer**:
    *   During installation, you will be asked to create a Qt Account.
    *   In the "Select Components" screen, make sure to select:
        *   The latest stable version of Qt (e.g., Qt 6.x.x).
        *   Under your chosen Qt version, ensure the following modules are checked:
            *   **Desktop (MinGW, MSVC, or macOS)**: Select the one appropriate for your system.
            *   **Android**: If you plan to develop for Android.
            *   **iOS**: If you plan to develop for iOS.
        *   Under "Additional Libraries", ensure **"Qt Debug Information Files"** is checked.
        *   Under "Developer and Designer Tools", ensure **"CMake"** and your compiler toolchain (e.g., **"MinGW"** on Windows) are selected.

3.  **Install Xcode Command Line Tools (macOS only)**:
    Open a terminal and run:
    ```bash
    xcode-select --install
    ```

## 3. Building the Project

Once your environment is set up, you can build the project using CMake.

1.  **Clone the repository**:
    ```bash
    git clone <your-repository-url>
    cd unidict
    ```

2.  **Configure with CMake (Out-of-source build)**:
    It's best practice to create a separate build directory.

    ```bash
    # From the project root directory (unidict/)
    cmake -B build -DCMAKE_PREFIX_PATH=/path/to/your/Qt/version/platform
    ```
    **Important**: You must replace `/path/to/your/Qt/version/platform` with the actual path to your Qt installation. For example:
    *   **macOS**: `~/Qt/6.5.0/macos`
    *   **Linux**: `~/Qt/6.5.0/gcc_64`
    *   **Windows**: `C:/Qt/6.5.0/mingw_64`

3.  **Compile the code**:
    ```bash
    cmake --build build
    ```

4.  **Run the applications**:
    The compiled executables will be in the `build` directory (or a subdirectory like `build/gui` and `build/cli`).
    *   GUI App: `build/gui/unidict_gui`
    *   CLI App: `build/cli/unidict_cli`

## 4. IDE Recommendations

You can use any C++ IDE that supports CMake projects.

*   **Qt Creator**: The official Qt IDE. It has the best integration with the Qt framework. Simply open the root `CMakeLists.txt` file in Qt Creator, configure the project with your Qt kit, and you're ready to go.
*   **CLion**: A powerful cross-platform C++ IDE from JetBrains. It has excellent CMake support.
*   **Visual Studio Code**: A lightweight and popular editor. You'll need to install the C/C++ and CMake Tools extensions.
