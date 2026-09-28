// html_renderer_std 分支缺口补测（真实缺边 57 条，第三大簇）。
//
// 缺边构成：utf8_safe_cut 回退到头的兜底臂、分词器畸形阶梯（未闭合
// 注释/无名标签/无'>'标签、注释头半臂、空属性名、布尔属性、无值'='、
// 引号在'>'后、未闭合引号、非引号值）、render 选项假臂（resolve_links
// 关）、ELEMENT_START 的 video 臂、带 resolver 的资源 URL 重写链、
// CSS 过滤的空段/无冒号段/单属性重建臂、DefaultResourceResolver 的
// 同名目录资源守卫两态、扩展名剥离循环的长度/不匹配臂、normalize_key
// 的全斜杠臂。全部真实输入驱动。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "std/html_renderer_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path base_dir() {
    fs::path d = fs::current_path() / "build-local" / "hrbr";
    fs::create_directories(d);
    return d;
}

// T1 截断护栏：单 TEXT token 内回退满 4 次仍全是续字节 →
// utf8_safe_cut 的 back<4 假臂（cut 走到 0），take=0 全丢
static void test_utf8_cut_exhaustion() {
    HtmlRendererStd r;
    r.set_max_text_length(4);
    // 6 字节：s[4..1] 连续 4 个续字节耗尽回退预算（非法 UTF-8 是真实
    // 输入面——词典文件损坏）
    RenderedHtml out = r.render("\x80\x80\x80\x80\x80x");
    assert(out.text == "...");   // take=0 全丢，只剩余标记
    assert(out.truncated);
}

// T2 分词器畸形输入矩阵
static void test_tokenizer_malformed() {
    HtmlRendererStd r;
    // 未闭合注释：find("-->") npos → break，注释后的内容丢弃
    {
        RenderedHtml out = r.render("keep<!--never closed");
        assert(out.html == "keep");
        assert(out.text == "keep");
    }
    // 标签名无终结符（EOF 前无空格/>）→ break
    {
        RenderedHtml out = r.render("keep<div");
        assert(out.html == "keep");
    }
    // 属性区无 '>' → break
    {
        RenderedHtml out = r.render("keep<div class");
        assert(out.html == "keep");
    }
    // 注释探测四半臂：pos+4<len 假 / '!' 后非 '-' / 第二 '-' 后非 '-' /
    // 第三 '-' 后非 '-'。前三者落到标签解析并因无名标签 break
    {
        assert(r.render("a<!x").html == "a");     // len 4：pos+4 不小于 len
        assert(r.render("a<!xyz").html == "a");   // [2] 非 '-'（len≥6 才会评到）
        assert(r.render("a<!-xy").html == "a");   // [3] 非 '-'
        assert(r.render("a<!--x").html == "a");   // [1][2][3] 全真：未闭合注释
    }
    // 空属性名（'=' 直接跟在空格后）
    {
        RenderedHtml out = r.render("<div =v>body</div>");
        assert(out.html == "<div>body</div>");
    }
    // 布尔属性：名字后直接 '>'（属性循环的 close 边）
    {
        RenderedHtml out = r.render("<div a>body</div>");
        assert(out.html == "<div>body</div>");
    }
    // 无值属性后跟另一属性：'=' 判定的假臂
    {
        RenderedHtml out = r.render("<p a b=2>body</p>");
        assert(out.html == "<p>body</p>");
    }
    // '=' 后只有空白和 '>'：引号判定的 close 边
    {
        RenderedHtml out = r.render("<p a= >body</p>");
        assert(out.html == "<p>body</p>");
    }
    // 非引号值（unquoted 臂）
    {
        RenderedHtml out = r.render("<p a=1>body</p>");
        assert(out.html == "<p>body</p>");
    }
    // 属性名直冲 '>'：name_end >= close_pos 的 break 臂
    {
        RenderedHtml out = r.render("<p attr>body</p>");
        assert(out.html == "<p>body</p>");
    }
    // 值后空格直达 '>'：skip-whitespace 的 close 退出 + 空属性名 break 臂
    {
        RenderedHtml out = r.render("<p a=1 >body</p>");
        assert(out.html == "<p>body</p>");
    }
    // 属性名后空格直达 '>'（无 '='）：skip-to-value 的 close 退出 +
    // '==' 判定的 close 边
    {
        RenderedHtml out = r.render("<p a     >body</p>");
        assert(out.html == "<p>body</p>");
    }
    // 闭引号在 '>' 之后：value_end < close_pos 的假臂。attr 解析
    // 已消费真正的 '>'，剩余的 '">body' 落为转义文本
    {
        RenderedHtml out = r.render("<div a=\">\">body</div>");
        assert(out.html == "<div>&quot;&gt;body</div>");
    }
    // 引号不闭合：value_end == npos 的假臂
    {
        RenderedHtml out = r.render("<p a=\"x>body</p>");
        assert(!out.html.empty());
    }
}

// T3 render 选项与媒体探测
static void test_render_options_and_media() {
    HtmlRendererStd r;
    // resolve_links=false：linked_words 不收集（251 假臂）
    HtmlRenderOptions opts;
    opts.resolve_links = false;
    RenderedHtml off = r.render("<a href=\"entry://foo\">x</a>", opts);
    assert(off.linked_words.empty());
    // ELEMENT_START 的 video 臂（audio 已有覆盖，video 走开标签）
    RenderedHtml vid = r.render("<video controls=\"controls\"></video>", opts);
    assert(vid.has_audio);
    RenderedHtml aud = r.render("<audio src=\"a.mp3\"></audio>", opts);
    assert(aud.has_audio);
    // 带 resolver + 默认 resolve_links：rewrite_resource_urls 经 render 触发
    auto renderer = HtmlRendererFactory::create_with_defaults();
    RenderedHtml with_res = renderer->render("<img src=\"pic.png\">");
    assert(with_res.html.find("pic.png") != std::string::npos);
    // resolver 非空 ∧ resolve_links=false：第二条件的假臂
    {
        auto r2 = HtmlRendererFactory::create_with_defaults();
        RenderedHtml off2 = r2->render("<a href=\"entry://foo\">x</a>", opts);
        assert(off2.linked_words.empty());
    }
    // rewrite 链里非词典资源 URL：is_dictionary_resource 假臂（原样保留）
    {
        RenderedHtml other = renderer->render("<img src=\"entry://pic\">");
        assert(other.html.find("entry://pic") != std::string::npos);
    }
}

// T4 CSS 过滤：空段、无冒号段、单属性重建、危险值与非白名单属性
// （sanitize_css_style 私有，经 render 的 style 属性全链路驱动）
static void test_css_filtering() {
    HtmlRendererStd r;
    // 连续分号产生空段：start==npos 的 continue 臂
    {
        RenderedHtml out = r.render("<p style=\"color:red;;width:10px\">b</p>");
        assert(out.html.find("style=\"color:red;width:10px\"") != std::string::npos);
    }
    // 无冒号段：直接丢弃
    {
        RenderedHtml out = r.render("<p style=\"color:red;junk;width:1px\">b</p>");
        assert(out.html.find("style=\"color:red;width:1px\"") != std::string::npos);
    }
    // 单属性：重建循环 i>0 假臂
    {
        RenderedHtml out = r.render("<p style=\"color:red\">b</p>");
        assert(out.html.find("style=\"color:red\"") != std::string::npos);
    }
    // 危险值与非白名单属性全滤光 → style 清空，sanitize_attribute 返回假
    {
        RenderedHtml out = r.render(
            "<p style=\"width:expression(x);behavior:url(y)\">b</p>");
        assert(out.html == "<p>b</p>");
    }
}

// T5 资源解析器：同名目录守卫、资源缺失、扩展名剥离臂、全斜杠 key
static void test_resource_resolver_edges() {
    fs::path res = base_dir() / "res";
    fs::create_directories(res);
    {
        std::ofstream f(res / "img.png", std::ios::binary | std::ios::trunc);
        f << "PNGDATA";
    }
    // 与资源同名的目录：find_resource_file 能"打开"，resolve 必须拒收
    fs::create_directories(res / "dir.png");

    DefaultResourceResolverStd resolver;
    resolver.register_dictionary("d1", res.string());

    // 正常命中：mime/size/base64 往返
    auto info = resolver.resolve("img.png", "d1");
    assert(!info.local_path.empty());
    assert(info.mime_type == "image/png");
    assert(info.size == 7);
    std::string data_url = resolver.get_data_url("img.png", "d1");
    assert(data_url.find("data:image/png;base64,") == 0);
    assert(resolver.exists("img.png", "d1"));
    // 同名目录：is_regular_file 假臂 → resource_path 清空
    auto dir_info = resolver.resolve("dir.png", "d1");
    assert(dir_info.local_path.empty());
    assert(!resolver.exists("dir.png", "d1"));
    // 缺失资源：find_resource_file 全臂未中 → 空路径短路
    assert(resolver.resolve("missing.png", "d1").local_path.empty());
    // 扩展名剥离循环：非图片扩展（==假臂）
    assert(resolver.resolve("zz.mp4", "d1").local_path.empty());
    // 扩展名剥离循环：长度不增（.png 恰好 4 字节，length>strlen 假臂）
    assert(resolver.resolve(".png", "d1").local_path.empty());
    // 全斜杠 key：normalize_key 的 find_first_not_of 假臂
    assert(resolver.resolve("///", "d1").local_path.empty());
    // 带协议的非 entry 资源：extract_resource_key 剥协议臂（正向命中）
    fs::create_directories(res / "x");
    {
        std::ofstream f(res / "x" / "y.png", std::ios::binary | std::ios::trunc);
        f << "SUB";
    }
    {
        auto hit = resolver.resolve("https://x/y.png", "d1");
        assert(hit.local_path.find("x/y.png") != std::string::npos);
    }
    // entry:// 永远不是资源
    assert(!resolver.is_dictionary_resource("entry://w"));
    assert(resolver.is_dictionary_resource("plain.png"));
}

// T6 工厂：strict 剔除脚本类标签、permissive 放行语义标签
static void test_factories() {
    auto strict = HtmlRendererFactory::create_strict();
    assert(strict->sanitize("<script>x</script>ok").find("script") == std::string::npos);
    auto permissive = HtmlRendererFactory::create_permissive();
    assert(permissive->sanitize("<details><summary>s</summary>d</details>")
               .find("details") != std::string::npos);
}

int main() {
    test_utf8_cut_exhaustion();
    test_tokenizer_malformed();
    test_render_options_and_media();
    test_css_filtering();
    test_resource_resolver_edges();
    test_factories();
    return 0;
}
