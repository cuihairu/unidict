// HTML rendering engine unit tests (std-only).
// Focused on public API behavior (sanitization, rewriting, rendering flags).

#include <cassert>
#include <string>
#include <unordered_map>

#include "std/html_renderer_std.h"

using namespace UnidictCoreStd;

static void test_basic_sanitize() {
    HtmlRendererStd renderer;

    std::string safe = "<div><p>Hello world</p></div>";
    std::string result = renderer.sanitize(safe);
    assert(result.find("<div>") != std::string::npos);
    assert(result.find("<p>") != std::string::npos);

    std::string dangerous = "<div><script>alert('xss')</script><p>Hello</p></div>";
    result = renderer.sanitize(dangerous);
    assert(result.find("<script>") == std::string::npos);
    assert(result.find("<p>") != std::string::npos);

    std::string events = "<div onclick=\"bad()\" onmouseover=\"evil()\">Content</div>";
    result = renderer.sanitize(events);
    assert(result.find("onclick") == std::string::npos);
    assert(result.find("onmouseover") == std::string::npos);
    assert(result.find("Content") != std::string::npos);
}

static void test_url_sanitization_via_sanitize() {
    HtmlRendererStd renderer;

    std::string html = R"HTML(
        <div>
            <a href="http://example.com">ok</a>
            <a href="ftp://example.com/file">no</a>
            <a href="javascript:alert(1)">js</a>
            <a href="data:text/html,<script>alert(1)</script>">data</a>
        </div>
    )HTML";

    std::string out = renderer.sanitize(html);
    assert(out.find("http://example.com") != std::string::npos);
    assert(out.find("ftp://example.com/file") == std::string::npos);
    assert(out.find("javascript:") == std::string::npos);
    assert(out.find("data:text/html") == std::string::npos);
}

// 交叉引用协议必须在清洗后存活：entry://（白名单老成员）、bword://
//（真实 MDict 词典的链接）、unidict://（重写产物）。bword/unidict 不
// 进白名单的话，"清洗→重写"与"重写→清洗"两种管线次序都会丢链接
// （QML 端 aggregateLookup 曾因次序+白名单组合断掉全部交叉引用）
static void test_cross_reference_protocols_survive_sanitize() {
    HtmlRendererStd renderer;
    std::string html =
        "<a href=\"entry://hello\">a</a>"
        "<a href=\"bword://toast\">b</a>"
        "<a href=\"unidict://lookup?word=car\">c</a>";
    std::string out = renderer.sanitize(html);
    assert(out.find("href=\"entry://hello\"") != std::string::npos);
    assert(out.find("href=\"bword://toast\"") != std::string::npos);
    assert(out.find("href=\"unidict://lookup?word=car\"") != std::string::npos);
}

static void test_css_style_sanitization_via_sanitize() {
    HtmlRendererStd renderer;

    std::string html = R"(<p style="color: red; width: expression(alert(1));">Hello</p>)";
    std::string out = renderer.sanitize(html);
    assert(out.find("style=") != std::string::npos);
    assert(out.find("color:red") != std::string::npos);
    assert(out.find("expression") == std::string::npos);
}

static void test_strip_tags_and_extract_text() {
    HtmlRendererStd renderer;

    std::string html = "<div><p>Hello <strong>world</strong></p></div>";
    std::string text = renderer.strip_tags(html);
    assert(text.find("<div>") == std::string::npos);
    assert(text.find("Hello") != std::string::npos);
    assert(text.find("world") != std::string::npos);

    std::string extracted = renderer.extract_text("<p>Hello <em>there</em>!</p>");
    assert(extracted.find("Hello") != std::string::npos);
    assert(extracted.find("there") != std::string::npos);
}

static void test_link_rewriting() {
    HtmlRendererStd renderer;

    std::string html = R"(
        <div>
            <a href="entry://hello">hello</a>
            @@@LINK=world
            <a href="http://example.com">external</a>
        </div>
    )";

    std::string rewritten = renderer.rewrite_links(html, "test_dict");
    assert(rewritten.find("href=\"#lookup:hello\"") != std::string::npos);
    assert(rewritten.find("href=\"#lookup:world\"") != std::string::npos);
    assert(rewritten.find("http://example.com") != std::string::npos);
}

static void test_render_flags() {
    HtmlRendererStd renderer;

    std::string html = R"(
        <div>
            <img src="pic.png"/>
            <audio src="sound.mp3"/>
        </div>
    )";

    RenderedHtml rendered = renderer.render(html);
    assert(rendered.has_images);
    assert(rendered.has_audio);
    assert(!rendered.has_math);
}

static void test_whitelists() {
    HtmlRendererStd renderer;

    assert(renderer.is_tag_allowed("div"));
    assert(renderer.is_tag_allowed("p"));
    assert(!renderer.is_tag_allowed("script"));

    assert(renderer.is_attribute_allowed("class"));
    assert(renderer.is_attribute_allowed("href"));
    assert(!renderer.is_attribute_allowed("onclick"));

    assert(renderer.is_css_property_allowed("color"));
    assert(!renderer.is_css_property_allowed("behavior"));
}

static void test_custom_tag_configuration() {
    HtmlRendererStd renderer;
    renderer.add_allowed_tag("article");
    assert(renderer.is_tag_allowed("article"));
    renderer.remove_allowed_tag("div");
    assert(!renderer.is_tag_allowed("div"));
}

static void test_resource_url_rewriting() {
    HtmlRendererStd renderer;

    std::string html = R"(
        <div>
            <img src="pic.png"/>
            <img src="images/photo.jpg"/>
            <audio src="sound.mp3"/>
        </div>
    )";

    std::unordered_map<std::string, std::string> url_map = {
        {"pic.png", "file:///cache/pic_abc123.png"},
        {"images/photo.jpg", "file:///cache/photo_def456.jpg"},
        {"sound.mp3", "file:///cache/sound_789.mp3"}
    };

    std::string rewritten = renderer.rewrite_resource_urls(html, url_map);
    assert(rewritten.find("file:///cache/pic_abc123.png") != std::string::npos);
    assert(rewritten.find("file:///cache/photo_def456.jpg") != std::string::npos);
    assert(rewritten.find("file:///cache/sound_789.mp3") != std::string::npos);
}

static void test_custom_link_resolver() {
    HtmlRendererStd renderer;

    renderer.set_link_resolver([](const std::string& word, const std::string& dict_id) {
        return "myapp://lookup/" + dict_id + "/" + word;
    });

    assert(renderer.resolve_cross_reference("entry://test", "mydict") == "myapp://lookup/mydict/test");

    std::string rewritten = renderer.rewrite_links(R"(<a href="entry://test">link</a>)", "ignored");
    assert(rewritten.find("myapp://lookup//test") != std::string::npos); // rewrite_links passes empty dict_id
}

int main() {
    test_basic_sanitize();
    test_url_sanitization_via_sanitize();
    test_css_style_sanitization_via_sanitize();
    test_cross_reference_protocols_survive_sanitize();
    test_strip_tags_and_extract_text();
    test_link_rewriting();
    test_render_flags();
    test_whitelists();
    test_custom_tag_configuration();
    test_resource_url_rewriting();
    test_custom_link_resolver();
    return 0;
}
