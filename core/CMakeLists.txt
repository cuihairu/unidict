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
    # M3b 适配器前置纯逻辑：模型词表解析 / 波形预处理 /
    # fp16+log_softmax（ctc_logits_std.h header-only）
    std/pron_vocab_std.cpp
    std/pron_vocab_std.h
    std/pron_wave_std.cpp
    std/pron_wave_std.h
    std/ctc_logits_std.h
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
