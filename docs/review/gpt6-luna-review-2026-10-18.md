[0m
> build · gpt-6-luna
[0m→ [0mSkill "code-review" 
> build · gpt-6-luna
我会按当前实现和仓库文档交叉核查；优先找有具体触发路径、影响边界明确的风险，不把“缺少某能力”直接当成缺陷。
[0m$ [0mgit status --short && git ls-files
.gitattributes
.github/pages/index.html
.github/workflows/ci.yml
.github/workflows/daily-build.yml
.github/workflows/deploy-docs.yml
.github/workflows/deploy-sync-worker.yml
.gitignore
.spec-workflow/templates/design-template.md
.spec-workflow/templates/product-template.md
.spec-workflow/templates/requirements-template.md
.spec-workflow/templates/structure-template.md
.spec-workflow/templates/tasks-template.md
.spec-workflow/templates/tech-template.md
.spec-workflow/user-templates/README.md
AGENTS.md
ARCHITECTURE.md
BUGS.md
CMakeLists.txt
CONTRIBUTING.md
LICENSE
MDICT_ENCRYPTION_RESEARCH.md
README.md
README.zh.md
adapters/CMakeLists.txt
adapters/android/CMakeLists.txt
adapters/android/jni/dict_jni.cpp
adapters/android/jni/jni_util.h
adapters/android/jni/lookup_jni.cpp
adapters/android/jni/store_jni.cpp
adapters/crash/CMakeLists.txt
adapters/crash/crash_reporter.cpp
adapters/crash/crash_reporter.h
adapters/pron/CMakeLists.txt
adapters/pron/onnx_pron_scorer.cpp
adapters/pron/onnx_pron_scorer.h
adapters/qt/CMakeLists.txt
adapters/qt/ai_service_qt.cpp
adapters/qt/ai_service_qt.h
adapters/qt/clipboard_qt.cpp
adapters/qt/clipboard_qt.h
adapters/qt/data_store_qt.cpp
adapters/qt/data_store_qt.h
adapters/qt/fulltext_manager_qt.cpp
adapters/qt/fulltext_manager_qt.h
adapters/qt/path_utils_qt.cpp
adapters/qt/path_utils_qt.h
adapters/qt/settings_qt.cpp
adapters/qt/settings_qt.h
adapters/qt/sync_http_transport_qt.cpp
adapters/qt/sync_http_transport_qt.h
adapters/qt/sync_manager_qt.cpp
adapters/qt/sync_manager_qt.h
adapters/qt/sync_service_qt.cpp
adapters/qt/sync_service_qt.h
android/.gitignore
android/app/build.gradle.kts
android/app/proguard-rules.pro
android/app/src/main/AndroidManifest.xml
android/app/src/main/kotlin/dev/unidict/mobile/DictRepository.kt
android/app/src/main/kotlin/dev/unidict/mobile/FloatingLookupService.kt
android/app/src/main/kotlin/dev/unidict/mobile/MainActivity.kt
android/app/src/main/kotlin/dev/unidict/mobile/QuickLookupWidgetProvider.kt
android/app/src/main/kotlin/dev/unidict/mobile/UnidictApp.kt
android/app/src/main/kotlin/dev/unidict/mobile/UnidictCore.kt
android/app/src/main/kotlin/dev/unidict/mobile/UnidictOnlinePron.kt
android/app/src/main/kotlin/dev/unidict/mobile/UnidictTheme.kt
android/app/src/main/kotlin/dev/unidict/mobile/UnidictTts.kt
android/app/src/main/res/layout/widget_quick_lookup.xml
android/app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml
android/app/src/main/res/mipmap-anydpi-v26/ic_launcher_round.xml
android/app/src/main/res/mipmap-hdpi/ic_launcher.png
android/app/src/main/res/mipmap-hdpi/ic_launcher_foreground.png
android/app/src/main/res/mipmap-hdpi/ic_launcher_round.png
android/app/src/main/res/mipmap-mdpi/ic_launcher.png
android/app/src/main/res/mipmap-mdpi/ic_launcher_foreground.png
android/app/src/main/res/mipmap-mdpi/ic_launcher_round.png
android/app/src/main/res/mipmap-xhdpi/ic_launcher.png
android/app/src/main/res/mipmap-xhdpi/ic_launcher_foreground.png
android/app/src/main/res/mipmap-xhdpi/ic_launcher_round.png
android/app/src/main/res/mipmap-xxhdpi/ic_launcher.png
android/app/src/main/res/mipmap-xxhdpi/ic_launcher_foreground.png
android/app/src/main/res/mipmap-xxhdpi/ic_launcher_round.png
android/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png
android/app/src/main/res/mipmap-xxxhdpi/ic_launcher_foreground.png
android/app/src/main/res/mipmap-xxxhdpi/ic_launcher_round.png
android/app/src/main/res/values-night/themes.xml
android/app/src/main/res/values/colors.xml
android/app/src/main/res/values/ic_launcher_colors.xml
android/app/src/main/res/values/strings.xml
android/app/src/main/res/values/themes.xml
android/app/src/main/res/xml/quick_lookup_widget_info.xml
android/app/src/main/res/xml/shortcuts.xml
android/build.gradle.kts
android/gradle.properties
android/gradle/wrapper/gradle-wrapper.jar
android/gradle/wrapper/gradle-wrapper.properties
android/gradlew
android/gradlew.bat
android/settings.gradle.kts
aqtinstall.log
architecture_diagram.txt
assets/icons/unidict.icns
assets/icons/unidict.ico
assets/icons/unidict_256.png
assets/icons/unidict_512.png
assets/linux/unidict.desktop
benchmark.sh
cli-std/CMakeLists.txt
cli-std/main.cpp
cmake/BuildOptions.cmake
cmake/MSVC_Options.cmake
cmake/UnidictConfig.cmake.in
cmake/config.h.in
cmake/crashpad/CMakeLists.txt
cmake/crashpad/LICENSE
cmake/crashpad/cmake/CTestCustom.cmake
cmake/crashpad/cmake/crashpad-client.cmake
cmake/crashpad/cmake/crashpad-common.cmake
cmake/crashpad/cmake/crashpad-compat.cmake
cmake/crashpad/cmake/crashpad-handler.cmake
cmake/crashpad/cmake/crashpad-mig.cmake
cmake/crashpad/cmake/crashpad-minidump.cmake
cmake/crashpad/cmake/crashpad-snapshot.cmake
cmake/crashpad/cmake/crashpad-tools.cmake
cmake/crashpad/cmake/crashpad-util.cmake
cmake/crashpad/cmake/minichromium.cmake
cmake/packaging_linux.cmake
codecov.yml
core/CMakeLists.txt
core/path_utils.cpp
core/path_utils.h
core/std/charset_codec_std.cpp
core/std/charset_codec_std.h
core/std/cross_reference_std.cpp
core/std/cross_reference_std.h
core/std/crypto_std.cpp
core/std/crypto_std.h
core/std/csv_parser_std.cpp
core/std/csv_parser_std.h
core/std/ctc_gop_std.cpp
core/std/ctc_gop_std.h
core/std/ctc_logits_std.h
core/std/data_store_std.cpp
core/std/data_store_std.h
core/std/dictionary_export_std.cpp
core/std/dictionary_export_std.h
core/std/dictionary_manager_std.cpp
core/std/dictionary_manager_std.h
core/std/dictionary_parser_std.h
core/std/dictionary_std.cpp
core/std/dictionary_std.h
core/std/dsl_parser_std.cpp
core/std/dsl_parser_std.h
core/std/epub_parser_std.cpp
core/std/epub_parser_std.h
core/std/espeak_arpabet_std.cpp
core/std/espeak_arpabet_std.h
core/std/fulltext_index_std.cpp
core/std/fulltext_index_std.h
core/std/gb18030_table_std.inc
core/std/html_renderer_std.cpp
core/std/html_renderer_std.h
core/std/index_engine_std.cpp
core/std/index_engine_std.h
core/std/ipa_to_arpabet_std.cpp
core/std/ipa_to_arpabet_std.h
core/std/json_parser_std.cpp
core/std/json_parser_std.h
core/std/lemma_std.cpp
core/std/lemma_std.h
core/std/mdd_resource_std.cpp
core/std/mdd_resource_std.h
core/std/mdict_crypto_std.cpp
core/std/mdict_crypto_std.h
core/std/mdict_decryptor_std.cpp
core/std/mdict_decryptor_std.h
core/std/mdict_parser_std.cpp
core/std/mdict_parser_std.h
core/std/mdx_v2_reader_std.cpp
core/std/mdx_v2_reader_std.h
core/std/notes_export_std.cpp
core/std/notes_export_std.h
core/std/online_pron_std.cpp
core/std/online_pron_std.h
core/std/parser_registry_std.cpp
core/std/parser_registry_std.h
core/std/path_utils_std.cpp
core/std/path_utils_std.h
core/std/pcm_util_std.h
core/std/pron_history_std.cpp
core/std/pron_history_std.h
core/std/pron_model_source_std.cpp
core/std/pron_model_source_std.h
core/std/pron_review_std.cpp
core/std/pron_review_std.h
core/std/pron_variants_std.cpp
core/std/pron_variants_std.h
core/std/pron_vocab_std.cpp
core/std/pron_vocab_std.h
core/std/pron_wave_std.cpp
core/std/pron_wave_std.h
core/std/pronunciation_score_std.cpp
core/std/pronunciation_score_std.h
core/std/ripemd128_std.cpp
core/std/ripemd128_std.h
core/std/sha256_std.cpp
core/std/sha256_std.h
core/std/spake2_std.cpp
core/std/spake2_std.h
core/std/stardict_parser_std.cpp
core/std/stardict_parser_std.h
core/std/stardict_resource_std.cpp
core/std/stardict_resource_std.h
core/std/sync_backup_std.cpp
core/std/sync_backup_std.h
core/std/sync_crypto_std.cpp
core/std/sync_crypto_std.h
core/std/sync_device_std.cpp
core/std/sync_device_std.h
core/std/sync_engine_std.cpp
core/std/sync_engine_std.h
core/std/sync_relay_state_std.cpp
core/std/sync_relay_state_std.h
core/std/sync_sealed_transport_std.cpp
core/std/sync_sealed_transport_std.h
core/std/text_norm_case_std.inc
core/std/text_norm_fold_std.inc
core/std/text_norm_std.cpp
core/std/text_norm_std.h
core/std/zip_reader_std.cpp
core/std/zip_reader_std.h
core/types.h
core/unidict_core.h
dictionaries/CC-CEDICT-ATTRIBUTION.md
dictionaries/ccedict-zh-en.json
dictionary_candidates.md
docs/CURRENT_ARCHITECTURE.en.md
docs/CURRENT_ARCHITECTURE.md
docs/CURRENT_FEATURE_MATRIX.en.md
docs/CURRENT_FEATURE_MATRIX.md
docs/TECH_DEBT.en.md
docs/TECH_DEBT.md
docs/USER_GUIDE.en.md
docs/USER_GUIDE.md
docs/architecture-boundaries.en.md
docs/architecture-boundaries.md
docs/crash_report_plan.en.md
docs/crash_report_plan.md
docs/design-references/eudic-lookup-page.png
docs/design/README.md
docs/design/README.zh.md
docs/design/sync-engine.en.md
docs/design/sync-engine.md
docs/design/text-normalization.en.md
docs/design/text-normalization.md
docs/dev.md
docs/dev.zh.md
docs/fulltext_index.md
docs/fulltext_index.zh.md
docs/gui-design.en.md
docs/gui-design.md
docs/gui-smoke-checklist.en.md
docs/gui-smoke-checklist.md
docs/gui-ui-structure.en.md
docs/gui-ui-structure.md
docs/icons/android-launcher-icons.png
docs/icons/desktop-icon-frames.png
docs/icons/product-icon-artifacts.png
docs/logo.svg
docs/mobile_plan.en.md
docs/mobile_plan.md
docs/mobile_smoke.cpp
docs/perf-benchmarks.en.md
docs/perf-benchmarks.md
docs/pro_dictionary_gap.en.md
docs/pro_dictionary_gap.md
docs/product-principles.en.md
docs/product-principles.md
docs/pronunciation-plan.en.md
docs/pronunciation-plan.md
docs/qmlui_mobile_adaptation_report.en.md
docs/qmlui_mobile_adaptation_report.md
docs/research/assets/eudic-desktop-01-main-lookup.png
docs/research/assets/eudic-desktop-02-dict-manage.png
docs/research/assets/eudic-desktop-03-vocab-table.png
docs/research/assets/eudic-desktop-04-word-card.png
docs/research/assets/eudic-desktop-05-hover-popup.png
docs/research/assets/eudic-desktop-06-ai-translate.png
docs/research/assets/eudic-mobile-01-lookup-detail.png
docs/research/assets/eudic-mobile-02-dict-library.png
docs/research/assets/eudic-mobile-03-review.png
docs/research/assets/eudic-mobile-04-progress.png
docs/research/assets/eudic-mobile-05-pronunciation.png
docs/research/assets/goldendict-desktop-01-win-light.webp
docs/research/assets/goldendict-desktop-02-mac-dark.webp
docs/research/assets/goldendict-desktop-03-linux-search.webp
docs/research/assets/goldendict-desktop-04-dict-dialog.webp
docs/research/assets/goldendict-desktop-05-dict-info.png
docs/research/assets/mdict-desktop-01-promo.jpg
docs/research/assets/mdict-mobile-01-entry.png
docs/research/assets/mdict-mobile-02-query.png
docs/research/assets/mdict-mobile-03-suggest.png
docs/research/assets/mdict-mobile-04-dicts.png
docs/research/assets/mdict-mobile-05-all.png
docs/research/dictionary-ui-research.en.md
docs/research/dictionary-ui-research.md
docs/roadmap.md
docs/roadmap.zh.md
docs/server_plan.en.md
docs/server_plan.md
docs/site/.vitepress/config.mts
docs/site/.vitepress/theme/custom.css
docs/site/.vitepress/theme/index.ts
docs/site/dictionaries.en.md
docs/site/dictionaries.md
docs/site/download.en.md
docs/site/download.md
docs/site/faq.en.md
docs/site/faq.md
docs/site/index.en.md
docs/site/index.md
docs/site/package-lock.json
docs/site/package.json
docs/site/pronunciation.en.md
docs/site/pronunciation.md
docs/site/prototypes.en.md
docs/site/prototypes.md
docs/site/prototypes/assets/dictionaries_dark.png
docs/site/prototypes/assets/dictionaries_light.png
docs/site/prototypes/assets/search_dark.png
docs/site/prototypes/assets/search_light.png
docs/site/prototypes/assets/settings_dark.png
docs/site/prototypes/assets/settings_light.png
docs/site/prototypes/assets/vocabulary_dark.png
docs/site/prototypes/assets/vocabulary_light.png
docs/site/prototypes/src/prototype.html
docs/site/prototypes/src/render.js
docs/site/public/logo.svg
docs/site/public/proto-search-light.png
docs/site/quickstart.en.md
docs/site/quickstart.md
docs/site/research/assets/anki.png
docs/site/research/assets/baidu_hanyu.png
docs/site/research/assets/bing_dict.png
docs/site/research/assets/cambridge.png
docs/site/research/assets/deepl.png
docs/site/research/assets/eudic.png
docs/site/research/assets/goldendict_ng.png
docs/site/research/assets/iciba.png
docs/site/research/assets/jisho.png
docs/site/research/assets/linguee.png
docs/site/research/assets/longman.png
docs/site/research/assets/mdict.png
docs/site/research/assets/pleco.png
docs/site/research/assets/xiandai_hanyu_cidian_app.png
docs/site/research/assets/youdao_web.png
docs/site/research/assets/zdic.png
docs/site/research/dictionary-market-survey.en.md
docs/site/research/dictionary-market-survey.md
docs/site/search.en.md
docs/site/search.md
docs/site/server.en.md
docs/site/server.md
docs/site/vocabulary.en.md
docs/site/vocabulary.md
docs/ui/compare/D3-result-dark.png
docs/ui/compare/D3-result-light.png
docs/ui/compare/README.en.md
docs/ui/compare/README.md
docs/ui/compare/gui/examples_dark.png
docs/ui/compare/gui/examples_light.png
docs/ui/compare/gui/history_dark.png
docs/ui/compare/gui/history_light.png
docs/ui/compare/gui/home_dark.png
docs/ui/compare/gui/home_light.png
docs/ui/compare/gui/manage_dark.png
docs/ui/compare/gui/manage_light.png
docs/ui/compare/gui/result_dark.png
docs/ui/compare/gui/result_light.png
docs/ui/compare/gui/vocab_dark.png
docs/ui/compare/gui/vocab_light.png
docs/ui/compare/home-dark.png
docs/ui/compare/home-light.png
docs/ui/compare/result-dark.png
docs/ui/compare/result-light.png
docs/ui/compare/settings-dark.png
docs/ui/compare/settings-light.png
docs/ui/compare/vocab-dark.png
docs/ui/compare/vocab-light.png
docs/ui/home-dark.png
docs/ui/home-light.png
docs/ui/result-dark.png
docs/ui/result-light.png
docs/ui/settings-dark.png
docs/ui/settings-light.png
docs/ui/vocab-dark.png
docs/ui/vocab-light.png
"docs/\345\256\241\350\256\241-\346\226\207\346\241\243\344\270\200\350\207\264\346\200\247.en.md"
"docs/\345\256\241\350\256\241-\346\226\207\346\241\243\344\270\200\350\207\264\346\200\247.md"
examples/dict.json
examples/simple.json
examples/test.csv
examples/test.dsl
examples/test.tsv
examples/test_dict.json
gui/CMakeLists.txt
gui/audio_recorder.cpp
gui/audio_recorder.h
gui/click_audit_test.cpp
gui/dev/ui_sandbox.cpp
gui/main.cpp
gui/model_downloader.cpp
gui/model_downloader.h
gui/pcm_playback.cpp
gui/pcm_playback.h
gui/pcm_util.h
gui/pronunciation_panel.cpp
gui/pronunciation_panel.h
gui/unidict.rc
gui/waveform_widget.cpp
gui/waveform_widget.h
install.ps1
install.sh
packaging/windows/unidict.iss
qmlui/CMakeLists.txt
qmlui/Main.qml
qmlui/MainDesktop.qml
qmlui/clipboard_monitor.cpp
qmlui/clipboard_monitor.h
qmlui/components/EntryResultsPane.qml
qmlui/components/GroupCard.qml
qmlui/components/QuickLookupPane.qml
qmlui/components/SidebarPanel.qml
qmlui/dev/fixtures/click_audit_dict.json
qmlui/dev/fixtures/click_audit_dict2.json
qmlui/dev/ui_click_audit.cpp
qmlui/dev/ui_sandbox.cpp
qmlui/global_hotkeys.cpp
qmlui/global_hotkeys.h
qmlui/global_hotkeys_mac.mm
qmlui/lookup_adapter.cpp
qmlui/lookup_adapter.h
qmlui/mac_selection_reader.cpp
qmlui/mac_selection_reader.h
qmlui/main.cpp
qmlui/mobile/android/AndroidManifest.xml
qmlui/mobile/android/res/values/styles.xml
qmlui/mobile/android/res/xml/file_paths.xml
qmlui/mobile/common/DefinitionContent.qml
qmlui/mobile/common/MobileFileDialog.qml
qmlui/mobile/common/ResponsiveLayout.qml
qmlui/mobile/ios/Info.plist
qmlui/mobile_utils.cpp
qmlui/mobile_utils.h
qmlui/platform_adapter.cpp
qmlui/platform_adapter.h
qmlui/resources.qrc
qmlui/selection_monitor.cpp
qmlui/selection_monitor.h
qmlui/startup_launcher.cpp
qmlui/startup_launcher.h
qmlui/theme.cpp
qmlui/theme.h
qmlui/theme_tokens.h
qmlui/tray_controller.cpp
qmlui/tray_controller.h
qmlui/unidict.rc
qmlui/vocab_store_adapter.cpp
qmlui/vocab_store_adapter.h
qmlui/voice_adapter.cpp
qmlui/voice_adapter.h
qmlui/win_selection_reader.cpp
qmlui/win_selection_reader.h
scripts/build_packages.py
scripts/build_stackwalk_tools.sh
scripts/check_text_norm_tables.py
scripts/coverage.sh
scripts/fetch_sample_dicts.sh
scripts/gen_fake_ctc_fixture.py
scripts/gen_gb18030_table.py
scripts/gen_mdict_crypto_vectors.py
scripts/gen_text_norm_fold.py
scripts/gen_text_norm_tables.py
scripts/gen_text_norm_test.py
scripts/symbols_split.sh
server/sync_relay/PROTOCOL.md
server/sync_relay/README.md
server/sync_relay/cpp/CMakeLists.txt
server/sync_relay/cpp/lan_host_std.cpp
server/sync_relay/cpp/lan_host_std.h
server/sync_relay/cpp/main.cpp
server/sync_relay/cpp/relay_http.cpp
server/sync_relay/cpp/relay_http.h
server/sync_relay/cpp/relay_routes.h
server/sync_relay/cpp/run_conformance.py
server/sync_relay/dev/sync_relay_dev.py
server/sync_relay/dev/test_relay_protocol.py
server/sync_relay/worker/schema.sql
server/sync_relay/worker/src/worker.js
server/sync_relay/worker/test/worker_test.mjs
server/sync_relay/worker/wrangler.toml
test_tts.sh
tests/CMakeLists.txt
tests/cache_prune_std_test.cpp
tests/charset_codec_std_branches_test.cpp
tests/cli_std_mdict_password_test.cpp
tests/cli_std_output_test.cpp
tests/clipboard_monitor_test.cpp
tests/core_std_cover_test.cpp
tests/crash_reporter_std_test.cpp
tests/cross_reference_std_branches_test.cpp
tests/cross_reference_std_test.cpp
tests/crypto_std_test.cpp
tests/csv_parser_std_cover_test.cpp
tests/csv_parser_std_test.cpp
tests/ctc_gop_std_test.cpp
tests/ctc_logits_std_test.cpp
tests/data_store_std_branches_test.cpp
tests/data_store_std_cover_test.cpp
tests/data_store_std_csv_escape_test.cpp
tests/data_store_std_escape_test.cpp
tests/data_store_std_notes_test.cpp
tests/data_store_std_pron_test.cpp
tests/data_store_std_remove_test.cpp
tests/data_store_std_tags_test.cpp
tests/data_store_std_test.cpp
tests/data_store_std_vocab_m3_test.cpp
tests/data_store_test.cpp
tests/dictionary_export_std_test.cpp
tests/dictionary_manager_scan_std_test.cpp
tests/dictionary_manager_std_branches_test.cpp
tests/dictionary_manager_std_grouped_test.cpp
tests/dictionary_manager_std_meta_std_test.cpp
tests/dictionary_manager_std_priority_test.cpp
tests/dictionary_manager_std_quarantine_test.cpp
tests/dictionary_manager_std_remove_test.cpp
tests/dictionary_manager_std_state_test.cpp
tests/dictionary_manager_std_test.cpp
tests/dictionary_std_test.cpp
tests/dsl_parser_std_test.cpp
tests/entry_presentation_test.cpp
tests/epub_fixture.h
tests/epub_parser_std_branches_test.cpp
tests/epub_parser_std_cover_test.cpp
tests/epub_zip_std_test.cpp
tests/espeak_arpabet_std_test.cpp
tests/fixtures/fake_ctc.onnx
tests/fixtures/fake_vocab.json
tests/fulltext_accept_version_test.cpp
tests/fulltext_index_std_branches_test.cpp
tests/fulltext_index_std_clear_test.cpp
tests/fulltext_index_std_cover_test.cpp
tests/fulltext_index_std_test.cpp
tests/fulltext_lazy_decode_stats_std_test.cpp
tests/fulltext_manager_qt_test.cpp
tests/fulltext_manager_reset_std_test.cpp
tests/fulltext_parallel_build_std_test.cpp
tests/fulltext_persistence_mismatch_std_test.cpp
tests/fulltext_persistence_pathchange_std_test.cpp
tests/fulltext_persistence_stardict_companion_change_std_test.cpp
tests/fulltext_persistence_std_test.cpp
tests/fulltext_std_test.cpp
tests/fulltext_tie_break_std_test.cpp
tests/fulltext_udft12_load_std_test.cpp
tests/fulltext_udft3_format_std_test.cpp
tests/global_hotkeys_test.cpp
tests/html_renderer_highlight_std_test.cpp
tests/html_renderer_std_branches_test.cpp
tests/html_renderer_std_cover_test.cpp
tests/html_renderer_std_edge_test.cpp
tests/html_renderer_std_limits_test.cpp
tests/html_renderer_std_test.cpp
tests/index_engine_std_edge_test.cpp
tests/index_engine_std_ops_test.cpp
tests/index_engine_std_test.cpp
tests/index_persistence_std_test.cpp
tests/ipa_to_arpabet_std_test.cpp
tests/json_parser_std_edge_test.cpp
tests/json_parser_std_test.cpp
tests/lemma_std_test.cpp
tests/lookup_adapter_test.cpp
tests/mac_selection_reader_test.cpp
tests/mdd_mdict_std_test.cpp
tests/mdd_resource_std_branches_test.cpp
tests/mdd_resource_std_cover2_test.cpp
tests/mdd_resource_std_cover_test.cpp
tests/mdd_resource_std_full_test.cpp
tests/mdd_resource_std_test.cpp
tests/mdict_crypto_std_test.cpp
tests/mdict_decryptor_std_cover_test.cpp
tests/mdict_decryptor_std_test.cpp
tests/mdict_encrypted_header_std_test.cpp
tests/mdict_fixture.h
tests/mdict_invalid_kidx_oob_std_test.cpp
tests/mdict_kbix_count_mismatch_std_test.cpp
tests/mdict_kbix_multi_std_test.cpp
tests/mdict_kbix_std_test.cpp
tests/mdict_keyb_empty_std_test.cpp
tests/mdict_keyb_std_test.cpp
tests/mdict_kidx_std_test.cpp
tests/mdict_mdd_resources_std_test.cpp
tests/mdict_mdxk_composite_edge_std_test.cpp
tests/mdict_mdxk_garbage_tail_std_test.cpp
tests/mdict_mdxk_oob_offsets_std_test.cpp
tests/mdict_mdxk_ulen_cap_std_test.cpp
tests/mdict_mdxkr_std_test.cpp
tests/mdict_parser_std_branches2_test.cpp
tests/mdict_parser_std_branches_test.cpp
tests/mdict_parser_std_cover_test.cpp
tests/mdict_password_env_std_test.cpp
tests/mdict_realheur_std_test.cpp
tests/mdict_simplekv_std_test.cpp
tests/mdict_std_test.cpp
tests/mdict_tsv_std_test.cpp
tests/mdict_utf16_header_std_test.cpp
tests/mdict_zlib_std_test.cpp
tests/mdx_v2_reader_std_test.cpp
tests/mobile_utils_test.cpp
tests/notes_export_std_test.cpp
tests/online_pron_std_test.cpp
tests/onnx_pron_scorer_test.cpp
tests/parser_registry_std_test.cpp
tests/path_utils_env_days_std_test.cpp
tests/path_utils_std_branches_test.cpp
tests/pcm_util_std_test.cpp
tests/pron_history_std_test.cpp
tests/pron_model_source_std_test.cpp
tests/pron_review_std_test.cpp
tests/pron_variants_std_test.cpp
tests/pron_vocab_std_test.cpp
tests/pron_wave_std_test.cpp
tests/pronunciation_panel_test.cpp
tests/pronunciation_score_std_test.cpp
tests/qt_adapters_test.cpp
tests/selection_monitor_test.cpp
tests/sha256_std_test.cpp
tests/spake2_std_test.cpp
tests/stardict_charset_std_test.cpp
tests/stardict_dz_std_test.cpp
tests/stardict_fixture.h
tests/stardict_malformed_std_test.cpp
tests/stardict_parser_std_branches_test.cpp
tests/stardict_parser_std_full_test.cpp
tests/stardict_resource_std_test.cpp
tests/stardict_std_test.cpp
tests/stardict_sts_std_test.cpp
tests/startup_launcher_test.cpp
tests/std_lookup_parity_test.cpp
tests/sync_backup_std_test.cpp
tests/sync_crypto_std_test.cpp
tests/sync_device_std_test.cpp
tests/sync_engine_std_test.cpp
tests/sync_lan_host_qt_test.cpp
tests/sync_manager_lan_qt_test.cpp
tests/sync_relay_memory_std.h
tests/sync_relay_state_std_test.cpp
tests/sync_sealed_transport_std_test.cpp
tests/sync_service_qt_test.cpp
tests/sync_transport_qt_test.cpp
tests/text_norm_profile_test.cpp
tests/text_norm_std_pro_test.cpp
tests/text_norm_std_test.cpp
tests/theme_test.cpp
tests/theme_tokens_contrast_test.cpp
tests/win_selection_reader_test.cpp
tests/zip_reader_std_defense_test.cpp
todo.md
tools/artifact_icon_sheet.py
tools/build_ccedict_dict.py
tools/build_icons.py
tools/pe_check.py
[0m✱ [0mGlob "**/*" [90min . · 1000 matches[0m
[0m✱ [0mGrep "TODO|FIXME|XXX|未实现|待实现|尚未" [90min . · 43 matches[0m
> build · gpt-6-luna
[0m→ [0mRead docs/CURRENT_ARCHITECTURE.md 
[0m→ [0mRead docs/CURRENT_FEATURE_MATRIX.md 
[0m→ [0mRead docs/TECH_DEBT.md 
[0m→ [0mRead todo.md 
[0m→ [0mRead docs/design/sync-engine.md 
[0m→ [0mRead ARCHITECTURE.md 
[0m→ [0mRead docs/roadmap.zh.md 
> build · gpt-6-luna
[0m→ [0mRead core/std/data_store_std.cpp [90m[offset=1, limit=240][0m
[0m→ [0mRead core/std/sync_engine_std.cpp [90m[limit=260][0m
[0m→ [0mRead core/std/sync_crypto_std.cpp [90m[limit=260][0m
[0m→ [0mRead core/std/sync_sealed_transport_std.cpp [90m[limit=260][0m
[0m→ [0mRead core/std/sync_backup_std.cpp [90m[limit=260][0m
[0m→ [0mRead core/std/html_renderer_std.cpp [90m[limit=300][0m
[0m→ [0mRead adapters/qt/sync_manager_qt.cpp [90m[limit=280][0m
> build · gpt-6-luna
[0m→ [0mRead adapters/qt/sync_manager_qt.cpp [90m[offset=280, limit=320][0m
[0m→ [0mRead core/std/sync_device_std.cpp [90m[limit=260][0m
[0m→ [0mRead core/std/sync_engine_std.h [90m[limit=280][0m
[0m→ [0mRead adapters/qt/sync_http_transport_qt.cpp [90m[limit=280][0m
[0m→ [0mRead core/std/sync_crypto_std.h [90m[limit=260][0m
[0m→ [0mRead server/sync_relay/PROTOCOL.md [90m[limit=280][0m
> build · gpt-6-luna
[0m✱ [0mGrep "sanitize_token|is_safe_url|validate.*url|DANGEROUS_CSS|allowed_protocols|sanitize_css" [90min core/std/html_renderer_std.cpp · 14 matches[0m
[0m✱ [0mGrep "write.*temp|rename|ofstream|std::ios::trunc|save\(\)" [90min core/std/data_store_std.cpp · 25 matches[0m
[0m→ [0mRead core/std/html_renderer_std.cpp [90m[offset=360, limit=300][0m
[0m✱ [0mGrep "100%|test|fuzz|property|真实词典|风险|缺失|限制|未实现" [90min docs/pro_dictionary_gap.md · 9 matches[0m
[0m→ [0mRead docs/architecture-boundaries.md [90m[limit=220][0m
[0m→ [0mRead docs/fulltext_index.zh.md [90m[limit=220][0m
> build · gpt-6-luna
[0m→ [0mRead docs/server_plan.md [90m[offset=130, limit=130][0m
[0m→ [0mRead docs/pro_dictionary_gap.md [90m[limit=100][0m
[0m→ [0mRead core/std/crypto_std.cpp [90m[limit=240][0m
[0m→ [0mRead core/std/html_renderer_std.cpp [90m[offset=660, limit=180][0m
[0m→ [0mRead core/std/data_store_std.cpp [90m[offset=330, limit=145][0m
[0m✱ [0mGrep "pairing|SPAKE|secure storage|Keychain|DPAPI|keyring|atomic|backup|restore" [90min docs · 37 matches[0m
> build · gpt-6-luna
[0m✱ [0mGrep "enqueue\(|SyncOpType::|syncNow\(" [90min . · 102 matches[0m
> build · gpt-6-luna
[0m→ [0mRead adapters/qt/sync_manager_qt.h [90m[limit=140][0m
[0m→ [0mRead qmlui/Main.qml [90m[offset=760, limit=90][0m
[0m→ [0mRead qmlui/MainDesktop.qml [90m[offset=1910, limit=180][0m
[0m✱ [0mGrep "setVocabulary|addVocabulary|removeVocabulary|setNote|addTag|history" [90min adapters/qt/sync_manager_qt.cpp · 3 matches[0m
[0m✱ [0mGrep "syncManager\.|sync\." [90min qmlui · 53 matches[0m
[0m✱ [0mGrep "B7|sync.*enqueue|DataStoreQt.*sync|同步.*生词|同步面" [90min tests · 11 matches[0m
> build · gpt-6-luna
[0m✱ [0mGrep "isalpha|isalnum|ASCII|tokeniz|Chinese|CJK|pinyin" [90min core/std/fulltext_index_std.cpp · 23 matches[0m
[0m→ [0mRead docs/roadmap.zh.md [90m[offset=40, limit=16][0m
[0m→ [0mRead docs/CURRENT_ARCHITECTURE.md [90m[offset=150, limit=34][0m
[0m→ [0mRead core/std/fulltext_index_std.cpp [90m[offset=30, limit=130][0m
[0m→ [0mRead adapters/qt/sync_manager_qt.cpp [90m[offset=400, limit=45][0m
[0m→ [0mRead docs/CURRENT_FEATURE_MATRIX.md [90m[offset=108, limit=23][0m
> build · gpt-6-luna
[0m→ [0mRead adapters/qt/ai_service_qt.h [90m[limit=180][0m
[0m✱ [0mGrep "AiProvider|CommandAiProvider|HeuristicAiProvider|stream|provider" [90min . · 100 matches[0m
[0m→ [0mRead adapters/qt/ai_service_qt.cpp [90m[limit=240][0m
[0m✱ [0mGrep "CURRENT_ARCHITECTURE|M8|M4.*未|双实现|EpubParser Qt" [90min docs · 100 matches[0m
> build · gpt-6-luna
## 1）最薄弱的 3 处（按风险排序）

1. **同步引擎没有接入真实用户数据，界面上的“立即同步”不等于生词本同步。**  
   `adapters/qt/sync_manager_qt.cpp:262-279` 只调用 `engine_.sync()`；`adapters/qt/sync_manager_qt.h:76-83` 没有从 DataStore 导入变更的接口，生产代码也没有 `engine_.enqueue()` 调用点。新安装后引擎 outbox 为空，用户数据不会被推送。配对也未接好：`adapters/qt/sync_manager_qt.h:24-29` 明说第二台设备在同组直接开启会生成独立密钥。**风险：用户误以为数据已跨设备同步，实际没有；错误配置还可能形成无法互通的组。**

2. **同步组密钥以明文落盘。**  
   `adapters/qt/sync_manager_qt.cpp:64-65,120-127` 将 keyring 写入应用数据目录的 `keyring.bin`；`core/std/sync_crypto_std.cpp:144-153` 的序列化内容包含原始密钥字节，没有本机加密或系统安全存储。**风险：设备账户目录或备份文件被读取时，组密钥随之泄露；中转端只见密文的保护无法抵御端点密钥被直接取走。**

3. **“全量加密备份”实际只收集部分数据，且可能把写入失败报成成功。**  
   `adapters/qt/sync_manager_qt.cpp:408-430` 只填充生词、标签和笔记；`436-437` 未检查 `QFile::write()` 返回值就返回成功。**风险：历史、偏好和词典清单不会进入备份，磁盘写满或短写时也可能留下残缺文件却显示导出成功。**

## 2）最值得补的 3 项新能力

1. **可实际使用的多设备同步闭环。**  
   把 DataStore 的生词、笔记、历史变更接入同步指令；完成配对、密钥交换及设备状态流程，并用桌面／Android 端到端测试验证数据真正互通。现在虽有 relay、引擎和加密组件，但产品调用链缺少数据接线，配对也未接入；这更像“底层已交付、用户能力未交付”。

2. **真实词典兼容性矩阵与授权测试语料。**  
   覆盖常见真实 MDict、StarDict、DSL、EPUB 及资源文件组合，记录加载、编码、富文本和交叉引用兼容性。现在主要靠合成夹具；仓库不提交词典资产的版权约束、缺少可注入且获授权的 CI 语料，是尚未系统化的原因。

3. **可配置、流式的 AI provider。**  
   提供用户自管凭据的在线 provider、流式输出、取消和错误状态，并清楚区分真实结果与 mock。当前接口已有，但实现只有外部命令和启发式 provider，没有在线 provider 或 streaming；仓库记录的阻塞原因是凭据、计费口径和服务隐私决策尚未确定。

## 3）文档与实现可能对不上的地方

- `docs/CURRENT_ARCHITECTURE.md:10-13` 的增量说明称 Android 已到 M8-B；但同文 `162-165` 仍写 M0–M3-C，称 M4、M5 未做。  
- 同文 `14-17` 称 B2/B5 客户端与 relay 已交付；但 `173-179` 仍记客户端引擎、C++ relay 未开始。这些正文注明保留审计日口径，但仍容易被当成现状。
- `docs/CURRENT_FEATURE_MATRIX.md:113` 称 B1–B7 全交付、客户端同步引擎已接；实际 `adapters/qt/sync_manager_qt.h:24-29` 明示配对未接，当前 UI 调用链也没有把 DataStore 变更 enqueue 到引擎。
- 备份范围不一致：`docs/roadmap.zh.md:246-251` 宣称备份覆盖 history、prefs、已装词典；`docs/CURRENT_FEATURE_MATRIX.md:125` 称“全量备份”；但 `adapters/qt/sync_manager_qt.cpp:408-425` 只收集生词、标签、笔记。
- `docs/roadmap.zh.md:48-51` 仍称 EPUB 有 Qt 与 std 双实现；`docs/CURRENT_ARCHITECTURE.md:31,84` 则称 legacy Qt 解析器已删除、std 单实现。
- AI 口径不一致：`docs/CURRENT_ARCHITECTURE.md:171-172` 称“无 provider 概念”；但 `adapters/qt/ai_service_qt.h:12-23` 已定义 `AiProvider` 抽象。准确说法应是“有 provider 抽象，但没有在线 provider 和 streaming”。
