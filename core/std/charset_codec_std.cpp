#include "std/charset_codec_std.h"

#include <algorithm>
#include <cstdint>

#include "std/gb18030_table_std.inc"

namespace UnidictCoreStd::CharsetCodec {
namespace {

// charset 名字归一化：只保留 [a-z0-9]，其余（空白、'-'、'_'、'.') 全丢。
// 于是 "GB-18030" / "gb 18030" / "GB_18030" / "GB18030" 归一到同一个串。
std::string normalize(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (c >= 'A' && c <= 'Z') {
            out.push_back(static_cast<char>(c - 'A' + 'a'));
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out.push_back(c);
        }
        // 其余字符（空白、'-'、'_'、'.'、'#' …）一律丢弃
    }
    return out;
}

// CP1252 与 Latin-1 只差 0x80–0x9F 这一段。Latin-1 在这段是 C0/C1 控制字符，
// CP1252 是排版字符（弯引号、破折号、欧元、 Trademark…）。老词典的
// description/释义里这些字符很常见，按 Latin-1 解会得到不可见控制字符。
const char16_t kCp1252High[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
};

void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    // GCOVR_EXCL_START
    // 4 字节（星平面）分支对当前全部调用方都不可达：Latin-1/CP1252 的码点
    // 上限是 U+0178，GB18030 两字节区是 U+00A4–U+FFE5，ASCII 走 1 字节分支。
    // 保留它是为了让这个函数作为 UTF-8 编码器是完整正确的（将来接 Big5 或
    // GB18030 四字节区时不必重写），而不是留一段"理论上够用"的残缺实现。
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    // GCOVR_EXCL_STOP
}

// 取 GB18030 两字节区的一个槽；b1/b2 越界或无映射时返回 false
bool gb18030_lookup(uint8_t b1, uint8_t b2, uint16_t& out_cp) {
    if (b1 < 0x81 || b1 > 0xFE) return false;
    if (b2 == 0x7F) return false;  // 标准里是未定义槽
    // trail 字节在 kTrailBytes 里的序号
    int idx = -1;
    if (b2 >= 0x40 && b2 <= 0x7E) {
        idx = b2 - 0x40;
    } else if (b2 >= 0x80 && b2 <= 0xFE) {
        idx = (0x7E - 0x40 + 1) + (b2 - 0x80);
    }
    if (idx < 0 || idx >= kTrailsPerLead) return false;
    const uint16_t cp = kGb18030ToUni[(b1 - 0x81) * kTrailsPerLead + idx];
    if (cp == 0) return false;  // 防御性：生成时该区全有映射
    out_cp = cp;
    return true;
}

std::string single_byte_to_utf8(const std::string& bytes, bool cp1252) {
    std::string out;
    out.reserve(bytes.size() + bytes.size() / 4);
    for (const char ch : bytes) {
        const uint8_t b = static_cast<uint8_t>(ch);
        if (b < 0x80) {
            append_utf8(out, b);  // ASCII 也走统一编码，不单独 push_back
        } else if (cp1252 && b >= 0x80 && b <= 0x9F) {
            append_utf8(out, kCp1252High[b - 0x80]);
        } else {
            append_utf8(out, b);  // Latin-1：高位字节即码点
        }
    }
    return out;
}

std::string gb18030_to_utf8(const std::string& bytes) {
    // 声明的编码就是权威，**不做任何启发式探测**。
    //
    // 试过两种探测都错，因为 GB18030 两字节区与 UTF-8 在字节层面根本无法
    // 无歧义地区分：
    //   1) 逐序列先探 UTF-8：GBK lead 0xC0-0xFE 与 UTF-8 lead 0xC0-0xFD
    //      完全重叠，绝大多数 GBK 双字节被误吃成 UTF-8——D6 B8（指）算出
    //      U+05B7（ָ，希伯来语元音点）。
    //   2) 整串判一次是否合法 UTF-8：单个 GBK 汉字本身就是合法的 UTF-8
    //      两字节序列（D6 B8 就既是"指"也是 U+05B7），所以按字符查的词典
    //      照样全错。
    // 只有词典作者在 .ifo 里写对编码才是正解；猜测只会静默产出乱码。
    // 需要兜底的场景（漏写/错标 charset）由调用方走 salvage_as_gb18030，
    // 那里的前提"数据确定不是合法 UTF-8"本身就是正确且安全的判据。
    std::string out;
    out.reserve(bytes.size() + bytes.size() / 3);
    size_t i = 0;
    while (i < bytes.size()) {
        const uint8_t b1 = static_cast<uint8_t>(bytes[i]);
        if (b1 < 0x80) {  // ASCII 直通
            out.push_back(bytes[i]);
            ++i;
            continue;
        }
        if (i + 1 < bytes.size()) {
            uint16_t cp = 0;
            if (gb18030_lookup(b1, static_cast<uint8_t>(bytes[i + 1]), cp)) {
                append_utf8(out, cp);
                i += 2;
                continue;
            }
        }
        // 非法字节：原样透传，只吃 1 字节。
        // 刻意不替换成 '?'：词条内容是用户的数据，静默改写等于篡改。
        out.push_back(bytes[i]);
        ++i;
    }
    return out;
}

}  // namespace

Charset from_name(const std::string& name_in) {
    const std::string n = normalize(name_in);
    if (n.empty()) return Charset::Unknown;

    if (n == "utf8" || n == "utf8bom" || n == "unicode11utf8") {
        return Charset::Utf8;
    }
    // GB2312 / GBK / GB18030 / CP936 两字节区完全一致，一张表通吃
    // 收录的都是**确实指中文编码**的别名。EUC-KR/EUC-JP 之类刻意不收：
    // 韩日字节被按 GBK 解会得到一堆不相干���汉字，看起来"有结果"实际全错，
    // 比老实报 Unknown 危害大得多。
    if (n == "gb2312" || n == "gbk" || n == "gb18030" || n == "gb231280" ||
        n == "csgb2312" || n == "eucn" || n == "cp936" || n == "windows936" ||
        n == "ms936" || n == "gb2312gbk" || n == "chinese" || n == "csgbk" ||
        n == "gb2312gb18030" || n == "xgbk") {
        return Charset::Gb18030;
    }
    if (n == "iso88591" || n == "latin1" || n == "l1" || n == "iso8859" ||
        n == "88591" || n == "cp819" || n == "iso_88591_1" || n == "latin") {
        return Charset::Latin1;
    }
    if (n == "windows1252" || n == "cp1252" || n == "1252" || n == "xcp1252") {
        return Charset::Cp1252;
    }
    // Big5 / EUC-KR / Shift_JIS 等暂不支持：如实报 Unknown，让 UI 提示，
    // 好过默默给用户乱码。要加的话是同一套"生成表 + 查表"的活。
    return Charset::Unknown;
}

const char* name(Charset cs) {
    switch (cs) {
        case Charset::Utf8:
            return "UTF-8";
        case Charset::Latin1:
            return "ISO-8859-1";
        case Charset::Cp1252:
            return "Windows-1252";
        case Charset::Gb18030:
            return "GB18030";
        case Charset::Unknown:
            break;
    }
    return "";
}

bool is_supported(Charset cs) {
    return cs != Charset::Unknown;
}

std::string to_utf8(Charset cs, const std::string& bytes) {
    switch (cs) {
        case Charset::Utf8:
            return bytes;  // 本来就是 UTF-8，原样最省事也最保真
        case Charset::Latin1:
            return single_byte_to_utf8(bytes, false);
        case Charset::Cp1252:
            return single_byte_to_utf8(bytes, true);
        case Charset::Gb18030:
            return gb18030_to_utf8(bytes);
        case Charset::Unknown:
            break;
    }
    return bytes;  // 不认识的编码：原样透传，绝不猜
}

bool is_valid_utf8(const std::string& bytes) {
    size_t i = 0;
    const size_t n = bytes.size();
    while (i < n) {
        const uint8_t b1 = static_cast<uint8_t>(bytes[i]);
        size_t len = 0;
        uint32_t cp = 0;
        if (b1 < 0x80) {
            ++i;
            continue;
        } else if ((b1 & 0xE0) == 0xC0) {
            len = 2;
            cp = b1 & 0x1F;
        } else if ((b1 & 0xF0) == 0xE0) {
            len = 3;
            cp = b1 & 0x0F;
        } else if ((b1 & 0xF8) == 0xF0) {
            len = 4;
            cp = b1 & 0x07;
        } else {
            return false;  // 0x80-0xBF 续字节开头 / 0xF8+ 非法
        }
        if (i + len > n) return false;
        for (size_t k = 1; k < len; ++k) {
            const uint8_t bk = static_cast<uint8_t>(bytes[i + k]);
            if ((bk & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (bk & 0x3F);
        }
        // 过长编码 / 代理区 / 超出 Unicode 范围
        if (len == 2 && cp < 0x80) return false;
        if (len == 3 && cp < 0x800) return false;
        if (len == 4 && cp < 0x10000) return false;
        if (cp > 0x10FFFF) return false;
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        i += len;
    }
    return true;
}

bool looks_like_gb18030(const std::string& bytes) {
    size_t i = 0;
    const size_t n = bytes.size();
    while (i < n) {
        const uint8_t b1 = static_cast<uint8_t>(bytes[i]);
        if (b1 < 0x80) {
            ++i;
            continue;
        }
        if (i + 1 >= n) return false;  // 落单的 lead
        uint16_t cp = 0;
        if (!gb18030_lookup(b1, static_cast<uint8_t>(bytes[i + 1]), cp)) {
            return false;
        }
        i += 2;
    }
    return true;
}

bool salvage_as_gb18030(const std::string& bytes, Charset cs, std::string& out) {
    // 声明的编码我们本来就能处理，或者压根就是合法 UTF-8 → 没什么可救的
    if (is_supported(cs) && cs != Charset::Utf8) return false;
    if (bytes.empty()) return false;
    if (is_valid_utf8(bytes)) return false;  // 本身合法，绝不碰
    if (!looks_like_gb18030(bytes)) return false;
    out = gb18030_to_utf8(bytes);
    return true;
}

}  // namespace UnidictCoreStd::CharsetCodec
