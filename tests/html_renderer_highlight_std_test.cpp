// HtmlRendererStd::highlight_term 文本节点级命中标注单测（P-4 gap P1）：
// 标签/属性明文不动、文本段内大小写不敏感字面匹配、命中片段包
// <span class="udict-hl">（保留原文大小写）、正则元字符按字面处理，
// 空 term/空 html/无命中原样返回。

#include <cassert>
#include <string>

#include "std/html_renderer_std.h"

using namespace UnidictCoreStd;

int main() {
    HtmlRendererStd r;

    // T1 文本段命中：大小写不敏感 + 原文大小写保留
    assert(r.highlight_term("<p>Hello world</p>", "hello") ==
           "<p><span class=\"udict-hl\">Hello</span> world</p>");

    // T2 标签与属性明文不动
    assert(r.highlight_term("<a href=\"hello\">hi</a>", "hello") ==
           "<a href=\"hello\">hi</a>");

    // T3 多段命中（文本段各自处理）
    assert(r.highlight_term("<p>a<b>hello</b>hello</p>", "hello") ==
           "<p>a<b><span class=\"udict-hl\">hello</span></b>"
           "<span class=\"udict-hl\">hello</span></p>");

    // T4 正则元字符按字面匹配（c++ / 括号）
    assert(r.highlight_term("c++ (x) set", "c++") ==
           "<span class=\"udict-hl\">c++</span> (x) set");
    assert(r.highlight_term("c++ (x) set", "(x)") ==
           "c++ <span class=\"udict-hl\">(x)</span> set");

    // T5 空 term / 空 html 原样返回
    assert(r.highlight_term("<p>x</p>", "") == "<p>x</p>");
    assert(r.highlight_term("", "x") == "");

    // T6 乱刺穿的 < 无配套 >：其后整体按文本段处理（仍可命中）
    assert(r.highlight_term("<p>abc", "abc") ==
           "<p><span class=\"udict-hl\">abc</span>");
    // T6b 全程无 >：从 < 起整体按文本段处理
    assert(r.highlight_term("2 < 3 says x", "x") ==
           "2 < 3 says <span class=\"udict-hl\">x</span>");

    // T7 term 大写匹配小写文本
    assert(r.highlight_term("say hello", "HELLO") ==
           "say <span class=\"udict-hl\">hello</span>");

    // T8 无命中原样返回
    assert(r.highlight_term("<p>nothing</p>", "xyz") == "<p>nothing</p>");

    // T9 词跨标签边界：分属两个文本段，不跨段匹配——"hello" 拆成
    // "he"/"llo" 后不构成任何文本段内的命中，输出原样
    assert(r.highlight_term("<em>he</em>llo", "hello") == "<em>he</em>llo");

    // T10 非重叠多次命中：正则从 "aaa" 消耗前两个 a，尾随单 a 无第二次命中
    assert(r.highlight_term("a-aa-aaa", "aa") ==
           "a-<span class=\"udict-hl\">aa</span>-<span class=\"udict-hl\">aa</span>a");

    return 0;
}