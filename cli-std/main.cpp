#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "std/dictionary_manager_std.h"
#include "std/path_utils_std.h"
#include "std/data_store_std.h"
#include "std/fulltext_index_std.h"

using namespace UnidictCoreStd;


static std::string lcase(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }

static std::vector<std::string> split_env_paths(const char* env) {
    std::vector<std::string> out; if (!env) return out; std::string s(env);
    size_t i = 0; while (i < s.size()) { size_t j = s.find_first_of(";:", i); out.push_back(s.substr(i, j == std::string::npos ? s.size() - i : j - i)); if (j == std::string::npos) break; i = j + 1; }
    return out;
}

static void set_process_env(const char* key, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(key, value.c_str());
#else
    ::setenv(key, value.c_str(), 1);
#endif
}

static void usage() {
    std::cout << "Unidict CLI - Universal Dictionary Lookup Tool\n\n";
    std::cout << "Basic Usage:\n";
    std::cout << "  unidict_cli_std [-d <dict> ...] [--mode <mode>] <word>\n\n";

    std::cout << "Options:\n";
    std::cout << "  -d, --dict <path>        Add dictionary file (support .mdx, .ifo, .json)\n";
    std::cout << "  -m, --mode <mode>        Search mode: exact, prefix, fuzzy, wildcard, regex, fulltext\n";
    std::cout << "  -p, --pattern <pattern>  Search pattern (for wildcard/regex/fulltext)\n";
    std::cout << "  --mdict-password <pw>    Password for encrypted MDict (.mdx/.mdd)\n";
    std::cout << "  --help                    Show this help message\n\n";

    std::cout << "Dictionary Management:\n";
    std::cout << "  --list-dicts             List loaded dictionaries\n";
    std::cout << "  --list-dicts-verbose     List dictionaries with word counts\n";
    std::cout << "  --drop-dict <name>        Remove dictionary by name\n";
    std::cout << "  --scan-dir <path>        Scan directory for dictionaries\n\n";

    std::cout << "Search & Lookup:\n";
    std::cout << "  --where <word>            Show which dictionaries contain the word\n";
    std::cout << "  --all                     Show all definitions for exact match\n\n";

    std::cout << "Vocabulary & History:\n";
    std::cout << "  --save                    Save exact match to vocabulary\n";
    std::cout << "  --show-vocab              Display vocabulary book\n";
    std::cout << "  --history [N]             Show search history (default: 20)\n";
    std::cout << "  --export-vocab <file>     Export vocabulary to CSV\n\n";

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

    std::cout << "Environment Variables:\n";
    std::cout << "  UNIDICT_DICTS            Colon-separated dictionary paths\n\n";
    std::cout << "  UNIDICT_MDICT_PASSWORD   Password for encrypted MDict (.mdx/.mdd)\n";
    std::cout << "  UNIDICT_PASSWORD         Alias of UNIDICT_MDICT_PASSWORD (deprecated)\n\n";

    std::cout << "Examples:\n";
    std::cout << "  unidict_cli_std -d dict.mdx hello\n";
    std::cout << "  unidict_cli_std --mode prefix inter\n";
    std::cout << "  UNIDICT_DICTS=\"dict1.mdx:dict2.ifo\" unidict_cli_std word\n";
    std::cout << "  unidict_cli_std --fulltext-index-save ft.index --mode fulltext greeting\n\n";
}

int main(int argc, char** argv) {
    std::vector<std::string> dict_paths;
    std::string mode = "exact";
    std::string pattern;
    bool list_dicts = false, list_dicts_verbose = false, do_history = false, do_save = false, show_vocab = false, do_all = false;
    bool list_plugins = false;
    std::string drop_dict;
    std::string mdx_debug_path;
    int history_n = 20, dump_n = 0;
    std::string where_word;
    std::string scan_dir;
    std::string index_save, index_load;
    bool clear_cache = false;
    int cache_prune_mb = -1;
    int cache_prune_days = -1;
    bool cache_size = false;
    bool print_cache_dir = false;
    bool print_data_dir = false;
    std::string export_vocab;
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

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto take = [&](std::string& dst) { if (i + 1 < argc) dst = argv[++i]; else { std::cerr << "Missing value for " << a << "\n"; std::exit(2);} };
        if (a == "-d" || a == "--dict") { std::string p; take(p); dict_paths.push_back(p); }
        else if (a == "-m" || a == "--mode") { take(mode); }
        else if (a == "-p" || a == "--pattern") { take(pattern); }
        else if (a == "--list-dicts") { list_dicts = true; }
        else if (a == "--list-dicts-verbose") { list_dicts_verbose = true; }
        else if (a == "--history") { std::string n; take(n); history_n = std::max(1, std::atoi(n.c_str())); do_history = true; }
        else if (a == "--save") { do_save = true; }
        else if (a == "--show-vocab") { show_vocab = true; }
        else if (a == "--all") { do_all = true; }
        else if (a == "--drop-dict") { take(drop_dict); }
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
        else if (a == "--export-vocab") { take(export_vocab); }
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
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (!a.empty() && a[0] == '-') { std::cerr << "Unknown option: " << a << "\n"; std::cerr << "Use --help for usage information.\n"; return 2; }
        else { word = a; }
    }

    if (!mdict_password.empty()) {
        set_process_env("UNIDICT_MDICT_PASSWORD", mdict_password);
    }

    if (dict_paths.empty()) {
        auto envs = split_env_paths(std::getenv("UNIDICT_DICTS"));
        dict_paths.insert(dict_paths.end(), envs.begin(), envs.end());
    }

    // Scan dir for supported files
    if (!scan_dir.empty()) {
        for (auto& p : std::filesystem::recursive_directory_iterator(scan_dir)) {
            if (!p.is_regular_file()) continue;
            auto ext = lcase(p.path().extension().string());
            if (ext == ".ifo" || ext == ".mdx" || ext == ".json") dict_paths.push_back(p.path().string());
        }
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

    if (!drop_dict.empty()) {
        bool ok = mgr.remove_dictionary(drop_dict);
        std::cout << (ok?"Removed":"Not found") << " " << drop_dict << "\n";
        return ok ? 0 : 6;
    }

    if (list_plugins) {
        std::cout << "Registered parser extensions:\njson\nifo\nmdx\ndsl\ncsv\ntsv\ntxt\n";
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
    if (!export_vocab.empty()) { DataStoreStd ds; bool ok = ds.export_vocabulary_csv(export_vocab); std::cout << (ok?"Exported":"Failed") << "\n"; return ok?0:5; }
    if (show_vocab) { DataStoreStd ds; auto v = ds.get_vocabulary(); for (auto& it : v) std::cout << it.word << ": " << it.definition << "\n"; return 0; }
    if (do_history) { DataStoreStd ds; auto h = ds.get_search_history(history_n); for (auto& s : h) std::cout << s << "\n"; return 0; }
    if (!where_word.empty()) {
        auto ds = mgr.dictionaries_for_word(where_word);
        for (auto& s : ds) std::cout << s << "\n";
        return 0;
    }

    if (word.empty()) { usage(); return 1; }

    // Perform search
    std::vector<std::string> results;
    std::string lower_mode = lcase(mode);
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
        bool any = false;
        for (auto& e : ents) {
            std::cout << e.word << ": " << e.definition << "\n";
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
    } else {
        for (auto& w : results) { std::cout << w << "\n"; any = true; }
    }

    // save to vocabulary/history if requested
    if (any) {
        DataStoreStd ds;
        ds.add_search_history(word);
        if (do_save && lower_mode == "exact") {
            auto ents = mgr.search_all(word);
            if (!ents.empty()) ds.add_vocabulary_item({word, ents.front().definition});
        }
    }

    if (!index_save.empty()) { mgr.save_index(index_save); }
    return any ? 0 : 7;
}
