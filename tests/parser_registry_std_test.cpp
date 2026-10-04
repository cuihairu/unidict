// 解析器工厂注册表单测（P-3 3.4）：
// 内建扩展名解析、归一化、自定义扩展名注册/覆盖内建（后注册者胜）、
// 空扩展名与空工厂拒绝，以及 DictionaryStd / DictionaryManagerStd
// 端到端走注册表面。

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "std/dictionary_manager_std.h"
#include "std/dictionary_std.h"
#include "std/json_parser_std.h"
#include "std/parser_registry_std.h"

using namespace UnidictCoreStd;

// 测试用自定义解析器：制表符分隔格式
//   首行  name<TAB>description   （元数据行）
//   其后  word<TAB>definition    （词条行）
class TabParser : public DictionaryParserStd {
public:
    bool load_dictionary(const std::string& path) override {
        std::ifstream in(path);
        if (!in) return false;
        std::string line;
        if (!std::getline(in, line)) return false;
        const auto meta_tab = line.find('\t');
        if (meta_tab == std::string::npos) return false;
        name_ = line.substr(0, meta_tab);
        desc_ = line.substr(meta_tab + 1);
        while (std::getline(in, line)) {
            const auto t = line.find('\t');
            if (t == std::string::npos) continue;
            entries_[line.substr(0, t)] = line.substr(t + 1);
            words_.push_back(line.substr(0, t));
        }
        loaded_ = !entries_.empty();
        return loaded_;
    }
    bool is_loaded() const override { return loaded_; }
    std::string lookup(const std::string& word) const override {
        const auto it = entries_.find(word);
        return it != entries_.end() ? it->second : std::string{};
    }
    std::string dictionary_name() const override { return name_; }
    std::string dictionary_description() const override { return desc_; }
    std::vector<std::string> all_words() const override { return words_; }

private:
    bool loaded_ = false;
    std::string name_;
    std::string desc_;
    std::unordered_map<std::string, std::string> entries_;
    std::vector<std::string> words_;
};

static std::filesystem::path base_dir() {
    std::filesystem::path dir = std::filesystem::current_path() / "build-local" / "parser_registry_std";
    std::filesystem::create_directories(dir);
    return dir;
}

static void write_tab_file(const std::filesystem::path& path, const std::string& body) {
    std::ofstream out(path, std::ios::trunc);
    out << body;
    assert(out.good());
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = base_dir();
    auto& reg = ParserRegistryStd::instance();

    // --- T1 内建扩展名：create 命中；未注册扩展名 nullptr ---
    {
        assert(reg.create(".json") != nullptr);
        assert(reg.create("json") != nullptr);   // 归一化补前导点
        assert(reg.create(".ifo") != nullptr);
        assert(reg.create(".mdx") != nullptr);
        assert(reg.create(".dsl") != nullptr);
        assert(reg.create(".csv") != nullptr);
        assert(reg.create(".tsv") != nullptr);
        assert(reg.create(".txt") != nullptr);
        assert(reg.create(".epub") != nullptr);
        assert(reg.create(".definitely_unregistered") == nullptr);
    }

    // --- T2 supported_extensions：含八内建且有序 ---
    {
        const auto exts = reg.supported_extensions();
        assert(std::is_sorted(exts.begin(), exts.end()));
        for (const char* e : {".csv", ".dsl", ".epub", ".ifo", ".json", ".mdx", ".txt", ".tsv"}) {
            bool found = false;
            for (const auto& x : exts) found = found || x == e;
            assert(found);
        }
    }

    // --- T3 自定义扩展名注册：create/load/lookup 全链可用 ---
    {
        reg.register_factory(".xyz", [] { return std::make_unique<TabParser>(); });
        auto p = reg.create(".xyz");
        assert(p != nullptr);
        fs::path f = dir / "tab.xyz";
        write_tab_file(f, "TabDict\ttab format\nhello\thi from tab\n");
        assert(p->load_dictionary(f.string()));
        assert(p->is_loaded());
        assert(p->dictionary_name() == "TabDict");
        assert(p->dictionary_description() == "tab format");
        assert(p->lookup("hello") == "hi from tab");
        assert(p->lookup("missing") == "");
        assert(p->all_words().size() == 1 && p->all_words()[0] == "hello");
    }

    // --- T4 覆盖内建扩展名：后注册者胜，返回值可恢复原状 ---
    {
        const auto prev = reg.register_factory(".json", [] { return std::make_unique<TabParser>(); });
        assert(prev);  // 先前是内建 JsonParserStd 工厂
        auto overridden = reg.create(".json");
        assert(dynamic_cast<TabParser*>(overridden.get()) != nullptr);
        reg.register_factory(".json", prev);  // 恢复内建
        auto restored = reg.create(".json");
        assert(dynamic_cast<JsonParserStd*>(restored.get()) != nullptr);
    }

    // --- T5 归一化：裸扩展名注册 + 大写扩展名查询 ---
    {
        reg.register_factory("xyz2", [] { return std::make_unique<TabParser>(); });
        assert(reg.create("xyz2") != nullptr);
        assert(reg.create(".XYZ2") != nullptr);  // 大小写不敏感
        assert(reg.create(".JSON") != nullptr);  // 内建同样大小写不敏感
    }

    // --- T6 非法注册：空扩展名 / 空工厂 ---
    {
        const auto none = reg.register_factory("", [] { return std::make_unique<TabParser>(); });
        assert(!none);           // 未注册过，无先前工厂
        assert(!reg.register_factory(".", [] { return std::make_unique<TabParser>(); }));
        assert(reg.create("") == nullptr);
        assert(reg.create(".") == nullptr);
        // 空工厂（nullptr）注册成功但 create 视同未注册，不可调用
        reg.register_factory(".nul", nullptr);
        assert(reg.create(".nul") == nullptr);
    }

    // --- T7 DictionaryStd 端到端：自定义扩展名 + 未注册扩展名 ---
    {
        fs::path f = dir / "d2.xyz";
        write_tab_file(f, "TabDict2\tvia dictionary\nalpha\tfirst letter\n");
        DictionaryStd d;
        assert(d.load(f.string()));
        assert(d.name() == "TabDict2");
        assert(d.description() == "via dictionary");
        assert(d.lookup("alpha") == "first letter");
        assert(d.words().size() == 1 && d.words()[0] == "alpha");

        fs::path u = dir / "d2.zzz";
        { std::ofstream o(u, std::ios::trunc); o << "whatever"; }
        DictionaryStd bad;
        assert(!bad.load(u.string()));
        assert(std::string(bad.load_error()).rfind(kUnsupportedExtPrefix, 0) == 0);
    }

    // --- T8 manager 走注册表面：add_dictionary 加载自定义格式词典 ---
    {
        fs::path f = dir / "m.xyz";
        write_tab_file(f, "TabDictM\tmanager route\nbeta\tsecond letter\n");
        DictionaryManagerStd m;
        assert(m.add_dictionary(f.string()));
        assert(m.loaded_dictionaries().size() == 1);
        assert(m.dictionaries_meta()[0].name == "TabDictM");
        assert(m.search_word("beta") == "second letter");
    }

    return 0;
}
