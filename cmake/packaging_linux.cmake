# Linux 系统包（deb/rpm）打包配置——std CLI 面向发行版包管理器分发。
#
# 由根 CMakeLists 在 `UNIX AND NOT APPLE AND UNIDICT_BUILD_STD_CLI` 时
# include：include(CPack) 后 `cpack -G DEB`/`-G RPM` 从构建目录直接出包。
# 依赖只声明运行时真实所需（libc/libstdc++/zlib），零 Qt 依赖——GUI 主
# 程序 unidict_qml 走每日构建 zip 分发（BUGS.md BUG-002），不进系统包。
# desktop 文件不适用：包内容是纯 CLI，无桌面入口语义（GUI 进系统包时
# 一并补 desktop + 图标）。
set(CPACK_PACKAGE_NAME "unidict")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "cuihairu")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Unidict offline dictionary CLI (std-only)")
set(CPACK_PACKAGE_DESCRIPTION
    "Unidict 离线词典命令行：多格式词典解析（MDict/StarDict/DSL/JSON/CSV/EPUB）、多模式检索、全文检索。std-only 构建，零 Qt 依赖。"
)
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/cuihairu/unidict")

# 单体打包：仅本文件+cli-std 的 install 规则生效（std 构建无其他 install）
set(CPACK_MONOLITHIC_INSTALL ON)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/dist")
set(CPACK_STRIP_FILES ON)

set(CPACK_GENERATOR "DEB;RPM")

# —— deb（Debian/Ubuntu 系）——
set(CPACK_DEBIAN_PACKAGE_NAME "unidict")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "cuihairu <cuihairu@users.noreply.github.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
# 运行时真实依赖：动态链接的 libc/libstdc++/libgcc + 系统 zlib
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libgcc-s1, zlib1g")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT") # unidict_<ver>_<arch>.deb

# —— rpm（Fedora/RHEL/openSUSE 系）——
set(CPACK_RPM_PACKAGE_NAME "unidict")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Text")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc, libstdc++, zlib")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT") # unidict-<ver>-<arch>.rpm

include(CPack)
