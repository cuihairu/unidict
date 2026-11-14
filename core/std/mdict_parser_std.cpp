#include "mdict_parser_std.h"
#include "mdict_layout_std.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <zlib.h>

#include "mdx_v2_reader_std.h"
#include "path_utils_std.h"
#include "text_norm_std.h"

namespace fs = std::filesystem;

namespace UnidictCoreStd {

// LAY-006：头部探测/布局编解码/启发式兜底拆到 mdict_layout_std.cpp
//（mdict_detail 内部面，声明见 mdict_layout_std.h），经 using 引入
// 保持下方调用点原样。
using mdict_detail::fnv1a64;
using mdict_detail::lcase_ascii;
using mdict_detail::url_encode_component;
using mdict_detail::make_file_url;
using mdict_detail::normalize_resource_key;
using mdict_detail::sanitize_relative_path;
using mdict_detail::read_head;
using mdict_detail::utf16_bom_to_utf8;
using mdict_detail::parse_simple_kv;
using mdict_detail::parse_kidx_rdef;
using mdict_detail::parse_keyb_recb;
using mdict_detail::parse_kbix_rbix;
using mdict_detail::parse_kbix_multirb;
using mdict_detail::parse_mdxk_mdxr;
using mdict_detail::parse_mdx_heuristic_real;
using mdict_detail::extract_attr;
using mdict_detail::get_mdict_password_env;
using mdict_detail::parse_mdict_body_from_file_best_effort;

MdictParserStd::MdictParserStd() : decryptor_(std::make_unique<MdictDecryptorStd>()) {}

bool MdictParserStd::load_resource_manifest() {
    // 私有方法，唯一调用点 load_companion_mdd 在调用前必已设置
    // resource_cache_root_，空值卫语句不可达（mdict 分支缺口收口时标注）
    if (resource_cache_root_.empty()) return false;  // GCOVR_EXCL_LINE
    fs::path root(resource_cache_root_);
    fs::path manifest = root / "manifest.tsv";
    std::ifstream in(manifest.string(), std::ios::binary);
    if (!in) return false;

    resource_file_by_key_.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string key = line.substr(0, tab);
        std::string rel = line.substr(tab + 1);
        if (key.empty() || rel.empty()) continue;
        fs::path p = root / rel;
        std::error_code ec;
        if (!fs::exists(p, ec)) continue;
        resource_file_by_key_[lcase_ascii(key)] = fs::absolute(p, ec).string();
    }
    return !resource_file_by_key_.empty();
}

bool MdictParserStd::extract_and_cache_resources_from_mdd(const std::string& mdd_path) {
    // 空值卫语句不可达（理由同上 load_resource_manifest）
    if (resource_cache_root_.empty()) return false;  // GCOVR_EXCL_LINE
    std::unordered_map<std::string, std::string> raw;
    std::vector<std::string> keys;
    if (!parse_mdict_body_from_file_best_effort(mdd_path, raw, keys)) return false;
    // best_effort 返回 true 即 words 非空，而各解析器写入 words 与 entries
    // 恒成对出现，故 raw 必非空，假侧不可达（mdict 分支缺口收口时标注）
    if (raw.empty()) return false;  // GCOVR_EXCL_LINE

    fs::path root(resource_cache_root_);
    UnidictCoreStd::PathUtilsStd::ensure_dir(root.string());

    // Write files & manifest.
    fs::path manifest = root / "manifest.tsv";
    std::ofstream mout(manifest.string(), std::ios::binary | std::ios::trunc);
    if (!mout) return false;

    resource_file_by_key_.clear();
    for (const auto& kv : raw) {
        const std::string rel = sanitize_relative_path(kv.first);
        if (rel.empty()) continue;
        fs::path outp = root / rel;
        std::error_code ec;
        fs::create_directories(outp.parent_path(), ec);
        std::ofstream out(outp.string(), std::ios::binary | std::ios::trunc);
        if (!out) continue;
        out.write(kv.second.data(), (std::streamsize)kv.second.size());
        out.close();
        // close 之后的流失败只在磁盘满/底层 IO 错误时出现，单线程测试
        // 无法构造（同 795 口径）
        if (!out) continue;  // GCOVR_EXCL_LINE

        const std::string key_norm = lcase_ascii(normalize_resource_key(kv.first));
        resource_file_by_key_[key_norm] = fs::absolute(outp, ec).string();
        mout << key_norm << "\t" << rel << "\n";
    }
    mout.close();
    return !resource_file_by_key_.empty();
}

bool MdictParserStd::load_companion_mdd(const std::string& mdx_path) {
    std::error_code ec;
    fs::path mdx(mdx_path);
    fs::path mdd = mdx; mdd.replace_extension(".mdd");
    if (!fs::exists(mdd, ec)) return false;

    // Cache location keyed by file signature (path + size + mtime).
    auto sz = fs::file_size(mdd, ec);
    auto ts = fs::last_write_time(mdd, ec).time_since_epoch().count();
    std::string sig = mdd.string() + "|" + std::to_string((unsigned long long)sz) + "|" + std::to_string((long long)ts);
    uint64_t hv = fnv1a64(sig.data(), sig.size());
    fs::path root = fs::path(UnidictCoreStd::PathUtilsStd::cache_dir()) / "mdict" / (std::string("mdd_") + std::to_string((unsigned long long)hv));
    resource_cache_root_ = root.string();

    if (load_resource_manifest()) return true;
    if (!extract_and_cache_resources_from_mdd(mdd.string())) return false;
    return true;
}

static std::string rewrite_mdict_link_markers(const std::string& s) {
    // Replace @@@LINK=word with clickable link.
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        size_t p = s.find("@@@LINK=", i);
        if (p == std::string::npos) { out.append(s.substr(i)); break; }
        out.append(s.substr(i, p - i));
        size_t w0 = p + 8;
        size_t w1 = w0;
        while (w1 < s.size()) {
            unsigned char c = (unsigned char)s[w1];
            if (std::isspace(c) || c == '<' || c == '"' || c == '\'') break;
            ++w1;
        }
        std::string w = s.substr(w0, w1 - w0);
        std::string href = "unidict://lookup?word=" + url_encode_component(w);
        out += "<a href=\"" + href + "\">" + w + "</a>";
        i = w1;
    }
    return out;
}

std::string MdictParserStd::render_entry_for_ui(const std::string& word, const std::string& definition) const {
    (void)word;
    const bool looks_html = (definition.find('<') != std::string::npos && definition.find('>') != std::string::npos);
    const bool has_markers =
        definition.find("@@@LINK=") != std::string::npos ||
        definition.find("entry://") != std::string::npos ||
        definition.find("bword://") != std::string::npos ||
        definition.find("sound://") != std::string::npos;
    if (!looks_html && !has_markers && resource_file_by_key_.empty()) return definition;

    std::string html = definition;
    if (html.find("@@@LINK=") != std::string::npos) {
        html = rewrite_mdict_link_markers(html);
    }

    auto map_url = [&](const std::string& raw) -> std::string {
        if (raw.empty()) return raw;
        auto starts_with_ci = [&](std::string_view pre) {
            return raw.size() >= pre.size() && std::equal(pre.begin(), pre.end(), raw.begin(),
                [](char a, char b){ return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
        };
        if (starts_with_ci("http://") || starts_with_ci("https://") || starts_with_ci("data:") || starts_with_ci("qrc:") || starts_with_ci("unidict://"))
            return raw;

        if (starts_with_ci("entry://") || starts_with_ci("bword://")) {
            std::string w = normalize_resource_key(raw);
            return "unidict://lookup?word=" + url_encode_component(w);
        }

        std::string key = normalize_resource_key(raw);
        std::string key_l = lcase_ascii(key);
        auto it = resource_file_by_key_.find(key_l);
        if (it != resource_file_by_key_.end()) return make_file_url(it->second);

        // Fall back to dictionary directory (external assets placed alongside the mdx).
        if (!dict_dir_.empty()) {
            std::error_code ec;
            fs::path p = fs::path(dict_dir_) / sanitize_relative_path(key);
            if (fs::exists(p, ec)) return make_file_url(fs::absolute(p, ec).string());
        }
        return raw;
    };

    auto rewrite_attr = [&](std::string_view attr) {
        std::string out;
        out.reserve(html.size());
        size_t i = 0;
        const std::string needle = std::string(attr) + "=\"";
        auto attr_boundary = [&html](size_t p) {
            if (p == 0) return true;  // 串首（畸形 HTML，按边界算）
            const char prev = html[p - 1];
            return prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r' ||
                   prev == '<' || prev == '/';
        };
        while (i < html.size()) {
            // 深审 P3：裸 find("src=\"") 也会命中 data-src=" 的尾巴——
            // 前一字符属属性名（如 data-）即非本属性边界，跳过续找
            size_t p = html.find(needle, i);
            while (p != std::string::npos && !attr_boundary(p)) {
                p = html.find(needle, p + 1);
            }
            if (p == std::string::npos) { out.append(html.substr(i)); break; }
            out.append(html.substr(i, p - i));
            out.append(needle);
            size_t v0 = p + needle.size();
            size_t v1 = html.find('"', v0);
            if (v1 == std::string::npos) { out.append(html.substr(v0)); break; }
            std::string raw = html.substr(v0, v1 - v0);
            std::string mapped = map_url(raw);
            out.append(mapped);
            out.push_back('"');
            i = v1 + 1;
        }
        html.swap(out);
    };

    rewrite_attr("src");
    rewrite_attr("href");
    return html;
}

bool MdictParserStd::load_dictionary(const std::string& mdx_path) {
    loaded_ = false;
    encrypted_ = false;
    encoding_.clear();
    compression_.clear();
    version_.clear();
    entries_.clear();
    words_.clear();
    folded_.clear();
    fold_dirty_ = true;
    name_.clear();
    desc_.clear();
    dict_dir_.clear();
    resource_cache_root_.clear();
    resource_file_by_key_.clear();

    fs::path p(mdx_path);
    if (p.has_extension()) {
        auto ext = lcase_ascii(p.extension().string());
        if (ext == ".mdd") {
            fs::path maybe_mdx = p; maybe_mdx.replace_extension(".mdx");
            if (fs::exists(maybe_mdx)) p = maybe_mdx;
        }
    }
    if (!fs::exists(p)) return false;
    dict_dir_ = p.parent_path().string();

    // Try to parse a minimal XML-like header near the file start.
    std::string head = read_head(p.string());
    if (!head.empty()) {
        std::string h2 = utf16_bom_to_utf8(head);
        if (!h2.empty()) head = std::move(h2);
        // Common attrs: title, description (varies by mdx)
        std::string t = extract_attr(head, "title");
        std::string d = extract_attr(head, "description");
        if (!t.empty()) name_ = t; else name_ = p.stem().string();
        if (!d.empty()) desc_ = d;
        encoding_ = extract_attr(head, "encoding");
        compression_ = extract_attr(head, "compression");
        version_ = extract_attr(head, "version");
        std::string enc = lcase_ascii(extract_attr(head, "encrypted"));
        if (!enc.empty()) {
            encrypted_ = !(enc == "0" || enc == "no" || enc == "false" || enc == "off");
        }
    } else {
        name_ = p.stem().string();
    }

    // ---- 真实 MDict v2 .mdx 忠实读取（引擎 2.0 节布局 + 加密 key 块 +
    //      Encoding 编码 + zip 容器；见 mdx_v2_reader_std.h）。放在既有
    //      实验容器与加密兜底链之前——真实格式优先，失败落回旧链兜底。
    //      头形态探测（UTF-16LE XML + 引擎 2.0）不合法时诊断为空、零成本
    //      返回假，对老实验容器文件无行为影响。
    {
        MdxV2Dict v2;
        std::string v2diag;
        if (MdxV2ReaderStd::read_file(p.string(), v2, v2diag)) {
            if (!v2.title.empty()) name_ = v2.title;
            if (!v2.description.empty()) desc_ = v2.description;
            if (!v2.encoding.empty()) encoding_ = v2.encoding;
            for (auto& e : v2.entries) {
                // 同词头（真实 mdx 大小写变体/重复词头）后到覆盖，词表只
                // 收录首个——与 folded_ 折叠索引的 first-wins 口径一致
                if (!entries_.count(e.key)) words_.push_back(e.key);
                entries_[e.key] = std::move(e.definition);
            }
            loaded_ = true;
            load_companion_mdd(p.string());
            return true;
        }
        // 失败诊断日志面：探测到疑似 v2 但解析失败时留痕（stderr，不惊动
        // 正常加载流；解析成功/非 v2 形态不打）
        if (!v2diag.empty()) {
            std::fprintf(stderr, "[unidict] mdx v2 parse failed (%s): %s\n",
                         p.filename().string().c_str(), v2diag.c_str());
        }
    }

    // If encrypted, try to decrypt using MdictDecryptor (best-effort).
    if (encrypted_) {
        // Read the entire file for best-effort parsing/decryption.
        std::ifstream fin(p.string(), std::ios::binary);
        // GCOVR_EXCL_START：这个 ifstream 打开的是几行之前刚成功打开过的
        // 同一个路径（load_dictionary 开头已打开并解析过头部），要让它在
        // 这里失败只能靠"文件在两次 open 之间被删除"，单线程测试无法构造。
        if (!fin) {  // GCOVR_EXCL_LINE
            loaded_ = true;  // GCOVR_EXCL_LINE
            load_companion_mdd(p.string());  // GCOVR_EXCL_LINE
            return true;  // GCOVR_EXCL_LINE
        }  // GCOVR_EXCL_STOP

        // Read file header and body
        std::vector<uint8_t> encrypted_body;
        char ch;
        while (fin.get(ch) && ch != '\n') {
        }
        std::ostringstream ssf;
        ssf << fin.rdbuf();
        std::string body_str = ssf.str();

        // Convert body to bytes (may already be encrypted).
        encrypted_body.assign(body_str.begin(), body_str.end());

        // First: try parsing the body as-is (some files may be flagged but not actually encrypted,
        // or only parts are protected). Never seed fallback entries for encrypted dictionaries.
        if (!body_str.empty()) {
            if (parse_mdxk_mdxr(body_str, entries_, words_) ||
                parse_keyb_recb(body_str, entries_, words_) ||
                parse_kbix_rbix(body_str, entries_, words_) ||
                parse_kbix_multirb(body_str, entries_, words_) ||
                parse_kidx_rdef(body_str, entries_, words_) ||
                parse_mdx_heuristic_real(body_str, entries_, words_)) {
                loaded_ = true;
                load_companion_mdd(p.string());
                return true;
            }
        }

        // Second: attempt a best-effort password-based SimpleXOR decryption.
        // decryptor_ 在构造函数中必创建且从不重置，判真分支不可达
        // （mdict 分支缺口收口时标注）
        if (!decryptor_) decryptor_ = std::make_unique<MdictDecryptorStd>();  // GCOVR_EXCL_LINE
        decryptor_->set_debug_mode(false); // Set to true for debugging

        if (const char* pw = get_mdict_password_env(); pw) {
            decryptor_->set_password(pw);

            auto decrypt_result = decryptor_->decrypt(encrypted_body, MdictEncryptionType::SIMPLE_XOR);
            if (decrypt_result.success) {
                const std::string& decrypted_body = decrypt_result.data;
                // SIMPLE_XOR 的"成功"与密钥流非空等价，而密钥流长度取
                // min(256, 输入长度)，为 0 当且仅当输入为空——成功即非空，
                // 假侧不可达（mdict 分支缺口收口时标注）
                if (!decrypted_body.empty()) {  // GCOVR_EXCL_LINE
                    if (parse_mdxk_mdxr(decrypted_body, entries_, words_) ||
                        parse_keyb_recb(decrypted_body, entries_, words_) ||
                        parse_kbix_rbix(decrypted_body, entries_, words_) ||
                        parse_kbix_multirb(decrypted_body, entries_, words_) ||
                        parse_kidx_rdef(decrypted_body, entries_, words_) ||
                        parse_mdx_heuristic_real(decrypted_body, entries_, words_)) {
                        loaded_ = true;
                        load_companion_mdd(p.string());
                        return true;
                    }
                }
            }
        }

        loaded_ = true;
        load_companion_mdd(p.string());
        return true;
    }

    // 1) Try experimental KEYB/RECB (zlib) container (key/record blocks)
    {
        // 加密分支里已打开过同一文件（795 口径）：这里 open 失败只能靠
        // "两次打开之间文件被删"，单线程测试无法构造
        std::ifstream fin(p.string(), std::ios::binary);  // GCOVR_EXCL_LINE
        if (!fin) return false;  // GCOVR_EXCL_LINE
        char ch; while (fin.get(ch)) { if (ch == '\n') break; }
        std::ostringstream ssf; ssf << fin.rdbuf(); std::string body = ssf.str();
        if (parse_mdxk_mdxr(body, entries_, words_) || parse_keyb_recb(body, entries_, words_) || parse_kbix_rbix(body, entries_, words_) || parse_kbix_multirb(body, entries_, words_) || parse_kidx_rdef(body, entries_, words_) || parse_mdx_heuristic_real(body, entries_, words_)) { loaded_ = true; load_companion_mdd(p.string()); return true; }
    }

    // 2) Try experimental SIMPLEKV container (optional zlib)
    {
        // Read file body after first newline (header line)
        std::ifstream fin(p.string(), std::ios::binary);  // GCOVR_EXCL_LINE（795 口径）
        if (!fin) return false;  // GCOVR_EXCL_LINE
        // consume until first '\n'
        char ch; while (fin.get(ch)) { if (ch == '\n') break; }
        std::ostringstream ssf; ssf << fin.rdbuf();
        std::string body = ssf.str();
        // If body starts with zlib, try to inflate
        bool parsed = false;
        if (body.size() >= 2) {
            unsigned char cmf = (unsigned char)body[0];
            unsigned char flg = (unsigned char)body[1];
            unsigned int hdr = ((unsigned int)cmf << 8) | (unsigned int)flg;
            if ((cmf & 0x0F) == 8 && (hdr % 31) == 0) {
                z_stream strm{};
                strm.next_in = (Bytef*)body.data();
                strm.avail_in = (uInt)body.size();
                if (inflateInit(&strm) == Z_OK) {  // GCOVR_EXCL_LINE（理由同 safe_inflate）
                    std::string out; out.resize(1024 * 1024);
                    strm.next_out = (Bytef*)out.data();
                    strm.avail_out = (uInt)out.size();
                    int rc = inflate(&strm, Z_FINISH);
                    inflateEnd(&strm);
                    if (rc == Z_STREAM_END) {
                        out.resize(out.size() - strm.avail_out);
                        parsed = parse_simple_kv(out, entries_, words_);
                    }
                }
            }
        }
        if (!parsed) {
            parsed = parse_simple_kv(body, entries_, words_);
        }
        if (parsed) { loaded_ = true; load_companion_mdd(p.string()); return true; }
    }

    // 3) Attempt to scan and decompress embedded zlib blocks and extract simple pairs
    // of the form: "word:<w>\ndefinition:<d>\n\n" for quick experiments.
    auto scan_and_decompress = [&](const std::string& data, int max_blocks, int max_out) {
        std::vector<std::string> outs;
        std::string out;
        out.reserve(static_cast<size_t>(max_out));
        for (size_t off = 0; off + 2 < data.size() && (int)outs.size() < max_blocks; ++off) {
            unsigned char cmf = (unsigned char)data[off];
            unsigned char flg = (unsigned char)data[off + 1];
            // zlib header check: (cmf*256 + flg) % 31 == 0 and CM=8
            if ((cmf & 0x0F) != 8) continue;
            unsigned int hdr = (static_cast<unsigned int>(cmf) << 8) | static_cast<unsigned int>(flg);
            if (hdr % 31 != 0) continue;
            z_stream strm{};
            strm.next_in = (Bytef*)data.data() + off;
            strm.avail_in = (uInt)(data.size() - off);
            if (inflateInit(&strm) != Z_OK) continue;  // GCOVR_EXCL_LINE（理由同 safe_inflate）
            out.resize(static_cast<size_t>(max_out));
            strm.next_out = (Bytef*)out.data();
            strm.avail_out = (uInt)out.size();
            int rc = inflate(&strm, Z_FINISH);
            const uLong consumed = strm.total_in;
            inflateEnd(&strm);
            if (rc == Z_STREAM_END) {
                size_t produced = out.size() - strm.avail_out;
                out.resize(produced);
                if (!out.empty()) outs.push_back(out);
                // 深审 P3：命中流后跳过整个压缩段再续扫（旧 +1 逐字节
                // 会在已消费的压缩流内部重复探测候选头，CPU/内存膨胀）
                off += static_cast<size_t>(consumed) - 1;  // 补回循环 ++off
            }
        }
        return outs;
    };

    // Read head bytes and entire file for scanning
    // 本函数上方已两次成功打开同一文件（795 口径），这里的流构造失败边
    // 同样只在"两次打开之间文件被删"时可达，单线程测试无法构造
    std::ifstream all(p.string(), std::ios::binary);  // GCOVR_EXCL_LINE
    std::ostringstream ssa; ssa << all.rdbuf();
    std::string full = ssa.str();
    auto blocks = scan_and_decompress(full, 8, 512 * 1024);
    for (const auto& text : blocks) {
        size_t i = 0;
        while (i < text.size()) {
            // Pattern A: "word:<w>\ndefinition:<d>\n\n"
            size_t pw = text.find("word:", i);
            size_t parsed_upto = std::string::npos;
            if (pw != std::string::npos) {
                size_t nl = text.find('\n', pw);
                if (nl != std::string::npos) {
                    std::string w = text.substr(pw + 5, nl - (pw + 5));
                    size_t pd = text.find("definition:", nl + 1);
                    if (pd != std::string::npos) {
                        size_t nl2 = text.find('\n', pd);
                        if (nl2 != std::string::npos) {
                            std::string d = text.substr(pd + 11, nl2 - (pd + 11));
                            if (!w.empty()) { entries_[w] = d; words_.push_back(w); }
                            size_t dbl = text.find("\n\n", nl2);
                            parsed_upto = (dbl == std::string::npos) ? (nl2 + 1) : (dbl + 2);
                        }
                    }
                }
            }
            if (parsed_upto == std::string::npos) {
                // Pattern B: "<w>\t<d>\n" lines
                size_t ln = text.find('\n', i);
                std::string line = text.substr(i, ln == std::string::npos ? std::string::npos : (ln - i));
                size_t tab = line.find('\t');
                if (tab != std::string::npos) {
                    std::string w = line.substr(0, tab);
                    std::string d = line.substr(tab + 1);
                    if (!w.empty()) { entries_[w] = d; words_.push_back(w); }
                    parsed_upto = (ln == std::string::npos) ? text.size() : (ln + 1);
                }
            }
            if (parsed_upto == std::string::npos) {
                // nothing matched; advance to next newline
                size_t ln = text.find('\n', i);
                i = (ln == std::string::npos) ? text.size() : (ln + 1);
            } else {
                i = parsed_upto;
            }
        }
    }

    if (words_.empty()) {
        // 未知变体兜底（降级口径）：真实 v2 路径解析成功不会到这里——
        // 到这里 = 所有已知容器（含真实 v2）都不认识。种占位词保住
        // index/search 通路，留痕给后续真实样本批次排查。
        std::fprintf(stderr,
                     "[unidict] mdx fell back to skeleton placeholder (%s): "
                     "unrecognized container variant\n",
                     p.filename().string().c_str());
        entries_["mdict"] = "MDict file loaded (skeleton).";
        entries_["unidict"] = "Unidict MDX support (WIP).";
        words_.push_back("mdict");
        words_.push_back("unidict");
    }

    loaded_ = true;
    load_companion_mdd(p.string());
    return true;
}

bool MdictParserStd::is_loaded() const { return loaded_; }
std::string MdictParserStd::dictionary_name() const { return name_.empty() ? std::string("MDict") : name_; }
std::string MdictParserStd::dictionary_description() const {
    std::string info = desc_;
    if (!version_.empty()) info += (info.empty()?"":" ") + std::string("(v=") + version_ + ")";
    if (!encoding_.empty()) info += std::string(" enc=") + encoding_;
    if (!compression_.empty()) info += std::string(" comp=") + compression_;
    if (encrypted_) info += " [encrypted]";
    return info;
}
int MdictParserStd::word_count() const { return (int)words_.size(); }

std::string MdictParserStd::lookup(const std::string& word) const {
    auto it = entries_.find(word);
    if (it == entries_.end()) {
        // 精确 miss → 折叠键回退（JsonParserStd::lookup 同口径）。折叠
        // 索引按 words_ 原序惰建：unordered_map 迭代序不定，同折键多
        // 词形时 canonical 要 first-wins 必须走有序词表；load 起点置脏。
        // 键按当前 norm_options_ 档位生成：Exact 档位下回退键即原样字节，
        // 不再命中额外词形（见 text_norm_profile_test）
        if (fold_dirty_) {
            folded_.clear();
            for (const auto& w : words_) folded_.emplace(TextNorm::fold_key(w, norm_options_), w);
            fold_dirty_ = false;
        }
        auto fit = folded_.find(TextNorm::fold_key(word, norm_options_));
        if (fit == folded_.end()) return {};
        it = entries_.find(fit->second);
        if (it == entries_.end()) return {};  // GCOVR_EXCL_LINE：folded_ 由 words_ 构建，词表词条与 entries_ 同点位插入，canonical 必在主表
        return render_entry_for_ui(word, it->second);
    }
    return render_entry_for_ui(word, it->second);
}

void MdictParserStd::set_norm_options(const TextNorm::Options& opt) {
    norm_options_ = opt;
    fold_dirty_ = true; // 缓存按新档位作废，下次查询重建
}

std::vector<std::string> MdictParserStd::find_similar(const std::string& word, int max_results) const {
    std::vector<std::string> out; out.reserve(std::min<int>((int)words_.size(), max_results));
    for (const auto& w : words_) { if ((int)out.size() >= max_results) break; if (w.rfind(word, 0) == 0) out.push_back(w); }
    return out;
}

std::vector<std::string> MdictParserStd::all_words() const { return words_; }

} // namespace UnidictCoreStd