enable_testing()

find_package(ZLIB REQUIRED)

# std 测试（*_std_test.cpp）用 assert 断言：Release 构建定义 NDEBUG 时
# 断言整体空转，进程 return 0 照样 passed——CI 三个工作流全是 Release，
# 等于这批测试从未真跑过（2026-09 写 epub/zip 测试时发现）。目录级剥掉
# NDEBUG；Qt 测试的 QVERIFY 不依赖 assert，不受影响。
add_compile_options($<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
                    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)

if(UNIDICT_BUILD_QT_TESTS AND UNIDICT_BUILD_QT_CORE)
  find_package(Qt6 REQUIRED COMPONENTS Test)

  add_executable(test_index_engine index_engine_test.cpp)
  target_link_libraries(test_index_engine PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_index_engine COMMAND test_index_engine)

  # core_lookup_tests 曾在 tests/CMakeLists.txt 大重构时被删掉 target，
  # 900 行状态持久化/StarDict/历史用例从此没进过构建——2026-09 挂回。
  add_executable(unidict_core_tests core_lookup_tests.cpp)
  target_link_libraries(unidict_core_tests PRIVATE unidict_core_qt Qt6::Core Qt6::Test ZLIB::ZLIB)
  add_test(NAME unidict_core_tests COMMAND unidict_core_tests)

  add_executable(test_data_store data_store_test.cpp)
  target_link_libraries(test_data_store PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_data_store COMMAND test_data_store)

  add_executable(test_lookup_service lookup_service_test.cpp)
  target_link_libraries(test_lookup_service PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_lookup_service COMMAND test_lookup_service)

  # qmlui 设计语言 token 的 WCAG AA 回归门禁（纯 token 表，不拖 core/Qt Quick）
  add_executable(test_theme_tokens_contrast theme_tokens_contrast_test.cpp)
  target_include_directories(test_theme_tokens_contrast PRIVATE "${CMAKE_SOURCE_DIR}/qmlui")
  target_link_libraries(test_theme_tokens_contrast PRIVATE Qt6::Core Qt6::Test)
  add_test(NAME test_theme_tokens_contrast COMMAND test_theme_tokens_contrast)

  # Q-6 覆盖收口：core/ 其余 legacy 解析器与接口默认实现（mdict/stardict/
  # epub/json parser 的失败分支 + DictionaryParser 默认 allEntries/prefixSearch
  # + plugin_manager/path_utils 0% 薄门面）
  add_executable(test_legacy_parsers_qt legacy_parsers_test.cpp)
  # epub_parser.h 直接 include "epub_parser_std.h"，测试 TU 需要 core/std 在路径上
  target_include_directories(test_legacy_parsers_qt PRIVATE "${CMAKE_SOURCE_DIR}/core/std")
  target_link_libraries(test_legacy_parsers_qt PRIVATE unidict_core_qt Qt6::Core Qt6::Test ZLIB::ZLIB)
  add_test(NAME test_legacy_parsers_qt COMMAND test_legacy_parsers_qt)

  # Q-7 覆盖收口：adapters/qt 的 0% 薄桥接（parser 桥 ×3 + AI 服务 +
  # 剪贴板 + 设置项）。ai_service/clipboard/settings 没有库目标（只有
  # qmlui 直接编译），照 test_sync_service_qt 的做法把源码挂进测试 target；
  # 链 Qt6::Gui 让 QTEST_MAIN 起 QGuiApplication（剪贴板需要）
  add_executable(test_qt_adapters_bridge
    qt_adapters_test.cpp
    ${CMAKE_SOURCE_DIR}/adapters/qt/ai_service_qt.cpp
    ${CMAKE_SOURCE_DIR}/adapters/qt/ai_service_qt.h
    ${CMAKE_SOURCE_DIR}/adapters/qt/clipboard_qt.cpp
    ${CMAKE_SOURCE_DIR}/adapters/qt/clipboard_qt.h
    ${CMAKE_SOURCE_DIR}/adapters/qt/settings_qt.cpp
    ${CMAKE_SOURCE_DIR}/adapters/qt/settings_qt.h
  )
  target_link_libraries(test_qt_adapters_bridge PRIVATE
    unidict_plugins_qt
    unidict_std_core
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    ZLIB::ZLIB
  )
  add_test(NAME test_qt_adapters_bridge COMMAND test_qt_adapters_bridge)
  # 剪贴板在无头 Linux/macOS 走 offscreen 平台插件
  if(NOT WIN32)
    set_tests_properties(test_qt_adapters_bridge PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()

  add_executable(test_dictionary_manager_fulltext dictionary_manager_fulltext_test.cpp)
  target_link_libraries(test_dictionary_manager_fulltext PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_dictionary_manager_fulltext COMMAND test_dictionary_manager_fulltext)

  add_executable(test_dictionary_manager_prefix dictionary_manager_prefix_test.cpp)
  target_link_libraries(test_dictionary_manager_prefix PRIVATE unidict_core_qt Qt6::Core Qt6::Test)
  add_test(NAME test_dictionary_manager_prefix COMMAND test_dictionary_manager_prefix)

  add_executable(test_global_hotkeys
    global_hotkeys_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.h
  )
  target_include_directories(test_global_hotkeys PRIVATE "${CMAKE_SOURCE_DIR}/qmlui")
  target_link_libraries(test_global_hotkeys PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
  add_test(NAME test_global_hotkeys COMMAND test_global_hotkeys)

  add_executable(test_startup_launcher
    startup_launcher_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/startup_launcher.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/startup_launcher.h
  )
  target_include_directories(test_startup_launcher PRIVATE "${CMAKE_SOURCE_DIR}/qmlui")
  target_link_libraries(test_startup_launcher PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
  add_test(NAME test_startup_launcher COMMAND test_startup_launcher)

  # Q-8 覆盖收口：剪贴板监听（qmlui 无库目标，源码直接挂进测试 target；
  # Qt6::Gui 让 QTEST_MAIN 起 QGuiApplication，剪贴板在 offscreen 下可读写）
  add_executable(test_clipboard_monitor
    clipboard_monitor_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.h
  )
  target_include_directories(test_clipboard_monitor PRIVATE "${CMAKE_SOURCE_DIR}/qmlui")
  target_link_libraries(test_clipboard_monitor PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
  add_test(NAME test_clipboard_monitor COMMAND test_clipboard_monitor)
  if(NOT WIN32)
    set_tests_properties(test_clipboard_monitor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()

  # Q-10 覆盖收口：发音练习面板的非设备逻辑（gui 无库目标，源码直接挂进
  # 测试 target；只编 UNIDICT_GUI_PRON=OFF 形态——覆盖门禁树同口径，评分
  # 路径属 unidict_pron 域不在此测）。Qt6::Widgets 让 QTEST_MAIN 起
  # QApplication（QDialog 需要）；测试不作任何音频设备假设
  add_executable(test_pronunciation_panel
    pronunciation_panel_test.cpp
    ${CMAKE_SOURCE_DIR}/gui/pronunciation_panel.cpp
    ${CMAKE_SOURCE_DIR}/gui/pronunciation_panel.h
    ${CMAKE_SOURCE_DIR}/gui/audio_recorder.cpp
    ${CMAKE_SOURCE_DIR}/gui/audio_recorder.h
    ${CMAKE_SOURCE_DIR}/gui/pcm_playback.cpp
    ${CMAKE_SOURCE_DIR}/gui/pcm_playback.h
    ${CMAKE_SOURCE_DIR}/gui/waveform_widget.cpp
    ${CMAKE_SOURCE_DIR}/gui/waveform_widget.h
  )
  target_include_directories(test_pronunciation_panel PRIVATE
    "${CMAKE_SOURCE_DIR}/gui"
    "${CMAKE_SOURCE_DIR}/core"  # pcm_util.h → std/pcm_util_std.h（header-only）
  )
  target_link_libraries(test_pronunciation_panel PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::TextToSpeech Qt6::Test)
  add_test(NAME test_pronunciation_panel COMMAND test_pronunciation_panel)
  if(NOT WIN32)
    set_tests_properties(test_pronunciation_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()

  # LearningManager（qmlui）此前 0% 覆盖：学习记录/答题/掌握度/标签笔记/
  # 日周进度统计/艾宾浩斯复习调度/成就/导入导出 24 个 Q_INVOKABLE 全无测试
  add_executable(test_learning_manager
    learning_manager_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/learning_manager.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/learning_manager.h
  )
  target_include_directories(test_learning_manager PRIVATE "${CMAKE_SOURCE_DIR}/qmlui")
  target_link_libraries(test_learning_manager PRIVATE Qt6::Core Qt6::Test)
  add_test(NAME test_learning_manager COMMAND test_learning_manager)

  # Theme 单例（qmlui）此前 0% 覆盖，是 Qt 层闸门 99.7% 的全部缺口：
  # 单例身份/亮暗 token 全表切换/同值短路/darkChanged/Q_PROPERTY 路径
  add_executable(test_theme
    theme_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/theme.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/theme.h
  )
  target_include_directories(test_theme PRIVATE "${CMAKE_SOURCE_DIR}/qmlui")
  target_link_libraries(test_theme PRIVATE Qt6::Core Qt6::Test)
  add_test(NAME test_theme COMMAND test_theme)

  # Qt 层 100% 收口：移动平台工具的桌面可编译面（平台旗标/选择器降级/
  # 路径与权限桌面语义）。QStandardPaths 测试模式隔离沙箱，无需 GUI
  add_executable(test_mobile_utils
    mobile_utils_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/mobile_utils.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/mobile_utils.h
  )
  target_include_directories(test_mobile_utils PRIVATE "${CMAKE_SOURCE_DIR}/qmlui")
  target_link_libraries(test_mobile_utils PRIVATE Qt6::Core Qt6::Test)
  add_test(NAME test_mobile_utils COMMAND test_mobile_utils)

  # Qt CLI（cli/main.cpp）此前 0% 覆盖：参数解析/--list/查词退出码/
  # 词典自动加载路径，子进程驱动真 unidict_cli（coverage 构建里子进程
  # 的 .gcda 写回本树，gcovr 归并）。unidict_cli 目标只在 QT_APPS 开时
  # 存在，条件挂 TARGET 上
  if(TARGET unidict_cli)
    add_executable(test_cli_main cli_main_test.cpp)
    target_link_libraries(test_cli_main PRIVATE Qt6::Core Qt6::Test)
    target_compile_definitions(test_cli_main PRIVATE
      UNIDICT_CLI_PATH="$<TARGET_FILE:unidict_cli>"
      UNIDICT_DICT_JSON="${CMAKE_SOURCE_DIR}/examples/dict.json")
    add_test(NAME test_cli_main COMMAND test_cli_main)
  endif()

  # FullTextManagerQt（adapters/qt）此前 0% 覆盖：索引落盘/加载/版本协商
  # （UDFT1/2/3）、签名校验、升级、源差异诊断与导出
  add_executable(test_fulltext_manager_qt
    fulltext_manager_qt_test.cpp
  )
  target_include_directories(test_fulltext_manager_qt PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/core"
    "${CMAKE_SOURCE_DIR}/adapters/qt"
  )
  target_link_libraries(test_fulltext_manager_qt PRIVATE
    unidict_fulltext_qt
    unidict_core_qt
    Qt6::Core
    Qt6::Test
  )
  add_test(NAME test_fulltext_manager_qt COMMAND test_fulltext_manager_qt)

  # SyncServiceQt（adapters/qt）此前 0% 覆盖：文件级同步的扫描/比对/合并/
  # 冲突预览与按条目选择应用——合并写回前先清库，判据错了会静默丢用户数据。
  # 该 TU 目前只有 qmlui 直接编译，没有库目标，照 test_learning_manager
  # 的做法把源码挂进测试 target
  add_executable(test_sync_service_qt
    sync_service_qt_test.cpp
    ${CMAKE_SOURCE_DIR}/adapters/qt/sync_service_qt.cpp
    ${CMAKE_SOURCE_DIR}/adapters/qt/sync_service_qt.h
  )
  target_include_directories(test_sync_service_qt PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/core"
    "${CMAKE_SOURCE_DIR}/adapters/qt"
  )
  target_link_libraries(test_sync_service_qt PRIVATE
    unidict_core_qt
    Qt6::Core
    Qt6::Test
  )
  add_test(NAME test_sync_service_qt COMMAND test_sync_service_qt)

  add_executable(test_lookup_adapter
    lookup_adapter_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/lookup_adapter.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/lookup_adapter.h
    ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.h
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.h
  )
  target_include_directories(test_lookup_adapter PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/core"
    "${CMAKE_SOURCE_DIR}/adapters/qt"
    "${CMAKE_SOURCE_DIR}/qmlui"
  )
  target_link_libraries(test_lookup_adapter PRIVATE
    unidict_core_qt
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::TextToSpeech
    Qt6::Network
    Qt6::Multimedia
    # lookup_adapter_test 经 mdict_fixture.h 直含 zlib.h，MSVC 无传递包含
    # （core 的 PRIVATE 链够不到测试 target），缺了在 Windows CI 直接
    # C1083 编译失败——逐 slot 诊断步从未有机会跑起来
    ZLIB::ZLIB
  )
  # 词条呈现与 .mdd 资源闭环（P0 ①）：真 .mdx + 真 .mdd 走全链路
  add_executable(test_entry_presentation
    entry_presentation_test.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/lookup_adapter.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/lookup_adapter.h
    ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.h
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/global_hotkeys.h
  )
  target_include_directories(test_entry_presentation PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/core"
    "${CMAKE_SOURCE_DIR}/adapters/qt"
    "${CMAKE_SOURCE_DIR}/qmlui"
  )
  target_link_libraries(test_entry_presentation PRIVATE
    unidict_core_qt
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::TextToSpeech
    Qt6::Network
    Qt6::Multimedia
    ZLIB::ZLIB # 同 test_lookup_adapter：mdict_fixture.h 直含 zlib.h
  )
  add_test(NAME test_entry_presentation COMMAND test_entry_presentation)

  add_test(NAME test_lookup_adapter COMMAND test_lookup_adapter)
  # 无头 Linux/macOS 用 offscreen 平台插件；Windows 不强制——CI runner 有
  # 交互桌面，Qt 6.10 的 offscreen 插件路径在 Windows 上静默崩溃（无输出
  # 退出），走默认 qwindows 主路径更稳
  if(NOT WIN32)
    set_tests_properties(test_lookup_adapter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    set_tests_properties(test_entry_presentation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()
endif()

# Std-only index engine test (no Qt dependency)
add_executable(test_index_engine_std
    index_engine_std_test.cpp
)

target_link_libraries(test_index_engine_std PRIVATE
    unidict_index_std
)

add_test(NAME test_index_engine_std COMMAND test_index_engine_std)

add_executable(test_data_store_std
    data_store_std_test.cpp
)

target_link_libraries(test_data_store_std PRIVATE unidict_std_core)

add_test(NAME test_data_store_std COMMAND test_data_store_std)

add_executable(test_json_parser_std
    json_parser_std_test.cpp
)

target_link_libraries(test_json_parser_std PRIVATE unidict_std_core)

add_test(NAME test_json_parser_std COMMAND test_json_parser_std)

add_executable(test_stardict_std
    stardict_std_test.cpp
)

target_link_libraries(test_stardict_std PRIVATE unidict_std_core)

add_test(NAME test_stardict_std COMMAND test_stardict_std)

add_executable(test_stardict_sts_std
    stardict_sts_std_test.cpp
)

target_link_libraries(test_stardict_sts_std PRIVATE unidict_std_core)

add_test(NAME test_stardict_sts_std COMMAND test_stardict_sts_std)

add_executable(test_text_norm_std
    text_norm_std_test.cpp
)

target_link_libraries(test_text_norm_std PRIVATE unidict_index_std)

add_test(NAME test_text_norm_std COMMAND test_text_norm_std)

add_executable(test_mdict_std
    mdict_std_test.cpp
)

target_link_libraries(test_mdict_std PRIVATE unidict_std_core)

add_test(NAME test_mdict_std COMMAND test_mdict_std)

add_executable(test_epub_zip_std
    epub_zip_std_test.cpp
)
# EPUB 解析边界补测：整条 decode_entities 实体族、容器/OPF 异常形态、
# heading 判定、词条状态机畸形输入（epub_parser_std.cpp 此前仅 80% 覆盖）
add_executable(test_epub_parser_std_cover
    epub_parser_std_cover_test.cpp
)

target_link_libraries(test_epub_zip_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_epub_zip_std COMMAND test_epub_zip_std)

target_link_libraries(test_epub_parser_std_cover PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_epub_parser_std_cover COMMAND test_epub_parser_std_cover)

# EPUB 分支缺口补测：畸形实体矩阵、闭引号缺失、extract_element_text
# 形态矩阵（属性空格/EOF 截断/无 '>'/无闭标签）、heading 前缀误配、
# 状态机 stray 闭标签
add_executable(test_epub_parser_branches_std
    epub_parser_std_branches_test.cpp
)
target_link_libraries(test_epub_parser_branches_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_epub_parser_branches_std COMMAND test_epub_parser_branches_std)

# ZipReaderStd 防御分支：EOCD 自洽性 / zip64 拒收 / CDE 越界与签名 /
# LFH 与条目数据越界 / 单条目解压上限 / deflate 流损坏·截断·长度·CRC
add_executable(test_zip_reader_std_defense
    zip_reader_std_defense_test.cpp
)
target_link_libraries(test_zip_reader_std_defense PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_zip_reader_std_defense COMMAND test_zip_reader_std_defense)

# core/std 残余缺口集中补测：html 不安全 URL 剔除与资源解析器早退、
# text_norm 四字节码点与非法序列、dsl 续行拼接、index_engine 通配符
# 元字符转义、ctc_logits 行最大值不在首元素、pcm_util 的 WAV 畸形布局、
# 解密器未知算法名、aggregate 释义长度分档
add_executable(test_core_std_cover
    core_std_cover_test.cpp
)
target_link_libraries(test_core_std_cover PRIVATE unidict_std_core)
add_test(NAME test_core_std_cover COMMAND test_core_std_cover)

# load_fulltext_index_relaxed 的 accept_version 闸门：auto 兼容模式只放行
# legacy v1，被拒的签名不匹配索引不得留在内存里继续服务全文检索
add_executable(test_fulltext_accept_version
    fulltext_accept_version_test.cpp
)
target_link_libraries(test_fulltext_accept_version PRIVATE unidict_std_core)
add_test(NAME test_fulltext_accept_version COMMAND test_fulltext_accept_version)

add_executable(test_stardict_dz_std
    stardict_dz_std_test.cpp
)

target_link_libraries(test_stardict_dz_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_stardict_dz_std COMMAND test_stardict_dz_std)

add_executable(test_stardict_parser_full_std
    stardict_parser_std_full_test.cpp
)

target_link_libraries(test_stardict_parser_full_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_stardict_parser_full_std COMMAND test_stardict_parser_full_std)

add_executable(test_mdict_zlib_std
    mdict_zlib_std_test.cpp
)

target_link_libraries(test_mdict_zlib_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_zlib_std COMMAND test_mdict_zlib_std)

add_executable(test_mdict_utf16_header_std
    mdict_utf16_header_std_test.cpp
)

target_link_libraries(test_mdict_utf16_header_std PRIVATE unidict_std_core)

add_test(NAME test_mdict_utf16_header_std COMMAND test_mdict_utf16_header_std)

add_executable(test_mdict_encrypted_header_std
    mdict_encrypted_header_std_test.cpp
)
target_link_libraries(test_mdict_encrypted_header_std PRIVATE unidict_std_core)
add_test(NAME test_mdict_encrypted_header_std COMMAND test_mdict_encrypted_header_std)

add_executable(test_mdict_password_env_std
    mdict_password_env_std_test.cpp
)
target_link_libraries(test_mdict_password_env_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_password_env_std COMMAND test_mdict_password_env_std)

add_executable(test_mdict_invalid_kidx_oob_std
    mdict_invalid_kidx_oob_std_test.cpp
)
target_link_libraries(test_mdict_invalid_kidx_oob_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_invalid_kidx_oob_std COMMAND test_mdict_invalid_kidx_oob_std)

add_executable(test_mdict_mdxk_ulen_cap_std
    mdict_mdxk_ulen_cap_std_test.cpp
)
target_link_libraries(test_mdict_mdxk_ulen_cap_std PRIVATE unidict_std_core)
add_test(NAME test_mdict_mdxk_ulen_cap_std COMMAND test_mdict_mdxk_ulen_cap_std)

add_executable(test_mdict_tsv_std
    mdict_tsv_std_test.cpp
)

target_link_libraries(test_mdict_tsv_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_tsv_std COMMAND test_mdict_tsv_std)

add_executable(test_mdict_simplekv_std
    mdict_simplekv_std_test.cpp
)

target_link_libraries(test_mdict_simplekv_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_simplekv_std COMMAND test_mdict_simplekv_std)

add_executable(test_mdict_mdd_resources_std
    mdict_mdd_resources_std_test.cpp
)

target_link_libraries(test_mdict_mdd_resources_std PRIVATE unidict_std_core)

add_test(NAME test_mdict_mdd_resources_std COMMAND test_mdict_mdd_resources_std)

add_executable(test_mdict_parser_cover_std
    mdict_parser_std_cover_test.cpp
)
target_link_libraries(test_mdict_parser_cover_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_parser_cover_std COMMAND test_mdict_parser_cover_std)

# 分支缺口补测（lines 已 100% 后收 branches Missing 最大模块）
add_executable(test_mdict_parser_branches_std
    mdict_parser_std_branches_test.cpp
)
target_link_libraries(test_mdict_parser_branches_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_parser_branches_std COMMAND test_mdict_parser_branches_std)

# 分支缺口第二批：五个容器解析器的截断/畸形阶梯、加密成功路径
# （XOR 密钥流 mt19937(hash(pw)) 可复现）、UTF-16 头部、manifest 坏行、
# 启发式各降级面与 decompress_all 上限
add_executable(test_mdict_parser_branches2_std
    mdict_parser_std_branches2_test.cpp
)
target_link_libraries(test_mdict_parser_branches2_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_parser_branches2_std COMMAND test_mdict_parser_branches2_std)

# 分支缺口第三批：aggregate_lookup 的 source 三元死臂清理后的
# 真实输入补测（长短词典名、上限卫语句、开关臂、停用词典、
# 释义内容边角、manager 边界、builder best_entry 两臂）
add_executable(test_aggregate_lookup_branches_std
    aggregate_lookup_std_branches_test.cpp
)
target_link_libraries(test_aggregate_lookup_branches_std PRIVATE unidict_std_core)
add_test(NAME test_aggregate_lookup_branches_std COMMAND test_aggregate_lookup_branches_std)

# 分支缺口第四批：data_store 的持久化解析半边（find_section 字符串感知
# 深度计数、for_each_object 状态机、obj_val/int/num/str_array 容错守卫、
# 畸形对象与区段、负数与非数字、空时间戳写省略）与 API 守卫臂
add_executable(test_data_store_branches_std
    data_store_std_branches_test.cpp
)
target_link_libraries(test_data_store_branches_std PRIVATE unidict_std_core)
add_test(NAME test_data_store_branches_std COMMAND test_data_store_branches_std)

# 分支缺口第五批：fulltext_index 的持久化半边（UDFT3 逐字段截断矩阵、
# UDFT1 posting 截断、坏 varint 三态、幽灵词回存、空词项）、
# 线程数兜底/钳制、idf 兜底、查询去重、候选扩展上限与 gram 归档跳过臂
add_executable(test_fulltext_index_branches_std
    fulltext_index_std_branches_test.cpp
)
target_link_libraries(test_fulltext_index_branches_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_index_branches_std COMMAND test_fulltext_index_branches_std)

# 分支缺口第六簇：dictionary_manager 的 Holder 链各臂与四扩展名加载失败、
# ifo 伴生文件矩阵（dict/仅 dz/全无）、mdx 同目录伴生扫描四象限、
# enabled 同态/翻转、search_all 禁用跳过与全 miss、full_text_search
# 守卫与上限 break、relaxed 索引越界 docId 四态、null 出参两态、
# accept_version=1 拒绝 v3、签名的源缺失与文件变目录。真实输入驱动。
# 测试直接用 gzopen 造 .dict.dz，需要显式链 zlib（MSVC 无传递包含）
add_executable(test_dictionary_manager_branches_std
    dictionary_manager_std_branches_test.cpp
)
target_link_libraries(test_dictionary_manager_branches_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_dictionary_manager_branches_std COMMAND test_dictionary_manager_branches_std)

add_executable(test_mdict_kidx_std
    mdict_kidx_std_test.cpp
)

target_link_libraries(test_mdict_kidx_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_kidx_std COMMAND test_mdict_kidx_std)

add_executable(test_mdict_keyb_std
    mdict_keyb_std_test.cpp
)

target_link_libraries(test_mdict_keyb_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_keyb_std COMMAND test_mdict_keyb_std)

add_executable(test_mdict_keyb_empty_std
    mdict_keyb_empty_std_test.cpp
)
target_link_libraries(test_mdict_keyb_empty_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_keyb_empty_std COMMAND test_mdict_keyb_empty_std)

add_executable(test_mdict_kbix_std
    mdict_kbix_std_test.cpp
)

target_link_libraries(test_mdict_kbix_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_kbix_std COMMAND test_mdict_kbix_std)

add_executable(test_mdict_kbix_multi_std
    mdict_kbix_multi_std_test.cpp
)

target_link_libraries(test_mdict_kbix_multi_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_kbix_multi_std COMMAND test_mdict_kbix_multi_std)

add_executable(test_mdict_kbix_count_mismatch_std
    mdict_kbix_count_mismatch_std_test.cpp
)
target_link_libraries(test_mdict_kbix_count_mismatch_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_kbix_count_mismatch_std COMMAND test_mdict_kbix_count_mismatch_std)

add_executable(test_mdict_mdxkr_std
    mdict_mdxkr_std_test.cpp
)

target_link_libraries(test_mdict_mdxkr_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_mdxkr_std COMMAND test_mdict_mdxkr_std)

add_executable(test_mdict_mdxk_composite_edge_std
    mdict_mdxk_composite_edge_std_test.cpp
)
target_link_libraries(test_mdict_mdxk_composite_edge_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_mdxk_composite_edge_std COMMAND test_mdict_mdxk_composite_edge_std)

add_executable(test_mdict_mdxk_oob_offsets_std
    mdict_mdxk_oob_offsets_std_test.cpp
)
target_link_libraries(test_mdict_mdxk_oob_offsets_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_mdxk_oob_offsets_std COMMAND test_mdict_mdxk_oob_offsets_std)

add_executable(test_mdict_mdxk_garbage_tail_std
    mdict_mdxk_garbage_tail_std_test.cpp
)
target_link_libraries(test_mdict_mdxk_garbage_tail_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdict_mdxk_garbage_tail_std COMMAND test_mdict_mdxk_garbage_tail_std)

add_executable(test_mdict_realheur_std
    mdict_realheur_std_test.cpp
)

target_link_libraries(test_mdict_realheur_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdict_realheur_std COMMAND test_mdict_realheur_std)

add_executable(test_dsl_std
    dsl_parser_std_test.cpp
)

target_link_libraries(test_dsl_std PRIVATE unidict_std_core)

add_test(NAME test_dsl_std COMMAND test_dsl_std)

add_executable(test_dictionary_manager_std
    dictionary_manager_std_test.cpp
)

# 测试直接用 gzopen 造 .dict.dz，需要显式链 zlib（MSVC 无传递包含）
target_link_libraries(test_dictionary_manager_std PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_dictionary_manager_std COMMAND test_dictionary_manager_std)

add_executable(test_dictionary_std
    dictionary_std_test.cpp
)

target_link_libraries(test_dictionary_std PRIVATE unidict_std_core)

add_test(NAME test_dictionary_std COMMAND test_dictionary_std)

add_executable(test_dictionary_manager_std_priority
    dictionary_manager_std_priority_test.cpp
)

target_link_libraries(test_dictionary_manager_std_priority PRIVATE unidict_std_core)

add_test(NAME test_dictionary_manager_std_priority COMMAND test_dictionary_manager_std_priority)

add_executable(test_dictionary_manager_std_quarantine
    dictionary_manager_std_quarantine_test.cpp
)

target_link_libraries(test_dictionary_manager_std_quarantine PRIVATE unidict_std_core)

add_test(NAME test_dictionary_manager_std_quarantine COMMAND test_dictionary_manager_std_quarantine)

add_executable(test_dictionary_manager_std_state
    dictionary_manager_std_state_test.cpp
)

target_link_libraries(test_dictionary_manager_std_state PRIVATE unidict_std_core)

add_test(NAME test_dictionary_manager_std_state COMMAND test_dictionary_manager_std_state)

add_executable(test_dictionary_manager_std_grouped
    dictionary_manager_std_grouped_test.cpp
)

target_link_libraries(test_dictionary_manager_std_grouped PRIVATE unidict_std_core)

add_test(NAME test_dictionary_manager_std_grouped COMMAND test_dictionary_manager_std_grouped)

add_executable(test_parser_registry_std
    parser_registry_std_test.cpp
)

target_link_libraries(test_parser_registry_std PRIVATE unidict_std_core)

add_test(NAME test_parser_registry_std COMMAND test_parser_registry_std)

add_executable(test_html_renderer_highlight_std
    html_renderer_highlight_std_test.cpp
)

target_link_libraries(test_html_renderer_highlight_std PRIVATE unidict_std_core)

add_test(NAME test_html_renderer_highlight_std COMMAND test_html_renderer_highlight_std)

add_executable(test_index_persistence_std
    index_persistence_std_test.cpp
)

target_link_libraries(test_index_persistence_std PRIVATE unidict_std_core)

add_test(NAME test_index_persistence_std COMMAND test_index_persistence_std)

if(UNIDICT_BUILD_STD_CLI)
  add_test(NAME test_cli_std_smoke
    COMMAND $<TARGET_FILE:unidict_cli_std> hello)
  # Ensure examples dict is available via env var
  set_tests_properties(test_cli_std_smoke PROPERTIES
    ENVIRONMENT "UNIDICT_DICTS=${CMAKE_SOURCE_DIR}/examples/dict.json")

  add_executable(test_cli_std_mdict_password
    cli_std_mdict_password_test.cpp
  )
  target_link_libraries(test_cli_std_mdict_password PRIVATE ZLIB::ZLIB)
  add_test(NAME test_cli_std_mdict_password
    COMMAND test_cli_std_mdict_password $<TARGET_FILE:unidict_cli_std>)
endif()

add_executable(test_cache_prune_std
    cache_prune_std_test.cpp
)

target_link_libraries(test_cache_prune_std PRIVATE unidict_std_core)

add_test(NAME test_cache_prune_std COMMAND test_cache_prune_std)

# 在线发音源：URL 拼装/转义、容忍式解析（口音推断/去重/截断不崩）、
# pick_clip 偏好链
add_executable(test_online_pron_std
    online_pron_std_test.cpp
)
target_link_libraries(test_online_pron_std PRIVATE unidict_std_core)
add_test(NAME test_online_pron_std COMMAND test_online_pron_std)

add_executable(test_fulltext_index_std
    fulltext_index_std_test.cpp
)
target_link_libraries(test_fulltext_index_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_index_std COMMAND test_fulltext_index_std)

# clear() 完整复位契约：诊断出口（version/signature/stats）不得留旧值，
# 且复用索引的结果要与全新实例一致（terms_sorted_ 悬空指针回归）
add_executable(test_fulltext_index_std_clear
    fulltext_index_std_clear_test.cpp
)
target_link_libraries(test_fulltext_index_std_clear PRIVATE unidict_std_core)
add_test(NAME test_fulltext_index_std_clear COMMAND test_fulltext_index_std_clear)

add_executable(test_fulltext_index_std_cover
    fulltext_index_std_cover_test.cpp
)
target_link_libraries(test_fulltext_index_std_cover PRIVATE unidict_std_core)
add_test(NAME test_fulltext_index_std_cover COMMAND test_fulltext_index_std_cover)

add_executable(test_fulltext_lazy_decode_stats_std
    fulltext_lazy_decode_stats_std_test.cpp
)
target_link_libraries(test_fulltext_lazy_decode_stats_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_lazy_decode_stats_std COMMAND test_fulltext_lazy_decode_stats_std)

add_executable(test_index_engine_std_ops
    index_engine_std_ops_test.cpp
)
target_link_libraries(test_index_engine_std_ops PRIVATE unidict_index_std)
add_test(NAME test_index_engine_std_ops COMMAND test_index_engine_std_ops)

add_executable(test_data_store_std_csv_escape
    data_store_std_csv_escape_test.cpp
)
target_link_libraries(test_data_store_std_csv_escape PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_csv_escape COMMAND test_data_store_std_csv_escape)

add_executable(test_data_store_std_vocab_m3
    data_store_std_vocab_m3_test.cpp
)
target_link_libraries(test_data_store_std_vocab_m3 PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_vocab_m3 COMMAND test_data_store_std_vocab_m3)

add_executable(test_data_store_std_remove
    data_store_std_remove_test.cpp
)
target_link_libraries(test_data_store_std_remove PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_remove COMMAND test_data_store_std_remove)

add_executable(test_data_store_std_tags
    data_store_std_tags_test.cpp
)
target_link_libraries(test_data_store_std_tags PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_tags COMMAND test_data_store_std_tags)

add_executable(test_data_store_std_notes
    data_store_std_notes_test.cpp
)
target_link_libraries(test_data_store_std_notes PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_notes COMMAND test_data_store_std_notes)

add_executable(test_csv_parser_std
    csv_parser_std_test.cpp
)

target_link_libraries(test_csv_parser_std PRIVATE unidict_std_core)

add_test(NAME test_csv_parser_std COMMAND test_csv_parser_std)

add_executable(test_csv_parser_std_cover
    csv_parser_std_cover_test.cpp
)
target_link_libraries(test_csv_parser_std_cover PRIVATE unidict_std_core)
add_test(NAME test_csv_parser_std_cover COMMAND test_csv_parser_std_cover)

add_executable(test_data_store_std_cover
    data_store_std_cover_test.cpp
)
target_link_libraries(test_data_store_std_cover PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_cover COMMAND test_data_store_std_cover)

# DataStoreStd JSON 转义/解析对称性：转义往返、多轮存盘幂等（钉死"每轮
# 载入存盘体积翻倍"的 store.json 膨胀）、释义里的结构字符不截断区段
add_executable(test_data_store_std_escape
    data_store_std_escape_test.cpp
)
target_link_libraries(test_data_store_std_escape PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_escape COMMAND test_data_store_std_escape)

add_executable(test_mdict_decryptor_std
    mdict_decryptor_std_test.cpp
)

target_link_libraries(test_mdict_decryptor_std PRIVATE unidict_std_core)

add_test(NAME test_mdict_decryptor_std COMMAND test_mdict_decryptor_std)

add_executable(test_mdict_decryptor_std_cover
    mdict_decryptor_std_cover_test.cpp
)
target_link_libraries(test_mdict_decryptor_std_cover PRIVATE unidict_std_core)
add_test(NAME test_mdict_decryptor_std_cover COMMAND test_mdict_decryptor_std_cover)

add_executable(test_json_parser_std_edge
    json_parser_std_edge_test.cpp
)
target_link_libraries(test_json_parser_std_edge PRIVATE unidict_std_core)
add_test(NAME test_json_parser_std_edge COMMAND test_json_parser_std_edge)

# std 面查词口径：折叠键回退 + 释义全文兜底 + 计数与可查一致（BUG-005）
add_executable(test_std_lookup_parity
    std_lookup_parity_test.cpp
)
target_link_libraries(test_std_lookup_parity PRIVATE unidict_std_core)
add_test(NAME test_std_lookup_parity COMMAND test_std_lookup_parity)

add_executable(test_dictionary_manager_std_remove
    dictionary_manager_std_remove_test.cpp
)
target_link_libraries(test_dictionary_manager_std_remove PRIVATE unidict_std_core)
add_test(NAME test_dictionary_manager_std_remove COMMAND test_dictionary_manager_std_remove)

if(UNIDICT_BUILD_STD_CLI)
  add_test(NAME test_cli_std_fulltext_smoke
    COMMAND $<TARGET_FILE:unidict_cli_std> --mode fulltext greet)
  set_tests_properties(test_cli_std_fulltext_smoke PROPERTIES
    ENVIRONMENT "UNIDICT_DICTS=${CMAKE_SOURCE_DIR}/examples/dict.json")
endif()

add_executable(test_index_engine_std_edge
    index_engine_std_edge_test.cpp
)
target_link_libraries(test_index_engine_std_edge PRIVATE unidict_index_std)
add_test(NAME test_index_engine_std_edge COMMAND test_index_engine_std_edge)

# StarDict charset 支持（GBK/Latin-1/CP1252 + 误标兜底）
# 查词键归一化的专业场景回归（大小写/标点/不可见字符/组合记号/连字/空白）
# MDX 加密原语（RIPEMD-128 + 半字节交换链式 XOR）
add_executable(test_mdict_crypto_std
    mdict_crypto_std_test.cpp
)
target_link_libraries(test_mdict_crypto_std PRIVATE unidict_std_core)
add_test(NAME test_mdict_crypto_std COMMAND test_mdict_crypto_std)

add_executable(test_text_norm_std_pro
    text_norm_std_pro_test.cpp
)
target_link_libraries(test_text_norm_std_pro PRIVATE unidict_index_std)
add_test(NAME test_text_norm_std_pro COMMAND test_text_norm_std_pro)

add_executable(test_stardict_charset_std
    stardict_charset_std_test.cpp
)
target_link_libraries(test_stardict_charset_std PRIVATE unidict_std_core)
add_test(NAME test_stardict_charset_std COMMAND test_stardict_charset_std)

add_executable(test_stardict_malformed_std
    stardict_malformed_std_test.cpp
)
target_link_libraries(test_stardict_malformed_std PRIVATE unidict_std_core)
add_test(NAME test_stardict_malformed_std COMMAND test_stardict_malformed_std)

add_executable(test_fulltext_persistence_std
    fulltext_persistence_std_test.cpp
)
target_link_libraries(test_fulltext_persistence_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_persistence_std COMMAND test_fulltext_persistence_std)

add_executable(test_fulltext_persistence_mismatch_std
    fulltext_persistence_mismatch_std_test.cpp
)
target_link_libraries(test_fulltext_persistence_mismatch_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_persistence_mismatch_std COMMAND test_fulltext_persistence_mismatch_std)

add_executable(test_fulltext_persistence_pathchange_std
    fulltext_persistence_pathchange_std_test.cpp
)
target_link_libraries(test_fulltext_persistence_pathchange_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_persistence_pathchange_std COMMAND test_fulltext_persistence_pathchange_std)

add_executable(test_fulltext_persistence_stardict_companion_change_std
    fulltext_persistence_stardict_companion_change_std_test.cpp
)
target_link_libraries(test_fulltext_persistence_stardict_companion_change_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_persistence_stardict_companion_change_std COMMAND test_fulltext_persistence_stardict_companion_change_std)

add_executable(test_fulltext_udft3_format_std
    fulltext_udft3_format_std_test.cpp
)
target_link_libraries(test_fulltext_udft3_format_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_udft3_format_std COMMAND test_fulltext_udft3_format_std)

add_executable(test_fulltext_std
    fulltext_std_test.cpp
)

target_link_libraries(test_fulltext_std PRIVATE unidict_std_core)

add_test(NAME test_fulltext_std COMMAND test_fulltext_std)

add_executable(test_fulltext_parallel_build_std
    fulltext_parallel_build_std_test.cpp
)
target_link_libraries(test_fulltext_parallel_build_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_parallel_build_std COMMAND test_fulltext_parallel_build_std)

add_executable(test_fulltext_udft12_load_std
    fulltext_udft12_load_std_test.cpp
)
target_link_libraries(test_fulltext_udft12_load_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_udft12_load_std COMMAND test_fulltext_udft12_load_std)

add_executable(test_fulltext_tie_break_std
    fulltext_tie_break_std_test.cpp
)
target_link_libraries(test_fulltext_tie_break_std PRIVATE unidict_std_core)
add_test(NAME test_fulltext_tie_break_std COMMAND test_fulltext_tie_break_std)

add_executable(test_path_utils_env_days_std
    path_utils_env_days_std_test.cpp
)
target_link_libraries(test_path_utils_env_days_std PRIVATE unidict_std_core)
add_test(NAME test_path_utils_env_days_std COMMAND test_path_utils_env_days_std)

# path_utils 分支缺口补测：空环境值回落、ensure_dir 三态、clear_cache
# 缺目录/目录是文件/空目录矩阵、cache_size 子目录、prune_bytes
# under-limit 早退与达标 break、prune_days days<=0 早退与 mtime 回写
add_executable(test_path_utils_branches_std
    path_utils_std_branches_test.cpp
)
target_link_libraries(test_path_utils_branches_std PRIVATE unidict_std_core)
add_test(NAME test_path_utils_branches_std COMMAND test_path_utils_branches_std)

add_executable(test_dictionary_manager_std_meta
    dictionary_manager_std_meta_std_test.cpp
)
target_link_libraries(test_dictionary_manager_std_meta PRIVATE unidict_std_core)
add_test(NAME test_dictionary_manager_std_meta COMMAND test_dictionary_manager_std_meta)

# P0 Professional Dictionary Feature Tests
add_executable(test_html_renderer_std
    html_renderer_std_test.cpp
)
target_link_libraries(test_html_renderer_std PRIVATE unidict_std_core)
add_test(NAME test_html_renderer_std COMMAND test_html_renderer_std)

# render() 护栏：max_text_length_（UTF-8 边界安全截断）/ max_nesting_depth_
# （深度超限丢开标签，void 元素不计数）——两者此前声明了却从未生效
add_executable(test_html_renderer_std_limits
    html_renderer_std_limits_test.cpp
)
target_link_libraries(test_html_renderer_std_limits PRIVATE unidict_std_core)
add_test(NAME test_html_renderer_std_limits COMMAND test_html_renderer_std_limits)

add_executable(test_html_renderer_std_edge
    html_renderer_std_edge_test.cpp
)
target_link_libraries(test_html_renderer_std_edge PRIVATE unidict_std_core)
add_test(NAME test_html_renderer_std_edge COMMAND test_html_renderer_std_edge)

add_executable(test_html_renderer_std_cover
    html_renderer_std_cover_test.cpp
)
target_link_libraries(test_html_renderer_std_cover PRIVATE unidict_std_core)
add_test(NAME test_html_renderer_std_cover COMMAND test_html_renderer_std_cover)

# 分支缺口第七簇：html_renderer 的分词器畸形阶梯（未闭合注释/无名标签/
# 无'>'、注释头三半臂、空属性名/布尔属性/无值'='/引号在'>'后/未闭合
# 引号/非引号值）、utf8_safe_cut 回退到头、render 选项假臂、video 开
# 标签探测、带 resolver 的资源重写链、CSS 空段/无冒号段/单属性重建、
# 解析器同名目录守卫与扩展名剥离臂
add_executable(test_html_renderer_std_branches
    html_renderer_std_branches_test.cpp
)
target_link_libraries(test_html_renderer_std_branches PRIVATE unidict_std_core)
add_test(NAME test_html_renderer_std_branches COMMAND test_html_renderer_std_branches)

# stardict_parser 分支缺口补测：ifo/idx/dict 形态矩阵、缓存命中、
# 权限与环境形态、decode 全类型码矩阵、lookup 守卫
add_executable(test_stardict_parser_branches_std
    stardict_parser_std_branches_test.cpp
)
target_link_libraries(test_stardict_parser_branches_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_stardict_parser_branches_std COMMAND test_stardict_parser_branches_std)

add_executable(test_mdd_resource_std
    mdd_resource_std_test.cpp
)
target_link_libraries(test_mdd_resource_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdd_resource_std COMMAND test_mdd_resource_std)

# mdd_resource 分支缺口补测：V1/V2/SimpleKV 容器形态矩阵、解压扩容与
# 截断流、块签名与块内条目断臂、越界条目、缓存名形态与缓存失效、
# manager 缓存回落
add_executable(test_mdd_resource_branches_std
    mdd_resource_std_branches_test.cpp
)
target_link_libraries(test_mdd_resource_branches_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdd_resource_branches_std COMMAND test_mdd_resource_branches_std)

# 真实 MDict .mdd（引擎 2.0，writemdict 规格）兼容：头属性/长度矩阵、
# key/record 节畸形矩阵（字节手术）、UTF-16LE 键解码、跨块资源、
# 存储块、块缓存、取资源防线、manager 闭环与格式判别回归
add_executable(test_mdd_mdict_std
    mdd_mdict_std_test.cpp
)
target_link_libraries(test_mdd_mdict_std PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdd_mdict_std COMMAND test_mdd_mdict_std)

add_executable(test_cross_reference_std
    cross_reference_std_test.cpp
)
target_link_libraries(test_cross_reference_std PRIVATE unidict_std_core)
add_test(NAME test_cross_reference_std COMMAND test_cross_reference_std)

# cross_reference 分支缺口补测：URL 编解码矩阵、链接类型分类、HTML
# 重写、导航历史 export/import 形态矩阵、变体管理器文件载入矩阵
add_executable(test_cross_reference_branches_std
    cross_reference_std_branches_test.cpp
)
target_link_libraries(test_cross_reference_branches_std PRIVATE unidict_std_core)
add_test(NAME test_cross_reference_branches_std COMMAND test_cross_reference_branches_std)

add_executable(test_aggregate_lookup_std
    aggregate_lookup_std_test.cpp
)
target_link_libraries(test_aggregate_lookup_std PRIVATE unidict_std_core)
add_test(NAME test_aggregate_lookup_std COMMAND test_aggregate_lookup_std)

add_executable(test_mdd_resource_std_full
    mdd_resource_std_full_test.cpp
)

target_link_libraries(test_mdd_resource_std_full PRIVATE unidict_std_core ZLIB::ZLIB)

add_test(NAME test_mdd_resource_std_full COMMAND test_mdd_resource_std_full)

add_executable(test_mdd_resource_std_cover
    mdd_resource_std_cover_test.cpp
)
target_link_libraries(test_mdd_resource_std_cover PRIVATE unidict_std_core)
add_test(NAME test_mdd_resource_std_cover COMMAND test_mdd_resource_std_cover)

# MddResourceParser 残余路径：压缩资源解压链路、multi-block 格式、
# read_bytes 的句柄/边界护栏、缓存写读失败、MIME 探测全表
add_executable(test_mdd_resource_std_cover2
    mdd_resource_std_cover2_test.cpp
)
target_link_libraries(test_mdd_resource_std_cover2 PRIVATE unidict_std_core ZLIB::ZLIB)
add_test(NAME test_mdd_resource_std_cover2 COMMAND test_mdd_resource_std_cover2)

# 发音练习 M1 纯逻辑（波形降采样 + WAV 封装）：header-only，不链任何库。
# pcm_util.h 2026-09-25 下沉到 core/std/（gui/pcm_util.h 只是转发头），
# include 路径给 core/ 与 gui/ 两个兜底
add_executable(test_pcm_util_std
    pcm_util_std_test.cpp
)
target_include_directories(test_pcm_util_std PRIVATE
    ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/gui)
add_test(NAME test_pcm_util_std COMMAND test_pcm_util_std)

# 发音评分内核（M3a）：ARPAbet 表 / NW 对齐 / 词分聚合，链 unidict_std_core
add_executable(test_pronunciation_score_std
    pronunciation_score_std_test.cpp
)
target_link_libraries(test_pronunciation_score_std PRIVATE unidict_std_core)
add_test(NAME test_pronunciation_score_std COMMAND test_pronunciation_score_std)

# M3b 发音评分纯逻辑：espeak 映射 + CTC 强制对齐 + GOP
add_executable(test_espeak_arpabet_std
    espeak_arpabet_std_test.cpp
)
target_link_libraries(test_espeak_arpabet_std PRIVATE unidict_std_core)
add_test(NAME test_espeak_arpabet_std COMMAND test_espeak_arpabet_std)

add_executable(test_ctc_gop_std
    ctc_gop_std_test.cpp
)
target_link_libraries(test_ctc_gop_std PRIVATE unidict_std_core)
add_test(NAME test_ctc_gop_std COMMAND test_ctc_gop_std)

# M3b 适配器前置纯逻辑：模型词表解析 / 波形预处理 / fp16+log_softmax
add_executable(test_pron_vocab_std
    pron_vocab_std_test.cpp
)
target_link_libraries(test_pron_vocab_std PRIVATE unidict_std_core)
add_test(NAME test_pron_vocab_std COMMAND test_pron_vocab_std)

add_executable(test_pron_wave_std
    pron_wave_std_test.cpp
)
target_link_libraries(test_pron_wave_std PRIVATE unidict_std_core)
add_test(NAME test_pron_wave_std COMMAND test_pron_wave_std)

# ctc_logits_std.h header-only：测试自带 TU，链 std_core 只为 include 路径
add_executable(test_ctc_logits_std
    ctc_logits_std_test.cpp
)
target_link_libraries(test_ctc_logits_std PRIVATE unidict_std_core)
add_test(NAME test_ctc_logits_std COMMAND test_ctc_logits_std)

# M4 变体容忍表：GOP 取 max(主键, 变体)
add_executable(test_pron_variants_std
    pron_variants_std_test.cpp
)
target_link_libraries(test_pron_variants_std PRIVATE unidict_std_core)
add_test(NAME test_pron_variants_std COMMAND test_pron_variants_std)

# M4 词典 IPA 文本 → ARPAbet（ARPAbet 分词双模式 + espeak 表最长前缀）
add_executable(test_ipa_to_arpabet_std
    ipa_to_arpabet_std_test.cpp
)
target_link_libraries(test_ipa_to_arpabet_std PRIVATE unidict_std_core)
add_test(NAME test_ipa_to_arpabet_std COMMAND test_ipa_to_arpabet_std)

# M8 生词本联动：词分 →「发音不稳」标签增删规则
add_executable(test_pron_review_std
    pron_review_std_test.cpp
)
target_link_libraries(test_pron_review_std PRIVATE unidict_std_core)
add_test(NAME test_pron_review_std COMMAND test_pron_review_std)

# M9 发音练习历史：跟自己的进步比（累计/趋势死区/练习清单次序）
add_executable(test_pron_history_std
    pron_history_std_test.cpp
)
target_link_libraries(test_pron_history_std PRIVATE unidict_std_core)
add_test(NAME test_pron_history_std COMMAND test_pron_history_std)

# M9 发音练习记录的持久化往返与容错读取
add_executable(test_data_store_std_pron
    data_store_std_pron_test.cpp
)
target_link_libraries(test_data_store_std_pron PRIVATE unidict_std_core)
add_test(NAME test_data_store_std_pron COMMAND test_data_store_std_pron)

# M10 模型资产自举下载：SHA-256 实现（NIST 向量/块边界/流式文件）与
# 资产清单 + 下载计划/续传策略/校验/落地决策
add_executable(test_sha256_std
    sha256_std_test.cpp
)
target_link_libraries(test_sha256_std PRIVATE unidict_std_core)
add_test(NAME test_sha256_std COMMAND test_sha256_std)

add_executable(test_pron_model_source_std
    pron_model_source_std_test.cpp
)
target_link_libraries(test_pron_model_source_std PRIVATE unidict_std_core)
add_test(NAME test_pron_model_source_std COMMAND test_pron_model_source_std)

# M3b onnxruntime 推理壳：1.5KB Constant 假模型全链路（加载→推理→
# 张量解析→对齐→GOP）。整个 target 挂 UNIDICT_BUILD_PRON——OFF 时
# ORT 完全不参与构建（docs/pronunciation-plan.md 分层纪律）
if(UNIDICT_BUILD_PRON)
    add_executable(test_onnx_pron_scorer
        onnx_pron_scorer_test.cpp
    )
    target_link_libraries(test_onnx_pron_scorer PRIVATE unidict_pron)
    target_compile_definitions(test_onnx_pron_scorer PRIVATE
        UNIDICT_PRON_FAKE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME test_onnx_pron_scorer COMMAND test_onnx_pron_scorer)
endif()

# server_plan §7 B1:同步中转协议契约符合性套件(Python dev 参考实现,
# 不依赖 Qt/core;Worker 形态单测见 server/sync_relay/worker/test/)
find_program(PYTHON3_EXECUTABLE python3)
if(PYTHON3_EXECUTABLE)
    add_test(NAME sync_relay_protocol
             COMMAND ${PYTHON3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/server/sync_relay/dev/test_relay_protocol.py)
endif()

# server_plan §7 B2:客户端同步引擎(指令生成/推拉回放/快照压缩/断点
# 续传)。内存 relay 模拟 B1 协议语义,故障注入旗标驱动失败路径
add_executable(test_sync_engine_std
    sync_engine_std_test.cpp
)
target_link_libraries(test_sync_engine_std PRIVATE unidict_std_core)
add_test(NAME test_sync_engine_std COMMAND test_sync_engine_std)

# §7 B3-a：XChaCha/HKDF/HMAC 权威向量（RFC 8439 / xchacha draft / RFC
# 5869 / RFC 4231）+ 组密钥环与配对码语义
add_executable(test_crypto_std
    crypto_std_test.cpp
)
target_link_libraries(test_crypto_std PRIVATE unidict_std_core)
add_test(NAME test_crypto_std COMMAND test_crypto_std)

add_executable(test_sync_crypto_std
    sync_crypto_std_test.cpp
)
target_link_libraries(test_sync_crypto_std PRIVATE unidict_std_core)
add_test(NAME test_sync_crypto_std COMMAND test_sync_crypto_std)
