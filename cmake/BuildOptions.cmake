# ==============================================================================
# Unidict 构建选项配置
# 提供集中的构建配置管理
# ==============================================================================

include_guard(BUILD_OPTIONS_CMAKE)

# 检测构建类型
if(NOT DEFINED BUILD_TYPE)
    set(BUILD_TYPE "Release" CACHE STRING "构建类型 (Debug/Release/RelWithDebInfo)")
endif()

# 平台检测
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
else()
    set(PLATFORM_LINUX TRUE)
endif()

# 编译器检测
if(MSVC)
    set(COMPILER_MSVC TRUE)
    set(COMPILER_CLANG FALSE)
    set(COMPILER_GCC FALSE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(COMPILER_CLANG TRUE)
    set(COMPILER_GCC FALSE)
else()
    set(COMPILER_GCC TRUE)
endif()

# ==============================================================================
# 基础选项
# ==============================================================================

# C++标准
set(DEFAULT_CMAKE_CXX_STANDARD 20 CACHE STRING "默认C++标准")
set(CMAKE_CXX_STANDARD ${DEFAULT_CMAKE_CXX_STANDARD} CACHE STRING "C++标准")

# 编译选项
option(UNIDICT_ENABLE_IPO "启用LTO优化" ON)
option(UNIDICT_ENABLE_LTO "启用LTO优化" OFF)
option(UNIDICT_ENABLE_DEBUG_SYMBOLS "生成调试符号" OFF)
option(UNIDICT_ENABLE_PROFILING "启用性能分析" OFF)
option(UNIDICT_ENABLE_ASAN "启用AddressSanitizer" OFF)
option(UNIDICT_ENABLE_UBSAN "启用UndefinedBehaviorSanitizer" OFF)
option(UNIDICT_ENABLE_TSAN "启用ThreadSanitizer" OFF)

# 依赖管理
set(UNIDICT_ENABLE_EXTERNAL_QT ON CACHE BOOL "使用系统Qt" OFF)
set(UNIDICT_ENABLE_EXTERNAL_ZLIB ON CACHE BOOL "使用系统zlib" OFF)

# ==============================================================================
# 组件构建选项
# ==============================================================================

option(UNIDICT_BUILD_STD_CORE "构建std核心库" ON)
option(UNIDICT_BUILD_QT_CORE "构建Qt核心库" ON)
option(UNIDICT_BUILD_ADAPTER_QT "构建Qt适配器" ON)
option(UNIDICT_BUILD_QT_APPS "构建Qt应用程序" ON)
option(UNIDICT_BUILD_STD_CLI "构建std命令行工具" ON)
option(UNIDICT_BUILD_QT_TESTS "构建Qt测试" ON)
option(UNIDICT_BUILD_STD_TESTS "构建std测试" ON)
option(UNIDICT_BUILD_EXAMPLES "构建示例程序" ON)
option(UNIDICT_BUILD_BENCHMARKS "构建性能测试" ON)
option(UNIDICT_BUILD_DOCUMENTATION "构建文档" ON)

# ==============================================================================
# 高级功能选项
# ==============================================================================

option(UNIDICT_ENABLE_FULLTEXT_SEARCH "启用全文搜索" ON)
option(UNIDICT_ENABLE_AI_INTEGRATION "启用AI集成" ON)
option(UNIDICT_ENABLE_SYNC_SERVICE "启用同步服务" ON)
option(UNIDICT_ENABLE_VOCABULARY "启用生词本功能" ON)
option(UNIDICT_ENABLE_PLUGINS "启用插件系统" ON)

# 默认启用的功能（可覆盖）
set(UNIDICT_DEFAULT_FEATURES
    "fulltext_search;ai_integration;sync_service;vocabulary"
    CACHE STRING "默认启用的功能列表"
)

# ==============================================================================
# 平台特定选项
# ==============================================================================

# Windows特定选项
if(PLATFORM_WINDOWS)
    option(UNIDICT_ENABLE_WINDOWS_SERVICE "启用Windows服务" OFF)
    option(UNIDICT_ENABLE_MSI_INSTALLER "创建MSI安装包" ON)
    option(UNIDICT_ENABLE_AUTO_UPDATE "启用自动更新" OFF)
endif()

# macOS特定选项
if(PLATFORM_MACOS)
    option(UNIDICT_ENABLE_APP_BUNDLE "创建App Bundle" ON)
    option(UNIDICT_ENABLE_DMG_INSTALLER "创建DMG安装包" ON)
    option(UNIDICT_ENABLE_SPARKLE "启用Sparkle自动更新" OFF)
    option(UNIDICT_ENABLE_HOMEBREW_CASK "创建Homebrew Cask" OFF)
endif()

# Linux特定选项
if(PLATFORM_LINUX)
    option(UNIDICT_ENABLE_APPIMAGE "创建AppImage" ON)
    option(UNIDICT_ENABLE_FLATPAK "创建Flatpak包" ON)
    option(UNIDICT_ENABLE_SNAP "创建Snap包" ON)
    option(UNIDICT_ENABLE_DEBIAN_PACKAGE "创建Debian包" ON)
    option(UNIDICT_ENABLE_RPM_PACKAGE "创建RPM包" ON)
    option(UNIDICT_ENABLE_SYSTEMD_SERVICE "启用Systemd服务" OFF)
endif()

# ==============================================================================
# 调试选项
# ==============================================================================

option(UNIDICT_VERBOSE_BUILD "详细构建输出" OFF)
option(UNIDICT_ENABLE_VERBOSE_LOGGING "启用详细日志" OFF)
option(UNIDICT_ENABLE_LOG_TIMING "启用日志计时" OFF)
option(UNIDICT_ENABLE_PERFORMANCE_LOGGING "启用性能日志" OFF)

# ==============================================================================
# 安装选项
# ==============================================================================

# 安装目录
option(UNIDICT_INSTALL_PREFIX "安装前缀" "${CMAKE_INSTALL_PREFIX}")
option(UNIDICT_INSTALL_BINDIR "可执行文件安装目录" "bin")
option(UNIDICT_INSTALL_LIBDIR "库文件安装目录" "lib")
option(UNIDICT_INSTALL_DATADIR "数据文件安装目录" "share/unidict")
option(UNIDICT_INSTALL_DOCDIR "文档安装目录" "share/doc/unidict")
option(UNIDICT_INSTALL_EXAMPLEDIR "示例安装目录" "share/doc/unidict/examples")

# 配置文件安装
option(UNIDICT_INSTALL_SYSCONFDIR "系统配置目录" "etc")
option(UNIDICT_INSTALL_PKGCONFIGDIR "pkg-config目录" "lib/pkgconfig")

# 用户配置目录
option(UNIDICT_USER_CONFIG_DIR "用户配置目录" ".config/unidict")
option(UNIDICT_USER_DATA_DIR "用户数据目录" ".local/share/unidict")
option(UNIDICT_USER_CACHE_DIR "用户缓存目录" ".cache/unidict")

# 启用/禁用组件
option(UNIDICT_INSTALL_SYSTEM_DESKTOP_FILE "安装系统桌面文件" ON)
option(UNIDICT_INSTALL_SYSTEM_MIME_FILE "安装MIME类型文件" ON)
option(UNIDICT_INSTALL_SYSTEM_MANPAGE "安装man页面" ON)
option(UNIDICT_INSTALL_ICONS "安装图标" ON)

# ==============================================================================
# 包版本信息
# ==============================================================================

# 版本号管理
set(UNIDICT_VERSION_MAJOR 1 CACHE STRING "主版本号")
set(UNIDICT_VERSION_MINOR 0 CACHE STRING "次版本号")
set(UNIDICT_VERSION_PATCH 0 CACHE STRING "修订版本号")
set(UNIDICT_VERSION_SUFFIX "" CACHE STRING "版本后缀")

# 构建完整版本号
set(UNIDICT_VERSION "${UNIDICT_VERSION_MAJOR}.${UNIDICT_VERSION_MINOR}.${UNIDICT_VERSION_PATCH}${UNIDICT_VERSION_SUFFIX}")

# 包信息
set(UNIDICT_PACKAGE_NAME "unidict" CACHE STRING "包名称")
set(UNIDICT_PACKAGE_DESCRIPTION "Universal dictionary lookup tool" CACHE STRING "包描述")
set(UNIDICT_PACKAGE_MAINTAINER "Unidict Team <team@unidict.org>" CACHE STRING "维护者")
set(UNIDICT_PACKAGE_URL "https://github.com/unidict/unidict" CACHE STRING "项目URL")
set(UNIDICT_PACKAGE_LICENSE "MIT" CACHE STRING "许可证")

# ==============================================================================
# 依赖版本要求
# ==============================================================================

# Qt版本
set(QT_MIN_VERSION 6.3.0 CACHE STRING "最低Qt版本要求")
set(QT_RECOMMENDED_VERSION 6.6.0 CACHE STRING "推荐Qt版本")

# 其他依赖
set(ZLIB_MIN_VERSION 1.2.11 CACHE STRING "最低zlib版本")
set(CMAKE_MIN_VERSION 3.20.0 CACHE STRING "最低CMake版本")

# ==============================================================================
# 编译定义和标志
# ==============================================================================

# 根据选项设置编译定义
if(UNIDICT_ENABLE_IPO AND BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-generate -fprofile-use")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-generate")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fprofile-generate")
endif()

if(UNIDICT_ENABLE_LTO AND BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -flto")
endif()

# 调试符号
if(UNIDICT_ENABLE_DEBUG_SYMBOLS AND BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -g")
endif()

# Sanitizer选项
if(UNIDICT_ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

if(UNIDICT_ENABLE_UBSAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
endif()

if(UNIDICT_ENABLE_TSAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
endif()

# ==============================================================================
# 输出配置
# ==============================================================================

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${BUILD_TYPE}")

# 设置库输出名称
set(CMAKE_ARCHIVE_OUTPUT_NAME "${UNIDICT_PACKAGE_NAME}-${CMAKE_BUILD_TYPE}")

# 设置可执行文件输出名称
set(CMAKE_RUNTIME_OUTPUT_NAME "${UNIDICT_PACKAGE_NAME}")

# ==============================================================================
# 工具链文件
# ==============================================================================

# 生成配置文件
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
    "${CMAKE_BINARY_DIR}/config.h"
    @ONLY
)

# 生成版本文件
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/UnidictConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/UnidictConfig.cmake"
    @ONLY
)

# ==============================================================================
# 包配置文件
# ==============================================================================

# CPack配置
set(CPACK_GENERATOR "TGZ;DEB;RPM;NSIS;DMG")

# 组件定义
set(CPACK_COMPONENTS "core;qt_apps;cli;docs")

# 组件显示
set(CPACK_COMPONENTS_ALL "core;qt_apps;cli;docs;plugins")

# CPack变量
set(CPACK_PACKAGE_NAME "${UNIDICT_PACKAGE_NAME}")
set(CPACK_PACKAGE_VERSION "${UNIDICT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "${UNIDICT_PACKAGE_DESCRIPTION}")
set(CPACK_PACKAGE_MAINTAINER "${UNIDICT_PACKAGE_MAINTAINER}")
set(CPACK_PACKAGE_VENDOR "Unidict")
set(CPACK_PACKAGE_CONTACT "${UNIDICT_PACKAGE_MAINTAINER}")
set(CPACK_PACKAGE_URL "${UNIDICT_PACKAGE_URL}")
set(CPACK_PACKAGE_LICENSE "${UNIDICT_PACKAGE_LICENSE}")

# 平台特定的CPack设置
if(PLATFORM_WINDOWS)
    set(CPACK_NSIS_DISPLAY_NAME "Unidict")
    set(CPACK_NSIS_PACKAGE_NAME "${UNIDICT_PACKAGE_NAME}")
    set(CPACK_NSIS_CONTACT "${UNIDICT_PACKAGE_MAINTAINER}")
    set(CPACK_NSIS_URL "${UNIDICT_PACKAGE_URL}")
    set(CPACK_NSIS_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/assets/icons/unidict.ico")
elseif(PLATFORM_MACOS)
    set(CPACK_BUNDLE_NAME "Unidict")
    set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/assets/icons/unidict.icns")
    set(CPACK_BUNDLE_STARTUP_COMMAND "${CMAKE_INSTALL_PREFIX}/bin/unidict_qml")
    set(CPACK_BUNDLE_IDENTIFIER "com.unidict.unidict")
    set(CPACK_DMG_FORMAT "UDZO")
elseif(PLATFORM_LINUX)
    set(CPACK_DEBIAN_PACKAGE_DEPENDENCIES "libqt6core6, libqt6gui6, libqt6quick6, zlib1g, libc6")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.3, qt6-qtdeclarative >= 6.3, qt6-qtquick >= 6.3, zlib >= 1.2.11")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
endif()

# 安装脚本
set(CPACK_INSTALL_CMAKE_PROJECTS_UNCONDITIONAL TRUE)
set(CPACK_INSTALL_PREFIX "${UNIDICT_INSTALL_PREFIX}")

# ==============================================================================
# 显示配置摘要
# ==============================================================================

message(STATUS "")
message(STATUS "Unidict 构建配置摘要:")
message(STATUS "  构建类型: ${BUILD_TYPE}")
message(STATUS "  C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "  平台: ${PLATFORM_NAME}")
message(STATUS "  编译器: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  版本: ${UNIDICT_VERSION}")
message(STATUS "  安装前缀: ${UNIDICT_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS " 启用的功能: ${UNIDICT_DEFAULT_FEATURES}")
message(STATUS "")

# 警告检查
if(BUILD_TYPE MATCHES "Release" AND (UNIDICT_ENABLE_ASAN OR UNIDICT_ENABLE_UBSAN OR UNIDICT_ENABLE_TSAN))
    message(WARNING "在Release构建中启用了Sanitizer - 可能影响性能")
endif()

if(UNIDICT_ENABLE_IPO AND UNIDICT_ENABLE_LTO)
    message(WARNING "同时启用了IPO和LTO - 可能导致编译问题")
endif()

# ==============================================================================
# 选项验证和依赖检查
# ==============================================================================

# Qt依赖检查
if(UNIDICT_BUILD_QT_APPS OR UNIDICT_BUILD_QT_TESTS)
    if(NOT UNIDICT_ENABLE_EXTERNAL_QT)
        find_package(Qt${QT_MIN_VERSION} COMPONENTS Core Gui Qml Quick Network REQUIRED)
        if(NOT Qt_FOUND)
            message(FATAL_ERROR "未找到Qt ${QT_MIN_VERSION} 或更高版本")
            message(FATAL_ERROR "请安装Qt或使用 -DUNIDICT_ENABLE_EXTERNAL_QT=OFF 使用std-only构建")
        endif()
    endif()
endif()

# zlib依赖检查
if(NOT UNIDICT_ENABLE_EXTERNAL_ZLIB AND (UNIDICT_BUILD_STD_CORE OR UNIDICT_BUILD_STD_CLI OR UNIDICT_BUILD_ADAPTER_QT))
    find_package(ZLIB ${ZLIB_MIN_VERSION} REQUIRED)
    if(NOT ZLIB_FOUND)
        message(FATAL_ERROR "未找到zlib ${ZLIB_MIN_VERSION} 或更高版本")
        endif()
endif()

# CMake版本检查
if(CMAKE_VERSION VERSION_LESS ${CMAKE_MIN_VERSION})
    message(FATAL_ERROR "需要CMake ${CMAKE_MIN_VERSION} 或更高版本")
endif()

# ==============================================================================
# 文件结构信息
# ==============================================================================

message(STATUS "配置文件已生成:")
message(STATUS "  - ${CMAKE_BINARY_DIR}/config.h")
message(STATUS "  - ${CMAKE_BINARY_DIR}/UnidictConfig.cmake")
message(STATUS "")
message(STATUS "下一步:")
message(STATUS "  cmake --build build -j$(nproc)")
message(STATUS "  ctest --test-dir build")
message(STATUS "")
message(STATUS "组件构建选项:")
message(STATUS "  -DUNIDICT_BUILD_QT_APPS=OFF     - 不构建Qt应用")
message(STATUS "  -DUNIDICT_BUILD_STD_CLI=OFF     - 不构建std CLI")
message(STATUS "  -DUNIDICT_ENABLE_FULLTEXT_SEARCH=OFF - 禁用全文搜索")
message(STATUS "  -DUNIDICT_ENABLE_PLUGINS=OFF       - 禁用插件系统")
message(STATUS "")