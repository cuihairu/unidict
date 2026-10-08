# Crashpad 崩溃采集薄封装（docs/crash_report_plan.md §3）：
# 无 Qt，只包 crashpad 客户端接线；平台路径/日志由壳负责。
# crashpad_client 的头文件与链接经其 INTERFACE 从 cmake/crashpad 传播
add_library(unidict_crash STATIC
    crash_reporter.cpp
    crash_reporter.h
)
target_link_libraries(unidict_crash PUBLIC crashpad_client)
target_include_directories(unidict_crash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# MSVC：crashpad_common 的 NOMINMAX 只护 crashpad 自家 TU，不沿链接传播
# 到这里——而 mini_chromium base/logging.h 的
# std::numeric_limits<int>::max() 撞 windows.h 的 max 宏（CI Windows 实测
# C2589）。按源文件收口，不外溢 qmlui 其他 Windows TU
if(MSVC)
    set_source_files_properties(crash_reporter.cpp PROPERTIES
        COMPILE_DEFINITIONS "NOMINMAX;WIN32_LEAN_AND_MEAN")
endif()
