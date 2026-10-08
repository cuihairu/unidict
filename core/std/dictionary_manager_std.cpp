#include <cstdint>
#include <cstring>
#include "dictionary_manager_std.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include "text_norm_std.h"

namespace fs = std::filesystem;

namespace UnidictCoreStd {

DictionaryManagerStd::DictionaryManagerStd() = default;

bool DictionaryManagerStd::add_dictionary(const std::string& path) {
    // 文件丢失：显式添加直接失败，不建隔离记录（运行期档语义——
    // 恢复时的重查在 load_state，见 unidict_core.h:38-46）
    std::error_code fec;
    if (!fs::is_regular_file(path, fec)) {
        last_error_ = "Dictionary file does not exist: " + path;
        return false;
    }
    DictionaryStd d;
    if (!d.load(path)) {
        // 扩展名不支持：不建记录；解析失败：持久隔离档
        if (d.load_error().compare(0, std::strlen(kUnsupportedExtPrefix), kUnsupportedExtPrefix) == 0) {
            last_error_ = d.load_error();
        } else {
            last_error_ = "Failed to load dictionary: " + path;
            record_failure(path, last_error_, true);
        }
        return false;
    }
    // 文件修好后重新添加（或重试成功）：摘除旧的失败/隔离记录，避免
    // 词典管理里同时出现正常行和 ⚠ 行
    const int fi = index_of_failure(path);
    if (fi >= 0) failures_.erase(failures_.begin() + fi);
    for (const auto& w : d.words()) index_.add_word(w, d.name());
    ft_index_.reset();
    prefix_index_dirty_ = true;
    dicts_.push_back(std::move(d));
    return true;
}

bool DictionaryManagerStd::retry_failed_dictionary(const std::string& file_path) {
    if (index_of_failure(file_path) < 0) {
        last_error_ = "Dictionary is not in the failed list: " + file_path;
        return false;
    }
    // 成功：add_dictionary 内部会摘除失败记录；失败：留在原地刷新原因
    if (add_dictionary(file_path)) return true;
    const int index = index_of_failure(file_path);
    if (index >= 0) {
        failures_[index].reason = last_error_;
        failures_[index].quarantined = true; // 重试又失败 → 确认隔离
    }
    return false;
}

int DictionaryManagerStd::index_of_failure(const std::string& file_path) const {
    for (int i = 0; i < (int)failures_.size(); ++i) {
        if (failures_[i].file_path == file_path) return i;
    }
    return -1;
}

void DictionaryManagerStd::record_failure(const std::string& file_path,
                                          const std::string& reason, bool quarantined) {
    const int index = index_of_failure(file_path);
    if (index < 0) {
        failures_.push_back({file_path, reason, quarantined});
        return;
    }
    // 同路径已有记录：刷新原因/隔离档位
    failures_[index].reason = reason;
    failures_[index].quarantined = quarantined;
}

bool DictionaryManagerStd::remove_dictionary(const std::string& dict_name) {
    bool removed = false;
    auto it = dicts_.begin();
    while (it != dicts_.end()) {
        if (it->name() == dict_name) {
            for (const auto& w : it->words()) index_.remove_word(w, dict_name);
            it = dicts_.erase(it); removed = true;
        } else { ++it; }
    }
    if (removed) {
        ft_index_.reset();
    }
    index_.build_index();
    prefix_index_dirty_ = false;
    return removed;
}

void DictionaryManagerStd::clear_dictionaries() {
    dicts_.clear();
    failures_.clear();
    last_error_.clear();
    index_.clear();
    ft_index_.reset();
    prefix_index_dirty_ = true;
}

std::vector<std::string> DictionaryManagerStd::loaded_dictionaries() const {
    std::vector<std::string> v; v.reserve(dicts_.size());
    for (const auto* dp : ordered_dictionaries()) v.push_back(dp->name());
    return v;
}

std::vector<std::string> DictionaryManagerStd::enabled_dictionaries() const {
    std::vector<std::string> v;
    v.reserve(dicts_.size());
    for (const auto* dp : ordered_dictionaries()) {
        if (dp->enabled()) v.push_back(dp->name());
    }
    return v;
}

bool DictionaryManagerStd::set_dictionary_enabled(const std::string& dict_name, bool enabled) {
    for (auto& d : dicts_) {
        if (d.name() != dict_name) continue;
        if (d.enabled() == enabled) return true;
        d.set_enabled(enabled);
        ft_index_.reset();
        return true;
    }
    return false;
}

bool DictionaryManagerStd::is_dictionary_enabled(const std::string& dict_name) const {
    const DictionaryStd* d = find_dictionary(dict_name);
    return d ? d->enabled() : false;
}

bool DictionaryManagerStd::set_dictionary_priority(const std::string& dict_name, int priority) {
    for (auto& d : dicts_) {
        if (d.name() != dict_name) continue;
        d.set_priority(priority);
        return true;
    }
    return false;
}

int DictionaryManagerStd::dictionary_priority(const std::string& dict_name) const {
    const DictionaryStd* d = find_dictionary(dict_name);
    return d ? d->priority() : 0;
}

bool DictionaryManagerStd::set_dictionary_tags(const std::string& dict_name, std::vector<std::string> tags) {
    for (auto& d : dicts_) {
        if (d.name() != dict_name) continue;
        d.set_tags(std::move(tags));
        return true;
    }
    return false;
}

std::vector<std::string> DictionaryManagerStd::dictionary_tags(const std::string& dict_name) const {
    const DictionaryStd* d = find_dictionary(dict_name);
    return d ? d->tags() : std::vector<std::string>{};
}

void DictionaryManagerStd::set_tag_filter(std::vector<std::string> tags) {
    tag_filter_ = std::move(tags);
}

const std::vector<std::string>& DictionaryManagerStd::tag_filter() const {
    return tag_filter_;
}

bool DictionaryManagerStd::participates(const DictionaryStd& d) const {
    if (tag_filter_.empty()) return true;
    for (const auto& f : tag_filter_) {
        for (const auto& t : d.tags()) {
            if (t == f) return true;
        }
    }
    return false;
}

std::vector<const DictionaryStd*> DictionaryManagerStd::ordered_dictionaries() const {
    std::vector<const DictionaryStd*> v; v.reserve(dicts_.size());
    for (const auto& d : dicts_) v.push_back(&d);
    // 稳定排序：同优先级保持装载序
    std::stable_sort(v.begin(), v.end(),
                     [](const DictionaryStd* a, const DictionaryStd* b) {
                         return a->priority() > b->priority();
                     });
    return v;
}

std::vector<DictionaryManagerStd::DictMeta> DictionaryManagerStd::dictionaries_meta() const {
    std::vector<DictMeta> out; out.reserve(dicts_.size());
    for (const auto* dp : ordered_dictionaries()) {
        const auto& d = *dp;
        // 描述文本的分派在 DictionaryStd::description()（六解析器内部封装）
        out.push_back({d.name(), (int)d.words().size(), d.description()});
    }
    return out;
}

std::string DictionaryManagerStd::search_word(const std::string& word, bool include_disabled) const {
    for (const auto* dp : ordered_dictionaries()) {
        const auto& d = *dp;
        if (!include_disabled && !d.enabled()) continue;
        if (!participates(d)) continue;
        auto def = d.lookup(word);
        if (!def.empty()) return def;
    }
    // 词头全 miss → 释义全文兜底（与 Qt 面 searchWord 同口径，BUGS.md
    // BUG-005）：汉英词典词头全是汉字，good/the 这类英文只存在于释义
    // 文本里，只查词头永远查不到。倒排索引懒构建、进程内缓存
    auto ft = full_text_search(word, 12);
    return ft.empty() ? std::string{} : ft.front().definition;
}

std::vector<DictEntryStd> DictionaryManagerStd::search_all(const std::string& word,
                                                           bool include_disabled,
                                                           bool allow_fulltext_fallback) const {
    std::vector<DictEntryStd> out;
    for (const auto* dp : ordered_dictionaries()) {
        const auto& d = *dp;
        if (!include_disabled && !d.enabled()) continue;
        if (!participates(d)) continue;
        auto def = d.lookup(word);
        if (!def.empty()) out.push_back({d.name(), word, def});
    }
    // 词头命中优先：命中就不掺兜底结果（Qt 面 searchAll 同口径）
    if (!out.empty() || !allow_fulltext_fallback) return out;

    // 层 1：词头前缀命中（跨词典前缀索引取词，逐词回查释义）——与
    // Qt 面 searchGrouped 三层降级同口径：精确 > 前缀 > 释义包含
    for (const auto& cand : prefix_search(word, 12)) {
        for (const auto* dp : ordered_dictionaries()) {
            const auto& d = *dp;
            if (!include_disabled && !d.enabled()) continue;
            if (!participates(d)) continue;
            auto def = d.lookup(cand);
            if (!def.empty()) out.push_back({d.name(), cand, def});
        }
    }
    if (!out.empty()) return out;

    // 层 2：释义全文兜底；同词典同词头（折叠键）去重保留首条
    std::vector<DictEntryStd> ft = full_text_search(word, 12);
    std::set<std::string> seen;
    for (auto& e : ft) {
        const std::string key = e.dict_name + '\x1f' + TextNorm::fold_key(e.word);
        if (seen.insert(key).second) out.push_back(std::move(e));
    }
    return out;
}

void DictionaryManagerStd::build_index() {
    index_.build_index();
    prefix_index_dirty_ = false;
}

std::vector<DictionaryManagerStd::DictionaryGroupStd>
DictionaryManagerStd::search_grouped(const std::string& word) const {
    std::vector<DictionaryGroupStd> groups;
    // 查询词修剪（与 legacy searchGrouped 同口径）
    const size_t b = word.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return groups;
    const size_t e = word.find_last_not_of(" \t\r\n");
    const std::string query = word.substr(b, e - b + 1);

    auto group_index_of = [&groups](const std::string& name) -> int {
        for (int i = 0; i < (int)groups.size(); ++i) {
            if (groups[i].dictionary_name == name) return i;
        }
        return -1;
    };
    // 一条命中进来源词典的组（没有则新建），带层级标注
    auto append_entry = [&groups, &group_index_of](const DictionaryStd& d,
                                                   const std::string& w,
                                                   std::string def,
                                                   int relevance, bool fulltext) {
        GroupedEntryStd ge{w, std::move(def), relevance, fulltext};
        const int gi = group_index_of(d.name());
        if (gi >= 0) {
            groups[gi].entries.push_back(std::move(ge));
        } else {
            groups.push_back({d.name(), {std::move(ge)}});
        }
    };
    // 组内同词头去重（fold_key 折叠，保留首条——层内条目按词典/索引
    // 原序给出，首条即相关度最高）
    auto dedupe = [&groups]() {
        for (auto& g : groups) {
            std::set<std::string> seen;
            std::vector<GroupedEntryStd> kept;
            kept.reserve(g.entries.size());
            for (auto& en : g.entries) {
                if (seen.insert(TextNorm::fold_key(en.word)).second) {
                    kept.push_back(std::move(en));
                }
            }
            g.entries = std::move(kept);
        }
    };

    // 层 0：词头精确命中
    for (const auto* dp : ordered_dictionaries()) {
        const auto& d = *dp;
        if (!d.enabled() || !participates(d)) continue;
        auto def = d.lookup(query);
        if (!def.empty()) append_entry(d, query, std::move(def), 0, false);
    }
    if (!groups.empty()) {
        dedupe();
        return groups;
    }

    // 层 1：词头前缀命中（跨词典前缀索引取词，逐词回查释义）。
    // trie 未显式构建时先补建（add/load_state 置脏），避免前缀层
    // 静默空手滑向层 2
    if (prefix_index_dirty_) {
        // 惰建与 ft_index_ 同款（ensure_fulltext_index_built 的 const_cast
        // 模式）：查询面 const，构建动作是缓存填充而非可观测状态变更
        const_cast<DictionaryManagerStd*>(this)->index_.build_index();
        prefix_index_dirty_ = false;
    }
    for (const auto& cand : prefix_search(query, 12)) {
        for (const auto* dp : ordered_dictionaries()) {
            const auto& d = *dp;
            if (!d.enabled() || !participates(d)) continue;
            auto def = d.lookup(cand);
            if (!def.empty()) append_entry(d, cand, std::move(def), 1, false);
        }
    }
    if (!groups.empty()) {
        dedupe();
        return groups;
    }

    // 层 2：释义包含（全文兜底，命中自带来源词典名）
    for (auto& en : full_text_search(query, 12)) {
        const DictionaryStd* d = find_dictionary(en.dict_name);
        if (d) append_entry(*d, en.word, std::move(en.definition), 2, true);
    }
    if (!groups.empty()) {
        dedupe();
        return groups;
    }

    // 层 3：词头模糊命中（编辑距离 ≤2，前三层全空才走到）。短查询（字节
    // 长 <3）不进模糊层：距离 2 对短串近乎全表命中，噪声淹没信号
    if (query.size() >= 3) {
        std::set<std::string> seen_cand;
        for (const auto& cand : fuzzy_search(query, 12)) {
            if (!seen_cand.insert(TextNorm::fold_key(cand)).second) continue;
            for (const auto* dp : ordered_dictionaries()) {
                const auto& d = *dp;
                if (!d.enabled() || !participates(d)) continue;
                auto def = d.lookup(cand);
                if (!def.empty()) append_entry(d, cand, std::move(def), 3, false);
            }
        }
    }
    dedupe();
    return groups;
}

std::vector<std::string> DictionaryManagerStd::exact_search(const std::string& word) const { return index_.exact_match(word); }

std::vector<std::string> DictionaryManagerStd::prefix_search(const std::string& prefix, int max_results) const { return index_.prefix_search(prefix, max_results); }
std::vector<std::string> DictionaryManagerStd::fuzzy_search(const std::string& word, int max_results) const { return index_.fuzzy_search(word, max_results); }
std::vector<std::string> DictionaryManagerStd::wildcard_search(const std::string& pattern, int max_results) const { return index_.wildcard_search(pattern, max_results); }
std::vector<std::string> DictionaryManagerStd::regex_search(const std::string& pattern, int max_results) const { return index_.regex_search(pattern, max_results); }
std::vector<std::string> DictionaryManagerStd::dictionaries_for_word(const std::string& word) const { return index_.dictionaries_for_word(word); }
std::vector<std::string> DictionaryManagerStd::all_indexed_words() const { return index_.all_words(); }
int DictionaryManagerStd::indexed_word_count() const { return index_.word_count(); }
bool DictionaryManagerStd::save_index(const std::string& f) const { return index_.save_index(f); }
bool DictionaryManagerStd::load_index(const std::string& f) {
    const bool ok = index_.load_index(f);
    if (ok) prefix_index_dirty_ = true;  // 词表被替换，前缀 trie 需补建
    return ok;
}

std::vector<DictEntryStd> DictionaryManagerStd::full_text_search(const std::string& query, int max_results) const {
    std::vector<DictEntryStd> out;
    if (query.empty() || max_results <= 0) return out;
    ensure_fulltext_index_built();
    // GCOVR_EXCL_LINE：ensure_fulltext_index_built 无条件赋值 ft_index_
    // （0 文档也构造空索引），此后指针恒非空，守卫臂结构不可达；留档
    // 作"返回空"的防御语义。
    if (!ft_index_) return out;  // GCOVR_EXCL_LINE
    auto refs = ft_index_->search(query, max_results);
    out.reserve((int)refs.size());
    for (auto& r : refs) {
        if (r.dict < 0 || r.dict >= (int)dicts_.size()) continue;
        const auto& d = dicts_[r.dict];
        // 标签过滤只影响查询路径：索引仍按全量已启用词典构建，
        // 命中后在此过滤（与 save 的 UDFT 签名解耦）
        if (!participates(d)) continue;
        if (r.word < 0 || r.word >= (int)d.words().size()) continue;
        const std::string& w = d.words()[r.word];
        std::string def = d.lookup(w);
        if (!def.empty()) out.push_back({ d.name(), w, std::move(def) });
        if ((int)out.size() >= max_results) break;
    }
    return out;
}

void DictionaryManagerStd::ensure_fulltext_index_built() const {
    if (ft_index_) return;
    // Build lazily: index all definitions into an inverted index
    std::unique_ptr<FullTextIndexStd> idx(new FullTextIndexStd());
    std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> docs;
    // Pre-collect documents for parallel build
    for (int di = 0; di < (int)dicts_.size(); ++di) {
        const auto& d = dicts_[di];
        if (!d.enabled()) continue;
        for (int wi = 0; wi < (int)d.words().size(); ++wi) {
            const std::string& w = d.words()[wi];
            std::string def = d.lookup(w);
            if (!def.empty()) docs.push_back({std::move(def), {di, wi}});
        }
    }
    idx->build_from_documents(docs, 0);
    const_cast<DictionaryManagerStd*>(this)->ft_index_ = std::move(idx);
}

bool DictionaryManagerStd::save_fulltext_index(const std::string& file) const {
    ensure_fulltext_index_built();
    // GCOVR_EXCL_LINE：同 full_text_search——ensure 无条件赋值，恒非空。
    if (!ft_index_) return false;  // GCOVR_EXCL_LINE
    ft_index_->set_signature(fulltext_signature());
    return ft_index_->save(file);
}

bool DictionaryManagerStd::load_fulltext_index(const std::string& file) {
    std::unique_ptr<FullTextIndexStd> idx(new FullTextIndexStd());
    if (!idx->load(file)) return false;
    // Check signature consistency
    const std::string cur = fulltext_signature();
    if (idx->signature() != cur) return false;
    ft_index_ = std::move(idx);
    return true;
}

static inline uint64_t fnv1a64(const void* data, size_t len) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

std::string DictionaryManagerStd::fulltext_signature() const {
    // Deterministic signature combining names/word stats AND filesystem metadata of source paths.
    std::ostringstream ss;
    // 规范化逻辑版本：fold_key 规则变更（kFoldKeyVersion 递增）时旧缓存自动失效重建
    ss << "NV=" << UnidictCoreStd::TextNorm::kFoldKeyVersion << ';';
    // 分词器行为版本：tokenize 产词规则变更（kTokenizerVersion 递增）时同理——
    // 分词器换了、缓存词表不换，是"签名绿灯但查不到"的静默坏数据
    ss << "TV=" << FullTextIndexStd::kTokenizerVersion << ';';
    ss << "N=" << dicts_.size() << ';';
    for (const auto& d : dicts_) {
        ss << d.name() << '|' << d.words().size() << '|';
        // GCOVR_EXCL_LINE：六个解析器加载成功都保证至少一个词条
        // （json/csv/dsl/epub 校验 entries 非空、stardict 校验 idx 解析出
        // 非空索引、mdict 兜底无条件登记骨架词），空词表实例不可达。
        if (!d.words().empty()) ss << d.words().front() << '|' << d.words().back();  // GCOVR_EXCL_LINE
        ss << '|';
        // filesystem metadata for all companion source paths (stable order)
        std::vector<std::string> srcs = d.src_paths();
        std::sort(srcs.begin(), srcs.end());
        std::error_code ec;
        for (const auto& sp : srcs) {
            fs::path p = sp;
            if (fs::exists(p, ec)) {
                auto sz = fs::is_regular_file(p, ec) ? fs::file_size(p, ec) : 0ull;
                auto ts = fs::last_write_time(p, ec).time_since_epoch().count();
                ss << p.string() << '|' << (unsigned long long)sz << '|' << (long long)ts;
            } else {
                ss << p.string() << "|(missing)";
            }
            ss << '#';
        }
        ss << ';';
    }
    std::string s = ss.str();
    uint64_t hv = fnv1a64(s.data(), s.size());
    std::ostringstream out; out << std::hex << hv << '|' << s;
    return out.str();
}

bool DictionaryManagerStd::load_fulltext_index_relaxed(const std::string& file, int* out_version, std::string* out_error, int accept_version) {
    std::unique_ptr<FullTextIndexStd> idx(new FullTextIndexStd());
    if (!idx->load(file)) {
        if (out_error) *out_error = idx->last_error();
        return false;
    }
    if (out_version) *out_version = idx->version();
    // accept_version==1：只放行 legacy v1。v2/v3 走到这里说明签名没匹配，
    // 拒绝并且不提交——提交了就是拿一套不属于当前词典的索引去做全文检索。
    if (accept_version == 1 && idx->version() != 1) {
        if (out_error) {
            *out_error = "signature mismatch (UDFT" + std::to_string(idx->version()) +
                         " index does not match currently loaded dictionaries)";
        }
        return false;
    }
    // Ignore signature; accept any version we can parse
    ft_index_ = std::move(idx);
    return true;
}

FullTextIndexStd::Stats DictionaryManagerStd::fulltext_stats() const {
    if (!ft_index_) return FullTextIndexStd::Stats{};
    return ft_index_->stats();
}

// ---- 状态文件手写 JSON 读写器（复用 data_store_std 的口径）----
// 唯一字符串读取入口：\\ \" \n \r \t 转义表与 data_store_std 一致；
// \uXXXX 不解码（本文件只由 json_escape 产出，外来 \u 按字面保留）。
static std::string state_parse_json_string(const std::string& s, size_t from,
                                           size_t* out_end) {
    std::string out;
    size_t i = from + 1;
    // GCOVR_EXCL_LINE：调用方（区段提取/对象切分）都由字符串感知的
    // 深度计数扫描器把关后才切入，传入串在本翻译单元内必然闭合——
    // i≥size 出口与串尾悬空反斜杠兜底不可达（防御留档）。
    while (i < s.size() && s[i] != '"') {  // GCOVR_EXCL_LINE
        if (s[i] != '\\') { out.push_back(s[i++]); continue; }
        if (i + 1 >= s.size()) break;  // GCOVR_EXCL_LINE
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
    *out_end = i < s.size() ? i + 1 : s.size();  // GCOVR_EXCL_LINE
    return out;
}

static std::string state_obj_val(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return {};
    p = o.find(':', p);
    if (p == std::string::npos) return {};
    p = o.find('"', p);
    if (p == std::string::npos) return {};
    size_t end = 0;
    return state_parse_json_string(o, p, &end);
}

static long long state_obj_int(const std::string& o, const std::string& key) {
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

static bool state_obj_bool(const std::string& o, const std::string& key, bool def) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return def;
    p = o.find(':', p);
    if (p == std::string::npos) return def;
    ++p;
    while (p < o.size() && (o[p] == ' ' || o[p] == '\t')) ++p;
    if (o.compare(p, 4, "true") == 0) return true;
    if (o.compare(p, 5, "false") == 0) return false;
    return def;
}

static std::vector<std::string> state_obj_str_array(const std::string& o, const std::string& key) {
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
        out.push_back(state_parse_json_string(o, k, &end));
        k = end;
    }
    return out;
}

// 提取 "key": <对象/数组> 区段：字符串感知深度计数（路径/原因里的
// 括号引号是内容而非结构）
static std::string state_find_section(const std::string& s, const std::string& key) {
    const std::string pattern = '"' + key + '"';
    size_t pos = s.find(pattern);
    if (pos == std::string::npos) return {};
    pos = s.find(':', pos);
    if (pos == std::string::npos) return {};
    const size_t start = s.find_first_of("[{", pos);
    if (start == std::string::npos) return {};
    const char open = s[start];
    const char close = (open == '[') ? ']' : '}';
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
        if (c == '"') in_str = true;
        else if (c == open) ++depth;
        else if (c == close) {
            --depth;
            if (depth == 0) return s.substr(start, i - start + 1);
        }
    }
    return {};  // 未闭合（截断文件）：空串按缺区段处理
}

// 遍历对象数组区段里的每个对象字符串（字符串感知深度计数定界）
template <typename F>
static void state_for_each_object(const std::string& sec, F&& fn) {
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
            if (c == '"') in_str = true;
            else if (c == '{') ++depth;
            else if (c == '}') --depth;
        }
        if (depth == 0) fn(sec.substr(obj, j - obj));
        i = j + 1;
    }
}

static std::string state_json_escape(const std::string& s) {
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

bool DictionaryManagerStd::save_state(const std::string& file) const {
    std::error_code ec;
    fs::create_directories(fs::path(file).parent_path(), ec);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << "{\n  \"version\": 1,\n  \"dictionaries\": [";
    for (size_t i = 0; i < dicts_.size(); ++i) {
        const auto& d = dicts_[i];
        out << (i ? ",\n" : "\n");
        // file_path 取最初 add 的路径（src_paths 首位，load() 无条件先压入）
        out << "    {\"file_path\":\"" << state_json_escape(d.src_paths().front()) << "\"";
        out << ",\"enabled\":" << (d.enabled() ? "true" : "false");
        out << ",\"priority\":" << d.priority();
        if (!d.tags().empty()) {
            out << ",\"tags\":[";
            for (size_t t = 0; t < d.tags().size(); ++t) {
                if (t) out << ",";
                out << '"' << state_json_escape(d.tags()[t]) << '"';
            }
            out << "]";
        }
        out << "}";
    }
    out << (dicts_.empty() ? "" : "\n") << "  ],\n  \"quarantined\": [";
    for (size_t i = 0; i < failures_.size(); ++i) {
        const auto& f = failures_[i];
        out << (i ? ",\n" : "\n");
        out << "    {\"file_path\":\"" << state_json_escape(f.file_path) << "\"";
        out << ",\"reason\":\"" << state_json_escape(f.reason) << "\"";
        out << ",\"quarantined\":" << (f.quarantined ? "true" : "false") << "}";
    }
    out << (failures_.empty() ? "" : "\n") << "  ]\n}\n";
    return out.good();
}

bool DictionaryManagerStd::load_state(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        last_error_ = "State file does not exist: " + file;
        return false;
    }
    std::ostringstream ss; ss << in.rdbuf();
    const std::string s = ss.str();

    struct DictState {
        std::string path;
        bool enabled = true;
        int priority = 0;
        std::vector<std::string> tags;
    };
    std::vector<DictState> entries;
    std::vector<DictionaryFailureStd> restored;

    const std::string dsec = state_find_section(s, "dictionaries");
    if (dsec.empty()) {
        last_error_ = "State file is missing dictionary list.";
        return false;
    }
    if (dsec.front() == '[') {
        state_for_each_object(dsec, [&](const std::string& o) {
            DictState e;
            e.path = state_obj_val(o, "file_path");
            e.enabled = state_obj_bool(o, "enabled", true);
            e.priority = (int)state_obj_int(o, "priority");
            e.tags = state_obj_str_array(o, "tags");
            if (!e.path.empty()) entries.push_back(std::move(e));
        });
    }
    const std::string qsec = state_find_section(s, "quarantined");
    if (!qsec.empty() && qsec.front() == '[') {
        state_for_each_object(qsec, [&](const std::string& o) {
            DictionaryFailureStd f;
            f.file_path = state_obj_val(o, "file_path");
            f.reason = state_obj_val(o, "reason");
            f.quarantined = state_obj_bool(o, "quarantined", false);
            if (!f.file_path.empty()) restored.push_back(std::move(f));
        });
    }

    // 提交相：先换失败表，再逐条恢复词典（add_dictionary 成功摘记录、
    // 失败刷新/补记录，两档语义由此收敛到一处）
    dicts_.clear();
    index_.clear();
    ft_index_.reset();
    failures_ = std::move(restored);
    last_error_.clear();

    std::set<std::string> seen;
    for (auto& e : entries) {
        if (!seen.insert(e.path).second) continue;  // 同路径重复条目：首个生效
        // 隔离中的路径不再尝试解析（重试必须显式走 retry_failed_dictionary）
        const int fi = index_of_failure(e.path);
        if (fi >= 0 && failures_[fi].quarantined) continue;
        if (!add_dictionary(e.path)) {
            std::error_code ec;
            if (!fs::is_regular_file(e.path, ec)) {
                // 运行期诊断档：每次载入重查，文件回来自动恢复加载
                record_failure(e.path, "File not found: " + e.path, false);
            } else if (last_error_.compare(0, std::strlen(kUnsupportedExtPrefix),
                                           kUnsupportedExtPrefix) == 0) {
                record_failure(e.path, "Unsupported dictionary format: " + e.path, false);
            }
            // 解析失败档已由 add_dictionary 记录（quarantined=true）
            continue;
        }
        // 状态字段落到刚加入的实例（add_dictionary 恒 push 到尾部）
        auto& d = dicts_.back();
        d.set_enabled(e.enabled);
        d.set_priority(e.priority);
        d.set_tags(std::move(e.tags));
    }
    return true;
}

bool DictionaryManagerStd::has_resource(const std::string& dict_name, const std::string& key) const {
    const DictionaryStd* d = find_dictionary(dict_name);
    if (!d) return false;
    for (const auto& p : d->mdd_parsers()) {
        if (p->has_resource(key)) return true;
    }
    return false;
}

std::vector<uint8_t> DictionaryManagerStd::resource_data(const std::string& dict_name,
                                                         const std::string& key) const {
    const DictionaryStd* d = find_dictionary(dict_name);
    if (!d) return {};
    for (const auto& p : d->mdd_parsers()) {
        if (p->has_resource(key)) return p->get_resource(key);
    }
    return {};
}

std::string DictionaryManagerStd::resource_string(const std::string& dict_name,
                                                  const std::string& key) const {
    const DictionaryStd* d = find_dictionary(dict_name);
    if (!d) return {};
    for (const auto& p : d->mdd_parsers()) {
        if (p->has_resource(key)) return p->get_resource_as_string(key);
    }
    return {};
}

std::vector<std::string> DictionaryManagerStd::dictionary_source_paths(
    const std::string& dict_name) const {
    const DictionaryStd* d = find_dictionary(dict_name);
    if (!d) return {};
    return d->src_paths();
}

const DictionaryStd* DictionaryManagerStd::find_dictionary(const std::string& dict_name) const {
    for (const auto& d : dicts_) {
        if (d.name() == dict_name) return &d;
    }
    return nullptr;
}

} // namespace UnidictCoreStd