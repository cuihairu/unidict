# Crashpad 崩溃采集薄封装（docs/crash_report_plan.md §3）：
# 无 Qt，只包 crashpad 客户端接线；平台路径/日志由壳负责。
# crashpad_client 的头文件与链接经其 INTERFACE 从 cmake/crashpad 传播
add_library(unidict_crash STATIC
    crash_reporter.cpp
    crash_reporter.h
)
target_link_libraries(unidict_crash PUBLIC crashpad_client)
target_include_directories(unidict_crash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
