// render() 护栏回归：max_text_length_ 与 max_nesting_depth_ 此前是
// "声明了但 render() 从没读过"的成员——一个自称 100KB 上限的安全护栏
// 实际不存在，超大/畸形词条可以在渲染器里无上限膨胀。这份测试把两条
// 护栏钉成可观测行为：
//   1. text 输出有字节上限，超限置 truncated 并补省略标记；
//   2. 截断点对齐 UTF-8 边界，不切出半个码点（否则界面显示 U+FFFD 方块）；
//   3. 嵌套深度超限就丢弃该开标签，void 元素不计入深度；
//   4. 未撞护栏时行为与从前完全一致（truncated=false，无省略号）。

#include <cassert>
#include <cstdio>
#include <string>

#include "std/html_renderer_std.h"

using UnidictCoreStd::HtmlRendererStd;
using UnidictCoreStd::RenderedHtml;

namespace {

// 校验 s 是不是合法 UTF-8。被截断的字符串里如果切进了码点中间，
// 就会留下落单的续字节(0b10xxxxxx)或超范围的首字节——下游
// QString::fromStdString 会把它们变成 U+FFFD 方块，界面上就是乱码。
// 断言"整体合法"比断言"末字节不是续字节"准确：完整的多字节字符本来
// 就以续字节结尾。
bool is_valid_utf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = (unsigned char)s[i];
        int extra;
        unsigned int cp;
        if (c < 0x80) { ++i; continue; }
        else if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1Fu; }
        else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0Fu; }
        else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07u; }
        else return false;  // 10xxxxxx 落单续字节，或 11111000+ 非法
        if (i + (size_t)extra >= s.size()) return false;  // 续字节被截没了
        for (int k = 1; k <= extra; ++k) {
            const unsigned char cc = (unsigned char)s[i + (size_t)k];
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        // 过长编码 / 代理区 / 超范围
        if (extra == 1 && cp < 0x80) return false;
        if (extra == 2 && cp < 0x800) return false;
        if (extra == 3 && cp < 0x10000) return false;
        if (cp > 0x10FFFF) return false;
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        i += (size_t)extra + 1;
    }
    return true;
}

void test_default_limits_are_readable() {
    HtmlRendererStd r;
    // 默认值与头文件承诺一致：100KB 文本 / 32 层嵌套
    assert(r.max_text_length() == 100000);
    assert(r.max_nesting_depth() == 32);
}

void test_text_under_limit_not_truncated() {
    HtmlRendererStd r;
    const auto out = r.render("<p>hello world</p>");
    assert(!out.truncated);
    assert(out.text == "hello world");
}

void test_text_capped_at_limit() {
    HtmlRendererStd r;
    r.set_max_text_length(16);

    // 单个 TEXT token 就超限：切到 16 字节 + 省略标记
    const auto out = r.render("<p>aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa</p>");
    assert(out.truncated);
    assert(out.text == "aaaaaaaaaaaaaaaa...");
    assert(out.text.size() == 16 + 3);
    // html 侧不受 text 上限影响（结构完整）
    assert(out.html == "<p>aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa</p>");
}

void test_text_capped_across_multiple_tokens() {
    HtmlRendererStd r;
    r.set_max_text_length(10);

    // 多个 TEXT token 累计超限：前几个收满后停止累积
    const auto out = r.render("<p>abcdef</p><p>ghijkl</p><p>mnopqr</p>");
    assert(out.truncated);
    assert(out.text == "abcdefghij...");
    assert(out.text.size() == 10 + 3);
}

void test_text_exactly_at_limit_not_truncated() {
    HtmlRendererStd r;
    r.set_max_text_length(5);
    // 正好等于上限：一字不多给，也不算截断
    const auto out = r.render("<p>abcde</p>");
    assert(!out.truncated);
    assert(out.text == "abcde");
}

void test_zero_limit_yields_ellipsis_only() {
    HtmlRendererStd r;
    r.set_max_text_length(0);
    // 上限 0：一个字节都不收，只留省略标记
    const auto out = r.render("<p>anything at all</p>");
    assert(out.truncated);
    assert(out.text == "...");
}

void test_utf8_boundary_not_split() {
    HtmlRendererStd r;
    // "漢"=E6BCA2(3B) "字"=E5AD97(3B)。上限 4 会落在"字"的中间，
    // 正确行为是只留"漢"（3 字节），不能留半个字。
    r.set_max_text_length(4);
    const auto out = r.render("<p>\xE6\xBC\xA2\xE5\xAD\x97\xE6\xB1\x89</p>");
    assert(out.truncated);
    assert(is_valid_utf8(out.text));
    // "漢"完整保留，"字"被整块丢弃
    assert(out.text == "\xE6\xBC\xA2...");
    assert(out.text.size() == 3 + 3);

    // 上限 6 = 两个完整码点（漢+字）：都在，不截断
    HtmlRendererStd r2;
    r2.set_max_text_length(6);
    const auto out2 = r2.render("<p>\xE6\xBC\xA2\xE5\xAD\x97</p>");
    assert(!out2.truncated);
    assert(is_valid_utf8(out2.text));
    assert(out2.text == "\xE6\xBC\xA2\xE5\xAD\x97");

    // 上限 9 = 三个完整码点：都在，不截断
    HtmlRendererStd r3;
    r3.set_max_text_length(9);
    const auto out3 = r3.render("<p>\xE6\xBC\xA2\xE5\xAD\x97\xE6\xB1\x89</p>");
    assert(!out3.truncated);
    assert(out3.text == "\xE6\xBC\xA2\xE5\xAD\x97\xE6\xB1\x89");

    // 上限 1..8 逐个扫：无论切在哪，输出都必须是合法 UTF-8
    for (size_t cap = 0; cap <= 10; ++cap) {
        HtmlRendererStd rr;
        rr.set_max_text_length(cap);
        const auto o = rr.render("<p>\xE6\xBC\xA2\xE5\xAD\x97\xE6\xB1\x89</p>");
        assert(is_valid_utf8(o.text));
    }
}

void test_nesting_depth_capped() {
    HtmlRendererStd r;
    r.set_max_nesting_depth(3);

    // 6 层 <div>：第 4 层起的开标签被丢弃
    std::string deep;
    for (int i = 0; i < 6; ++i) deep += "<div>";
    for (int i = 0; i < 6; ++i) deep += "</div>";

    const auto out = r.render(deep);
    assert(out.truncated);
    // 前 3 层保留，第 4 个开标签没了
    size_t opens = 0;
    for (size_t i = 0; i < out.html.size(); ++i) {
        if (out.html.compare(i, 5, "<div>") == 0) ++opens;
    }
    assert(opens == 3);
}

void test_nesting_within_limit_not_truncated() {
    HtmlRendererStd r;
    r.set_max_nesting_depth(4);
    std::string ok;
    for (int i = 0; i < 4; ++i) ok += "<div>";
    for (int i = 0; i < 4; ++i) ok += "</div>";

    const auto out = r.render(ok);
    assert(!out.truncated);
    size_t opens = 0;
    for (size_t i = 0; i < out.html.size(); ++i) {
        if (out.html.compare(i, 5, "<div>") == 0) ++opens;
    }
    assert(opens == 4);
}

void test_void_elements_do_not_count_depth() {
    HtmlRendererStd r;
    r.set_max_nesting_depth(2);

    // 一层 div 里塞 50 个 <br/>：void 元素不该把深度顶上去
    std::string s = "<div>";
    for (int i = 0; i < 50; ++i) s += "<br/>";
    s += "</div>";

    const auto out = r.render(s);
    assert(!out.truncated);
    assert(out.html.find("<br />") != std::string::npos);
}

void test_zero_depth_limit_drops_all_starts() {
    HtmlRendererStd r;
    r.set_max_nesting_depth(0);
    // 深度上限 0：任何非 void 开标签都被丢
    const auto out = r.render("<div><p>text</p></div>");
    assert(out.truncated);
    assert(out.html.find("<div>") == std::string::npos);
    assert(out.html.find("<p>") == std::string::npos);
    // 文本不受深度影响
    assert(out.text == "text");
}

void test_sanitize_and_strip_tags_respect_text_limit() {
    HtmlRendererStd r;
    r.set_max_text_length(4);
    // sanitize() 走 render().html，不受 text 上限影响
    assert(r.sanitize("<p>abcdefghij</p>") == "<p>abcdefghij</p>");
    // strip_tags() 独立于 render()，text 无限额——这是既有语义，
    // 这里只是钉住"护栏只加在 render() 上，没有意外改变 strip_tags()"
    assert(r.strip_tags("<p>abcdefghij</p>") == "abcdefghij");
}

void test_media_flags_survive_truncation() {
    // 护栏不该把 <img>/<audio> 检测顺手关掉
    HtmlRendererStd r;
    r.set_max_text_length(2);
    const auto out = r.render("<p>abcdefgh</p><img src=\"x.png\"/><audio src=\"a.mp3\"></audio>");
    assert(out.truncated);
    assert(out.has_images);
    assert(out.has_audio);
}

}  // namespace

int main() {
    test_default_limits_are_readable();
    test_text_under_limit_not_truncated();
    test_text_capped_at_limit();
    test_text_capped_across_multiple_tokens();
    test_text_exactly_at_limit_not_truncated();
    test_zero_limit_yields_ellipsis_only();
    test_utf8_boundary_not_split();
    test_nesting_depth_capped();
    test_nesting_within_limit_not_truncated();
    test_void_elements_do_not_count_depth();
    test_zero_depth_limit_drops_all_starts();
    test_sanitize_and_strip_tags_respect_text_limit();
    test_media_flags_survive_truncation();
    std::printf("OK\n");
    return 0;
}
