// online_pron_std 单测：URL 拼装与转义 / 容忍式解析（口音推断、
// 去重、空 audio 过滤、截断与 malformed 不崩）/ pick_clip 偏好链。
// 风格：assert + main（std-only，无 Qt）。

#include <cassert>
#include <string>
#include <vector>
#include "std/online_pron_std.h"

using namespace UnidictCoreStd;

namespace {

// dictionaryapi.dev 真实响应形状的代表性 fixture：phonetics 混排
// 无标注口音的 gstatic 旧链（_gb_ 文件名段）与带 -us 段的新链，
// 另有一条空 audio 与一条重复 URL
const char kFixture[] = R"([
  {"word":"hello","phonetic":"həˈləʊ",
   "phonetics":[
     {"text":"həˈləʊ","audio":"//ssl.gstatic.com/dictionary/static/sounds/20200429/hello--_gb_1.mp3"},
     {"text":"hɛˈloʊ","audio":"https://api.dictionaryapi.dev/media/pronunciations/en/hello-us.mp3"},
     {"text":"həˈləʊ","audio":""},
     {"text":"","audio":"https://api.dictionaryapi.dev/media/pronunciations/en/hello-us.mp3"}
   ],
   "meanings":[{"partOfSpeech":"noun","definitions":[{"definition":"utterance of \"hello\"","example":"hello world"}]}]
  }
])";

int clips_with_accent(const std::vector<PronClip>& clips, PronAccent a) {
    int n = 0;
    for (const PronClip& c : clips) {
        if (c.accent == a) ++n;
    }
    return n;
}

void test_request_url() {
    FreeDictionarySource src;
    assert(std::string(src.name()) == "dictionaryapi.dev");

    assert(src.request_url("hello") ==
           "https://api.dictionaryapi.dev/api/v2/entries/en/hello");
    // 空查询不发请求
    assert(src.request_url("").empty());
}

void test_escape() {
    assert(url_path_escape("hello") == "hello");
    // 空格 → %20（path 域，不做 + 号）
    assert(url_path_escape("black smith") == "black%20smith");
    // 撇号（API 路径里真实会出现的形状）
    assert(url_path_escape("don't") == "don%27t");
    // 非 ASCII 逐字节百分号转义（é = C3 A9）
    assert(url_path_escape("\xc3\xa9") == "%C3%A9");
    // unreserved 保留
    assert(url_path_escape("a-b_c.d~e") == "a-b_c.d~e");
    // 路径分隔符必须转义（防查询词携带路径穿越形状）
    assert(url_path_escape("a/b") == "a%2Fb");
}

void test_parse_fixture() {
    std::vector<PronClip> clips = parse_free_dictionary(kFixture);
    // 空 audio 过滤 + 重复 URL 去重 → 2 条
    assert(clips.size() == 2);
    // gstatic 旧链的 _gb_ 文件名段 → UK
    assert(clips[0].accent == PronAccent::UK);
    assert(clips[0].label == "həˈləʊ");
    // -us 段 → US
    assert(clips[1].accent == PronAccent::US);
    // 转义解码：definition 里的 \"hello\" 不影响扫描（不属于我们的键）
    assert(clips[0].url.find("gstatic.com") != std::string::npos);
}

void test_accent_inference() {
    assert(accent_from_url("https://x/en/hello-us.mp3") == PronAccent::US);
    assert(accent_from_url("https://x/en/hello_us.mp3") == PronAccent::US);
    assert(accent_from_url("//ssl.gstatic.com/sounds/hello--_gb_1.mp3") ==
           PronAccent::UK);
    assert(accent_from_url("https://x/en/hello-uk.mp3") == PronAccent::UK);
    assert(accent_from_url("https://x/en/hello_au.mp3") == PronAccent::AU);
    // 路径中段的 us（/usr/）不算——只看文件名段
    assert(accent_from_url("https://x/usr/share/a.mp3") == PronAccent::Unknown);
    assert(accent_from_url("https://x/en/hello.mp3") == PronAccent::Unknown);

    assert(accent_from_label("us") == PronAccent::US);
    assert(accent_from_label("US") == PronAccent::US);
    assert(accent_from_label("American") == PronAccent::US);
    assert(accent_from_label("uk") == PronAccent::UK);
    assert(accent_from_label("British") == PronAccent::UK);
    assert(accent_from_label("Australian") == PronAccent::AU);
    assert(accent_from_label("") == PronAccent::Unknown);
    // text 音标不是口音标注，不误判
    assert(accent_from_label("həˈləʊ") == PronAccent::Unknown);
}

void test_parse_tolerant() {
    // 截断：闭括号缺失，已完整读到的条目照常产出
    std::string truncated =
        R"([{"phonetics":[{"text":"x","audio":"https://a/hello-us.mp3"},{"te)";
    std::vector<PronClip> clips = parse_free_dictionary(truncated);
    assert(clips.size() == 1);
    assert(clips[0].accent == PronAccent::US);

    // malformed 各形态：不崩、最多空表
    assert(parse_free_dictionary("").empty());
    assert(parse_free_dictionary("not json at all").empty());
    assert(parse_free_dictionary("{\"audio\":\"https://a/x-us.mp3\"}").size() == 1);
    assert(parse_free_dictionary("[[[{\"audio\":").empty());
    // 字符串里混括号/引号不把扫描器带偏
    std::vector<PronClip> tricky = parse_free_dictionary(
        R"({"note":"braces } \" inside","phonetics":[{"audio":"https://a/w-uk.mp3"}]})");
    assert(tricky.size() == 1);
    assert(tricky[0].accent == PronAccent::UK);
    // \uXXXX 转义的 text 标注解码（\u0160 → UTF-8 C5 A0）
    std::vector<PronClip> escaped = parse_free_dictionary(
        R"([{"phonetics":[{"text":"\u0160","audio":"https://a/y.mp3"}]}])");
    assert(escaped.size() == 1);
    assert(escaped[0].label == "\xc5\xa0");
    // 孤立代理不崩、静默丢弃
    std::vector<PronClip> lone = parse_free_dictionary(
        R"([{"phonetics":[{"text":"\ud800x","audio":"https://a/z.mp3"}]}])");
    assert(lone.size() == 1);
    assert(lone[0].label == "x");
}

void test_pick_clip() {
    std::vector<PronClip> clips = {
        {"u1", PronAccent::UK, "uk"},
        {"a1", PronAccent::AU, "au"},
        {"n1", PronAccent::Unknown, ""},
    };
    // 偏好命中优先
    assert(pick_clip(clips, PronAccent::UK)->url == "u1");
    assert(pick_clip(clips, PronAccent::AU)->url == "a1");
    // 偏好未命中（无 US）：默认次序 UK → AU
    assert(pick_clip(clips, PronAccent::US)->url == "u1");
    // 无偏好：US → UK → AU → Unknown
    assert(pick_clip(clips, PronAccent::Unknown)->url == "u1");
    // Unknown 也能兜到
    std::vector<PronClip> only_unknown = {{"n1", PronAccent::Unknown, ""}};
    assert(pick_clip(only_unknown, PronAccent::US)->url == "n1");
    assert(pick_clip(only_unknown, PronAccent::Unknown)->url == "n1");
    // 空表
    assert(pick_clip({}, PronAccent::US) == nullptr);
    // 只有 AU 也能被 US 偏好兜到
    std::vector<PronClip> only_au = {{"a1", PronAccent::AU, ""}};
    assert(pick_clip(only_au, PronAccent::US)->url == "a1");
}

void test_source_interface() {
    // 接口多态口径：经基类调用与直调一致
    FreeDictionarySource src;
    PronunciationSourceStd& base = src;
    assert(base.request_url("hi") ==
           "https://api.dictionaryapi.dev/api/v2/entries/en/hi");
    std::vector<PronClip> clips = base.parse_response(kFixture);
    assert(clips.size() == 2);
    assert(clips_with_accent(clips, PronAccent::US) == 1);
    assert(clips_with_accent(clips, PronAccent::UK) == 1);
}

}  // namespace

int main() {
    test_request_url();
    test_escape();
    test_parse_fixture();
    test_accent_inference();
    test_parse_tolerant();
    test_pick_clip();
    test_source_interface();
    return 0;
}
