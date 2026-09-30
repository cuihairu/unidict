#include "data_store_std.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>

namespace fs = std::filesystem;

namespace UnidictCoreStd {

static inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

// 词形相等（大小写不敏感）——生词本/笔记的键口径
static inline bool ieq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

DataStoreStd::DataStoreStd() {
    // default: ./data/unidict.json
    path_ = (fs::current_path() / "data" / "unidict.json").string();
}

// ---------- tolerant JSON 辅助（解析本文件自产格式，容错即可） ----------

// 解析一个 JSON 字符串字面量：s[from] 应为起始引号，按 JSON 转义规则
// 解码内容，*out_end 指向闭合引号之后的位置。
//
// 调用方（区段提取与对象切分都是字符串感知的）传进来的串必然闭合，
// "未闭合"只是越界兜底：*out_end 取串尾，调用方的下标循环自然收尾，
// 不需要额外的哨兵分支。
//
// 唯一的字符串读取入口——曾经三处各写一份、两份还是错的：
//   · 历史数组那条状态机把 \n \r \t 解成了字母 n r t（转义后的字符
//     直接 push），含控制字符的搜索词每次载入都变成另一个串，
//     add_search_history 的去重永远匹配不上 → 每搜一次多一条历史；
//   · obj_val 用 find('"') 找闭引号，含 \" 的值被截断（quo"te → quo\），
//     再存盘时反斜杠被 json_escape 加倍 → 每轮载入存盘体积翻倍，
//     30 轮就能把用户的 store.json 撑到 GB 级（实测 2GB）。
// \uXXXX 不解码：本文件只由 json_escape 产出（只写 \\ \" \n \r \t），
// 外来 \u 走默认分支按字面保留，不臆造代理对规则。
static std::string parse_json_string(const std::string& s, size_t from,
                                     size_t* out_end) {
    std::string out;
    size_t i = from + 1;
    // GCOVR_EXCL_LINE：三个调用方（区段提取/对象切分/数组元素扫描）都由
    // 字符串感知的深度计数扫描器把关后才切入，且扫描规则与本函数完全
    // 一致（\" 作转义、闭引号即收）——传入串在本翻译单元内必然闭合，
    // while 的 i≥size 出口与下方串尾悬空反斜杠兜底均不可达（函数头
    // 注释的"越界兜底"留档，不删防御代码）。
    while (i < s.size() && s[i] != '"') {  // GCOVR_EXCL_LINE
        if (s[i] != '\\') {
            out.push_back(s[i++]);
            continue;
        }
        if (i + 1 >= s.size()) break;  // GCOVR_EXCL_LINE 串尾悬空反斜杠：吞掉它收尾
        const char e = s[i + 1];
        switch (e) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            default: out.push_back(e); break;  // \" \\ \/ 及未知转义取字面
        }
        i += 2;
    }
    // 唯一出口：闭合引号之后；未闭合（兜底）指向串尾，调用方的下标
    // 循环自然收尾——写成单一出口而不是分支返回，免得留一条死路径
    // GCOVR_EXCL_LINE：同上，未闭合时取 s.size() 的兜底臂不可达
    *out_end = i < s.size() ? i + 1 : s.size();  // GCOVR_EXCL_LINE
    return out;
}

// 从对象字符串提取字符串字段（无该字段返回空）
static std::string obj_val(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return {};
    p = o.find(':', p);
    if (p == std::string::npos) return {};
    p = o.find('"', p);
    if (p == std::string::npos) return {};
    size_t end = 0;
    return parse_json_string(o, p, &end);
}

// 从对象字符串提取整数字段（无该字段/无数字返回 0）
static long long obj_int(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return 0;
    p = o.find(':', p);
    if (p == std::string::npos) return 0;
    ++p;
    while (p < o.size() && (o[p] == ' ' || o[p] == '\t')) ++p;
    bool neg = false;
    if (p < o.size() && o[p] == '-') { neg = true; ++p; }
    long long v = 0;
    bool any = false;
    while (p < o.size() && o[p] >= '0' && o[p] <= '9') { v = v * 10 + (o[p] - '0'); ++p; any = true; }
    if (!any) return 0;
    return neg ? -v : v;
}

// 从对象字符串提取浮点字段（M9 词分；无该字段/无可解析数字返回 0）
//
// 词分是 0-1 的实数，obj_int 只吃整数、会把 0.795 当成 0（丢掉小数部分
// 就等于把记录算成"从没错过"），所以这里单独一路：整数部分 + 可选小数
// 部分。不解析指数——本文件只由 save() 用默认精度写出十进制，实数词分
// 永远走这条路径。
static double obj_num(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return 0.0;
    p = o.find(':', p);
    if (p == std::string::npos) return 0.0;
    ++p;
    while (p < o.size() && (o[p] == ' ' || o[p] == '\t')) ++p;
    bool neg = false;
    if (p < o.size() && o[p] == '-') { neg = true; ++p; }
    double v = 0.0;
    bool any = false;
    while (p < o.size() && o[p] >= '0' && o[p] <= '9') { v = v * 10 + (o[p] - '0'); ++p; any = true; }
    if (p < o.size() && o[p] == '.') {
        ++p;
        double scale = 0.1;
        while (p < o.size() && o[p] >= '0' && o[p] <= '9') { v += (o[p] - '0') * scale; scale *= 0.1; ++p; any = true; }
    }
    if (!any) return 0.0;
    return neg ? -v : v;
}

// 从对象字符串提取字符串数组字段（生词标签；旧格式无此字段返回空）
static std::vector<std::string> obj_str_array(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return {};
    p = o.find('[', p);
    if (p == std::string::npos) return {};
    std::vector<std::string> out;
    for (size_t k = p + 1; k < o.size();) {
        const char c = o[k];
        if (c == ']') break;
        if (c != '"') { ++k; continue; }  // 容错：跳过非字符串元素
        size_t end = 0;
        out.push_back(parse_json_string(o, k, &end));
        k = end;
    }
    return out;
}

// 遍历对象数组区段里的每个对象字符串（深度计数定界；必须字符串感知——
// 释义里的 '{' '}' 是内容，裸数括号会把对象截断在半个定义上）
template <typename F>
static void for_each_object(const std::string& sec, F&& fn) {
    size_t i = 1;
    while (i < sec.size()) {
        size_t obj = sec.find('{', i);
        if (obj == std::string::npos) break;
        int depth = 1;
        bool in_str = false, esc = false;
        size_t j = obj + 1;
        for (; j < sec.size() && depth > 0; ++j) {
            const char c = sec[j];
            if (in_str) {
                if (esc) esc = false;
                else if (c == '\\') esc = true;
                else if (c == '"') in_str = false;
                continue;
            }
            if (c == '"') in_str = true;  // 下一个 '{' '}' 才是结构
            else if (c == '{') ++depth;
            else if (c == '}') --depth;
        }
        if (depth == 0) {
            fn(sec.substr(obj, j - obj));
        }
        i = j + 1;
    }
}

void DataStoreStd::set_storage_path(const std::string& file_path) { path_ = file_path; }
std::string DataStoreStd::storage_path() const { return path_; }

void DataStoreStd::ensure_loaded() const {
    if (loaded_) return;
    const_cast<DataStoreStd*>(this)->load();
}

bool DataStoreStd::load() {
    loaded_ = true;
    history_.clear();
    vocab_.clear();
    notes_.clear();
    pron_.clear();

    std::error_code ec;
    fs::path p(path_);
    if (!fs::exists(p, ec)) {
        fs::create_directories(p.parent_path(), ec);
        return save();
    }

    std::ifstream in(path_, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss; ss << in.rdbuf();
    const std::string s = ss.str();

    // Minimal tolerant parser for our own JSON format
    //
    // 已知局限（非本次修复范围，留档）：区段/键名查找是纯子串定位，若释义
    // 或笔记里恰好写了 '"notes":' 这类字面文本，会被当成真键——子串先于
    // 真键出现时该区段读不回来。要根治得换成上下文感知的键扫描（只在
    // 顶层对象、字符串之外匹配键名），那是解析器重写，单独评估。
    auto find_section = [&](const std::string& key) -> std::string {
        const std::string pattern = '"' + key + '"';
        size_t pos = s.find(pattern);
        if (pos == std::string::npos) return {};
        pos = s.find(':', pos);
        if (pos == std::string::npos) return {};
        size_t start = s.find_first_of("[{", pos);
        if (start == std::string::npos) return {};
        // 深度计数必须字符串感知：释义/笔记里的 '}'、']'、'{' 是内容而非
        // 结构（"int main() { }" 这种释义很常见），早退会静默截断整个
        // vocab 区段——后面的生词连同 added_at 一起读不回来。
        int depth = 0;
        bool in_str = false, esc = false;
        for (size_t i = start; i < s.size(); ++i) {
            const char c = s[i];
            if (in_str) {
                if (esc) esc = false;
                else if (c == '\\') esc = true;
                else if (c == '"') in_str = false;
                continue;
            }
            if (c == '"') { in_str = true; continue; }
            if (c == '[' || c == '{') ++depth;
            else if (c == ']' || c == '}') {
                --depth;
                if (depth == 0) return s.substr(start, i - start + 1);
            }
        }
        return {};
    };

    // Parse history array ["a","b",...]
    std::string hsec = find_section("history");
    if (!hsec.empty() && hsec.front() == '[') {
        for (size_t i = 1; i < hsec.size();) {
            const char c = hsec[i];
            if (c != '"') { ++i; continue; }  // 容错：跳过非字符串元素
            size_t end = 0;
            history_.push_back(parse_json_string(hsec, i, &end));
            i = end;
        }
    }

    // Parse vocab array of objects [{"word":"","definition":"","added_at":123,...},...]
    std::string vsec = find_section("vocab");
    if (!vsec.empty() && vsec.front() == '[') {
        for_each_object(vsec, [&](const std::string& o) {
            VocabItemStd vi{ obj_val(o, "word"), obj_val(o, "definition"),
                             obj_int(o, "added_at"), obj_str_array(o, "tags") };
            if (!vi.word.empty()) vocab_.push_back(std::move(vi));
        });
    }

    // Parse notes array of objects [{"word":"","text":"","updated_at":123},...]
    std::string nsec = find_section("notes");
    if (!nsec.empty() && nsec.front() == '[') {
        for_each_object(nsec, [&](const std::string& o) {
            NoteItemStd ni{ obj_val(o, "word"), obj_val(o, "text"),
                            obj_int(o, "updated_at") };
            if (!ni.word.empty()) notes_.push_back(std::move(ni));
        });
    }

    // Parse pron array of objects [{"word":"","last_score":0.8,"best_score":0.9,
    // "attempts":3,"last_at":123},...]（M9 发音练习历史）
    //
    // 区段名取 "pron_records" 而非短名：这个 store 的区段查找是子串定位
    // （已知局限见上），释义里出现 `"pron":` 这种字面（pron. 用作
    // pronunciation 缩写不算罕见）就会把短名区段错认出来
    std::string prsec = find_section("pron_records");
    if (!prsec.empty() && prsec.front() == '[') {
        for_each_object(prsec, [&](const std::string& o) {
            PronRecordStd pr{ obj_val(o, "word"), obj_num(o, "last_score"),
                              obj_num(o, "best_score"),
                              static_cast<int>(obj_int(o, "attempts")),
                              obj_int(o, "last_at") };
            if (!pr.word.empty()) pron_.push_back(std::move(pr));
        });
    }

    return true;
}

bool DataStoreStd::save() const {
    std::error_code ec;
    fs::create_directories(fs::path(path_).parent_path(), ec);
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << "{\n";
    out << "  \"history\": [";
    for (size_t i = 0; i < history_.size(); ++i) {
        if (i) out << ",";
        out << '"' << json_escape(history_[i]) << '"';
    }
    out << "],\n";
    out << "  \"vocab\": [\n";
    for (size_t i = 0; i < vocab_.size(); ++i) {
        const auto& v = vocab_[i];
        out << "    {\"word\":\"" << json_escape(v.word) << "\",\"definition\":\"" << json_escape(v.definition) << "\"";
        if (v.added_at > 0) out << ",\"added_at\":" << v.added_at;
        if (!v.tags.empty()) {
            out << ",\"tags\":[";
            for (size_t t = 0; t < v.tags.size(); ++t) {
                if (t) out << ",";
                out << '"' << json_escape(v.tags[t]) << '"';
            }
            out << "]";
        }
        out << "}";
        if (i + 1 < vocab_.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"notes\": [\n";
    for (size_t i = 0; i < notes_.size(); ++i) {
        const auto& n = notes_[i];
        out << "    {\"word\":\"" << json_escape(n.word) << "\",\"text\":\"" << json_escape(n.text) << "\"";
        if (n.updated_at > 0) out << ",\"updated_at\":" << n.updated_at;
        out << "}";
        if (i + 1 < notes_.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    // M9 发音练习历史：空也写出空数组（与 history/notes 一致，老数据文件
    // 缺该字段时 load 走"无记录"，往返对称）
    out << "  \"pron_records\": [\n";
    for (size_t i = 0; i < pron_.size(); ++i) {
        const auto& r = pron_[i];
        out << "    {\"word\":\"" << json_escape(r.word) << "\",\"last_score\":"
            << r.last_score << ",\"best_score\":" << r.best_score
            << ",\"attempts\":" << r.attempts;
        if (r.last_at > 0) out << ",\"last_at\":" << r.last_at;
        out << "}";
        if (i + 1 < pron_.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}

void DataStoreStd::add_search_history(const std::string& word) {
    ensure_loaded();
    // dedupe old entries (case-insensitive, ASCII)
    auto eq = [&](const std::string& s){
        if (s.size() != word.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)word[i])) return false;
        return true;
    };
    history_.erase(std::remove_if(history_.begin(), history_.end(), eq), history_.end());
    history_.push_back(word);
    save();
}

std::vector<std::string> DataStoreStd::get_search_history(int limit) const {
    ensure_loaded();
    std::vector<std::string> out;
    if (limit <= 0) return out;
    const int n = (int)history_.size();
    const int start = std::max(0, n - limit);
    for (int i = start; i < n; ++i) out.push_back(history_[i]);
    return out;
}

void DataStoreStd::clear_history() {
    ensure_loaded();
    history_.clear();
    save();
}

void DataStoreStd::add_vocabulary_item(const VocabItemStd& item) {
    ensure_loaded();
    // upsert by word (case-insensitive ASCII)
    auto eq = [&](const std::string& s){
        if (s.size() != item.word.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)item.word[i])) return false;
        return true;
    };
    bool updated = false;
    for (auto& v : vocab_) {
        if (eq(v.word)) { v.definition = item.definition; /* keep original added_at */ updated = true; break; }
    }
    if (!updated) {
        VocabItemStd vi = item;
        if (vi.added_at == 0) {
            vi.added_at = (long long)std::time(nullptr);
        }
        vocab_.push_back(std::move(vi));
    }
    save();
}

void DataStoreStd::remove_vocabulary_item(const std::string& word) {
    ensure_loaded();
    auto eq = [&](const std::string& s){
        if (s.size() != word.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)word[i])) return false;
        return true;
    };
    vocab_.erase(std::remove_if(vocab_.begin(), vocab_.end(), [&](const VocabItemStd& v){ return eq(v.word); }), vocab_.end());
    save();
}

std::vector<VocabItemStd> DataStoreStd::get_vocabulary() const {
    ensure_loaded();
    return vocab_;
}

bool DataStoreStd::set_vocabulary_item_tags(const std::string& word,
                                            const std::vector<std::string>& tags) {
    ensure_loaded();
    for (auto& v : vocab_) {
        if (ieq(v.word, word)) {
            v.tags = tags;
            save();
            return true;
        }
    }
    return false;
}

// M3 标签管理：add 幂等（同标签不重复），remove 双命中才删
bool DataStoreStd::add_vocabulary_item_tag(const std::string& word,
                                           const std::string& tag) {
    ensure_loaded();
    if (tag.empty()) return false;
    for (auto& v : vocab_) {
        if (!ieq(v.word, word)) continue;
        for (const auto& t : v.tags) {
            if (t == tag) return true;  // 已存在，幂等
        }
        v.tags.push_back(tag);
        save();
        return true;
    }
    return false;
}

bool DataStoreStd::remove_vocabulary_item_tag(const std::string& word,
                                              const std::string& tag) {
    ensure_loaded();
    for (auto& v : vocab_) {
        if (!ieq(v.word, word)) continue;
        for (auto it = v.tags.begin(); it != v.tags.end(); ++it) {
            if (*it == tag) {
                v.tags.erase(it);
                save();
                return true;
            }
        }
        return false;  // 词条命中但无此标签
    }
    return false;
}

std::vector<VocabItemStd> DataStoreStd::get_vocabulary_by_tag(
    const std::string& tag) const {
    ensure_loaded();
    std::vector<VocabItemStd> out;
    for (const auto& v : vocab_) {
        for (const auto& t : v.tags) {
            if (t == tag) {
                out.push_back(v);
                break;
            }
        }
    }
    return out;
}

void DataStoreStd::clear_vocabulary() {
    ensure_loaded();
    vocab_.clear();
    save();
}

bool DataStoreStd::export_vocabulary_csv(const std::string& file_path) const {
    ensure_loaded();
    std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    // UTF-8 BOM：Excel 按 UTF-8 解（M3 口径）；标签 ';' 连接，笔记按词联查
    out << "\xEF\xBB\xBFword,definition,tags,note\n";
    auto esc = [](const std::string& s) {
        std::string t; t.reserve(s.size() + 8);
        for (char c : s) { if (c == '"') t.push_back('"'); t.push_back(c); }
        return t;
    };
    for (const auto& v : vocab_) {
        std::string tags;
        for (size_t i = 0; i < v.tags.size(); ++i) {
            if (i > 0) tags += ';';
            tags += v.tags[i];
        }
        std::string note;
        for (const auto& n : notes_) {
            if (ieq(n.word, v.word)) { note = n.text; break; }
        }
        out << '"' << esc(v.word) << '"' << ','
            << '"' << esc(v.definition) << '"' << ','
            << '"' << esc(tags) << '"' << ','
            << '"' << esc(note) << '"' << '\n';
    }
    return true;
}

void DataStoreStd::set_note(const std::string& word, const std::string& text) {
    ensure_loaded();
    auto eq = [&](const std::string& s){
        if (s.size() != word.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)word[i])) return false;
        return true;
    };
    if (text.empty()) { // 空文本=移除该词笔记
        notes_.erase(std::remove_if(notes_.begin(), notes_.end(),
                                    [&](const NoteItemStd& n){ return eq(n.word); }),
                     notes_.end());
        save();
        return;
    }
    for (auto& n : notes_) {
        if (eq(n.word)) {
            n.text = text;
            n.updated_at = (long long)std::time(nullptr);
            save();
            return;
        }
    }
    NoteItemStd ni;
    ni.word = word;
    ni.text = text;
    ni.updated_at = (long long)std::time(nullptr);
    notes_.push_back(std::move(ni));
    save();
}

std::string DataStoreStd::get_note(const std::string& word) const {
    ensure_loaded();
    for (const auto& n : notes_) {
        if (n.word.size() == word.size()) {
            bool same = true;
            for (size_t i = 0; i < n.word.size(); ++i) {
                if (std::tolower((unsigned char)n.word[i]) != std::tolower((unsigned char)word[i])) { same = false; break; }
            }
            if (same) return n.text;
        }
    }
    return {};
}

std::vector<NoteItemStd> DataStoreStd::get_notes() const {
    ensure_loaded();
    return notes_;
}

void DataStoreStd::set_pron_record(const PronRecordStd& record) {
    ensure_loaded();
    // word 是键，空串没有可归属的词：直接忽略（不落一条无名记录）
    if (record.word.empty()) {
        return;
    }
    auto eq = [&](const std::string& s){
        if (s.size() != record.word.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)record.word[i])) return false;
        return true;
    };
    for (auto& r : pron_) {
        if (eq(r.word)) { r = record; save(); return; }
    }
    pron_.push_back(record);
    save();
}

std::optional<PronRecordStd> DataStoreStd::get_pron_record(const std::string& word) const {
    ensure_loaded();
    for (const auto& r : pron_) {
        if (r.word.size() == word.size()) {
            bool same = true;
            for (size_t i = 0; i < r.word.size(); ++i) {
                if (std::tolower((unsigned char)r.word[i]) != std::tolower((unsigned char)word[i])) { same = false; break; }
            }
            if (same) return r;
        }
    }
    return std::nullopt;
}

std::vector<PronRecordStd> DataStoreStd::get_pron_records() const {
    ensure_loaded();
    return pron_;
}

void DataStoreStd::clear_pron_records() {
    ensure_loaded();
    pron_.clear();
    save();
}

std::string DataStoreStd::json_escape(const std::string& s) {
    std::string out; out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back((char)c); break;
        }
    }
    return out;
}

} // namespace UnidictCoreStd
