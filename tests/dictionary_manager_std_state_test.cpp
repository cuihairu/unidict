// DictionaryManagerStd 状态持久化 + 伴生 .mdd 资源访问单测（P-3 3.2c）：
// save/load_state 往返（enabled/priority/tags/两档失败表）、还原语义
// （隔离跳过/运行期自愈/解析失败升级）、截断容错、资源组合查询。

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;

namespace fs = std::filesystem;
static std::error_code ec_ignore;

static void be16w(std::vector<unsigned char>& v, uint16_t x) { v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }
static void be32w(std::vector<unsigned char>& v, uint32_t x) { v.push_back((x >> 24) & 0xFF); v.push_back((x >> 16) & 0xFF); v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }

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

static void write_mdict_like_file(const std::filesystem::path& path,
                                  const std::vector<unsigned char>& body) {
    std::string header = "<Dictionary title=\"StateMDX\" description=\"st mdx\"/>\n";
    std::ofstream out(path.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
    assert(out.good());
}

static std::filesystem::path base_dir() {
    namespace fs = std::filesystem;
    fs::path d = fs::current_path() / "build-local" / "dict_mgr_state";
    fs::create_directories(d);
    return d;
}

static std::filesystem::path write_json(const std::string& name, const std::string& dict_name,
                                        const std::string& word, const std::string& def) {
    fs::path p = base_dir() / name;
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o << "{\"name\":\"" << dict_name << "\",\"description\":\"d\",\"entries\":"
      << "[{\"word\":\"" << word << "\",\"definition\":\"" << def << "\"}]}";
    return p;
}

static void write_file(const std::filesystem::path& p, const std::string& content) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o << content;
}

int main() {
    namespace fs = std::filesystem;
    fs::path base = base_dir();

    // --- T1 全量往返：enabled/priority/tags（含转义字符）+ 隔离记录 ---
    {
        auto g1 = write_json("r1.json", "R1", "alpha", "first");
        auto g2 = write_json("r2.json", "R2", "beta", "second");
        auto bad = base / "r_bad.json";
        write_file(bad, "{broken");

        DictionaryManagerStd m;
        assert(m.add_dictionary(g1.string()));
        assert(m.add_dictionary(g2.string()));
        assert(!m.add_dictionary(bad.string()));  // 入持久隔离
        assert(m.set_dictionary_enabled("R1", false));
        assert(m.set_dictionary_priority("R2", 4));
        // 标签含全部受限转义字符，往返必须逐字节一致
        m.set_dictionary_tags("R2", {"a\\b\"c\nd\te\rf", "plain"});

        fs::path state = base / "sub" / "state.json";  // 目录不存在 → 自动创建
        assert(m.save_state(state.string()));

        DictionaryManagerStd m2;
        assert(m2.load_state(state.string()));
        assert(m2.loaded_dictionaries().size() == 2);
        assert(!m2.is_dictionary_enabled("R1"));
        assert(m2.is_dictionary_enabled("R2"));
        assert(m2.dictionary_priority("R2") == 4);
        assert((m2.dictionary_tags("R2") ==
                std::vector<std::string>{"a\\b\"c\nd\te\rf", "plain"}));
        // 隔离记录还原且未重复解析坏文件
        assert(m2.failed_dictionaries().size() == 1);
        assert(m2.failed_dictionaries()[0].file_path == bad.string());
        assert(m2.failed_dictionaries()[0].quarantined);
        assert(m2.loaded_dictionaries()[0] == "R2");  // 优先级生效
        assert(m2.search_word("alpha") == "");        // R1 禁用生效
        assert(m2.search_word("beta") == "second");
    }

    // --- T2 状态文件缺失 ---
    {
        DictionaryManagerStd m;
        assert(!m.load_state((base / "no_such_state.json").string()));
        assert(m.last_error().find("does not exist") != std::string::npos);
    }

    // --- T3 运行期诊断档：文件缺失记录 → 文件回来自动恢复 ---
    {
        fs::path ghost = base / "ghost3.json";
        fs::remove(ghost, ec_ignore);  // 幂等：上轮运行可能已创建
        std::string state =
            "{\"version\":1,\"dictionaries\":[{\"file_path\":\"" + ghost.string() +
            "\",\"enabled\":true,\"priority\":0}],\"quarantined\":[]}";
        fs::path sf = base / "s3.json";
        write_file(sf, state);

        DictionaryManagerStd m;
        assert(m.load_state(sf.string()));  // 文件缺失：记录运行期档
        assert(m.loaded_dictionaries().empty());
        assert(m.failed_dictionaries().size() == 1);
        assert(!m.failed_dictionaries()[0].quarantined);
        assert(m.failed_dictionaries()[0].reason.find("File not found") != std::string::npos);

        // 文件回来：再载入自动恢复，记录消失
        write_json("ghost3.json", "G3", "word", "def");
        DictionaryManagerStd m2;
        assert(m2.load_state(sf.string()));
        assert(m2.loaded_dictionaries() == std::vector<std::string>{"G3"});
        assert(m2.failed_dictionaries().empty());
    }

    // --- T4 隔离跳过：dictionaries 与 quarantined 同路径 → 不再解析 ---
    {
        auto good = write_json("t4_good.json", "T4G", "w", "d");
        auto bad = base / "t4_bad.json";
        write_file(bad, "{still broken");
        std::string state =
            "{\"version\":1,\"dictionaries\":["
            "{\"file_path\":\"" + bad.string() + "\",\"enabled\":true},"
            "{\"file_path\":\"" + good.string() + "\",\"enabled\":true}],"
            "\"quarantined\":[{\"file_path\":\"" + bad.string() +
            "\",\"reason\":\"earlier\",\"quarantined\":true}]}";
        fs::path sf = base / "s4.json";
        write_file(sf, state);

        DictionaryManagerStd m;
        assert(m.load_state(sf.string()));
        assert(m.loaded_dictionaries() == std::vector<std::string>{"T4G"});
        assert(m.failed_dictionaries().size() == 1);
        assert(m.failed_dictionaries()[0].reason == "earlier");  // 未被刷新
    }

    // --- T5 解析失败升级：状态里有、文件在但坏 → 升级持久隔离 ---
    {
        auto bad = base / "t5_bad.json";
        write_file(bad, "{bad");
        std::string state =
            "{\"dictionaries\":[{\"file_path\":\"" + bad.string() + "\"}]}";
        fs::path sf = base / "s5.json";
        write_file(sf, state);

        DictionaryManagerStd m;
        assert(m.load_state(sf.string()));
        assert(m.loaded_dictionaries().empty());
        assert(m.failed_dictionaries().size() == 1);
        assert(m.failed_dictionaries()[0].quarantined);
    }

    // --- T6 手写极简态 + 转义解码矩阵（\b \f \/ \\ \" \n \t \r 与未知转义）---
    {
        auto good = write_json("t6_good.json", "T6G", "w", "d");
        // reason 源文本：r:\b\f\/\\\"\n\t\r\x end → 解码含全部转义臂；
        // enabled 写成数字 1：非 true/false 字面 → 落布尔默认值臂
        std::string state =
            "{\"dictionaries\":[{\"file_path\":\"" + good.string() +
            "\",\"enabled\":1}],"
            "\"quarantined\":[{\"file_path\":\"x\",\"reason\":"
            "\"r:\\b\\f\\/\\\\\\\"\\n\\t\\r\\x end\",\"quarantined\":false}]}";
        fs::path sf = base / "s6.json";
        write_file(sf, state);

        DictionaryManagerStd m;
        assert(m.load_state(sf.string()));
        assert(m.loaded_dictionaries() == std::vector<std::string>{"T6G"});
        assert(m.dictionary_tags("T6G").empty());  // 缺省字段 → 默认值
        assert(m.dictionary_priority("T6G") == 0);
        assert(m.failed_dictionaries().size() == 1);
        assert(m.failed_dictionaries()[0].reason ==
               "r:\b\f/\\\"\n\t\r" "x end");
    }

    // --- T7 截断状态文件：区段未闭合按缺字典列表处理 ---
    {
        fs::path sf = base / "s7.json";
        write_file(sf, "{\"dictionaries\": [{\"file_path\":\"x\"");
        DictionaryManagerStd m;
        assert(!m.load_state(sf.string()));
        assert(m.last_error().find("missing dictionary list") != std::string::npos);
    }

    // --- T8 伴生 .mdd 资源组合访问 ---
    {
        fs::path mdx = base / "t8.mdx";
        fs::path mdd = base / "t8.mdd";
        const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + "DUMMY";
        write_mdict_like_file(mdx, make_simplekv({{"hello", "<div>hi</div>"}}));
        write_mdict_like_file(mdd, make_simplekv({{"pic.png", png}}));

        DictionaryManagerStd m;
        assert(m.add_dictionary(mdx.string()));
        assert(m.has_resource("StateMDX", "pic.png"));
        assert(!m.has_resource("StateMDX", "nope.png"));
        assert(!m.has_resource("NoSuchDict", "pic.png"));
        auto data = m.resource_data("StateMDX", "pic.png");
        assert(data.size() == png.size());
        assert(m.resource_string("StateMDX", "pic.png") == png);
        assert(m.resource_string("StateMDX", "nope.png").empty());
        assert(m.resource_data("StateMDX", "nope.png").empty());
        assert(m.resource_data("NoSuchDict", "pic.png").empty());
    }

    // --- T10 load_state 遇不支持扩展名：记运行期诊断档 ---
    {
        fs::path xyz = base / "t10.xyz";
        write_file(xyz, "whatever");
        std::string state =
            "{\"dictionaries\":[{\"file_path\":\"" + xyz.string() + "\"}]}";
        fs::path sf = base / "s10.json";
        write_file(sf, state);

        DictionaryManagerStd m;
        assert(m.load_state(sf.string()));
        assert(m.loaded_dictionaries().empty());
        assert(m.failed_dictionaries().size() == 1);
        assert(!m.failed_dictionaries()[0].quarantined);
        assert(m.failed_dictionaries()[0].reason.find("Unsupported dictionary format") !=
               std::string::npos);
    }

    // --- T9 空集合往返 ---
    {
        DictionaryManagerStd m;
        fs::path sf = base / "s9.json";
        assert(m.save_state(sf.string()));
        DictionaryManagerStd m2;
        assert(m2.load_state(sf.string()));
        assert(m2.loaded_dictionaries().empty());
        assert(m2.failed_dictionaries().empty());
    }

    return 0;
}