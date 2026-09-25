# 发音评分 onnxruntime 适配器（M3b）。
# 分层（docs/pronunciation-plan.md）：core/std 纯逻辑吃帧级 log 概率，
# 这里只做 ORT 推理壳。模型文件不进 git，运行时给路径。

# onnxruntime 预编译包：版本锁定 + URL 可覆盖（镜像/离线缓存场景）。
# FetchContent 不用——归档里没有 CMakeLists，手工 IMPORTED 更透明
set(UNIDICT_PRON_ORT_VERSION "1.30.0" CACHE STRING "onnxruntime 版本")
set(UNIDICT_PRON_ORT_URL ""
    CACHE STRING "onnxruntime 预编译包下载地址（留空按平台拼默认值）")

if(NOT UNIDICT_PRON_ORT_URL)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(_pron_ort_pkg "onnxruntime-linux-x64-${UNIDICT_PRON_ORT_VERSION}")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_pron_ort_pkg "onnxruntime-linux-arm64-${UNIDICT_PRON_ORT_VERSION}")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(_pron_ort_pkg "onnxruntime-win-x64-${UNIDICT_PRON_ORT_VERSION}")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(_pron_ort_pkg "onnxruntime-osx-arm64-${UNIDICT_PRON_ORT_VERSION}")
    else()
        message(FATAL_ERROR "UNIDICT_BUILD_PRON: 平台 ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR} 无预编译包映射，请设置 UNIDICT_PRON_ORT_URL")
    endif()
    set(UNIDICT_PRON_ORT_URL
        "https://github.com/microsoft/onnxruntime/releases/download/v${UNIDICT_PRON_ORT_VERSION}/${_pron_ort_pkg}.tgz")
endif()
set(_pron_ort_dir "${CMAKE_BINARY_DIR}/_ext/onnxruntime")
set(_pron_ort_archive "${CMAKE_BINARY_DIR}/_ext/${_pron_ort_pkg}.tgz")

# 解包/提取只做一次；预放置的归档（离线缓存）优先于现场下载
if(NOT EXISTS "${_pron_ort_dir}/lib/libonnxruntime.so"
   AND NOT EXISTS "${_pron_ort_dir}/lib/onnxruntime.lib")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_ext")
    if(NOT EXISTS "${_pron_ort_archive}")
        message(STATUS "发音评分适配器：下载 onnxruntime ${UNIDICT_PRON_ORT_VERSION} → ${_pron_ort_dir}")
        file(DOWNLOAD "${UNIDICT_PRON_ORT_URL}" "${_pron_ort_archive}"
             STATUS _pron_ort_dl_status)
        list(GET _pron_ort_dl_status 0 _pron_ort_dl_code)
        if(NOT _pron_ort_dl_code EQUAL 0)
            list(GET _pron_ort_dl_status 1 _pron_ort_dl_msg)
            file(REMOVE "${_pron_ort_archive}")
            message(FATAL_ERROR "onnxruntime 下载失败: ${_pron_ort_dl_msg}（可并行下载后预放置 ${_pron_ort_archive}，或设 UNIDICT_PRON_ORT_URL 换镜像）")
        endif()
    else()
        message(STATUS "发音评分适配器：使用预放置的 ${_pron_ort_archive}")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${_pron_ort_archive}" DESTINATION "${_pron_ort_dir}_tmp")
    # 预编译包内层目录名不定（onnxruntime-linux-x64-…），取唯一子目录。
    # 目标目录可能残留上次中断的半截解包——RENAME 不覆盖，先清场
    file(GLOB _pron_ort_inner "${_pron_ort_dir}_tmp/*")
    list(GET _pron_ort_inner 0 _pron_ort_src)
    file(REMOVE_RECURSE "${_pron_ort_dir}")
    file(RENAME "${_pron_ort_src}" "${_pron_ort_dir}")
    file(REMOVE_RECURSE "${_pron_ort_dir}_tmp")
    file(REMOVE "${_pron_ort_archive}")
endif()

add_library(onnxruntime SHARED IMPORTED GLOBAL)
if(WIN32)
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${_pron_ort_dir}/lib/onnxruntime.dll"
        IMPORTED_IMPLIB "${_pron_ort_dir}/lib/onnxruntime.lib")
else()
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${_pron_ort_dir}/lib/libonnxruntime.so")
endif()
target_include_directories(onnxruntime INTERFACE "${_pron_ort_dir}/include")

add_library(unidict_pron STATIC
    onnx_pron_scorer.cpp
    onnx_pron_scorer.h
)
target_link_libraries(unidict_pron PUBLIC unidict_std_core onnxruntime)
target_include_directories(unidict_pron PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# 测试可执行文件运行期找 libonnxruntime.so：构建树 rpath 指到解包目录
set_target_properties(unidict_pron PROPERTIES
    BUILD_RPATH "${_pron_ort_dir}/lib"
    INSTALL_RPATH "\$ORIGIN")
