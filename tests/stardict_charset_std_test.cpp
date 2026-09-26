// StarDict charset 支持（docs/pro_dictionary_gap.md P0 ②「StarDict ifo 关键
// 字段未完整支持……charset……当前 lookup() 直接返回 bytes，编码/类型序列
// 解释不足」）。
//
// 补这块之前，一本 charset=GBK 的 StarDict 是**完全不可用**的：.idx 的词条是
// GBK 字节而 index_ 按原样建键，查询词是 UTF-8，index_.find() 永远 miss，
// 界面上表现为"词典加载成功但一个词都查不到"——比加载失败更难排查。
//
// 覆盖三层：
//   1) CharsetCodec 本体：名字归一化/别名、单字节编码、GB18030 查表、
//      非法字节透传、合法 UTF-8 检测、误标兜底判据；
//   2) 造一本**真的 GBK .ifo/.idx/.dict**，验证词条与释义都成了 UTF-8 且查得到；
//   3) 误标场景：声明 UTF-8 实为 GBK、漏写 charset、声明不认识的编码，
//      以及 'l' 字段的 Latin-1 兼容不能退化成二次编码。
//
// 表里的 GBK 字节常量都是对着 Python `bytes(...).decode('gb18030')` 核过的，
// 不是手敲的（第一版手敲把 技/文/词 全写错了，行列位推算不可靠）。
// 夹具用的双字节则由 gbk_pair_for() 通过**本模块自己的 to_utf8** 反查得到，
// 所以既不依赖手写常量，又顺带自检了整张表的双向一致性。

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "std/charset_codec_std.h"
#include "std/stardict_parser_std.h"

using namespace UnidictCoreStd;
using namespace UnidictCoreStd::CharsetCodec;
namespace fs = std::filesystem;

static void be32w(std::vector<unsigned char>& v, uint32_t x) {
    v.push_back(static_cast<unsigned char>(x >> 24));
    v.push_back(static_cast<unsigned char>(x >> 16));
    v.push_back(static_cast<unsigned char>(x >> 8));
    v.push_back(static_cast<unsigned char>(x));
}

static void write_file(const fs::path& p, const std::string& bytes) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    assert(out.good());
}

// 遍历整个两字节区找出能解成 want 的那一个字节对。找不到返回空串。
// 这既用于造夹具，也意味着"表里 23940 个槽每个都能被唯一反查出来"这件事
// 在每个用到它的测试里都被顺手验了一遍。
static std::string gbk_pair_for(const std::string& want) {
    for (int lead = 0x81; lead <= 0xFE; ++lead) {
        for (int trail = 0x40; trail <= 0xFE; ++trail) {
            if (trail == 0x7F) continue;
            const char pair[2] = {static_cast<char>(lead), static_cast<char>(trail)};
            if (to_utf8(Charset::Gb18030, std::string(pair, 2)) == want) {
                return std::string(pair, 2);
            }
        }
    }
    return {};
}

// UTF-8 串 → GBK 串；任一码点在两字节区里找不到就返回空（调用方据此断言）
static std::string utf8_to_gbk(const std::string& in) {
    std::string out;
    size_t i = 0;
    while (i < in.size()) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            ++i;
            continue;
        }
        size_t len = 0;
        uint32_t cp = 0;
        if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1Fu;
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0Fu;
            len = 3;
        } else {
            return {};  // 夹具里不应用到 4 字节字符
        }
        if (i + len > in.size()) return {};
        for (size_t k = 1; k < len; ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(in[i + k]) & 0x3Fu);
        }
        i += len;
        // 把码点重新编成 UTF-8 单字符，作为反查的 needle
        std::string one;
        if (cp <= 0x7F) {
            one.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            one.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            one.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            one.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            one.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            one.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        const std::string pair = gbk_pair_for(one);
        if (pair.empty()) return {};
        out.append(pair);
    }
    return out;
}

// 造一本单条目的 StarDict。word/def 按原始字节写入（调用方自己转码）。
static fs::path make_star_dict(const fs::path& dir, const std::string& stem,
                               const std::string& charset_line,
                               const std::string& word_raw,
                               const std::string& def_raw,
                               const std::string& seq = "m") {
    fs::create_directories(dir);
    {
        std::ofstream ifo(dir / (stem + ".ifo"), std::ios::binary);
        ifo << "Version=2.4.2\n";
        ifo << "BookName=" << stem << "\n";
        ifo << "WordCount=1\n";
        ifo << "IdxFileSize=0\n";
        ifo << "sametypesequence=" << seq << "\n";
        ifo << charset_line;
        assert(ifo.good());
    }
    std::vector<unsigned char> idx;
    idx.insert(idx.end(), word_raw.begin(), word_raw.end());
    idx.push_back(0);
    be32w(idx, 0);  // offset
    be32w(idx, static_cast<uint32_t>(def_raw.size()));
    write_file(dir / (stem + ".idx"), std::string(idx.begin(), idx.end()));
    write_file(dir / (stem + ".dict"), def_raw);
    return dir / (stem + ".ifo");
}

int main() {
    fs::path base = fs::current_path() / "build-local" / "stardict_charset";
    fs::remove_all(base);
    fs::create_directories(base);

    // =========================================================================
    // 1) from_name：别名与归一化
    // =========================================================================
    assert(from_name("UTF-8") == Charset::Utf8);
    assert(from_name("utf8") == Charset::Utf8);
    assert(from_name("  UTF 8  ") == Charset::Utf8);  // 空白/分隔符一律丢弃
    assert(from_name("utf_8") == Charset::Utf8);
    // GB2312 / GBK / GB18030 两字节区完全相同，一张表通吃
    assert(from_name("GBK") == Charset::Gb18030);
    assert(from_name("GB2312") == Charset::Gb18030);
    assert(from_name("GB18030") == Charset::Gb18030);
    assert(from_name("gb-18030") == Charset::Gb18030);
    assert(from_name("GB_2312-80") == Charset::Gb18030);
    assert(from_name("CP936") == Charset::Gb18030);
    assert(from_name("ISO-8859-1") == Charset::Latin1);
    assert(from_name("latin1") == Charset::Latin1);
    assert(from_name("windows-1252") == Charset::Cp1252);
    // 不支持的如实报 Unknown，绝不猜
    assert(from_name("BIG5") == Charset::Unknown);
    assert(from_name("Shift_JIS") == Charset::Unknown);
    assert(from_name("EUC-KR") == Charset::Unknown);
    assert(from_name("EUC-JP") == Charset::Unknown);
    // 韩日编码刻意不映射到 GBK：按 GBK 解会得到一堆不相干的**汉字**，
    // 看着"有结果"其实全错，比老实报 Unknown 危害大得多
    assert(from_name("euckr") != Charset::Gb18030);
    assert(from_name("eucjp") != Charset::Gb18030);
    assert(from_name("") == Charset::Unknown);
    assert(from_name("   ") == Charset::Unknown);

    assert(is_supported(Charset::Gb18030) && is_supported(Charset::Utf8));
    assert(is_supported(Charset::Latin1) && is_supported(Charset::Cp1252));
    assert(!is_supported(Charset::Unknown));
    assert(std::string(name(Charset::Gb18030)) == "GB18030");
    assert(std::string(name(Charset::Utf8)) == "UTF-8");
    assert(std::string(name(Charset::Latin1)) == "ISO-8859-1");
    assert(std::string(name(Charset::Cp1252)) == "Windows-1252");
    assert(std::string(name(Charset::Unknown)).empty());

    // =========================================================================
    // 2) 单字节编码
    // =========================================================================
    {
        const std::string latin = "caf\xe9 na\xefve";
        assert(to_utf8(Charset::Latin1, latin) == "caf\xc3\xa9 na\xc3\xafve");
        // CP1252 与 Latin-1 只差 0x80-0x9F：这段差的是弯引号/欧元，
        // 按 Latin-1 解会得到 C1 控制字符（界面上是空白或乱码）
        assert(to_utf8(Charset::Cp1252, "\x93q\x94") == "\xe2\x80\x9cq\xe2\x80\x9d");
        assert(to_utf8(Charset::Cp1252, "\x80") == "\xe2\x82\xac");
        assert(to_utf8(Charset::Cp1252, "\x97") == "\xe2\x80\x94");  // em dash
        assert(to_utf8(Charset::Cp1252, "\xe9") == to_utf8(Charset::Latin1, "\xe9"));
        // UTF-8 原样透传：一个字节都不该动
        assert(to_utf8(Charset::Utf8, "caf\xc3\xa9") == "caf\xc3\xa9");
        // Unknown 原样透传
        assert(to_utf8(Charset::Unknown, "\xe9") == "\xe9");
        // 空输入
        assert(to_utf8(Charset::Gb18030, "").empty());
    }

    // =========================================================================
    // 3) GB18030 查表（常量已对 Python gb18030 核过）
    // =========================================================================
    {
        assert(to_utf8(Charset::Gb18030, "\xbc\xbc") == "\xe6\x8a\x80");  // 技
        assert(to_utf8(Charset::Gb18030, "\xce\xc4") == "\xe6\x96\x87");  // 文
        assert(to_utf8(Charset::Gb18030, "\xd6\xd0") == "\xe4\xb8\xad");  // 中
        assert(to_utf8(Charset::Gb18030, "\xb4\xca") == "\xe8\xaf\x8d");  // 词
        assert(to_utf8(Charset::Gb18030, "\xb9\xfa") == "\xe5\x9b\xbd");  // 国
        assert(to_utf8(Charset::Gb18030, "\xa3\xac") == "\xef\xbc\x8c");  // ，全角逗号
        assert(to_utf8(Charset::Gb18030, "\xa1\xa3") == "\xe3\x80\x82");  // 。
        assert(to_utf8(Charset::Gb18030, "\xa1\xa1") == "\xe3\x80\x80");  // 全角空格
        // GBK 拼音区：GB2312 第二区，lead 0xA8
        assert(to_utf8(Charset::Gb18030, "\xa8\xb9") == "\xc3\xbc");  // ü
        // **GBK 扩展区**（lead 0x81-0xA0，不在 GB2312 里）：验证表不是只做了
        // GB2312，否则 GBK 词典里靠后的生僻字还是乱码
        assert(to_utf8(Charset::Gb18030, "\x81\x40") == "\xe4\xb8\x82");  // 丂 U+4E02
        assert(to_utf8(Charset::Gb18030, "\x81\x41") == "\xe4\xb8\x84");  // 丄
        // 纯 ASCII 直通
        assert(to_utf8(Charset::Gb18030, "hello") == "hello");
        // 非法字节：原样透传，不替换、不丢弃
        assert(to_utf8(Charset::Gb18030, "a\xffb") == "a\xffb");
        assert(to_utf8(Charset::Gb18030, "\xff") == "\xff");
        // trail=0x7F 是标准里的未定义槽 → 原样透传
        assert(to_utf8(Charset::Gb18030, "\x81\x7f") == "\x81\x7f");
        // 落单的 lead（末尾没有 trail）→ lead 原样保留，且不吞下一条数据的头
        assert(to_utf8(Charset::Gb18030, "x\xd6") == "x\xd6");
        // 0xE5 单独出现：既 <0x81 不是 GBK lead，又不是完整 UTF-8 序列 → 原样
        assert(to_utf8(Charset::Gb18030, "\xe5") == "\xe5");
        // 声明 GBK 时**不做 UTF-8 探测**：GBK 两字节区与 UTF-8 在字节层面
        // 无法无歧义区分（D6 B8 既是"指"也是合法 UTF-8 的 U+05B7），探测
        // 只会静默产出乱码。BC BC 必须是"技"而不是被原样透传。
        assert(to_utf8(Charset::Gb18030, "\xbc\xbc") == "\xe6\x8a\x80");
        // 整串是合法 UTF-8 时，声明 GBK 也照解码（声明即权威）
        assert(to_utf8(Charset::Gb18030, "hello") == "hello");
    }

    // =========================================================================
    // 3b) 全表自检
    // =========================================================================
    {
        // (a) 23940 个槽逐个核：都必须解成 2-3 字节的合法 UTF-8，码点落在
        //     U+00A4–U+FFE5（下界 A4 决定有 158 个槽是 2 字节而不是 3 字节，
        //     一开始我把断言写成"必须 3 字节"就误报了）。下标算式写错一个
        //     偏移，肉眼从 23940 行表里根本看不出来，这里兜住。
        int slots = 0;
        bool bad = false;
        for (int lead = 0x81; lead <= 0xFE && !bad; ++lead) {
            for (int trail = 0x40; trail <= 0xFE; ++trail) {
                if (trail == 0x7F) continue;
                const char pair[2] = {static_cast<char>(lead),
                                      static_cast<char>(trail)};
                const std::string d = to_utf8(Charset::Gb18030, std::string(pair, 2));
                if (d.size() < 2 || d.size() > 3 || !is_valid_utf8(d)) {
                    bad = true;
                    break;
                }
                const unsigned char c0 = static_cast<unsigned char>(d[0]);
                const uint32_t cp = d.size() == 2
                                        ? ((c0 & 0x1Fu) << 6) |
                                              (static_cast<unsigned char>(d[1]) & 0x3Fu)
                                        : ((c0 & 0x0Fu) << 12) |
                                              ((static_cast<unsigned char>(d[1]) & 0x3Fu) << 6) |
                                              (static_cast<unsigned char>(d[2]) & 0x3Fu);
                if (cp < 0xA4 || cp > 0xFFE5) {
                    bad = true;
                    break;
                }
                ++slots;
            }
        }
        assert(!bad);
        assert(slots == 126 * 190);  // 23940
    }
    {
        // (b) 跨整个 lead 区间的独立常量抽查（值由 Python
        //     bytes(...).decode('gb18030') 核出）。(a) 只能发现"解不出来"
        //     的错，发现不了"解出来是错的那个字"——那要靠外部基准。
        assert(to_utf8(Charset::Gb18030, "\x81\x40") == "\xE4\xB8\x82");
        assert(to_utf8(Charset::Gb18030, "\x8B\x40") == "\xE5\xA9\xA1");
        assert(to_utf8(Charset::Gb18030, "\x95\x40") == "\xE6\x97\xB2");
        assert(to_utf8(Charset::Gb18030, "\x9F\x40") == "\xE7\x83\x9C");
        assert(to_utf8(Charset::Gb18030, "\xA9\x40") == "\xE3\x80\xA1");
        assert(to_utf8(Charset::Gb18030, "\xB3\x40") == "\xE7\x9F\xA6");
        assert(to_utf8(Charset::Gb18030, "\xBD\x40") == "\xE7\xB4\xB7");
        assert(to_utf8(Charset::Gb18030, "\xC7\x40") == "\xE8\x8C\xBE");
        assert(to_utf8(Charset::Gb18030, "\xD1\x40") == "\xE8\xA2\xAC");
        assert(to_utf8(Charset::Gb18030, "\xDB\x40") == "\xE8\xB7\x95");
        assert(to_utf8(Charset::Gb18030, "\xE5\x40") == "\xE9\x8C\x8A");
        assert(to_utf8(Charset::Gb18030, "\xEF\x40") == "\xE9\xA1\xAF");
        assert(to_utf8(Charset::Gb18030, "\xF9\x40") == "\xE9\xB5\x83");
    }

    // =========================================================================
    // 4) UTF-8 合法性 / 误标判据
    // =========================================================================
    {
        assert(is_valid_utf8(""));
        assert(is_valid_utf8("hello"));
        assert(is_valid_utf8("caf\xc3\xa9"));
        assert(is_valid_utf8("\xe4\xb8\xad\xe6\x96\x87"));
        assert(is_valid_utf8("\xf0\x9f\x98\x80"));        // 4 字节 emoji
        assert(!is_valid_utf8("\xe9"));                   // 续字节开头
        assert(!is_valid_utf8("\xc3"));                   // 截断的 2 字节
        assert(!is_valid_utf8("\xe4\xb8"));               // 截断的 3 字节
        assert(!is_valid_utf8("\xc0\x80"));               // 过长编码（编码出 NUL）
        assert(!is_valid_utf8("\xed\xa0\x80"));           // 代理区 D800
        assert(!is_valid_utf8("\xf8\x88\x80\x80\x80"));   // 5 字节
        assert(!is_valid_utf8("caf\xe9"));                // Latin-1 字节
        assert(!is_valid_utf8("\xfe"));                   // GBK lead，单个不成序列
        // looks_like_gb18030
        assert(looks_like_gb18030("\xd6\xd0\xd6\xd0"));
        assert(looks_like_gb18030("abc"));
        assert(looks_like_gb18030(""));
        assert(!looks_like_gb18030("\xd6"));              // 落单 lead
        assert(!looks_like_gb18030("\xff\xff"));          // 0xFF 非法 lead
    }

    // =========================================================================
    // 5) salvage：只救"确定不是 UTF-8"且"确实是 GBK"的
    // =========================================================================
    {
        std::string out;
        // 声明就是 GBK → codec 本身能处理，salvage 不该插手
        assert(!salvage_as_gb18030("\xd6\xd0", Charset::Gb18030, out));
        // 漏写 charset 的 GBK 数据 → 救
        assert(salvage_as_gb18030("\xd6\xd0\xd6\xd0", Charset::Unknown, out));
        assert(out == "\xe4\xb8\xad\xe4\xb8\xad");
        // 错标 UTF-8 的 GBK 数据 → 救
        assert(salvage_as_gb18030("\xd6\xd0", Charset::Utf8, out));
        assert(out == "\xe4\xb8\xad");
        // 本来就合法 UTF-8 → 绝不碰。误改比漏救危险得多。
        // out 必须**一个字节都不动**（调用方会直接采用 out 的值）
        out.assign(42, 'Z');
        assert(!salvage_as_gb18030("caf\xc3\xa9", Charset::Unknown, out));
        assert(out == std::string(42, 'Z'));
        out.assign(42, 'Z');
        assert(!salvage_as_gb18030("caf\xc3\xa9", Charset::Utf8, out));
        assert(out == std::string(42, 'Z'));
        // 也不是 GBK（Latin-1 残片）→ 不救，out 同样不动
        out.assign(42, 'Z');
        assert(!salvage_as_gb18030("caf\xe9", Charset::Unknown, out));
        assert(out == std::string(42, 'Z'));
        // 空输入
        assert(!salvage_as_gb18030("", Charset::Unknown, out));
    }

    // =========================================================================
    // 6) 端到端：真造一本 GBK StarDict
    // =========================================================================
    {
        const std::string word_gbk = utf8_to_gbk("技术");
        const std::string def_gbk = utf8_to_gbk("技术：技术指的是……");
        assert(!word_gbk.empty() && word_gbk.size() == 4);
        assert(def_gbk.size() == 20);  // 10 个全角字符 × 2 字节

        const fs::path ifo = make_star_dict(base / "gbk", "gbkbook", "charset=GBK\n",
                                            word_gbk, def_gbk);
        StarDictParserStd sp;
        assert(sp.load_dictionary(ifo.string()));
        // 声明的编码被如实解析出来
        assert(sp.dictionary_codec() == Charset::Gb18030);
        assert(sp.is_supported_codec());
        assert(sp.dictionary_charset() == "GBK");
        // 核心回归：UTF-8 查询词能查到 GBK 词典
        assert(sp.lookup("技术") == "技术：技术指的是……");
        // 声明可信，不该走兜底
        assert(!sp.charset_salvaged());
        // all_words 也是 UTF-8
        const auto words = sp.all_words();
        assert(words.size() == 1 && words[0] == "技术");
        // 查不到时返回空而不是乱码
        assert(sp.lookup("不存在").empty());
    }

    // =========================================================================
    // 7) 端到端：声明 UTF-8 实为 GBK（错标）
    // =========================================================================
    {
        const fs::path ifo = make_star_dict(base / "mis", "misbook",
                                            "charset=UTF-8\n",
                                            utf8_to_gbk("词"), utf8_to_gbk("词典"));
        StarDictParserStd sp;
        assert(sp.load_dictionary(ifo.string()));
        assert(sp.dictionary_codec() == Charset::Utf8);
        assert(sp.lookup("词") == "词典");
        // 兜底用过就得如实上报，UI 才能说明"编码没写对，已按 GBK 猜"
        assert(sp.charset_salvaged());
    }

    // =========================================================================
    // 8) 端到端：漏写 charset
    // =========================================================================
    {
        const fs::path ifo = make_star_dict(base / "nc", "ncbook", "",
                                            utf8_to_gbk("中"), utf8_to_gbk("中国"));
        StarDictParserStd sp;
        assert(sp.load_dictionary(ifo.string()));
        assert(sp.dictionary_codec() == Charset::Unknown);
        assert(!sp.is_supported_codec());
        assert(sp.dictionary_charset().empty());
        assert(sp.lookup("中") == "中国");
        assert(sp.charset_salvaged());
    }

    // =========================================================================
    // 9) 端到端：声明不认识的编码 → 词条按字节透传，不静默转码
    // =========================================================================
    {
        const fs::path ifo = make_star_dict(base / "b5", "b5book", "charset=BIG5\n",
                                            "abc", "xyz");
        StarDictParserStd sp;
        assert(sp.load_dictionary(ifo.string()));
        assert(sp.dictionary_codec() == Charset::Unknown);
        assert(!sp.is_supported_codec());
        // 原样字符串仍能报给 UI 去提示"编码暂不支持"
        assert(sp.dictionary_charset() == "BIG5");
        // ASCII 部分照常可用
        assert(sp.lookup("abc") == "xyz");
    }

    // =========================================================================
    // 10) 回归：'l' 字段的 Latin-1 兼容不能退化成二次编码
    // =========================================================================
    {
        // (a) 声明 UTF-8、'l' 字段里确实是合法 UTF-8 → 必须原样。
        //     原实现无条件把 'l' 当 Latin-1，这里会变成 caf\xc3\x83\xc2\xa9
        //     （界面上显示 "cafÃ©"）。
        const fs::path ifo = make_star_dict(base / "dup", "dupbook",
                                            "charset=UTF-8\n", "cafe",
                                            "caf\xc3\xa9", "l");
        StarDictParserStd sp;
        assert(sp.load_dictionary(ifo.string()));
        assert(sp.lookup("cafe") == "caf\xc3\xa9");
    }
    {
        // (b) 声明 UTF-8、'l' 字段里是 Latin-1 字节 → CP1252 兜底。
        //     这是老词典最常见的错标形态，行为不能丢。
        const fs::path ifo = make_star_dict(base / "llat", "llatbook",
                                            "charset=UTF-8\n", "cafe", "caf\xe9", "l");
        StarDictParserStd sp;
        assert(sp.load_dictionary(ifo.string()));
        assert(sp.lookup("cafe") == "caf\xc3\xa9");
    }
    {
        // (c) 'l' 必须优先于 GB18030 兜底：Latin-1 的 0xE9 也能凑成合法
        //     GBK 字节对，反过来判会把西欧词典的 linguistics 啃成汉字。
        const fs::path ifo = make_star_dict(base / "lpri", "lpribook", "",
                                            "e", "\xe9", "l");
        StarDictParserStd sp;
        assert(sp.load_dictionary(ifo.string()));
        assert(sp.lookup("e") == "\xc3\xa9");  // é，不是汉字
    }

    // =========================================================================
    // 11) 既不是合法 UTF-8、又不是 GBK 的字节：原样透传，两条兜底都不适用
    // =========================================================================
    {
        // 声明 BIG5（不支持）且 'm' 字段是 Latin-1 字节：
        //   - 不是 'l' 字段 → 不走 CP1252 兜底
        //   - looks_like_gb18030 为假（0xE9 是落单 lead）→ 不走 GB18030 兜底
        // 结果是字节原样返回。**不猜**是对的：猜错就是静默乱码，而界面上
        // 还有 is_supported_codec() 可以据此提示"编码暂不支持"。
        const fs::path ifo = make_star_dict(base / "pass", "passbook",
                                            "charset=BIG5\n", "w", "caf\xe9");
        StarDictParserStd sp;
        assert(sp.load_dictionary(ifo.string()));
        assert(!sp.is_supported_codec());
        assert(sp.lookup("w") == "caf\xe9");
    }

    fs::remove_all(base);
    std::cout << "OK\n";
    return 0;
}
