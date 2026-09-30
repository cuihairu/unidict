# adapters/android —— Android 壳的 JNI 胶水（mobile_plan §4）。
# 同时服务两条构建路径：
#   1) AGP externalNativeBuild（android/app 打包 libunidict_jni.so 进 APK）
#   2) 命令行 NDK 直配（本批快速门禁，不经 gradle）
# core/ 保持零平台头文件：jni.h 只出现在本目录 jni/ 下（AGENTS.md 约束不破）。
cmake_minimum_required(VERSION 3.22.1)
project(unidict_jni LANGUAGES CXX)

set(UNIDICT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# std-only 口径（与 M0 NDK 实测配方一致）：只建 core 静态库，
# Qt / 应用 / 测试 / 发音评分全关；BUILD_TESTING 关掉根 CTest 把
# std 测试一起拖进 NDK 构建的路径。
set(BUILD_TESTING            OFF CACHE BOOL "" FORCE)
set(UNIDICT_BUILD_STD_CORE   ON  CACHE BOOL "" FORCE)
set(UNIDICT_BUILD_QT_CORE    OFF CACHE BOOL "" FORCE)
set(UNIDICT_BUILD_ADAPTER_QT OFF CACHE BOOL "" FORCE)
set(UNIDICT_BUILD_QT_APPS    OFF CACHE BOOL "" FORCE)
set(UNIDICT_BUILD_QT_TESTS   OFF CACHE BOOL "" FORCE)
set(UNIDICT_BUILD_PRON       OFF CACHE BOOL "" FORCE)

# 仓库根作为子目录引入：复用 core/CMakeLists 的 unidict_std_core /
# unidict_index_std 目标与 zlib 语义（NDK sysroot -lz，M0 已实测）。
add_subdirectory("${UNIDICT_ROOT}" "${CMAKE_BINARY_DIR}/unidict-core")

add_library(unidict_jni SHARED
    jni/jni_util.h
    jni/dict_jni.cpp
    jni/lookup_jni.cpp
    jni/store_jni.cpp
)
target_include_directories(unidict_jni PRIVATE "${UNIDICT_ROOT}/core")
target_link_libraries(unidict_jni PRIVATE unidict_std_core log)
