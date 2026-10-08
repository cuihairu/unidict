if(UNIDICT_BUILD_ADAPTER_QT)
  add_subdirectory(qt)
endif()

# 发音评分适配器（无 Qt 依赖，只依赖 std core + onnxruntime）
if(UNIDICT_BUILD_PRON)
  add_subdirectory(pron)
endif()

# Crashpad 崩溃采集适配（无 Qt，包 crashpad 客户端接线；选项随桌面
# qmlui 壳走，见 docs/crash_report_plan.md §1 接入面边界）
if(UNIDICT_BUILD_CRASH)
  add_subdirectory(crash)
endif()

