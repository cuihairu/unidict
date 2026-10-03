#include "json_parser_std.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>

#include "text_norm_std.h"

namespace UnidictCoreStd {

static inline std::string lcase(const std::string& s) {
    std::string t; t.reserve(s.size());
    for (unsigned char c : s) t.push_back((char)std::tolower(c));
    return t;
}

JsonParserStd::JsonParserStd() = default;

bool JsonParserStd::load_dictionary(const std::string& file_path) {
    entries_.clear(); lower_words_.clear(); words_.clear();
    loaded_ = false; name_.clear(); desc_.clear();
    std::ifstream in(file_path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss; ss << in.rdbuf();
    const std::string s = ss.str();
    const std::string_view sv(s);

    // 在 [from, bound) 内取 "key" 后冒号再后引号串的值（bound=npos
    // 为全串）。语义与旧版一致，只是不再复制对象子串、不再逐调用
    // 构造 pattern std::string —— 大词典（20 万条/19MB）装载里这两
    // 个分配是解析热路径（BUG-004：解析 1.9s 的主成本）。
    constexpr size_t npos = std::string_view::npos;
    auto find_str_val = [&](std::string_view key, size_t from, size_t bound)
        -> std::string_view {
        const size_t p0 = sv.find(key, from);
        if (p0 == npos || p0 >= bound) return {};
        const size_t p = sv.find(':', p0);
        if (p == npos || p >= bound) return {};
        const size_t q = sv.find('"', p);
        if (q == npos || q >= bound) return {};
        const size_t r = sv.find('"', q + 1);
        if (r == npos || r >= bound) return {};
        return sv.substr(q + 1, r - q - 1);
    };

    name_ = std::string(find_str_val("\"name\"", 0, npos));
    desc_ = std::string(find_str_val("\"description\"", 0, npos));

    // entries array scan
    size_t ep = sv.find("\"entries\""); if (ep == npos) return false;
    ep = sv.find('[', ep); if (ep == npos) return false;
    int depth = 0; size_t i = ep;
    for (; i < sv.size(); ++i) { if (sv[i] == '[') { ++depth; break; } }
    if (depth == 0) return false;
    ++i;
    // 词数预估：对象开括号计数，一次线性扫描换 words_ 免翻倍重分配
    size_t est = 0;
    for (size_t k = i; k < sv.size(); ++k) { if (sv[k] == '{') ++est; }
    if (est > 0) words_.reserve(est);
    while (i < sv.size()) {
        // find next object
        size_t obj = sv.find('{', i);
        if (obj == npos) break;
        int d = 1; size_t j = obj + 1;
        for (; j < sv.size() && d > 0; ++j) {
            if (sv[j] == '{') ++d; else if (sv[j] == '}') --d;
        }
        if (d == 0) {
            std::string_view w = find_str_val("\"word\"", obj, j);
            if (!w.empty()) {
                std::string_view dfn = find_str_val("\"definition\"", obj, j);
                entries_[std::string(w)] = std::string(dfn);
                words_.emplace_back(w);
            }
        }
        i = j + 1;
        // break at end of entries array
        size_t close = sv.find(']', i);
        if (close != npos && close < sv.find('{', i)) break;
    }

    // 折叠键表：查词侧大小写/全半角互通（Hello → hello、ｈｅｌｌｏ →
    // hello）。键用 TextNorm::fold_key，与 IndexEngineStd::exact_match 的
    // 归一口径一致（索引侧本来就能大小写命中，解析器侧漏了）。与词数同阶
    // 的一次性建表、查询期零成本——漏了这一步，demo 词典里的 hello/qt 在
    // std 面只有小写形态可查（BUGS.md BUG-005）
    lower_words_.reserve(words_.size());
    for (const auto& w : words_) lower_words_[TextNorm::fold_key(w)] = w;

    loaded_ = !entries_.empty();
    return loaded_;
}

bool JsonParserStd::is_loaded() const { return loaded_; }
std::string JsonParserStd::name() const { return name_.empty() ? std::string("JSON Dictionary") : name_; }
std::string JsonParserStd::description() const { return desc_; }
int JsonParserStd::word_count() const { return (int)words_.size(); }

std::string JsonParserStd::lookup(const std::string& word) const {
    auto it = entries_.find(word);
    if (it != entries_.end()) return it->second;
    // 精确 miss → 折叠键回退（与 stardict/mdict/dsl 及 Qt 面同口径）。
    // 命中的是 canonical 词形的释义，不是查询串本身
    auto folded = lower_words_.find(TextNorm::fold_key(word));
    if (folded == lower_words_.end()) return {};
    auto canonical = entries_.find(folded->second);
    return canonical == entries_.end() ? std::string{} : canonical->second;
}

std::vector<std::string> JsonParserStd::find_similar(const std::string& word, int max_results) const {
    std::vector<std::string> out;
    const std::string lw = lcase(word);
    for (const auto& w : words_) {
        if ((int)out.size() >= max_results) break;
        std::string wl = lcase(w);
        if (wl.rfind(lw, 0) == 0) out.push_back(w);
    }
    return out;
}

std::vector<std::string> JsonParserStd::all_words() const { return words_; }

} // namespace UnidictCoreStd

