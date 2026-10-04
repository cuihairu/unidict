# core/CMakeLists.txt

# Legacy Qt core target moved to adapters/qt (target: unidict_core_qt).

# Experimental: Qt-free index engine (std-only). This is a separate target for now
# and is intended to replace the Qt-based IndexEngine gradually.
add_library(unidict_index_std STATIC
    std/index_engine_std.cpp
    std/text_norm_std.cpp
    std/text_norm_std.h
)

target_include_directories(unidict_index_std PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/std"
)

# Std-only core components (parsers, utils, datastore)
find_package(ZLIB REQUIRED)
add_library(unidict_std_core STATIC
    std/path_utils_std.cpp
    std/data_store_std.cpp
    std/json_parser_std.cpp
    std/stardict_parser_std.cpp
    std/text_norm_case_std.inc
    std/text_norm_fold_std.inc
    std/charset_codec_std.cpp
    std/charset_codec_std.h
    std/gb18030_table_std.inc
    std/dictionary_manager_std.cpp
    std/dictionary_manager_std.h
    # 词典实例类（P-3 3.1 抽取）：manager 只持 DictionaryStd
    std/dictionary_std.cpp
    std/dictionary_std.h
    # 解析器工厂注册表（P-3 3.4）：extension → DictionaryParserStd 工厂，
    # Developer dictionaries 经同一表面注册自定义格式
    std/dictionary_parser_std.h
    std/parser_registry_std.cpp
    std/parser_registry_std.h
    std/fulltext_index_std.cpp
    std/fulltext_index_std.h
    std/ripemd128_std.cpp
    std/ripemd128_std.h
    std/mdict_crypto_std.cpp
    std/mdict_crypto_std.h
    std/mdict_decryptor_std.cpp
    std/mdict_decryptor_std.h
    std/mdict_parser_std.cpp
    std/dsl_parser_std.cpp
    std/csv_parser_std.cpp
    # EPUB 词典（zip 容器 + XHTML 词条提取）
    std/zip_reader_std.cpp
    std/zip_reader_std.h
    std/epub_parser_std.cpp
    std/epub_parser_std.h
    # HTML rendering engine
    std/html_renderer_std.cpp
    std/html_renderer_std.h
    # MDD resource parser
    std/mdd_resource_std.cpp
    std/mdd_resource_std.h
    # Aggregated lookup
    std/aggregate_lookup_std.cpp
    std/aggregate_lookup_std.h
    # Cross-reference links
    std/cross_reference_std.cpp
    std/cross_reference_std.h
    # 发音评分内核（M3a，纯逻辑；平台壳见 gui/）
    std/pronunciation_score_std.cpp
    std/pronunciation_score_std.h
    # M3b：espeak→ARPAbet 映射 + CTC 强制对齐 + GOP（纯逻辑层，
    # onnxruntime 推理壳按 ctc_gop_std.h 接口喂数据）
    std/espeak_arpabet_std.cpp
    std/espeak_arpabet_std.h
    std/ctc_gop_std.cpp
    std/ctc_gop_std.h
    # M4 变体容忍表：GOP 取 max(主键, 变体)
    std/pron_variants_std.cpp
    std/pron_variants_std.h
    # M4 词典 IPA 文本 → ARPAbet（match_espeak_prefix 前缀原语在
    # espeak_arpabet_std 里）
    std/ipa_to_arpabet_std.cpp
    std/ipa_to_arpabet_std.h
    # M3b 适配器前置纯逻辑：模型词表解析 / 波形预处理 /
    # fp16+log_softmax（ctc_logits_std.h header-only）
    std/pron_vocab_std.cpp
    std/pron_vocab_std.h
    std/pron_wave_std.cpp
    std/pron_wave_std.h
    std/ctc_logits_std.h
    # M8 生词本联动：词分 →「发音不稳」标签增删规则（纯逻辑，
    # 生词本读写在 gui/ 平台壳）
    std/pron_review_std.cpp
    std/pron_review_std.h
    # M9 发音练习历史：跟自己的进步比（累计/进步幅度/练习清单次序）。
    # 记录形态随生词/笔记存在同一个 store 文件里
    std/pron_history_std.cpp
    std/pron_history_std.h
    # M10 模型资产自举下载：SHA-256 + 资产清单 + 下载/续传/校验/落地
    # 决策（HTTP 传输在 cli-std 的 curl 桥与 gui 的 QNetwork 壳里）
    std/sha256_std.cpp
    std/sha256_std.h
    std/pron_model_source_std.cpp
    std/pron_model_source_std.h
    # 在线发音源：URL 拼装 + 容忍式响应解析 + 口音挑选（纯逻辑；
    # HTTP/播放/设置三态在平台壳）
    std/online_pron_std.cpp
    std/online_pron_std.h
    # server_plan §7 B2 客户端同步引擎：指令生成/推拉/确定性回放/快照
    # 压缩/断点续传（传输面 SyncTransportStd 窄接口；HTTP/WebDAV/LAN
    # 绑定归 B5 平台壳）
    std/sync_engine_std.cpp
    std/sync_engine_std.h
)

target_include_directories(unidict_std_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/std"
)

# mdd_resource_std 的 zlib 解压分支靠 USE_ZLIB 打开——此前只链库没定义宏，
# 压缩 .mdd 资源一直在走"原样拷贝"的 fallback（等于不支持压缩）
target_compile_definitions(unidict_std_core PRIVATE USE_ZLIB)
target_link_libraries(unidict_std_core PRIVATE ZLIB::ZLIB)
target_link_libraries(unidict_std_core PUBLIC unidict_index_std)
