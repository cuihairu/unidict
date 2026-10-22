> 语言: **简体中文** | [English](../dev.md)

# Unidict 开发者指南

本指南介绍如何为 Unidict 搭建开发环境，以及如何从源码构建本项目。

## 1. 前置要求

你的系统上需要安装以下工具：

1.  **C++ 编译器**：
    *   **Windows**：MSVC（通过 Visual Studio 提供）或 MinGW。
    *   **macOS**：Clang（通过 Xcode Command Line Tools 提供）。
    *   **Linux**：GCC 或 Clang。
2.  **CMake**：3.16 或更高版本。这是本项目使用的构建系统。
3.  **Qt 框架**：6.5 或更高版本（主题跟随系统功能会无条件使用
    `QStyleHints::colorScheme`；Qt 6.2–6.4 无法构建）。

## 2. 环境搭建

获得可用的 C++ 与 Qt 环境，最简单的方式是使用官方 Qt Online Installer。

1.  **下载 Qt**：前往 [Qt 下载页面](https://www.qt.io/download-qt-installer)下载在线安装器。
2.  **运行安装器**：
    *   安装过程中会要求你创建一个 Qt 账号。
    *   在「Select Components」界面，请务必勾选：
        *   最新稳定版 Qt（例如 Qt 6.x.x）。
        *   在你选定的 Qt 版本下，确认以下模块已勾选：
            *   **Desktop（MinGW、MSVC 或 macOS）**：选择与你的系统对应的一项。
            *   **Android**：如果你计划进行 Android 开发。
            *   **iOS**：如果你计划进行 iOS 开发。
        *   在「Additional Libraries」下，确认 **"Qt Debug Information Files"** 已勾选。
        *   在「Developer and Designer Tools」下，确认 **"CMake"** 与你的编译器工具链（例如 Windows 上的 **"MinGW"**）已选择。

3.  **安装 Xcode Command Line Tools（仅 macOS）**：
    打开终端并运行：
    ```bash
    xcode-select --install
    ```

## 3. 构建项目

环境搭建完成后，即可使用 CMake 构建本项目。

1.  **克隆仓库**：
    ```bash
    git clone <your-repository-url>
    cd unidict
    ```

2.  **使用 CMake 配置（out-of-source 构建）**：
    最佳实践是创建一个独立的构建目录。

    ```bash
    # 在项目根目录（unidict/）下执行
    cmake -B build -DCMAKE_PREFIX_PATH=/path/to/your/Qt/version/platform
    ```
    **重要**：你必须把 `/path/to/your/Qt/version/platform` 替换为你的 Qt 安装的实际路径。例如：
    *   **macOS**：`~/Qt/6.5.0/macos`
    *   **Linux**：`~/Qt/6.5.0/gcc_64`
    *   **Windows**：`C:/Qt/6.5.0/mingw_64`

3.  **编译代码**：
    ```bash
    cmake --build build
    ```

4.  **运行应用程序**：
    编译出的可执行文件位于 `build` 目录（使用多配置生成器时，位于 `build/Release/`）。
    *   GUI 应用：`build/qmlui/unidict_qml`（Qt Quick，主桌面壳）或 `build/gui/unidict_gui`（Widgets，发音练习壳）
    *   CLI 应用：`build/cli-std/unidict_cli_std`（唯一的 CLI，无 Qt）

## 4. IDE 推荐

你可以使用任何支持 CMake 项目的 C++ IDE。

*   **Qt Creator**：Qt 官方 IDE。打开根目录的 `CMakeLists.txt`，选择你的 Qt kit，即可直接构建。
*   **CLion**：JetBrains 出品的跨平台 C++ IDE，内置 CMake 支持。
*   **Visual Studio Code**：需要安装 C/C++ 与 CMake Tools 扩展。
