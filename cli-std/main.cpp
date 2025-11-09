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

using namespace UnidictCoreStd;


static std::string lcase(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }

static std::vector<std::string> split_env_paths(const char* env) {
    std::vector<std::string> out; if (!env) return out; std::string s(env);
    size_t i = 0; while (i < s.size()) { size_t j = s.find_first_of(";:", i); out.push_back(s.substr(i, j == std::string::npos ? s.size() - i : j - i)); if (j == std::string::npos) break; i = j + 1; }
    return out;
}

static void usage() {
    std::cout << "Usage: unidict_cli_std [-d <dict> ...] [--mode exact|prefix|fuzzy|wildcard|regex|fulltext] <word>\n";
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
    bool ft_dry_run = false;
    std::string word;
    std::string ft_compat = "auto"; // strict|auto|loose

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
        else if (a == "--ft-index-upgrade-suffix") { take(ft_up_suffix); }
        else if (a == "--ft-index-dry-run") { ft_dry_run = true; }
        else if (a == "--ft-index-compat") { take(ft_compat); }
        else if (a == "--index-count") { index_count = true; }
        else if (!a.empty() && a[0] == '-') { std::cerr << "Unknown option: " << a << "\n"; return 2; }
        else { word = a; }
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
        for (auto& de : std::filesystem::recursive_directory_iterator(ft_up_dir)) {
            if (!de.is_regular_file()) continue;
            const std::string path = de.path().string();
            ++total;
            int ver = 0; std::string err;
            if (!mgr.load_fulltext_index_relaxed(path, &ver, &err)) { ++skipped; continue; }
            if (ver >= 2) { ++skipped; continue; }
            const std::string out = path + ft_up_suffix;
            if (ft_dry_run) {
                std::cout << "DRY-RUN upgrade v" << ver << ": " << path << " -> " << out << "\n";
                ++upgraded; // count as would-upgrade
                continue;
            }
            if (mgr.save_fulltext_index(out)) {
                std::cout << "Upgraded: " << path << " -> " << out << "\n";
                ++upgraded;
            } else {
                std::cerr << "Failed to save upgraded index for: " << path << "\n";
                ++failed;
            }
        }
        std::cout << "Batch upgrade summary: total=" << total << ", upgraded=" << upgraded << ", skipped=" << skipped << ", failed=" << failed << "\n";
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
