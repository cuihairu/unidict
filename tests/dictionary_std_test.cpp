// DictionaryStd 实例层单测（P-3 3.1 抽取）：
// 实例级 load/元数据/状态访问器/伴生 .mdd 附加/无解析器兜底。
// manager 面的行为（search/enable/remove/签名）由既有 manager 测试守卫
// （dictionary_manager_std_test / _branches / _meta_std），本文件只测
// 实例本身的契约。

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_std.h"

using namespace UnidictCoreStd;

static void be16w(std::vector<unsigned char>& v, uint16_t x) { v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }
static void be32w(std::vector<unsigned char>& v, uint32_t x) { v.push_back((x >> 24) & 0xFF); v.push_back((x >> 16) & 0xFF); v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }

// MDict 词典体：SIMPLEKV 容器（键值对序列）
static std::vector<unsigned char> make_simplekv(const std::vector<std::pair<std::string, std::string>>& kv) {
    std::vector<unsigned char> v;
    const std::string magic = "SIMPLEKV";
    v.insert(v.end(), magic.begin(), magic.end());
    be32w(v, (uint32_t)kv.size());
    for (const auto& p : kv) {
        be16w(v, (uint16_t)p.first.size());
        v.insert(v.end(), p.first.begin(), p.first.end());
        be32w(v, (uint32_t)p.second.size());
        v.insert(v.end(), p.second.begin(), p.second.end());
    }
    return v;
}

// MDict 文件 = XML 头一行 + 容器体（同 dictionary_manager_std_test 夹具）
static void write_mdict_like_file(const std::filesystem::path& path,
                                  const std::vector<unsigned char>& body) {
    std::string header = "<Dictionary title=\"DSMDX\" description=\"ds mdx\"/>\n";
    std::ofstream out(path.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
    assert(out.good());
}

static std::filesystem::path base_dir() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "dict_std";
    fs::create_directories(dir);
    return dir;
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = base_dir();

    // --- T1 json 实例：元数据 + 状态访问器 + 分派 ---
    {
        std::string json =
            "{\"name\":\"DS JSON\",\"description\":\"dict std json\",\"entries\":["
            "{\"word\":\"hello\",\"definition\":\"hi there\"}]}";
        fs::path jp = dir / "a.json";
        { std::ofstream o(jp, std::ios::trunc); o << json; }

        DictionaryStd d;
        assert(d.load(jp.string()));
        assert(d.name() == "DS JSON");
        assert(d.words().size() == 1 && d.words()[0] == "hello");
        assert(d.src_paths().size() == 1 && d.src_paths()[0] == jp.string());
        assert(d.enabled());
        assert(d.priority() == 0);
        assert(d.tags().empty());
        assert(d.mdd_parsers().empty());
        assert(d.lookup("hello") == "hi there");
        assert(d.lookup("missing") == "");
        assert(d.description() == "dict std json");

        d.set_enabled(false);
        d.set_priority(7);
        d.set_tags({"tag-a", "tag-b"});
        assert(!d.enabled());
        assert(d.priority() == 7);
        assert(d.tags().size() == 2 && d.tags()[0] == "tag-a");
    }

    // --- T2 mdx + 伴生完好 .mdd：资源附加 + src_paths 双路径 ---
    {
        fs::path mdx = dir / "b.mdx";
        fs::path mdd = dir / "b.mdd";
        const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + "DUMMY";
        write_mdict_like_file(mdx, make_simplekv({{"hello", "<div>hi</div>"}}));
        write_mdict_like_file(mdd, make_simplekv({{"pic.png", png}}));

        DictionaryStd d;
        assert(d.load(mdx.string()));
        assert(d.name() == "DSMDX");
        assert(d.lookup("hello").find("hi") != std::string::npos);
        assert(d.mdd_parsers().size() == 1);
        assert(d.mdd_parsers()[0]->file_path() == mdd.string());
        assert(d.src_paths().size() == 2);
        const bool has_mdd =
            std::find(d.src_paths().begin(), d.src_paths().end(), mdd.string()) != d.src_paths().end();
        assert(has_mdd);
    }

    // --- T3 mdx + 伴生损坏 .mdd：词典照常加载，资源跳过 ---
    {
        fs::path mdx = dir / "c.mdx";
        fs::path mdd = dir / "c.mdd";
        write_mdict_like_file(mdx, make_simplekv({{"alpha", "<div>a</div>"}}));
        { std::ofstream o(mdd, std::ios::binary | std::ios::trunc); o << "not an mdd at all"; }

        DictionaryStd d;
        assert(d.load(mdx.string()));
        assert(d.mdd_parsers().empty());
        assert(d.src_paths().size() == 2);  // 路径仍进签名绑定
        assert(d.lookup("alpha").find("a") != std::string::npos);
    }

    // --- T4 未知扩展名：load 失败 ---
    {
        fs::path up = dir / "d.xyz";
        { std::ofstream o(up, std::ios::trunc); o << "whatever"; }
        DictionaryStd d;
        assert(!d.load(up.string()));
    }

    // --- T5 空实例（未 load/load 失败）：lookup/description 安全空返回 ---
    {
        DictionaryStd d;
        assert(d.lookup("hello") == "");
        assert(d.description() == "");
    }

    // --- T6 非 .mdx 词典不持有 .mdd ---
    {
        fs::path jp = dir / "e.json";
        {
            std::ofstream o(jp, std::ios::trunc);
            o << "{\"name\":\"E\",\"description\":\"e\",\"entries\":["
                 "{\"word\":\"x\",\"definition\":\"y\"}]}";
        }
        DictionaryStd d;
        assert(d.load(jp.string()));
        assert(d.mdd_parsers().empty());
    }

    return 0;
}