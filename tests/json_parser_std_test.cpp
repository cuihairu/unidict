#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "std/json_parser_std.h"
#include "std/text_norm_std.h"

int main() {
    UnidictCoreStd::JsonParserStd jp;
    bool ok = false;
    const char* candidates[] = {"examples/dict.json", "../examples/dict.json", "../../examples/dict.json"};
    for (const char* p : candidates) { if (jp.load_dictionary(p)) { ok = true; break; } }
    assert(ok);
    assert(jp.is_loaded());
    assert(jp.word_count() >= 3);
    auto d = jp.lookup("hello");
    assert(!d.empty());
    auto sim = jp.find_similar("he", 10);
    bool has_hello = false; for (auto& s : sim) if (s == "hello") has_hello = true; assert(has_hello);

    // ---- norm options：Exact 档掐掉折叠回退（P1 假命中防线） ----
    {
        namespace fs = std::filesystem;
        fs::path dir = fs::current_path() / "build-local";
        fs::create_directories(dir);
        fs::path p = dir / "json_norm_fallback.json";
        {
            std::ofstream o(p, std::ios::binary);
            o << "{\"entries\":[{\"word\":\"école\",\"definition\":\"school\"},"
                 "{\"word\":\"don\xE2\x80\x99t\",\"definition\":\"not\"}]}";
        }
        UnidictCoreStd::JsonParserStd jp2;
        assert(jp2.load_dictionary(p.string()));
        // 默认宽容：大写/重音形态经折叠回退命中
        assert(!jp2.lookup("École").empty());
        assert(jp2.lookup("don't") == "not");
        // Exact：回退按原样键折叠，形态差异不再互撞
        jp2.set_norm_options(
            UnidictCoreStd::TextNorm::options_for_profile(
                UnidictCoreStd::TextNorm::Profile::Exact));
        assert(jp2.lookup("École").empty());
        assert(jp2.lookup("don't").empty());
        assert(jp2.lookup("école") == "school");
        assert(jp2.lookup("don\xE2\x80\x99t") == "not");
        // 切回宽容：缓存按档位重建，回退恢复
        jp2.set_norm_options(
            UnidictCoreStd::TextNorm::options_for_profile(
                UnidictCoreStd::TextNorm::Profile::Tolerant));
        assert(!jp2.lookup("École").empty());
        assert(jp2.lookup("don't") == "not");
    }

    std::cout << "OK\n";
    return 0;
}
