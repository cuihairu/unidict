if(UNIDICT_BUILD_ADAPTER_QT)
  add_subdirectory(qt)
endif()

# 发音评分适配器（无 Qt 依赖，只依赖 std core + onnxruntime）
if(UNIDICT_BUILD_PRON)
  add_subdirectory(pron)
endif()

