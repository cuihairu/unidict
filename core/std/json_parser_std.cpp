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

// 受限转义解码（与 dictionary_manager_std 状态文件读写、data_store_std
// 同一口径）：只解码 \\ \" \n \r \t；\uXXXX 等其余序列按字面保留
//（本格式导出侧从不产出 \u——UTF-8 直通）。此前值提取不识别转义，
// 含 \" 的释义在开引号后的第一个未配对引号处被截断（真实缺陷：导入
// 侧写合法 JSON 转义的文件，释义丢尾巴）。
static std::string decode_json_string(std::string_view raw) {
    std::string out; out.reserve(raw.size());
    for (size_t k = 0; k < raw.size(); ++k) {
        if (raw[k] != '\\') { out.push_back(raw[k]); continue; }
        // GCOVR_EXCL_LINE：调用方 find_str_val 的闭引号扫描按转义配对
        // 推进，span 尾不可能悬着单个反斜杠（有则闭引号已越过界）——
        // 兜底留档不可达（同 dictionary_manager_std 状态解码器口径）。
        if (k + 1 >= raw.size()) { out.push_back('\\'); break; }  // GCOVR_EXCL_LINE
        switch (raw[++k]) {
            case '\\': out.push_back('\\'); break;
            case '"': out.push_back('"'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            default: out.push_back('\\'); out.push_back(raw[k]); break;
        }
    }
    return out;
}

bool JsonParserStd::load_dictionary(const std::string& file_path) {
    entries_.clear(); lower_words_.clear(); words_.clear();
    loaded_ = false; name_.clear(); desc_.clear();
    std::ifstream in(file_path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss; ss << in.rdbuf();
    const std::string s = ss.str();
    const std::string_view sv(s);

    // 在 [from, bound) 内取 "key" 后冒号再后引号串的值（bound=npos
    // 为全串），闭引号扫描带转义感知（\\" 不终止），返回值过受限转义
    // 解码。语义与旧版一致，只是不再复制对象子串、不再逐调用构造
    // pattern std::string —— 大词典（20 万条/19MB）装载里这两个分配
    // 是解析热路径（BUG-004：解析 1.9s 的主成本）；解码串直接在
    // lambda 里构造，调用侧分配数与旧版持平。
    constexpr size_t npos = std::string_view::npos;
    auto find_str_val = [&](std::string_view key, size_t from, size_t bound)
        -> std::string {
        const size_t p0 = sv.find(key, from);
        if (p0 == npos || p0 >= bound) return {};
        const size_t p = sv.find(':', p0);
        if (p == npos || p >= bound) return {};
        const size_t q = sv.find('"', p);
        if (q == npos || q >= bound) return {};
        size_t r = q + 1;
        while (r < sv.size() && r < bound) {
            if (sv[r] == '\\') { r += 2; continue; }  // 逃过转义对（含 \"）
            if (sv[r] == '"') break;
            ++r;
        }
        if (r >= sv.size() || r >= bound) return {};  // 未闭合
        return decode_json_string(sv.substr(q + 1, r - q - 1));
    };

    name_ = find_str_val("\"name\"", 0, npos);
    desc_ = find_str_val("\"description\"", 0, npos);

    // entries array scan
    size_t ep = sv.find("\"entries\""); if (ep == npos) return false;
    ep = sv.find('[', ep); if (ep == npos) return false;
    int depth = 0; size_t i = ep;
    for (; i < sv.size(); ++i) { if (sv[i] == '[') { ++depth; break; } }
    if (depth == 0) return false;
    ++i;
    // 数组终点一趟定位（字符串感知的 [ ] 配对扫描；词条释义含引号转义
    // 不破配对）。旧版主循环每条目 sv.find(']', i) 重扫余下全缓冲——
    // 10 万条 27MB 装载被拖成平方级 153s（同规模 CSV 6s，BUG-004 同款
    // 热路径教训）；arr_end 先验后主循环不越界，实测 10 万条 → ~7s。
    // 无闭合（坏文件）时 arr_end=sv.size()，行为同旧版扫到尾
    size_t arr_end = sv.size();
    {
        int bracket = 0;
        bool in_str = false;
        for (size_t k = i; k < sv.size(); ++k) {
            const char c = sv[k];
            if (in_str) {
                if (c == '\\') ++k;
                else if (c == '"') in_str = false;
            } else if (c == '"') in_str = true;
            else if (c == '[') ++bracket;
            else if (c == ']') {
                --bracket;
                if (bracket == 0) { arr_end = k; break; }
            }
        }
    }
    // 词数预估：对象开括号计数，一趟线性扫描换 words_ 免翻倍重分配
    //（字符串内的 '{' 会计入——只是容量高估，无正确性影响）
    size_t est = 0;
    for (size_t k = i; k < arr_end; ++k) { if (sv[k] == '{') ++est; }
    if (est > 0) words_.reserve(est);
    while (i < arr_end) {
        // find next object（数组层定位：上一对象已完整消费后才再找，
        // 此处必在字符串外）
        size_t obj = sv.find('{', i);
        if (obj == npos || obj >= arr_end) break;
        // 对象边界扫描字符串感知：释义里裸 {/}（合法 JSON，导出侧
        // 不转义——JSON 无括号转义）不计深度，否则对象被腰斩
        int d = 1; size_t j = obj + 1;
        bool in_str = false;
        for (; j < sv.size() && d > 0; ++j) {
            const char c = sv[j];
            if (in_str) {
                if (c == '\\') ++j;
                else if (c == '"') in_str = false;
            } else {
                if (c == '"') in_str = true;
                else if (c == '{') ++d;
                else if (c == '}') --d;
            }
        }
        if (d == 0) {
            std::string w = find_str_val("\"word\"", obj, j);
            if (!w.empty()) {
                entries_[w] = find_str_val("\"definition\"", obj, j);
                words_.push_back(std::move(w));
            }
        }
        i = j + 1;
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

