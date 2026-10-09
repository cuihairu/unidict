#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "std/dictionary_manager_std.h"
#include "std/dictionary_export_std.h"
#include "std/path_utils_std.h"
#include "std/fulltext_index_std.h"
#include "std/ipa_to_arpabet_std.h"
#include "std/pron_model_source_std.h"
#if UNIDICT_HAVE_PRON
#include "onnx_pron_scorer.h"
#include "std/pron_wave_std.h"
#endif

using namespace UnidictCoreStd;


static std::string lcase(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }

// 分隔符跟平台 PATH 惯例：Windows 用 ';'（盘符自带 ':'，不能当分隔符，
// 否则 'C:\dict.mdx' 会被劈碎），POSIX 用 ':'
static std::vector<std::string> split_env_paths(const char* env) {
    std::vector<std::string> out; if (!env) return out; std::string s(env);
#if defined(_WIN32)
    const char* seps = ";";
#else
    const char* seps = ":";
#endif
    size_t i = 0; while (i < s.size()) { size_t j = s.find_first_of(seps, i); out.push_back(s.substr(i, j == std::string::npos ? s.size() - i : j - i)); if (j == std::string::npos) break; i = j + 1; }
    return out;
}

static void set_process_env(const char* key, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(key, value.c_str());
#else
    ::setenv(key, value.c_str(), 1);
#endif
}

// ---------------------------------------------------------------- M10
// 模型资产自举下载：HTTP 传输放在壳里（core/std 不引网络依赖，只回答
// "下一步该干什么"——下/续传/校验/落地）。命令行上只传一个 curl 配置
// 文件的路径：URL、输出路径、Range 偏移全写进配置文件的引号里，任何
// 平台上都不必跟 shell 的引号规则缠斗，路径里有空格或分号也不可能被
// 解释成别的东西。

// curl 配置的一行：value 用引号包住，内部引号/反斜杠按 curl 规则转义
static std::string curl_config_line(const std::string& key,
                                    const std::string& value) {
    std::string out = key + " = \"";
    for (const char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"\n";
}

// 写一次 curl 会话的配置文件（紧挨断点文件；名字带 .curlrc，用户一眼
// 看得出是本工具的中间产物，删不删都不影响下次运行）
static void write_curl_config(const std::string& path, const std::string& url,
                              const std::string& output,
                              long long resume_from) {
    std::ofstream cfg(path, std::ios::binary);
    cfg << curl_config_line("url", url) << curl_config_line("output", output);
    if (resume_from > 0) {
        // continue-at 让 curl 发 Range 并**追加**到已有断点后面
        cfg << curl_config_line("continue-at", std::to_string(resume_from));
    }
    // fail：404/500 不再被当成"下到了 0 字节的合法文件"（那正是把
    // HTML 错误页存成模型、几分钟后才在 onnxruntime 里炸掉的路径）
    cfg << "fail\nlocation\nretry = 3\nshow-error\n";
}

// curl 可用性预检。Windows 10 1803 前的系统没有内置 curl：此时 std::system
// 的返回既非 0 也无定向含义（cmd 的 9009 截断到 8 位后与 curl 真错误码
// 撞车），"下载失败（退出码 X）"只会把人往错的方向引。下载入口先探一
// 次，缺了给定向提示。
static bool curl_available() {
#if defined(_WIN32)
    const int rc = std::system("curl --version >nul 2>&1");
#else
    const int rc = std::system("curl --version >/dev/null 2>&1");
#endif
    return rc == 0;
}

// 起 curl。返回值：0 = 成功，非 0 = 失败（POSIX 下退出码在低 8 位，
// 被信号杀掉时也是低 8 位；Windows 下 system 直接给退出码）
static int run_curl(const std::string& config_path) {
#if defined(_WIN32)
    // Windows 路径不可能含引号，直接双引号包一层
    const std::string cmd = "curl --config \"" + config_path + "\"";
#else
    std::string quoted = "'";
    for (const char c : config_path) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    quoted += "'";
    const std::string cmd = "curl --config " + quoted;
#endif
    const int rc = std::system(cmd.c_str());
    if (rc == -1) return -1;
    return rc & 0xff;
}

// 盘上现状（core 只做判断，不替壳查文件系统）
static UnidictCoreStd::AssetState asset_state_of(
    const UnidictCoreStd::ModelAsset& asset, const std::string& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    UnidictCoreStd::AssetState st;
    const fs::path final_path =
        fs::path(UnidictCoreStd::pron_asset_path(asset, dir));
    st.final_exists = fs::exists(final_path, ec);
    if (st.final_exists) {
        st.final_size = static_cast<long long>(fs::file_size(final_path, ec));
    }
    const fs::path part_path =
        fs::path(UnidictCoreStd::pron_asset_part_path(asset, dir));
    st.part_bytes = static_cast<long long>(fs::file_size(part_path, ec));
    return st;
}

// 取一个资产：按 core 的下载计划决定下/续传/只校验，curl 拉完后由 core
// 校验并落地。坏包（续传失败、哈希不符、断点比资产还大）删掉断点从头
// 来一次；再失败就把可读的原因交出去，评分功能照旧回退到"无评分跟读"。
static bool fetch_asset(const UnidictCoreStd::ModelAsset& asset,
                        const std::string& dir) {
    namespace fs = std::filesystem;
    for (int attempt = 0; attempt < 2; ++attempt) {
        const UnidictCoreStd::FetchDecision d =
            UnidictCoreStd::plan_fetch(asset, dir, asset_state_of(asset, dir));
        std::cout << "==> " << asset.filename << "：" << d.reason << "（"
                  << asset.size_bytes << " 字节）\n";
        if (d.plan == UnidictCoreStd::FetchPlan::kDoneVerified) {
            return true;
        }
        if (d.plan != UnidictCoreStd::FetchPlan::kVerifyPart) {
            const std::string cfg = d.part_path + ".curlrc";
            write_curl_config(cfg, asset.url, d.part_path, d.resume_from);
            const int rc = run_curl(cfg);
            std::error_code rm;
            fs::remove(cfg, rm);
            if (rc != 0) {
                std::cerr << "    下载失败（curl 退出码 " << rc << "，报错见上）\n";
                if (d.plan == UnidictCoreStd::FetchPlan::kResume && attempt == 0) {
                    std::cerr << "    续传失败（服务器可能不支持 Range），删断点重下一次\n";
                    fs::remove(d.part_path, rm);
                    continue;
                }
                return false;
            }
        }
        // 尺寸先看：没下全就不必谈哈希（core 的 verify_asset 同序）
        std::error_code ec;
        const long long got =
            static_cast<long long>(fs::file_size(d.part_path, ec));
        if (ec) {
            std::cerr << "    断点文件读不到（" << ec.message() << "）\n";
            return false;
        }
        if (got > asset.size_bytes) {
            // 服务器无视了 Range（或断点是旧版资产留下的）：全量接在半截
            // 后面 = "尺寸对但字节坏"的坏包，只能丢弃重来
            std::cerr << "    断点文件比资产还大（" << got << " > "
                      << asset.size_bytes << "），已丢弃\n";
            fs::remove(d.part_path, ec);
            if (attempt == 0) continue;
            return false;
        }
        if (got < asset.size_bytes) {
            std::cerr << "    只下到 " << got << " / " << asset.size_bytes
                      << " 字节；断点已保留（" << d.part_path
                      << "），重跑本命令可续传\n";
            return false;
        }
        std::string err;
        const UnidictCoreStd::InstallStatus st =
            UnidictCoreStd::install_part(asset, dir, err);
        if (st == UnidictCoreStd::InstallStatus::kOk) {
            std::cout << "    已就位并通过哈希校验：" << d.final_path << "\n";
            return true;
        }
        std::cerr << "    落地失败：" << err << "\n";
        if (st == UnidictCoreStd::InstallStatus::kVerifyFailed && attempt == 0) {
            std::cerr << "    字节校验没过（坏包，断点已删），重下一次\n";
            continue;
        }
        return false;
    }
    return false;
}

// 模型/词表路径：显式参数 > env > 约定目录（core/std 单一真源，GUI 同源，
// 不再各硬编码一份 ~/.cache 路径）
static std::string resolve_model_path(const std::string& explicit_path,
                                      const char* env_key,
                                      const UnidictCoreStd::ModelAsset& asset) {
    if (!explicit_path.empty()) return explicit_path;
    const char* v = std::getenv(env_key);
    if (v && *v) return std::string(v);
    return UnidictCoreStd::pron_asset_path(asset,
                                           UnidictCoreStd::pron_model_dir());
}

static void usage() {
    // CLI 定位：查词 + 词典/索引/缓存诊断。生词本、历史等学习管理走桌面 GUI。
    std::cout << "Unidict CLI - Universal Dictionary Lookup Tool\n\n";
    std::cout << "Basic Usage:\n";
    std::cout << "  unidict_cli_std [-d <dict> ...] [--mode <mode>] <word>\n\n";

    std::cout << "Options:\n";
    std::cout << "  -d, --dict <path>        Add dictionary file (support .mdx, .ifo, .json, .epub)\n";
    std::cout << "  -m, --mode <mode>        Search mode: exact, prefix, fuzzy, wildcard, regex, fulltext\n";
    std::cout << "  -p, --pattern <pattern>  Search pattern (for wildcard/regex/fulltext)\n";
    std::cout << "  --mdict-password <pw>    Password for encrypted MDict (.mdx/.mdd)\n";
    std::cout << "  --help                    Show this help message\n\n";

    std::cout << "Dictionary Management:\n";
    std::cout << "  --list-dicts             List loaded dictionaries\n";
    std::cout << "  --list-dicts-verbose     List dictionaries with word counts\n";
    std::cout << "  --drop-dict <name>        Remove dictionary by name\n";
    std::cout << "  --scan-dir <path>        Scan directory for dictionaries\n";
    std::cout << "  --export-dict <name> <out.json>  Export dictionary to project JSON format\n\n";

    std::cout << "Search & Lookup:\n";
    std::cout << "  --where <word>            Show which dictionaries contain the word\n";
    std::cout << "  --all                     Show all definitions for exact match\n\n";

    std::cout << "Index Management:\n";
    std::cout << "  --index-save <file>       Save index to file\n";
    std::cout << "  --index-load <file>       Load index from file\n";
    std::cout << "  --index-count             Show indexed word count\n";
    std::cout << "  --dump-words [N]          Dump first N indexed words\n\n";

    std::cout << "Full-Text Index:\n";
    std::cout << "  --fulltext-index-save <file>  Save full-text index\n";
    std::cout << "  --fulltext-index-load <file>  Load full-text index\n";
    std::cout << "  --ft-index-stats <file>      Show full-text index statistics\n";
    std::cout << "  --ft-index-verify <file>     Verify full-text index\n\n";

    std::cout << "Cache Management:\n";
    std::cout << "  --clear-cache            Clear all cache\n";
    std::cout << "  --cache-prune-mb <size>  Prune cache to max size (MB)\n";
    std::cout << "  --cache-prune-days <N>   Remove entries older than N days\n";
    std::cout << "  --cache-size             Show current cache size\n";
    std::cout << "  --cache-dir              Show cache directory path\n\n";

    std::cout << "System Information:\n";
    std::cout << "  --data-dir               Show data directory path\n";
    std::cout << "  --list-plugins           Show supported parser extensions\n";
    std::cout << "  --mdx-debug <file>       Debug MDict file structure\n\n";

    std::cout << "Pronunciation Scoring (M3b, requires UNIDICT_BUILD_PRON build):\n";
    std::cout << "  --pron-score <wav>       Score a 16kHz/mono/16bit wav against target phones\n";
    std::cout << "  --pron-phones \"<K AE T>\" Target ARPAbet phone sequence (space-separated)\n";
    std::cout << "  --pron-ipa \"<text>\"      Target as dictionary IPA text (e.g. \"/\xC9\x99\xCB\x88la\xC9\xAAv/\"),\n"
                 "                           converted to ARPAbet; exclusive with --pron-phones\n";
    std::cout << "  --pron-model <onnx>      Acoustic model (wav2vec2-espeak-ctc model.onnx)\n";
    std::cout << "  --pron-vocab <json>      Model vocab.json (espeak IPA -> class id)\n";
    std::cout << "  --pron-dump              Dump per-frame top-1 class instead of scoring\n";
    std::cout << "  --pron-fetch-model       Download + sha256-verify the scoring model into the\n";
    std::cout << "                           model dir (resumable; needs curl on PATH)\n\n";

    std::cout << "Environment Variables:\n";
    std::cout << "  UNIDICT_DICTS            Path list for dictionaries (':'-separated, ';' on Windows)\n\n";
    std::cout << "  UNIDICT_MDICT_PASSWORD   Password for encrypted MDict (.mdx/.mdd)\n";
    std::cout << "  UNIDICT_PASSWORD         Alias of UNIDICT_MDICT_PASSWORD (deprecated)\n";
    std::cout << "  UNIDICT_PRON_MODEL_DIR   Model dir for --pron-fetch-model (default\n";
    std::cout << "                           ~/.cache/unidict-models/wav2vec2-espeak-ctc)\n\n";

    std::cout << "Examples:\n";
    std::cout << "  unidict_cli_std -d dict.mdx hello\n";
    std::cout << "  unidict_cli_std --mode prefix inter    # prints '# prefix ... -> N matches'\n";
    std::cout << "                                          # then 'word<TAB>dicts' per hit\n";
    std::cout << "  UNIDICT_DICTS=\"dict1.mdx:dict2.ifo\" unidict_cli_std word\n";
    std::cout << "  unidict_cli_std --fulltext-index-save ft.index --mode fulltext greeting\n";
    std::cout << "  unidict_cli_std --pron-fetch-model        # once: fetch + verify the 606MB model\n";
    std::cout << "  unidict_cli_std --pron-phones \"K AE T\" --pron-score cat.wav\n\n";
}

int main(int argc, char** argv) {
    std::vector<std::string> dict_paths;
    std::string mode = "exact";
    std::string pattern;
    bool list_dicts = false, list_dicts_verbose = false, do_all = false;
    bool list_plugins = false;
    std::string drop_dict;
    std::string mdx_debug_path;
    int dump_n = 0;
    std::string where_word;
    std::string scan_dir;
    std::string index_save, index_load;
    bool clear_cache = false;
    int cache_prune_mb = -1;
    int cache_prune_days = -1;
    bool cache_size = false;
    bool print_cache_dir = false;
    bool print_data_dir = false;
    bool index_count = false;
    std::string ft_index_save, ft_index_load;
    std::string ft_up_in, ft_up_out;
    std::string ft_up_dir, ft_up_suffix = ".v2";
    std::string ft_out_dir; // optional destination root for batch upgrade
    bool ft_dry_run = false;
    std::string ft_filter_exts; // comma-separated, e.g. .idx,.index
    bool ft_force = false;
    std::string ft_exclude_glob; // comma-separated glob patterns (e.g. */backup/*,*.bak)
    std::string ft_log_path; // optional CSV log output for batch
    std::string word;
    std::string ft_stats_path;
    std::string ft_verify_path;
    std::string ft_compat = "auto"; // strict|auto|loose
    std::string mdict_password;
    std::string pron_score_wav;   // 发音评分：wav 路径（非空 = 执行评分）
    std::string pron_phones;      // 目标 ARPAbet 音素序列
    std::string pron_ipa;         // 目标 IPA 文本（词典音标直填）
    std::string pron_model;       // model.onnx 路径
    std::string pron_vocab_path;  // vocab.json 路径
    bool pron_dump = false;       // 帧级诊断（argmax 逐帧打印）
    bool pron_fetch = false;      // M10：下载 + 校验发音模型资产
    std::string export_dict_name; // 本地词典导出：词典名
    std::string export_dict_out;  // 本地词典导出：输出 JSON 路径

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto take = [&](std::string& dst) { if (i + 1 < argc) dst = argv[++i]; else { std::cerr << "Missing value for " << a << "\n"; std::exit(2);} };
        if (a == "-d" || a == "--dict") { std::string p; take(p); dict_paths.push_back(p); }
        else if (a == "-m" || a == "--mode") { take(mode); }
        else if (a == "-p" || a == "--pattern") { take(pattern); }
        else if (a == "--list-dicts") { list_dicts = true; }
        else if (a == "--list-dicts-verbose") { list_dicts_verbose = true; }
        else if (a == "--all") { do_all = true; }
        else if (a == "--drop-dict") { take(drop_dict); }
        else if (a == "--export-dict") { take(export_dict_name); take(export_dict_out); }
        else if (a == "--list-plugins") { list_plugins = true; }
        else if (a == "--mdx-debug") { take(mdx_debug_path); }
        else if (a == "--where") { take(where_word); }
        else if (a == "--scan-dir") { take(scan_dir); }
        else if (a == "--index-save") { take(index_save); }
        else if (a == "--index-load") { take(index_load); }
        else if (a == "--clear-cache") { clear_cache = true; }
        else if (a == "--cache-prune-mb") { std::string n; take(n); cache_prune_mb = std::max(0, std::atoi(n.c_str())); }
        else if (a == "--cache-prune-days") { std::string n; take(n); cache_prune_days = std::max(0, std::atoi(n.c_str())); }
        else if (a == "--cache-size") { cache_size = true; }
        else if (a == "--cache-dir") { print_cache_dir = true; }
        else if (a == "--data-dir") { print_data_dir = true; }
        else if (a == "--dump-words") { std::string n; take(n); dump_n = std::max(1, std::atoi(n.c_str())); }
        else if (a == "--fulltext-index-save" || a == "--ft-index-save") { take(ft_index_save); }
        else if (a == "--fulltext-index-load" || a == "--ft-index-load") { take(ft_index_load); }
        else if (a == "--ft-index-upgrade-in") { take(ft_up_in); }
        else if (a == "--ft-index-upgrade-out") { take(ft_up_out); }
        else if (a == "--ft-index-upgrade-dir") { take(ft_up_dir); }
        else if (a == "--ft-index-out-dir") { take(ft_out_dir); }
        else if (a == "--ft-index-upgrade-suffix") { take(ft_up_suffix); }
        else if (a == "--ft-index-dry-run") { ft_dry_run = true; }
        else if (a == "--ft-index-filter-ext") { take(ft_filter_exts); }
        else if (a == "--ft-index-force") { ft_force = true; }
        else if (a == "--ft-index-exclude-glob") { take(ft_exclude_glob); }
        else if (a == "--ft-index-log") { take(ft_log_path); }
        else if (a == "--ft-index-compat") { take(ft_compat); }
        else if (a == "--index-count") { index_count = true; }
        else if (a == "--fulltext-index-stats" || a == "--ft-index-stats") { take(ft_stats_path); }
        else if (a == "--ft-index-verify") { take(ft_verify_path); }
        else if (a == "--mdict-password") { take(mdict_password); }
        else if (a == "--pron-score") { take(pron_score_wav); }
        else if (a == "--pron-phones") { take(pron_phones); }
        else if (a == "--pron-ipa") { take(pron_ipa); }
        else if (a == "--pron-model") { take(pron_model); }
        else if (a == "--pron-vocab") { take(pron_vocab_path); }
        else if (a == "--pron-dump") { pron_dump = true; }
        else if (a == "--pron-fetch-model") { pron_fetch = true; }
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (!a.empty() && a[0] == '-') { std::cerr << "Unknown option: " << a << "\n"; std::cerr << "Use --help for usage information.\n"; return 2; }
        else { word = a; }
    }

    if (!mdict_password.empty()) {
        set_process_env("UNIDICT_MDICT_PASSWORD", mdict_password);
    }

    // M10 模型资产自举下载：取齐并校验评分模型（断点续传，坏包自动重来
    // 一次）。不挂 UNIDICT_HAVE_PRON：取资产本身不需要推理运行时，而且
    // std-only 构建恰恰是最需要它的场景（先取模型，再用 PRON 构建评分）
    if (pron_fetch) {
        const std::string dir = pron_model_dir();
        std::cout << "模型目录：" << dir << "\n";
        std::error_code mk;
        std::filesystem::create_directories(dir, mk);
        if (mk) {
            std::cerr << "建不了模型目录：" << mk.message() << "\n";
            return 5;
        }
        if (!curl_available()) {
            std::cerr << "curl 不可用：本命令经系统 curl 完成下载（Windows 10 "
                         "1803 前无内置 curl）。请安装 curl 后重试，或手动下载"
                         "资产后放到 " << dir << "。\n";
            return 5;
        }
        bool ok = true;
        for (const UnidictCoreStd::ModelAsset& a : pron_model_assets()) {
            ok = fetch_asset(a, dir) && ok;   // 不短路：两个资产都报一遍
        }
        if (!ok) {
            std::cerr << "模型资产没取齐；评分功能保持不可用（跟读模式不受影响）\n";
            return 5;
        }
        std::cout << "模型资产齐了（哈希已校验），可以直接 --pron-score，"
                     "不必再带 --pron-model/--pron-vocab\n";
        return 0;
    }

    // 发音评分（M3b）：wav + ARPAbet 音素 → 每音素 GOP + 词分。
    // 不走词典路径，独立成一支；模型/词表默认取约定目录里的资产
    if (!pron_score_wav.empty() || pron_dump) {
#if UNIDICT_HAVE_PRON
        if (!pron_ipa.empty() && !pron_phones.empty()) {
            std::cerr << "--pron-ipa and --pron-phones are mutually exclusive\n";
            return 2;
        }
        if (!pron_dump && pron_phones.empty() && pron_ipa.empty()) {
            std::cerr << "--pron-score requires --pron-phones or --pron-ipa"
                         " (target phones)\n";
            return 2;
        }
        // 模型/词表：显式参数 > env > 约定目录（core/std 单一真源）
        if (const UnidictCoreStd::ModelAsset* m =
                find_pron_model_asset("model")) {
            pron_model = resolve_model_path(pron_model, "UNIDICT_PRON_MODEL", *m);
        }
        if (const UnidictCoreStd::ModelAsset* v =
                find_pron_model_asset("vocab")) {
            pron_vocab_path =
                resolve_model_path(pron_vocab_path, "UNIDICT_PRON_VOCAB", *v);
        }
        // 缺文件时说清去哪儿取（比 onnxruntime 几分钟后那句加载失败强）
        for (const std::string& p : {pron_model, pron_vocab_path}) {
            if (!std::filesystem::exists(p)) {
                std::cerr << "模型文件不存在：" << p
                          << "\n提示：unidict_cli_std --pron-fetch-model 可下载并校验（目录 "
                          << pron_model_dir() << "）\n";
                return 2;
            }
        }
        if (pron_score_wav.empty()) {
            std::cerr << "--pron-dump requires --pron-score <wav> as audio source\n";
            return 2;
        }
        std::vector<int16_t> pcm;
        std::string err;
        if (!UnidictCoreStd::load_wav_16k_mono(pron_score_wav, pcm, err)) {
            std::cerr << err << "\n";
            return 2;
        }
        // 目标音素：ARPAbet 直给，或词典 IPA 文本转换（M4）；解析失败
        // 整串拒收，绝不拿半截序列去评分
        std::vector<std::string> phones;
        if (!pron_ipa.empty()) {
            auto conv = UnidictCoreStd::phonetic_text_to_arpabet(pron_ipa);
            if (!conv) {
                std::cerr << "--pron-ipa: not parseable as ARPAbet or English"
                             " IPA: " << pron_ipa << "\n";
                return 2;
            }
            phones = std::move(*conv);
        } else {
            // 空格拆音素（多个连续空格容忍）
            for (size_t i = 0; i < pron_phones.size();) {
                while (i < pron_phones.size() && pron_phones[i] == ' ') ++i;
                size_t j = pron_phones.find(' ', i);
                if (j == std::string::npos) { phones.push_back(pron_phones.substr(i)); break; }
                phones.push_back(pron_phones.substr(i, j - i));
                i = j;
            }
        }
        UnidictPron::PronScorerOnnx::Config cfg;
        cfg.model_path = pron_model;
        cfg.vocab_path = pron_vocab_path;
        auto scorer = UnidictPron::PronScorerOnnx::load(cfg, err);
        if (!scorer) { std::cerr << err << "\n"; return 3; }

        if (pron_dump) {
            auto frames = scorer->diagnose(pcm, err);
            if (frames.empty() && !err.empty()) { std::cerr << err << "\n"; return 4; }
            for (const auto& f : frames) {
                std::cout << std::fixed << std::setprecision(0)
                          << "t=" << f.frame * 20 << "ms"
                          << std::setprecision(3)
                          << "  top1=\"" << f.best_label << "\" logp="
                          << f.best_logp << "\n";
            }
            return 0;
        }
        auto result = scorer->score(pcm, phones, err);
        if (!result) { std::cerr << err << "\n"; return 4; }
        const auto& vocab = scorer->vocab();
        for (const auto& p : result->phones) {
            // 展示域换算：ARPAbet → espeak 主键（词表里必然有——
            // score 已保证，否则返回 nullopt）
            const std::string espeak = UnidictCoreStd::arpabet_to_espeak(p.arpabet);
            const double ms_start = p.start_frame * 1000.0 / UnidictPron::PronScorerOnnx::kFramesPerSecond;
            const double ms_end = p.end_frame * 1000.0 / UnidictPron::PronScorerOnnx::kFramesPerSecond;
            std::cout << std::setw(4) << p.arpabet << " (" << std::setw(4) << espeak << ") "
                      << std::fixed << std::setprecision(0)
                      << "[" << ms_start << "-" << ms_end << " ms] "
                      << std::setprecision(3) << "gop=" << p.score
                      << "  (mean_logp=" << p.mean_log_prob << ")";
            // 混淆定位（M6）与地道变体实报（M7）：-> 是错读（已扣分），
            // ~ 是位置感知容忍的实读（按它记分，未扣分）
            if (!p.confused_with.empty()) {
                std::cout << "  -> " << p.confused_with;
            } else if (!p.realized_as.empty()) {
                std::cout << "  ~ " << p.realized_as << " (tolerated)";
            }
            std::cout << "\n";
        }
        std::cout << "word_score=" << std::fixed << std::setprecision(3)
                  << result->word_score << "\n";
        return 0;
#else
        std::cerr << "pron scoring not built (configure with -DUNIDICT_BUILD_PRON=ON)\n";
        return 2;
#endif
    }

    if (dict_paths.empty()) {
        auto envs = split_env_paths(std::getenv("UNIDICT_DICTS"));
        dict_paths.insert(dict_paths.end(), envs.begin(), envs.end());
    }

    // Scan dir for supported files（manager 单一口径：registry 扩展名
    // 全集 + canonical 排序；cli 新 manager 无已装载/隔离，全量返回）
    if (!scan_dir.empty()) {
        UnidictCoreStd::DictionaryManagerStd scanner;
        auto scanned = scanner.scan_directory(scan_dir);
        dict_paths.insert(dict_paths.end(), scanned.begin(), scanned.end());
    }

    // Quick stats/verify without full manager load
    if (!ft_stats_path.empty()) {
        UnidictCoreStd::FullTextIndexStd ft;
        if (!ft.load(ft_stats_path)) { std::cerr << "Load failed: " << ft.last_error() << "\n"; return 3; }
        auto s = ft.stats();
        std::cout << "version=" << s.version << "\n";
        std::cout << "docs=" << s.docs << "\n";
        std::cout << "terms=" << s.terms << "\n";
        std::cout << "postings=" << s.postings << "\n";
        std::cout << "compressed_terms=" << s.compressed_terms << "\n";
        std::cout << "compressed_bytes=" << s.compressed_bytes << "\n";
        std::cout << "pairs_decompressed=" << s.pairs_decompressed << "\n";
        std::cout << "avg_df=" << s.avg_df << "\n";
        return 0;
    }
    if (!ft_verify_path.empty()) {
        UnidictCoreStd::FullTextIndexStd ft;
        if (!ft.load(ft_verify_path)) { std::cerr << "Verify fail: " << ft.last_error() << "\n"; return 3; }
        std::cout << "OK version=" << ft.version() << "\n";
        return 0;
    }

    // Load dictionaries through std manager
    DictionaryManagerStd mgr;

    for (const auto& p : dict_paths) mgr.add_dictionary(p);
    mgr.build_index();

    // Upgrade operation (single-file): requires dicts to sign the new index
    if (!ft_up_in.empty() && !ft_up_out.empty()) {
        int ver = 0; std::string err;
        bool ok = mgr.load_fulltext_index_relaxed(ft_up_in, &ver, &err);
        if (!ok) { std::cerr << "Upgrade failed to load input index: " << err << "\n"; return 3; }
        bool saved = mgr.save_fulltext_index(ft_up_out);
        if (!saved) { std::cerr << "Upgrade failed to save output index\n"; return 3; }
        std::cout << "Upgraded fulltext index from v" << ver << " to v2 with signature: " << mgr.fulltext_signature() << "\n";
        return 0;
    }

    // Batch upgrade (directory, recursive), only upgrades legacy v1 files; writes <file><suffix>
    if (!ft_up_dir.empty()) {
        int total = 0, upgraded = 0, skipped = 0, failed = 0;
        // Build extension filter set
        std::vector<std::string> filter_exts;
        if (!ft_filter_exts.empty()) {
            std::string s = ft_filter_exts; size_t i = 0;
            while (i < s.size()) {
                size_t j = s.find(',', i);
                std::string e = s.substr(i, j==std::string::npos ? s.size()-i : j-i);
                // normalize to lower and ensure leading dot
                for (auto& c : e) c = (char)std::tolower((unsigned char)c);
                if (!e.empty() && e[0] != '.') e = std::string(".") + e;
                if (!e.empty()) filter_exts.push_back(e);
                if (j == std::string::npos) break; i = j + 1;
            }
        }
        // Build exclude regex list from glob
        auto glob_to_regex = [](const std::string& g) {
            std::string re; re.reserve(g.size()*2); re.push_back('^');
            for (char c : g) {
                switch (c) {
                    case '*': re += ".*"; break;
                    case '?': re.push_back('.'); break;
                    case '.': case '\\': case '+': case '(': case ')': case '[': case ']': case '{': case '}': case '^': case '$': case '|':
                        re.push_back('\\'); re.push_back(c); break;
                    default: re.push_back(c); break;
                }
            }
            re.push_back('$'); return std::regex(re, std::regex::icase);
        };
        std::vector<std::regex> exclude_res;
        if (!ft_exclude_glob.empty()) {
            std::string s = ft_exclude_glob; size_t i = 0;
            while (i < s.size()) {
                size_t j = s.find(',', i);
                std::string pat = s.substr(i, j==std::string::npos ? s.size()-i : j-i);
                if (!pat.empty()) exclude_res.push_back(glob_to_regex(pat));
                if (j == std::string::npos) break; i = j + 1;
            }
        }
        struct LogItem { std::string path; std::string out; std::string action; std::string reason; int old_ver=0; int new_ver=0; std::string sig_hex; };
        std::vector<LogItem> logs;
        for (auto& de : std::filesystem::recursive_directory_iterator(ft_up_dir)) {
            if (!de.is_regular_file()) continue;
            const std::string path = de.path().string();
            // exclude glob
            bool excluded = false;
            if (!exclude_res.empty()) {
                for (const auto& re : exclude_res) { if (std::regex_match(path, re)) { excluded = true; break; } }
            }
            if (excluded) { ++skipped; logs.push_back({path, "", "skipped", "excluded-by-glob", 0, 0, ""}); continue; }
            // extension filter
            if (!filter_exts.empty()) {
                std::string ext = de.path().extension().string();
                for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
                bool match = false; for (auto& e : filter_exts) if (ext == e) { match = true; break; }
                if (!match) { logs.push_back({path, "", "skipped", "filtered-by-ext", 0, 0, ""}); continue; }
            }
            ++total;
            int ver = 0; std::string err;
            if (!mgr.load_fulltext_index_relaxed(path, &ver, &err)) { ++skipped; logs.push_back({path, "", "skipped", std::string("load-failed:") + err, ver, 0, ""}); continue; }
            if (ver >= 2) { ++skipped; logs.push_back({path, "", "skipped", "already-signed", ver, ver, ""}); continue; }
            std::string out;
            if (!ft_out_dir.empty()) {
                try {
                    std::filesystem::path rel = std::filesystem::relative(de.path(), std::filesystem::path(ft_up_dir));
                    out = (std::filesystem::path(ft_out_dir) / rel).string() + ft_up_suffix;
                } catch (...) {
                    out = (std::filesystem::path(ft_out_dir) / de.path().filename()).string() + ft_up_suffix;
                }
            } else {
                out = path + ft_up_suffix;
            }
            if (!ft_force && std::filesystem::exists(out)) { ++skipped; logs.push_back({path, out, "skipped", "exists", ver, 2, ""}); continue; }
            if (ft_dry_run) {
                // compute signature hex prefix
                std::string sig = mgr.fulltext_signature();
                size_t bar = sig.find('|');
                std::string hex = (bar==std::string::npos)? sig : sig.substr(0, bar);
                std::cout << "DRY-RUN upgrade v" << ver << ": " << path << " -> " << out << " (sig=" << hex << ")\n";
                logs.push_back({path, out, "dry-run", "", ver, 2, hex});
                ++upgraded; // count as would-upgrade
                continue;
            }
            // ensure destination dir exists when using out-dir
            if (!ft_out_dir.empty()) {
                std::error_code ec; std::filesystem::create_directories(std::filesystem::path(out).parent_path(), ec);
            }
            if (mgr.save_fulltext_index(out)) {
                std::cout << "Upgraded: " << path << " -> " << out << "\n";
                std::string sig = mgr.fulltext_signature();
                size_t bar = sig.find('|');
                std::string hex = (bar==std::string::npos)? sig : sig.substr(0, bar);
                logs.push_back({path, out, "upgraded", "", ver, 2, hex});
                ++upgraded;
            } else {
                std::cerr << "Failed to save upgraded index for: " << path << "\n";
                logs.push_back({path, out, "failed", "save-failed", ver, 2, ""});
                ++failed;
            }
        }
        std::cout << "Batch upgrade summary: total=" << total << ", upgraded=" << upgraded << ", skipped=" << skipped << ", failed=" << failed << "\n";
        if (!ft_log_path.empty()) {
            std::error_code ec; std::filesystem::create_directories(std::filesystem::path(ft_log_path).parent_path(), ec);
            std::ofstream log(ft_log_path, std::ios::binary | std::ios::trunc);
            if (log) {
                log << "path,out,action,reason,old_version,new_version,signature\n";
                auto esc = [](const std::string& s){ std::string t; t.reserve(s.size()+8); for(char c: s){ if(c=='"') t.push_back('"'); t.push_back(c);} return t; };
                for (const auto& li : logs) {
                    log << '"' << esc(li.path) << '"' << ','
                        << '"' << esc(li.out) << '"' << ','
                        << li.action << ',' << li.reason << ','
                        << li.old_ver << ',' << li.new_ver << ','
                        << '"' << esc(li.sig_hex) << '"' << '\n';
                }
            } else {
                std::cerr << "Failed to write log: " << ft_log_path << "\n";
            }
        }
        return failed == 0 ? 0 : 3;
    }

    if (list_dicts || list_dicts_verbose) {
        auto names = mgr.loaded_dictionaries();
        std::cout << "Loaded dictionaries (" << names.size() << ")\n";
        if (list_dicts_verbose) {
            auto metas = mgr.dictionaries_meta();
            for (auto& m : metas) std::cout << "- " << m.name << " (words=" << m.word_count << ") " << m.description << "\n";
        } else {
            for (auto& n : names) std::cout << "- " << n << "\n";
        }
        return 0;
    }

    if (!export_dict_name.empty()) {
        const UnidictCoreStd::DictionaryExportResultStd r =
            UnidictCoreStd::export_dictionary_json(mgr, export_dict_name, export_dict_out);
        if (r.ok) {
            std::cout << "Exported " << r.entry_count << " entries -> "
                      << export_dict_out << "\n";
        } else {
            std::cerr << "Export failed: " << r.error << "\n";
        }
        return r.ok ? 0 : 6;
    }

    if (!drop_dict.empty()) {
        bool ok = mgr.remove_dictionary(drop_dict);
        std::cout << (ok?"Removed":"Not found") << " " << drop_dict << "\n";
        return ok ? 0 : 6;
    }

    if (list_plugins) {
        std::cout << "Registered parser extensions:\njson\nifo\nmdx\ndsl\ncsv\ntsv\ntxt\nepub\n";
        return 0;
    }

    if (!mdx_debug_path.empty()) {
        // Heuristic debug: show header line and known container markers
        std::ifstream fin(mdx_debug_path, std::ios::binary);
        if (!fin) { std::cerr << "Cannot open: " << mdx_debug_path << "\n"; return 2; }
        std::string file; {
            std::ostringstream ss; ss << fin.rdbuf(); file = ss.str();
        }
        auto header_end = file.find('\n');
        std::string header = header_end == std::string::npos ? file.substr(0, std::min<size_t>(file.size(), 256))
                                                             : file.substr(0, std::min<size_t>(header_end, 256));
        bool utf16le = file.size()>=2 && (unsigned char)file[0]==0xFF && (unsigned char)file[1]==0xFE;
        bool utf16be = file.size()>=2 && (unsigned char)file[0]==0xFE && (unsigned char)file[1]==0xFF;
        std::cout << "Header (first line or 256 bytes):\n" << header << "\n";
        std::cout << "UTF16LE=" << (utf16le?"yes":"no") << ", UTF16BE=" << (utf16be?"yes":"no") << "\n";
        auto scan = [&](const char* tag){ size_t pos = 0, cnt=0; while (true) { pos = file.find(tag, pos); if (pos==std::string::npos) break; ++cnt; pos+=4; } return cnt; };
        const char* tags[] = {"MDXK","MDXR","KBIX","RBIX","RBCT","RBLK","KEYB","RECB","KIDX","RDEF","SIMPLEKV"};
        for (auto t : tags) std::cout << t << ": " << scan(t) << "\n";
        // quick zlib header count
        size_t zc = 0; for (size_t i=0;i+1<file.size();++i){ unsigned char cmf=file[i],flg=file[i+1]; if ((cmf&0x0F)==8 && (((unsigned int)cmf<<8|flg)%31)==0) ++zc; }
        std::cout << "zlib_header_candidates: " << zc << "\n";
        return 0;
    }

    if (!index_load.empty()) { mgr.load_index(index_load); }
    if (!ft_index_load.empty()) {
        auto lc = lcase(ft_compat);
        if (lc != "strict" && lc != "auto" && lc != "loose") lc = "auto";
        bool ok = false;
        if (lc == "strict") {
            ok = mgr.load_fulltext_index(ft_index_load);
            if (!ok) std::cerr << "Fulltext index load failed in strict mode (signature/version).\n";
        } else if (lc == "auto") {
            ok = mgr.load_fulltext_index(ft_index_load);
            if (!ok) {
                int ver = 0; std::string err;
                if (mgr.load_fulltext_index_relaxed(ft_index_load, &ver, &err)) {
                    if (ver == 1) {
                        std::cerr << "Loaded legacy fulltext index v1 without signature (auto mode).\n";
                        ok = true;
                    } else {
                        std::cerr << "Fulltext index load failed: " << err << "\n";
                    }
                } else {
                    std::cerr << "Fulltext index load failed: " << err << "\n";
                }
            }
        } else { // loose
            if (!mgr.load_fulltext_index(ft_index_load)) {
                int ver = 0; std::string err;
                if (mgr.load_fulltext_index_relaxed(ft_index_load, &ver, &err)) {
                    std::cerr << "WARNING: Fulltext index loaded in loose mode (signature not verified, version=" << ver << ").\n";
                    ok = true;
                } else {
                    std::cerr << "Fulltext index load failed even in loose mode: " << err << "\n";
                }
            } else {
                ok = true;
            }
        }
        (void)ok;
    }
    if (clear_cache) { bool ok = PathUtilsStd::clear_cache(); std::cout << (ok?"Cache cleared":"Cache clear failed") << "\n"; if (word.empty()) return ok?0:4; }
    if (cache_prune_mb >= 0) {
        bool ok = PathUtilsStd::prune_cache_bytes((std::uint64_t)cache_prune_mb * 1024ull * 1024ull);
        std::cout << (ok?"Cache pruned":"Cache prune failed") << " (max MB=" << cache_prune_mb << ")\n";
        if (word.empty()) return ok?0:4;
    }
    if (cache_prune_days >= 0) {
        bool ok = PathUtilsStd::prune_cache_older_than_days(cache_prune_days);
        std::cout << (ok?"Cache pruned by age":"Cache age prune failed") << " (days=" << cache_prune_days << ")\n";
        if (word.empty()) return ok?0:4;
    }
    if (cache_size) { std::cout << PathUtilsStd::cache_size_bytes() << "\n"; if (word.empty()) return 0; }
    if (print_cache_dir) { std::cout << PathUtilsStd::cache_dir() << "\n"; if (word.empty()) return 0; }
    if (print_data_dir) { std::cout << PathUtilsStd::data_dir() << "\n"; if (word.empty()) return 0; }
    if (index_count) { std::cout << mgr.indexed_word_count() << "\n"; if (word.empty()) return 0; }
    if (dump_n > 0 && word.empty()) { auto w = mgr.all_indexed_words(); for (int i=0;i<(int)w.size() && i<dump_n;++i) std::cout << w[i] << "\n"; return 0; }
    if (!where_word.empty()) {
        auto ds = mgr.dictionaries_for_word(where_word);
        for (auto& s : ds) std::cout << s << "\n";
        return 0;
    }

    if (word.empty()) { usage(); return 1; }

    // Perform search
    std::vector<std::string> results;
    std::string lower_mode = lcase(mode);
    // 计数头里显示的查询串：wildcard/fulltext 的真查询是 -p/--pattern（缺省
    // 才落到位置参数），其余模式就是位置参数本身
    const std::string query_display =
        (lower_mode == "wildcard" || lower_mode == "fulltext") && !pattern.empty()
            ? pattern : word;
    if (lower_mode == "exact") {
        auto v = mgr.exact_search(word);
        results.insert(results.end(), v.begin(), v.end());
    } else if (lower_mode == "prefix") {
        results = mgr.prefix_search(word, 50);
    } else if (lower_mode == "fuzzy") {
        results = mgr.fuzzy_search(word, 50);
    } else if (lower_mode == "wildcard") {
        std::string pat = pattern.empty()?word:pattern;
        results = mgr.wildcard_search(pat, 50);
    } else if (lower_mode == "regex") {
        results = mgr.regex_search(word, 50);
    } else if (lower_mode == "fulltext") {
        auto ents = mgr.full_text_search(pattern.empty()?word:pattern, 20);
        std::cout << "# fulltext \"" << query_display << "\" -> "
                  << ents.size() << " matches\n";
        bool any = false;
        for (auto& e : ents) {
            std::cout << e.word << " [" << e.dict_name << "]: " << e.definition << "\n";
            any = true;
        }
        if (!index_save.empty()) { mgr.save_index(index_save); }
        if (!ft_index_save.empty()) { mgr.save_fulltext_index(ft_index_save); }
        return any ? 0 : 7;
    } else {
        std::cerr << "Unknown mode: " << mode << "\n"; return 2;
    }

    // Print and optionally lookup definitions
    auto print_def = [&](const std::string& w){
        auto ents = mgr.search_all(w);
        if (!ents.empty()) { std::cout << w << ": " << ents.front().definition << "\n"; return true; }
        std::cout << "Word not found: " << w << "\n"; return false;
    };

    bool any = false;
    if (lower_mode == "exact" && !results.empty()) {
        if (do_all) {
            auto ents = mgr.search_all(word);
            for (auto& e : ents) { std::cout << e.dict_name << ": " << e.definition << "\n"; any = true; }
        } else {
            any = print_def(word);
        }
    } else if (lower_mode == "exact") {
        // 词头索引没命中（大小写差异、或英文只存在于释义里）：直查各词典
        // 释义并允许释义全文兜底，再不济也明确报「未找到」——原来这条
        // 分支什么都不打就 exit 7，用户只看到空输出（BUGS.md BUG-005）
        auto ents = mgr.search_all(word, /*include_disabled=*/false,
                                   /*allow_fulltext_fallback=*/true);
        for (auto& e : ents) {
            std::cout << e.word << "（" << e.dict_name << "，释义匹配）: "
                      << e.definition << "\n";
            any = true;
        }
        if (!any) {
            // 只有「确实有词典可查」才报未找到。全部词典都没打开（加密缺
            // 密码/损坏等）时报出来等于把「查不了」说成「查不到」，且与既有
            // 契约（静默 + exit 7，见 test_cli_std_mdict_password）冲突
            if (mgr.indexed_word_count() > 0) {
                std::cout << "Word not found: " << word << "\n";
                // Did-you-mean：模糊优先 + 前缀补位（suggest_corrections 统一
                // 口径，与 qmlui 同源）；空建议不打印块。同在「有词典可查」
                // 守卫内——查不了时保持全静默
                const auto suggestions = mgr.suggest_corrections(word, 5);
                if (!suggestions.empty()) {
                    std::cout << "Did you mean:\n";
                    for (const auto& s : suggestions) std::cout << "  " << s << "\n";
                }
            }
        }
    } else {
        // 词表四模式（prefix/fuzzy/wildcard/regex）：计数头 + 「词头<TAB>词典
        // 归属」行。# 头 grep -v 一行过滤；cut -f1 仍取回裸词表——grep 形
        // 消费兼容，归属与命中规模一眼可读
        std::cout << "# " << lower_mode << " \"" << query_display << "\" -> "
                  << results.size() << " matches\n";
        for (const auto& w : results) {
            std::cout << w;
            const auto dicts = mgr.dictionaries_for_word(w);
            if (!dicts.empty()) {
                std::cout << '\t';
                for (size_t i = 0; i < dicts.size(); ++i) {
                    if (i) std::cout << ", ";
                    std::cout << dicts[i];
                }
            }
            std::cout << "\n";
            any = true;
        }
    }

    if (!index_save.empty()) { mgr.save_index(index_save); }
    return any ? 0 : 7;
}
