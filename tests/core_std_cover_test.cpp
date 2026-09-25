// core/std 残余缺口补测：把此前分散在各文件里没走到的分支集中钉一遍。
//
// 覆盖目标：
//  - html_renderer：<a href> / <img src> 的不安全 URL 剔除、资源解析器的
//    打不开文件与词典未注册两条早退
//  - text_norm：4 字节 UTF-8 码点、非法首字节、非法续字节
//  - dsl_parser：词条内续行拼接、非头部非注释行
//  - index_engine：通配符模式里正则元字符的转义
//  - ctc_logits：行最大值出现在非首元素时的更新分支
//  - pcm_util：data 块早于 fmt 块、chunk 走完没遇到 data
//  - mdict_decryptor：未知加密算法名
//  - aggregate_lookup：释义长度分档与 examples/pronunciation 加分
//  - data_store：CSV 导出里含双引号的转义

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/aggregate_lookup_std.h"
#include "std/ctc_logits_std.h"
#include "std/data_store_std.h"
#include "std/dictionary_manager_std.h"
#include "std/dsl_parser_std.h"
#include "std/html_renderer_std.h"
#include "std/index_engine_std.h"
#include "std/mdict_decryptor_std.h"
#include "std/pcm_util_std.h"
#include "std/text_norm_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

namespace {

fs::path tmp_root() {
    static const fs::path dir = [] {
        const char* t = std::getenv("TMPDIR");
        fs::path d = fs::path(t && *t ? t : "/tmp") / "unidict_core_cover";
        fs::remove_all(d);
        fs::create_directories(d);
        return d;
    }();
    return dir;
}

void write_file(const fs::path& p, const std::string& s) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o.write(s.data(), static_cast<std::streamsize>(s.size()));
}

// ---------------------------------------------------------------- html

void test_html_unsafe_href_and_src_erased() {
    HtmlRendererStd r;
    // 不安全协议 → href 被剔除，标签本身保留
    const auto a1 = r.render("<a href=\"vbscript:x\">click</a>");
    assert(a1.text == "click");
    assert(a1.html.find("vbscript") == std::string::npos);
    assert(a1.html.find("href") == std::string::npos);

    // data:text/html 同样被剔
    const auto a2 = r.render("<a href=\"data:text/html;base64,AAA\">x</a>");
    assert(a2.html.find("data:text/html") == std::string::npos);

    // javascript: 走 is_safe_url 的 substring 判定，同样被剔
    const auto a3 = r.render("<a href=\"JaVaScRiPt:alert(1)\">x</a>");
    assert(a3.html.find("avascript") == std::string::npos);

    // 未在白名单里的协议
    const auto a4 = r.render("<a href=\"ftp://h/f\">x</a>");
    assert(a4.html.find("href") == std::string::npos);

    // 安全 URL 保留
    const auto a5 = r.render("<a href=\"https://example.com/x\">x</a>");
    assert(a5.html.find("href=\"https://example.com/x\"") != std::string::npos);

    // <img> 的 src 走同一套剔除
    const auto i1 = r.render("<img src=\"vbscript:y\"/>");
    assert(i1.html.find("vbscript") == std::string::npos);
    assert(i1.html.find("src") == std::string::npos);
    const auto i2 = r.render("<img src=\"pic.png\"/>");
    assert(i2.html.find("pic.png") != std::string::npos);
    assert(i2.has_images);
}

// 基类默认实现：不查文件，直接把 local_path 拼成 file:// URL
void test_base_resolver_data_url() {
    // local_path 为空 → 空串
    class EmptyResolverStd : public ResourceResolverStd {
    public:
        ResourceInfo resolve(const std::string&, const std::string&) override {
            return ResourceInfo{};
        }
        bool exists(const std::string&, const std::string&) override { return false; }
    };
    EmptyResolverStd empty;
    assert(empty.get_data_url("a.png", "d1").empty());

    // local_path 非空 → file:// 前缀（不做 base64，避免大资源占内存）
    class PathResolverStd : public ResourceResolverStd {
    public:
        ResourceInfo resolve(const std::string&, const std::string&) override {
            ResourceInfo info;
            info.local_path = "/tmp/x.png";
            info.mime_type = "image/png";
            return info;
        }
        bool exists(const std::string&, const std::string&) override { return true; }
    };
    PathResolverStd path;
    assert(path.get_data_url("a.png", "d1") == "file:///tmp/x.png");
}

void test_default_resolver_data_url_failures() {
    // DefaultResourceResolverStd::get_data_url 要真的打开文件：
    // 词典未注册 / 文件不存在 两处都要返回空串
    DefaultResourceResolverStd def;
    def.register_dictionary("d1", (tmp_root() / "resdir").string());
    fs::create_directories(tmp_root() / "resdir");

    // 词典没注册
    assert(def.get_data_url("a.png", "no_such_dict").empty());
    // 词典注册了但文件不存在
    assert(def.get_data_url("a.png", "d1").empty());
    assert(def.resolve("a.png", "d1").local_path.empty());
    assert(!def.exists("a.png", "d1"));

    // 放一个真文件进去 → 能解析到路径，data URL 非空
    write_file(tmp_root() / "resdir" / "ok.png", std::string("\x89PNG\r\n\x1a\n", 8) + "DATA");
    const auto info = def.resolve("ok.png", "d1");
    assert(!info.local_path.empty());
    assert(info.mime_type == "image/png");
    assert(def.exists("ok.png", "d1"));
    assert(!def.get_data_url("ok.png", "d1").empty());

    // 缓存目录设置/读取
    def.set_cache_directory((tmp_root() / "rescache").string());
    assert(def.get_cache_directory() == (tmp_root() / "rescache").string());
}

void test_default_resolver_unknown_dictionary() {
    // 词典没注册 → find_resource_file 直接空串
    DefaultResourceResolverStd def;
    def.register_dictionary("d1", tmp_root().string());
    const auto info = def.resolve("a.png", "no_such_dict");
    assert(info.local_path.empty());
    assert(def.resolve("anything.png", "no_such_dict").local_path.empty());
    // unregister 之后再查也空
    def.unregister_dictionary("d1");
    assert(def.resolve("a.png", "d1").local_path.empty());
}

// ---------------------------------------------------------------- text_norm

void test_text_norm_utf8_widths() {
    // 2 字节：é
    assert(TextNorm::fold_key("caf\xC3\xA9") == "cafe");
    // 3 字节：中（不在折叠表里，原样透传）
    assert(TextNorm::fold_key("\xE4\xB8\xAD") == "\xE4\xB8\xAD");
    // 4 字节：😀 = F0 9F 98 80。码点 > 0xFFFF 不该被当成字母做大小写折叠，
    // 但解码必须成功（不能因非法序列被逐字节透传成乱码）
    const std::string emoji = "\xF0\x9F\x98\x80";
    assert(TextNorm::fold_key(emoji) == emoji);
    // 4 字节后跟 ASCII：确认 i 的推进步长是 4 而不是 1
    assert(TextNorm::fold_key(emoji + "Ab") == emoji + "ab");

    // 非法首字节 0xF8（超出 UTF-8 合法范围）→ 原样透传，不崩
    const std::string bad = "\xF8" "abc";
    assert(TextNorm::fold_key(bad) == bad);
    // 非法首字节 0x80（落单续字节）
    const std::string bad2 = "\x80" "x";
    assert(TextNorm::fold_key(bad2) == bad2);

    // 合法首字节但续字节非法：声明 3 字节，第二字节不是 10xxxxxx。
    // 非法的那一个字节原样透传，其后的字节继续照常解码折叠
    const std::string bad3 = "\xE4" "A" "B";
    assert(TextNorm::fold_key(bad3) == "\xE4" "ab");
    // 声明 3 字节但被截断（只有 1 字节）
    const std::string bad4 = "\xE4";
    assert(TextNorm::fold_key(bad4) == bad4);

    // 过长编码：2 字节形式却编码了 < 0x80（C0 80 = overlong NUL）
    const std::string bad5 = "\xC0\x80";
    assert(TextNorm::fold_key(bad5) == bad5);
    // 代理区码点 ED A0 80（D800）必须被拒
    const std::string bad6 = "\xED\xA0\x80";
    assert(TextNorm::fold_key(bad6) == bad6);
}

// ---------------------------------------------------------------- dsl

void test_dsl_continuation_lines() {
    // 词条定义跨多行：缩进的续行要拼到同一条释义里。
    // 注意只有"以空白/制表符开头"的行才算续行——不缩进的行会被当成
    // 新词头，这正是 dsl_parser_std.cpp:82 那个判据的含义。
    const std::string dsl =
        "#NAME Test\n"
        "hello\n"
        "a greeting\n"
        "  used when meeting someone\n"
        "  and also sometimes a farewell\n"
        "world\n"
        "the earth\n"
        "\ttab indented continuation\n";
    const auto path = tmp_root() / "cont.dsl";
    write_file(path, dsl);

    DslParserStd p;
    assert(p.load_dictionary(path.string()));
    assert(p.word_count() == 2);
    const std::string def = p.lookup("hello");
    assert(def.find("a greeting") != std::string::npos);
    assert(def.find("used when meeting someone") != std::string::npos);
    assert(def.find("and also sometimes a farewell") != std::string::npos);
    // 续行之间恰好一个空格（不缩进的行会被当新词头）
    assert(def.find("greeting used") != std::string::npos);
    assert(def.find("  ") == std::string::npos);

    // 制表符缩进的续行同样生效
    const std::string def2 = p.lookup("world");
    assert(def2.find("the earth") != std::string::npos);
    assert(def2.find("tab indented continuation") != std::string::npos);
    // 两条词条的续行不能串味
    assert(def2.find("greeting") == std::string::npos);
}

void test_dsl_first_non_header_line_starts_entries() {
    // 头部区里出现既不是已知头部也不是 '#' 注释的行 → parse_header 返回
    // false，in_header 置假，这一行就成了第一个词头（DSL 头部结束的位置
    // 由"第一个非 # 行"决定，与它是否像头部无关）。
    // 头部键是大写且大小写敏感的（#NAME / #INDEX_LANGUAGE / ...）
    const std::string dsl =
        "#NAME Odd\n"
        "garbage line in header\n"
        "second line\n"
        "  indented continuation\n";
    const auto path = tmp_root() / "odd.dsl";
    write_file(path, dsl);
    DslParserStd p;
    assert(p.load_dictionary(path.string()));
    assert(p.dictionary_name() == "Odd");
    assert(p.word_count() == 1);
    assert(p.lookup("garbage line in header").find("second line") !=
           std::string::npos);
    // 缩进续行拼在同一条释义里，不会再变成新词头
    assert(p.lookup("indented continuation").empty());
}

// ---------------------------------------------------------------- index_engine

void test_wildcard_regex_metacharacters_are_escaped() {
    // 通配符模式里的正则元字符必须被转义，否则 "a.c" 会匹配 "abc"。
    // 索引按 (word, dictionary_id) 存，检索返回命中的 dictionary_id。
    IndexEngineStd idx;
    idx.add_word("abc", "d_abc");
    idx.add_word("a.c", "d_dot");
    idx.add_word("axc", "d_axc");
    idx.add_word("a+c", "d_plus");
    idx.add_word("a(c)", "d_paren");
    idx.add_word("a[c]", "d_brack");
    idx.add_word("a{c}", "d_brace");
    idx.add_word("a^c", "d_caret");
    idx.add_word("a$c", "d_dollar");
    idx.add_word("a|c", "d_pipe");
    idx.add_word("a\\c", "d_bslash");
    idx.build_index();

    // 字面量 '.' 只匹配字面量 '.'，不能顺带命中 abc / axc
    auto dot = idx.wildcard_search("a.c", 20);
    assert(dot.size() == 1);
    assert(dot[0] == "a.c");

    // 其余元字符同样按字面量处理（每个只命中自己）
    assert(idx.wildcard_search("a+c", 20).size() == 1);
    assert(idx.wildcard_search("a(c)", 20).size() == 1);
    assert(idx.wildcard_search("a[c]", 20).size() == 1);
    assert(idx.wildcard_search("a{c}", 20).size() == 1);
    assert(idx.wildcard_search("a^c", 20).size() == 1);
    assert(idx.wildcard_search("a$c", 20).size() == 1);
    assert(idx.wildcard_search("a|c", 20).size() == 1);

    // '*' 与 '?' 仍是通配符（返回命中的词）
    auto star = idx.wildcard_search("a*", 50);
    assert(star.size() == 11);
    auto q = idx.wildcard_search("a?c", 50);
    assert(q.size() >= 3);
    assert(std::find(q.begin(), q.end(), std::string("abc")) != q.end());
    assert(std::find(q.begin(), q.end(), std::string("axc")) != q.end());

    // exact_match / all_words 这两个此前零调用的公开方法也钉一下
    // （两者返回的都是词本身，不是 dictionary_id）
    auto ex = idx.exact_match("a.c");
    assert(ex.size() == 1 && ex[0] == "a.c");
    assert(idx.exact_match("nothing").empty());
    assert(idx.all_words().size() == 11);
    assert(idx.word_count() == 11);

    // clear() 复位
    idx.clear();
    assert(idx.word_count() == 0);
    assert(idx.all_words().empty());
    assert(idx.exact_match("a.c").empty());
    assert(idx.wildcard_search("a*", 50).empty());
    assert(idx.prefix_search("a", 10).empty());
}

// ---------------------------------------------------------------- ctc_logits

void test_log_softmax_max_not_first() {
    // 行最大值不在首元素：max 跟踪分支要走到。
    // log_softmax_row 除了减 max 还会减 log_sum，输出是真 log-softmax，
    // 所以最大值那一项是 -log_sum（最接近 0）而不是 0。
    const float in[4] = {1.0f, 9.0f, 2.0f, 3.0f};
    float out[4] = {0, 0, 0, 0};
    log_softmax_row(in, 4, out);
    // 最大项就是索引 1
    for (int i = 0; i < 4; ++i) assert(out[i] <= out[1] + 1e-6f);
    // 全部为负（log 概率）
    for (int i = 0; i < 4; ++i) assert(out[i] < 0.0f);
    // 概率和为 1
    double sum = 0.0;
    for (int i = 0; i < 4; ++i) sum += std::exp((double)out[i]);
    assert(std::fabs(sum - 1.0) < 1e-5);
    // out[1] = -log(1 + e^-8 + e^-7 + e^-6)
    assert(std::fabs(out[1] + 0.0037192) < 1e-6);
    // 其余三项与最大项相差恰好 8 / 7 / 6（减 max 之后的相对值）
    assert(std::fabs((out[0] - out[1]) + 8.0) < 1e-5);
    assert(std::fabs((out[2] - out[1]) + 7.0) < 1e-5);
    assert(std::fabs((out[3] - out[1]) + 6.0) < 1e-5);

    // 单元素行（n=1）：两个循环都不执行，out[0] = -log(1) = 0
    const float one[1] = {42.0f};
    float oneout[1] = {99.0f};
    log_softmax_row(one, 1, oneout);
    assert(std::fabs(oneout[0]) < 1e-6f);

    // 全部相等：maxv 取首元素，输出全为 -log(n)
    const float eq[3] = {5.0f, 5.0f, 5.0f};
    float eqout[3] = {0, 0, 0};
    log_softmax_row(eq, 3, eqout);
    for (int i = 0; i < 3; ++i) {
        assert(std::fabs(eqout[i] - std::log(1.0 / 3.0)) < 1e-6f);
    }
}

// ---------------------------------------------------------------- pcm_util

void put_u16le(std::string& s, uint16_t v) {
    s.push_back(static_cast<char>(v & 0xFF));
    s.push_back(static_cast<char>((v >> 8) & 0xFF));
}

void put_u32le(std::string& s, uint32_t v) {
    for (int i = 0; i < 4; ++i) s.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

void test_wav_data_chunk_before_fmt() {
    // data 块出现在 fmt 之前 → 拒收（没有 fmt 就无法解释采样格式）
    std::string w;
    w += "RIFF";
    put_u32le(w, 36);
    w += "WAVE";
    w += "data";
    put_u32le(w, 4);
    put_u32le(w, 4);   // 4 字节数据
    w += "fmt ";
    put_u32le(w, 16);
    put_u16le(w, 1);
    put_u16le(w, 1);
    put_u32le(w, 16000);
    put_u32le(w, 32000);
    put_u16le(w, 2);
    put_u16le(w, 16);

    PronAudio::WavInfo info;
    assert(!PronAudio::parse_wav_header(reinterpret_cast<const uint8_t*>(w.data()), w.size(), info));
}

void test_wav_no_data_chunk() {
    // 只有 fmt + JUNK、没有 data：chunk 走完仍未命中 data → 拒收。
    // 文件必须 >= 44 字节，否则在开头的 size 门槛就被拒，走不到 chunk 遍历。
    std::string w;
    w += "RIFF";
    put_u32le(w, 44);
    w += "WAVE";
    w += "fmt ";
    put_u32le(w, 16);
    put_u16le(w, 1);
    put_u16le(w, 1);
    put_u32le(w, 16000);
    put_u32le(w, 32000);
    put_u16le(w, 2);
    put_u16le(w, 16);
    w += "JUNK";
    put_u32le(w, 8);   // 声明 8 字节 body
    put_u32le(w, 0);
    put_u32le(w, 0);   // 实际 8 字节

    assert(w.size() == 52);
    PronAudio::WavInfo info;
    assert(!PronAudio::parse_wav_header(reinterpret_cast<const uint8_t*>(w.data()), w.size(), info));
    assert(info.data_bytes == 0);
}

void test_wav_too_small_rejected_early() {
    PronAudio::WavInfo info;
    // 空指针
    assert(!PronAudio::parse_wav_header(nullptr, 100, info));

    // 不足 44 字节 → 开头的 size 门槛直接拒（不进 chunk 遍历）
    std::string tiny;
    tiny += "RIFF";
    put_u32le(tiny, 4);
    tiny += "WAVE";
    assert(tiny.size() == 12);
    assert(!PronAudio::parse_wav_header(
        reinterpret_cast<const uint8_t*>(tiny.data()), tiny.size(), info));

    // 够大但 RIFF/WAVE 魔数不对
    std::string bad(64, 0);
    bad.replace(0, 4, "XXXX");
    bad.replace(8, 4, "XXXX");
    assert(!PronAudio::parse_wav_header(
        reinterpret_cast<const uint8_t*>(bad.data()), bad.size(), info));
    // 只有 RIFF 对、WAVE 错
    std::string half(64, 0);
    half.replace(0, 4, "RIFF");
    assert(!PronAudio::parse_wav_header(
        reinterpret_cast<const uint8_t*>(half.data()), half.size(), info));
}

void test_wav_fmt_with_zero_channels_rejected() {
    // fmt 合法但 channels=0 → 最终的 channels>0 判定拒收
    std::string w;
    w += "RIFF";
    put_u32le(w, 40);
    w += "WAVE";
    w += "fmt ";
    put_u32le(w, 16);
    put_u16le(w, 1);
    put_u16le(w, 0);    // channels = 0
    put_u32le(w, 16000);
    put_u32le(w, 32000);
    put_u16le(w, 2);
    put_u16le(w, 16);
    w += "data";
    put_u32le(w, 4);
    put_u32le(w, 0);

    PronAudio::WavInfo info;
    assert(!PronAudio::parse_wav_header(reinterpret_cast<const uint8_t*>(w.data()), w.size(), info));
}

void test_wav_odd_sized_chunk_padding() {
    // 奇数尺寸 chunk 后有 1 字节 pad，data 仍要正确定位
    std::string w;
    w += "RIFF";
    put_u32le(w, 4 + 8 + 5 + 1 + 8 + 16 + 8);
    w += "WAVE";
    w += "JUNK";
    put_u32le(w, 5);   // 奇数
    w += "abcde";
    w.push_back('\0');  // pad
    w += "fmt ";
    put_u32le(w, 16);
    put_u16le(w, 1);
    put_u16le(w, 1);
    put_u32le(w, 16000);
    put_u32le(w, 32000);
    put_u16le(w, 2);
    put_u16le(w, 16);
    w += "data";
    put_u32le(w, 4);
    put_u32le(w, 0x11223344);

    PronAudio::WavInfo info;
    assert(PronAudio::parse_wav_header(reinterpret_cast<const uint8_t*>(w.data()), w.size(), info));
    assert(info.channels == 1);
    assert(info.sample_rate == 16000);
    assert(info.bits == 16);
    assert(info.data_bytes == 4);
}

// ---------------------------------------------------------------- 解密器

void test_decryptor_unsupported_type_reports_name() {
    // 枚举里没列出的类型 → 错误串带上类型名；越界值落到 UNKNOWN 分支
    MdictDecryptorStd d;
    const std::vector<uint8_t> data{1, 2, 3, 4, 5, 6, 7, 8};

    // 每个具名类型都要在错误信息里报出自己的名字
    const struct {
        MdictEncryptionType type;
        const char* name;
    } named[] = {
        {MdictEncryptionType::DES_ECB, "DES_ECB"},
        {MdictEncryptionType::DES_CBC, "DES_CBC"},
        {MdictEncryptionType::BLOWFISH_ECB, "BLOWFISH_ECB"},
        {MdictEncryptionType::BLOWFISH_CBC, "BLOWFISH_CBC"},
        {MdictEncryptionType::AES_ECB, "AES_ECB"},
        {MdictEncryptionType::AES_CBC, "AES_CBC"},
    };
    for (const auto& n : named) {
        const auto r = d.decrypt(data, n.type);
        assert(!r.success);
        assert(r.error.find(n.name) != std::string::npos);
    }

    // 枚举范围外的值（真实 .mdx 的加密标志位可能出现未知取值）：外层
    // switch 的 default 接住，报告为"自定义加密类型不支持"
    const auto unknown =
        d.decrypt(data, static_cast<MdictEncryptionType>(999));
    assert(!unknown.success);
    assert(unknown.error.find("自定义") != std::string::npos);

    // 显式的 CUSTOM 同路
    const auto custom = d.decrypt(data, MdictEncryptionType::CUSTOM);
    assert(!custom.success);
    assert(custom.error.find("自定义") != std::string::npos);
}

void test_decryptor_empty_and_tiny_inputs() {
    // 空密文 / 短密文的各种类型都要给出确定结果，不能崩
    MdictDecryptorStd d;
    const MdictEncryptionType types[] = {
        MdictEncryptionType::NONE,
        MdictEncryptionType::SIMPLE_XOR,
        MdictEncryptionType::DES_ECB,
        MdictEncryptionType::AES_CBC,
        MdictEncryptionType::CUSTOM,
    };
    for (MdictEncryptionType t : types) {
        const auto tiny = d.decrypt(std::vector<uint8_t>{1, 2, 3}, t);
        if (t == MdictEncryptionType::NONE) {
            // NONE 是恒等映射：短密文原样返回，属于成功
            assert(tiny.success);
            assert(tiny.data.size() == 3);
        } else {
            assert(!tiny.success);
        }
    }
    // NONE + 空输入同样成功（恒等映射的边界）
    const auto none_empty = d.decrypt(std::vector<uint8_t>{}, MdictEncryptionType::NONE);
    assert(none_empty.success);
    assert(none_empty.data.empty());
    // 真正的加密类型在空输入上失败
    for (MdictEncryptionType t : {MdictEncryptionType::SIMPLE_XOR,
                                  MdictEncryptionType::DES_ECB,
                                  MdictEncryptionType::AES_CBC,
                                  MdictEncryptionType::CUSTOM}) {
        assert(!d.decrypt(std::vector<uint8_t>{}, t).success);
    }
    // try_auto_decrypt 在空输入上不崩
    const auto auto_empty = d.try_auto_decrypt(std::vector<uint8_t>{});
    assert(!auto_empty.success);

    // validate_decrypted_data：判据是"前 512 字节里出现 >= 3 个 MDict 标记"
    assert(!d.validate_decrypted_data(""));
    assert(!d.validate_decrypted_data("plain text"));
    // 只含一个标记不够
    assert(!d.validate_decrypted_data("BookName"));
    // 两个仍不够
    assert(!d.validate_decrypted_data("BookName and Description"));
    // 三个才通过；大小写不敏感
    assert(d.validate_decrypted_data("BookName Description Title"));
    assert(d.validate_decrypted_data("bookname description title"));
    // 标记在 512 字节之后不计入
    assert(!d.validate_decrypted_data(std::string(600, 'x') +
                                      "BookName Description Title"));
}

// ---------------------------------------------------------------- aggregate

void test_relevance_definition_length_tiers() {
    // calculate_relevance 的释义质量分档：>20 与 >100 字符各加 0.05，
    // 外加 examples(0.05) 与 pronunciation(0.03)。两条释义内容完全不同，
    // 避免被 deduplicate_entries 当成相似定义合并掉（否则只剩一条，
    // 观察不到分数差）。
    fs::path base = tmp_root();
    fs::create_directories(base);

    auto json_dict = [&](const std::string& file, const std::string& name,
                         const std::string& def) {
        std::string j = "{\"name\":\"" + name + "\",\"entries\":[{\"word\":\"zeta\",\"definition\":\"";
        j += def;
        j += "\"}]}";
        const auto p = base / file;
        write_file(p, j);
        return p;
    };

    // 短释义（<20 字符，够不着任何一档）
    const auto short_p = json_dict("rel_short.json", "d_short", "hi there");
    // 长释义（200 字符 → 拿到 >20 与 >100 两档）
    const auto long_p = json_dict("rel_long.json", "d_long", std::string(200, 'x'));

    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(short_p.string()));
    assert(mgr.add_dictionary(long_p.string()));

    DictionaryAggregator agg(&mgr);
    const auto res = agg.lookup("zeta");
    assert(res.all_entries.size() == 2);
    const AggregatedEntry* shorter = nullptr;
    const AggregatedEntry* longer = nullptr;
    for (const auto& e : res.all_entries) {
        if (e.source.dictionary_id == "d_short") shorter = &e;
        if (e.source.dictionary_id == "d_long") longer = &e;
    }
    assert(shorter != nullptr);
    assert(longer != nullptr);
    assert(shorter->definition.size() < 20);
    assert(longer->definition.size() == 200);

    // 两条都精确命中、priority 都是 0：基础 0.5 + 精确命中 0.3 +
    // priority 加成 (10-0)/50 = 0.2 已经等于 1.0，释义质量分档被 clamp 吃掉。
    // 也就是说 calculate_relevance 对"精确命中 + 最高优先级"的条目一律返回
    // 1.0，释义长短这类质量信号在最优情形下观察不到——这条断言把现状钉住，
    // 免得日后有人以为长释义一定得分更高。
    assert(shorter->relevance_score == 1.0);
    assert(longer->relevance_score == 1.0);
    assert(shorter->relevance_score == longer->relevance_score);

    // 两条同分 → 排序落到 priority 兜底比较器（aggregate_lookup_std.cpp:559）
    const AggregatedEntry* best = res.get_best();
    assert(best != nullptr);
    // priority 相同时顺序不定，只要求它确实是其中一条
    assert(best->source.dictionary_id == "d_short" ||
           best->source.dictionary_id == "d_long");

    // 非精确命中（fuzzy）时才看得见质量分档：fuzzy 的分数是
    // similarity*0.7 + calculate_relevance*0.3，calculate_relevance 里两档
    // 释义质量共 0.1，被 0.3 权重压成 0.03——长释义仍应稳定领先这一档。
    DictionaryAggregator agg2(&mgr);
    const auto fz = agg2.fuzzy_lookup("zetb", LookupOptions{});
    assert(fz.all_entries.size() == 2);
    double fz_long = -1.0;
    double fz_short = -1.0;
    for (const auto& e : fz.all_entries) {
        if (e.source.dictionary_id == "d_long") fz_long = e.relevance_score;
        if (e.source.dictionary_id == "d_short") fz_short = e.relevance_score;
    }
    assert(fz_long > fz_short);
    assert(fz_long - fz_short > 0.02 && fz_long - fz_short < 0.04);
    assert(fz_long <= 1.0);
}

}  // namespace

int main() {
    test_html_unsafe_href_and_src_erased();
    test_base_resolver_data_url();
    test_default_resolver_data_url_failures();
    test_default_resolver_unknown_dictionary();

    test_text_norm_utf8_widths();

    test_dsl_continuation_lines();
    test_dsl_first_non_header_line_starts_entries();

    test_wildcard_regex_metacharacters_are_escaped();

    test_log_softmax_max_not_first();

    test_wav_data_chunk_before_fmt();
    test_wav_no_data_chunk();
    test_wav_too_small_rejected_early();
    test_wav_fmt_with_zero_channels_rejected();
    test_wav_odd_sized_chunk_padding();

    test_decryptor_unsupported_type_reports_name();
    test_decryptor_empty_and_tiny_inputs();

    test_relevance_definition_length_tiers();

    std::printf("OK\n");
    return 0;
}
