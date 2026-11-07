#include "index_engine_std.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>
#include "text_norm_std.h"

namespace UnidictCoreStd {

namespace {
// 全表扫描层的单次耗时上限：每 256 个候选核对一次时钟（逐条 steady_clock
// 调用本身有开销），到点即止。budget_ms<=0 = 不设上限（测试/CLI 要确定性
// 结果的口径）——大词典下联想下拉靠它保证不卡 UI 线程
class ScanBudget {
public:
    explicit ScanBudget(int budget_ms)
        : unlimited_(budget_ms <= 0)
        , deadline_(unlimited_ ? std::chrono::steady_clock::time_point{}
                               : std::chrono::steady_clock::now() +
                                     std::chrono::milliseconds(budget_ms)) {}
    bool keep_going() {
        if (unlimited_) return true;
        if ((++checked_ & 0xFF) != 0) return true;
        return std::chrono::steady_clock::now() < deadline_;
    }

private:
    bool unlimited_;
    std::chrono::steady_clock::time_point deadline_;
    long long checked_ = 0;
};

// 大小写不敏感的子串匹配：ASCII 分支小写（std::tolower 走 locale，逐字节
// 调用在 10 万级扫描里占大头），非 ASCII 字节与 lcase 同口径原样比较。
// 避免逐候选 lcase() 分配
bool contains_ci(const std::string& hay, const std::string& needle_lower) {
    if (needle_lower.empty()) return true;
    if (hay.size() < needle_lower.size()) return false;
    return std::search(hay.begin(), hay.end(), needle_lower.begin(), needle_lower.end(),
                       [](unsigned char a, unsigned char b) {
                           if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a + 32);
                           return a == b;
                       }) != hay.end();
}
} // namespace

// to-lower ascii and trim spaces
static inline std::string lcase(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (unsigned char c : s) out.push_back((char)std::tolower(c));
    return out;
}

static inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e-1])) --e;
    return s.substr(b, e - b);
}

void TrieNode::insert(const std::string& word) {
    TrieNode* cur = this;
    for (char ch : lcase(word)) {
        auto it = cur->children.find(ch);
        if (it == cur->children.end()) {
            cur->children.emplace(ch, std::make_unique<TrieNode>());
            it = cur->children.find(ch);
        }
        cur = it->second.get();
    }
    cur->words.insert(word);
}

void TrieNode::collect(const std::string& prefix, std::vector<std::string>& out, int& count, int max_results) const {
    if (count >= max_results) return;
    for (const auto& w : words) {
        if (count >= max_results) break;
        out.push_back(w);
        ++count;
    }
    for (const auto& kv : children) {
        if (count >= max_results) break;
        kv.second->collect(prefix + kv.first, out, count, max_results);
    }
}

IndexEngineStd::IndexEngineStd() : trie_(new TrieNode) {}

void IndexEngineStd::add_word(const std::string& word, const std::string& dictionary_id) {
    if (word.empty()) return;
    const std::string norm = normalize(word);
    IndexEntry& e = word_index_[norm];
    const bool fresh = e.word.empty();
    if (fresh) { e.word = word; e.normalized_word = norm; }
    // 扫描缓存占槽：只在词条首建时占（重复 add 幂等）。指向节点内
    // IndexEntry::word——unordered_map 再 rehash 也不搬元素地址
    if (fresh && e.scan_id == IndexEntry::kNoScanSlot &&
        scan_words_.size() < IndexEntry::kNoScanSlot) {
        e.scan_id = static_cast<std::uint32_t>(scan_words_.size());
        scan_words_.push_back(&e.word);
    }
    if (std::find(e.dictionary_ids.begin(), e.dictionary_ids.end(), dictionary_id) == e.dictionary_ids.end()) {
        e.dictionary_ids.push_back(dictionary_id);
    }
    ++e.frequency;
    dict_[dictionary_id].insert(word);
    // 前缀 trie 增量维护：导入即索引，add 完立即可 prefix_search，不再
    // 依赖调用方记得 build_index（曾因此导入/重启后前缀层静默空手）。
    // 只插折叠键的代表词（e.word）——与 build_index() 的重建产物逐字
    // 等价（同键变体 Apple/apple 只出代表形，去重序稳定）；words 是
    // set，重复 add 幂等
    trie_->insert(e.word);
    // 乱序索引同步维护（与前缀 trie 同拍，导入即索引）：排序字母键 → 词，
    // 桶内按代表词去重——重复 add 幂等，同折叠键变体只出代表形（与
    // build_index() 的重建产物逐字等价）
    auto& bucket = anagram_index_[anagram_key(e.word)];
    if (std::find(bucket.begin(), bucket.end(), e.word) == bucket.end()) {
        bucket.push_back(e.word);
    }
    built_ = true;
}

void IndexEngineStd::remove_word(const std::string& word, const std::string& dictionary_id) {
    const std::string norm = normalize(word);
    // 折叠键整个消失时，被摘的 trie 词条 = 该键的代表词（add_word 只插
    // 代表词，摘除口径与其对称——build_index 重建产物等价性不变）
    std::string removed_canonical;
    auto it = word_index_.find(norm);
    if (it != word_index_.end()) {
        auto& vec = it->second.dictionary_ids;
        vec.erase(std::remove(vec.begin(), vec.end(), dictionary_id), vec.end());
        if (vec.empty()) {
            removed_canonical = it->second.word;
            // 扫描缓存槽位置空（不移动元素，其余槽位下标恒定）
            if (it->second.scan_id < scan_words_.size()) {
                scan_words_[it->second.scan_id] = nullptr;
            }
            word_index_.erase(it);
        }
    }
    auto dit = dict_.find(dictionary_id);
    if (dit != dict_.end()) {
        dit->second.erase(word);
        if (dit->second.empty()) dict_.erase(dit);
    }
    if (!removed_canonical.empty()) {
        TrieNode* cur = trie_.get();
        for (char ch : lcase(removed_canonical)) {
            auto cit = cur->children.find(ch);
            if (cit == cur->children.end()) { cur = nullptr; break; }
            cur = cit->second.get();
        }
        if (cur) cur->words.erase(removed_canonical);
        // 中间节点不回收：空 words/children 节点对 collect 不可见，无害
        // 乱序索引对称摘除：桶空整键回收——remove 后 anagram_search 不留
        // 鬼影（与 trie 同口径，折叠键整个消失才摘）
        auto ait = anagram_index_.find(anagram_key(removed_canonical));
        if (ait != anagram_index_.end()) {
            ait->second.erase(std::remove(ait->second.begin(), ait->second.end(),
                                          removed_canonical),
                              ait->second.end());
            if (ait->second.empty()) anagram_index_.erase(ait);
        }
    }
}

void IndexEngineStd::clear_dictionary(const std::string& dictionary_id) {
    auto it = dict_.find(dictionary_id);
    if (it == dict_.end()) return;
    // Copy to a stable list to avoid iterator invalidation while erasing
    std::vector<std::string> words(it->second.begin(), it->second.end());
    for (const auto& w : words) remove_word(w, dictionary_id);
}

void IndexEngineStd::clear() {
    trie_.reset(new TrieNode);
    word_index_.clear();
    dict_.clear();
    anagram_index_.clear();
    scan_words_.clear();
    built_ = false;
}

void IndexEngineStd::build_index() {
    trie_.reset(new TrieNode);
    anagram_index_.clear();
    // 扫描缓存整体压实：重占槽位，顺带丢掉 remove 留下的空槽
    scan_words_.clear();
    for (auto& kv : word_index_) {
        trie_->insert(kv.second.word);
        if (scan_words_.size() < IndexEntry::kNoScanSlot) {
            // 无条件重占槽：增量 add 已分配的旧下标在整体压实后作废，
            // 不重写会导致 remove 摘错槽、留下悬垂指针
            kv.second.scan_id = static_cast<std::uint32_t>(scan_words_.size());
        } else {
            kv.second.scan_id = IndexEntry::kNoScanSlot;
        }
        scan_words_.push_back(&kv.second.word);
        // 乱序索引与前缀 trie 同一次重建：load_index/set_norm_options 的
        // 整表替换路径两层索引一起刷新，不留旧键鬼影
        auto& bucket = anagram_index_[anagram_key(kv.second.word)];
        if (std::find(bucket.begin(), bucket.end(), kv.second.word) == bucket.end()) {
            bucket.push_back(kv.second.word);
        }
    }
    built_ = true;
}

std::vector<std::string> IndexEngineStd::exact_match(const std::string& word) const {
    const std::string norm = normalize(word);
    auto it = word_index_.find(norm);
    if (it == word_index_.end()) return {};
    return {it->second.word};
}

std::vector<std::string> IndexEngineStd::prefix_search(const std::string& prefix, int max_results) const {
    std::vector<std::string> out;
    if (!trie_) return out;
    // Walk down the trie to the node matching the lowercased prefix
    const std::string lp = lcase(prefix);
    const TrieNode* cur = trie_.get();
    for (char ch : lp) {
        auto it = cur->children.find(ch);
        if (it == cur->children.end()) return out;
        cur = it->second.get();
    }
    int count = 0;
    cur->collect(prefix, out, count, max_results);
    return out;
}

std::vector<std::string> IndexEngineStd::fuzzy_search(const std::string& word, int max_results,
                                                      int budget_ms) const {
    std::vector<std::pair<int, std::string>> scored;
    const std::string lw = lcase(word);
    ScanBudget budget(budget_ms);
    // 距离 ≤2 的必要条件是长度差 ≤2：先按长度筛掉绝大多数候选再做 DP。
    // DP 行缓冲复用 + 逐字节现场小写比较——全表扫描不再为每个候选付
    // 两次 vector 分配与一次 lcase 分配（121k 词即 36 万次堆分配）
    std::vector<int> prev, cur;
    const int qlen = (int)lw.size();
    for (const std::string* pw : scan_words_) {
        if (!pw) continue;
        if (!budget.keep_going()) break;
        const int wl = (int)pw->size();
        if (qlen - wl > 2 || wl - qlen > 2) continue;
        const int d = edit_distance_buf(lw, *pw, prev, cur);
        if (d <= 2) scored.emplace_back(d, *pw);
    }
    // 距离同分按词头字典序：建议序与模糊层（search_grouped 层 4）候选
    // 序确定性——候选来源是 unordered 容器，不稳定排序会漂
    std::sort(scored.begin(), scored.end(), [](auto& a, auto& b){
        if (a.first != b.first) return a.first < b.first;
        return a.second < b.second;
    });
    std::vector<std::string> out; out.reserve(std::min<int>(max_results, (int)scored.size()));
    for (auto& p : scored) { if ((int)out.size() >= max_results) break; out.push_back(p.second); }
    return out;
}

std::vector<std::string> IndexEngineStd::wildcard_search(const std::string& pattern, int max_results,
                                                         int budget_ms) const {
    std::vector<std::string> out;
    if (pattern.empty() || max_results <= 0 || !trie_) return out;
    std::regex re;
    try {
        // 一次编译复用（旧实现逐词编译正则：2M 词 = 2M 次 regex 构造）
        re = std::regex(wildcard_to_regex(pattern), std::regex::icase);
    } catch (const std::regex_error&) {
        return out;
    }
    // 字面前缀收窄：首个元字符之前的字面量在 trie 上定位，只在该子树内
    // 匹配——hel*o 只扫 "hel" 子树，O(前缀长 + 子树规模)；字面量不在
    // trie 上即零命中直接返回
    const std::string lp = lcase(pattern);
    size_t lit_len = lp.find_first_of("*?");
    if (lit_len == std::string::npos) lit_len = lp.size();
    const TrieNode* cur = trie_.get();
    for (size_t i = 0; i < lit_len; ++i) {
        auto it = cur->children.find(lp[i]);
        if (it == cur->children.end()) return out;
        cur = it->second.get();
    }
    ScanBudget budget(budget_ms);
    if (lit_len == 0) {
        // '*'/ '?' 打头无字面前缀可收窄：退化为连续扫描缓存上的限时限表
        // 扫描（比 trie 子树的桶序遍历更友好）
        for (const std::string* pw : scan_words_) {
            if (!pw) continue;
            if ((int)out.size() >= max_results) break;
            if (!budget.keep_going()) break;
            if (std::regex_match(*pw, re)) out.push_back(*pw);
        }
        return out;
    }
    std::vector<const TrieNode*> stack;
    stack.push_back(cur);
    while (!stack.empty()) {
        const TrieNode* n = stack.back();
        stack.pop_back();
        for (const auto& w : n->words) {
            if ((int)out.size() >= max_results) return out;
            if (!budget.keep_going()) return out;
            if (std::regex_match(w, re)) out.push_back(w);
        }
        for (const auto& kv : n->children) stack.push_back(kv.second.get());
    }
    return out;
}

std::vector<std::string> IndexEngineStd::anagram_search(const std::string& word,
                                                        int max_results) const {
    std::vector<std::string> out;
    if (word.empty() || max_results <= 0) return out;
    const auto it = anagram_index_.find(anagram_key(word));
    if (it == anagram_index_.end()) return out;
    const std::string self = lcase(word);
    for (const auto& w : it->second) {
        if ((int)out.size() >= max_results) break;
        // 查询词自身不出：精确/前缀层已经给了它，重复即噪声
        if (lcase(w) == self) continue;
        out.push_back(w);
    }
    // 桶内按字典序：桶序 = 插入序（词典装载序），跨进程不稳，排序钉死
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> IndexEngineStd::substring_search(const std::string& sub, int max_results,
                                                          int budget_ms) const {
    std::vector<std::string> out;
    if (sub.empty() || max_results <= 0) return out;
    const std::string needle = lcase(sub);
    ScanBudget budget(budget_ms);
    for (const std::string* pw : scan_words_) {
        if (!pw) continue;
        if ((int)out.size() >= max_results) break;
        if (!budget.keep_going()) break;
        if (contains_ci(*pw, needle)) out.push_back(*pw);
    }
    // 候选来源顺序不定：字典序钉死结果序（截图/测试可比）
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> IndexEngineStd::regex_search(const std::string& pattern, int max_results) const {
    std::vector<std::string> out;
    try {
        std::regex re(pattern, std::regex::icase);
        for (const std::string* pw : scan_words_) {
            if (!pw) continue;
            if ((int)out.size() >= max_results) break;
            if (std::regex_search(*pw, re)) out.push_back(*pw);
        }
    } catch (const std::regex_error&) {
        // invalid pattern: return empty
        return {};
    }
    return out;
}

std::vector<std::string> IndexEngineStd::all_words() const {
    std::vector<std::string> v; v.reserve(word_index_.size());
    for (const auto& kv : word_index_) v.push_back(kv.second.word);
    return v;
}

std::vector<std::string> IndexEngineStd::dictionaries_for_word(const std::string& word) const {
    const std::string norm = normalize(word);
    auto it = word_index_.find(norm);
    if (it == word_index_.end()) return {};
    return it->second.dictionary_ids;
}

int IndexEngineStd::word_count() const { return (int)word_index_.size(); }

bool IndexEngineStd::save_index(const std::string& file_path) const {
    std::ofstream out(file_path, std::ios::binary);
    if (!out) return false;
    // Simple line format: word\tfrequency\tdict1|dict2|...\n
    for (const auto& kv : word_index_) {
        const auto& e = kv.second;
        out << e.word << "\t" << e.frequency << "\t";
        for (size_t i = 0; i < e.dictionary_ids.size(); ++i) {
            if (i) out << '|';
            out << e.dictionary_ids[i];
        }
        out << "\n";
    }
    return true;
}

bool IndexEngineStd::load_index(const std::string& file_path) {
    std::ifstream in(file_path, std::ios::binary);
    if (!in) return false;
    word_index_.clear();
    dict_.clear();
    scan_words_.clear();   // 整表替换，指针缓存随 build_index 重建
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string word; int freq = 0; std::string dicts;
        if (!std::getline(iss, word, '\t')) continue;
        std::string freq_s; if (!std::getline(iss, freq_s, '\t')) continue; freq = std::stoi(freq_s);
        std::getline(iss, dicts);
        IndexEntry e; e.word = word; e.normalized_word = normalize(word); e.frequency = freq;
        std::istringstream ds(dicts);
        std::string id;
        while (std::getline(ds, id, '|')) {
            if (!id.empty()) { e.dictionary_ids.push_back(id); dict_[id].insert(word); }
        }
        word_index_[e.normalized_word] = std::move(e);
    }
    build_index();
    return true;
}

std::string IndexEngineStd::normalize(const std::string& s) const { return TextNorm::fold_key(s, norm_options_); }

void IndexEngineStd::set_norm_options(const TextNorm::Options& opt) {
    norm_options_ = opt;
    // 词表原形在 dict_ 里，重灌即可换键：clear 后逐条 add_word 走新
    // norm_options_ 的 normalize。unordered_set::insert 已存在的键是
    // 迭代器安全的（不增删元素），边遍历边插不会失效
    word_index_.clear();
    scan_words_.clear();        // 节点已析构，指针缓存必须同批作废
    trie_.reset(new TrieNode);
    anagram_index_.clear();  // 旧键随档位换键作废，add_word 逐条重建
    for (const auto& kv : dict_)
        for (const auto& w : kv.second) add_word(w, kv.first);
    build_index();
}

int IndexEngineStd::edit_distance_buf(const std::string& a_lower, const std::string& b,
                                      std::vector<int>& prev, std::vector<int>& cur) {
    const int n = (int)a_lower.size(), m = (int)b.size();
    // 行缓冲由调用方跨候选复用：resize 只在变宽时分配，m 回落后不缩容
    prev.resize(m + 1);
    cur.resize(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        for (int j = 1; j <= m; ++j) {
            // b 现场小写（a 已小写）：与 lcase 后再比对同口径，但免掉
            // 每个候选一次 std::string 分配
            const unsigned char bj = (unsigned char)b[j - 1];
            const unsigned char bl = (bj >= 'A' && bj <= 'Z') ? (unsigned char)(bj + 32) : bj;
            const int cost = (a_lower[i - 1] == bl) ? 0 : 1;
            cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost });
        }
        prev.swap(cur);
    }
    return prev[m];
}

std::string IndexEngineStd::anagram_key(const std::string& word) {
    std::string k = lcase(word);
    std::sort(k.begin(), k.end());
    return k;
}

std::string IndexEngineStd::wildcard_to_regex(const std::string& pattern) {
    std::string re;
    re.reserve(pattern.size() * 2);
    re.push_back('^');
    for (char c : pattern) {
        switch (c) {
            case '*': re += ".*"; break;
            case '?': re += '.'; break;
            case '.': case '\\': case '+': case '(': case ')': case '[': case ']': case '{': case '}': case '^': case '$': case '|':
                re.push_back('\\'); re.push_back(c); break;
            default: re.push_back(c); break;
        }
    }
    re.push_back('$');
    return re;
}

} // namespace UnidictCoreStd
