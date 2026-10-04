// 在线发音源实现。见 online_pron_std.h 的模块注释。
//
// 解析器是容忍式扫描器而非完整 JSON 解析器：dictionaryapi.dev 的
// phonetics 条目形如 {"text":"həˈləʊ","audio":"...-us.mp3"}，我们只
// 关心"带非空 audio 的对象 + 同级 text 标注"这一个局部形状。容忍式
// 的动机是线上响应不可控——字段缺失、结构变体、网络截断都可能发生；
// 扫描器对截断的天然友好（截断点之前完整读到的条目照常产出）比
// "整文档合法才解析"更贴合发音源这种"有一条就能播"的消费方。

#include "std/online_pron_std.h"

#include <cstddef>

namespace UnidictCoreStd {

namespace {

constexpr char kFreeDictUrlBase[] =
    "https://api.dictionaryapi.dev/api/v2/entries/en/";

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// 小写化（ASCII；标注与 URL 段标记都是 ASCII 域）
std::string to_lower_ascii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

bool read_hex4(const std::string& s, size_t& pos, unsigned& v) {
    if (pos + 4 > s.size()) return false;
    v = 0;
    for (int k = 0; k < 4; ++k) {
        char c = s[pos + static_cast<size_t>(k)];
        unsigned d;
        if (c >= '0' && c <= '9') d = static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
        else return false;
        v = (v << 4) | d;
    }
    pos += 4;
    return true;
}

void append_utf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// 读取 pos 处（指向开引号）的 JSON 字符串字面量，解码转义后写入
// out，pos 推进到闭引号之后。返回 false 表示未闭合（输入截断）——
// 此时 out 内容不完整，调用方应整体放弃这个值。
bool read_json_string(const std::string& s, size_t& pos, std::string& out) {
    const size_t n = s.size();
    if (pos >= n || s[pos] != '"') return false;
    ++pos;
    out.clear();
    while (pos < n) {
        char c = s[pos];
        if (c == '"') {
            ++pos;
            return true;
        }
        if (c != '\\') {
            out += c;
            ++pos;
            continue;
        }
        ++pos;  // 越过反斜杠
        if (pos >= n) return false;
        char e = s[pos];
        switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                ++pos;
                unsigned cp = 0;
                if (!read_hex4(s, pos, cp)) return false;
                // 高代理 + \u 低代理 → 合成基本平面外码点；配不上
                // 就是残缺输入，静默丢弃该字符（容错口径）
                if (cp >= 0xD800 && cp <= 0xDBFF && pos + 1 < n &&
                    s[pos] == '\\' && s[pos + 1] == 'u') {
                    size_t save = pos;
                    pos += 2;
                    unsigned lo = 0;
                    if (read_hex4(s, pos, lo) && lo >= 0xDC00 &&
                        lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else {
                        pos = save;
                        cp = 0;
                    }
                }
                if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0;  // 孤立代理丢弃
                if (cp != 0) append_utf8(out, cp);
                continue;  // read_hex4 已推进，勿再 ++pos
            }
            default:
                out += e;  // 非法转义：原样收，不中断
                break;
        }
        ++pos;
    }
    return false;
}

// 扫描中的对象帧：只记我们关心的两个键
struct ObjFrame {
    bool has_audio = false;
    std::string audio;
    std::string text;
};

// 容器种类：对象里的字符串是 key，数组里的字符串一律是值
enum class Ctx { Object, Array };

// 一帧收完：拼 clip、推断口音、按 url 去重后入库
void emit_frame(const ObjFrame& f, std::vector<PronClip>& clips) {
    if (!f.has_audio || f.audio.empty()) return;
    PronClip clip;
    clip.url = f.audio;
    clip.label = f.text;
    clip.accent = accent_from_url(f.audio);
    if (clip.accent == PronAccent::Unknown) {
        clip.accent = accent_from_label(f.text);
    }
    for (const PronClip& e : clips) {
        if (e.url == clip.url) return;  // 去重：同一 URL 只收一条
    }
    clips.push_back(std::move(clip));
}

const PronClip* find_accent(const std::vector<PronClip>& clips,
                            PronAccent a) {
    for (const PronClip& c : clips) {
        if (c.accent == a) return &c;
    }
    return nullptr;
}

}  // namespace

const char* FreeDictionarySource::name() const { return "dictionaryapi.dev"; }

std::string FreeDictionarySource::request_url(const std::string& word) const {
    if (word.empty()) return {};  // 空查询不发请求（隐私口径的最小面）
    return std::string(kFreeDictUrlBase) + url_path_escape(word);
}

std::vector<PronClip> FreeDictionarySource::parse_response(
    const std::string& body) const {
    return parse_free_dictionary(body);
}

std::string url_path_escape(const std::string& raw) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(raw.size());
    for (char ch : raw) {
        unsigned char c = static_cast<unsigned char>(ch);
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                          c == '.' || c == '~';
        if (unreserved) {
            out += ch;
        } else {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0x0F];
        }
    }
    return out;
}

PronAccent accent_from_url(const std::string& url) {
    // 只看最后一段路径（文件名），避免 "/usr/" 这类路径段误命中；
    // 已知文件名形状：hello-us.mp3 / hello--_gb_1.mp3 / hello_au.mp3
    size_t slash = url.find_last_of('/');
    std::string file =
        to_lower_ascii(slash == std::string::npos ? url : url.substr(slash + 1));
    // 查询串不参与判断
    size_t q = file.find('?');
    if (q != std::string::npos) file.resize(q);
    auto has = [&file](const char* marker) {
        return file.find(marker) != std::string::npos;
    };
    if (has("-us") || has("_us")) return PronAccent::US;
    if (has("-uk") || has("_uk") || has("-gb") || has("_gb")) {
        return PronAccent::UK;
    }
    if (has("-au") || has("_au")) return PronAccent::AU;
    return PronAccent::Unknown;
}

PronAccent accent_from_label(const std::string& text) {
    std::string t = to_lower_ascii(text);
    auto has = [&t](const char* sub) {
        return t.find(sub) != std::string::npos;
    };
    if (has("american") || t == "us" || t == "usa") return PronAccent::US;
    if (has("british") || t == "uk" || t == "gb") return PronAccent::UK;
    if (has("australian") || t == "au") return PronAccent::AU;
    return PronAccent::Unknown;
}

std::vector<PronClip> parse_free_dictionary(const std::string& body) {
    std::vector<PronClip> clips;
    std::vector<Ctx> ctxs;   // 容器栈：区分对象（字符串是 key）与数组
    std::vector<ObjFrame> frames;  // 与 '{' 一一对应
    std::string pending_key;  // 对象里刚读完、还没见到冒号的 key
    bool await_value = false;  // 冒号已过，下一个字符串是值

    const size_t n = body.size();
    size_t i = 0;
    while (i < n) {
        char c = body[i];
        if (c == '{') {
            ctxs.push_back(Ctx::Object);
            frames.emplace_back();
            await_value = false;
            pending_key.clear();
            ++i;
            continue;
        }
        if (c == '[') {
            ctxs.push_back(Ctx::Array);
            await_value = false;
            pending_key.clear();
            ++i;
            continue;
        }
        if (c == '}' || c == ']') {
            if (!ctxs.empty()) {
                Ctx top = ctxs.back();
                ctxs.pop_back();
                if (top == Ctx::Object && !frames.empty()) {
                    ObjFrame f = frames.back();
                    frames.pop_back();
                    emit_frame(f, clips);
                }
            }
            await_value = false;
            pending_key.clear();
            ++i;
            continue;
        }
        if (c == ',') {  // 逗号后回到 key/元素起点
            await_value = false;
            pending_key.clear();
            ++i;
            continue;
        }
        if (c == '"') {
            std::string val;
            if (!read_json_string(body, i, val)) break;  // 截断，收尾
            bool in_object = !ctxs.empty() && ctxs.back() == Ctx::Object;
            if (!await_value && in_object) {
                // key 位：记下 key，见冒号则下一个字符串是值
                pending_key = std::move(val);
                size_t j = i;
                while (j < n && is_space(body[j])) ++j;
                if (j < n && body[j] == ':') {
                    await_value = true;
                    i = j + 1;
                } else {
                    pending_key.clear();  // 非法结构：容错，不当中断
                }
            } else {
                // 值位：只捕获 audio / text，其余丢弃
                if (await_value && !frames.empty()) {
                    if (pending_key == "audio") {
                        if (!val.empty()) {
                            frames.back().has_audio = true;
                            frames.back().audio = std::move(val);
                        }
                    } else if (pending_key == "text") {
                        frames.back().text = std::move(val);
                    }
                }
                await_value = false;
                pending_key.clear();
            }
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++i;
            continue;
        }
        // 数字 / true / false / null：吃到分隔符为止
        while (i < n && body[i] != ',' && body[i] != '}' && body[i] != ']' &&
               !is_space(body[i])) {
            ++i;
        }
        await_value = false;
        pending_key.clear();
    }
    // 截断收尾：未闭合对象里已完整读到的 audio 照常产出
    for (const ObjFrame& f : frames) emit_frame(f, clips);
    return clips;
}

const PronClip* pick_clip(const std::vector<PronClip>& clips,
                          PronAccent prefer) {
    if (clips.empty()) return nullptr;
    if (prefer != PronAccent::Unknown) {
        if (const PronClip* hit = find_accent(clips, prefer)) return hit;
    }
    // 偏好未命中：按 US → UK → AU → Unknown 的默认次序取
    static const PronAccent kOrder[] = {
        PronAccent::US, PronAccent::UK, PronAccent::AU, PronAccent::Unknown};
    for (PronAccent a : kOrder) {
        if (const PronClip* hit = find_accent(clips, a)) return hit;
    }
    return &clips[0];  // GCOVR_EXCL_LINE：理论不可达（四类之外无值），防御兜底
}

}  // namespace UnidictCoreStd
