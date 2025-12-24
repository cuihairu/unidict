// HTML rendering engine unit tests (std-only).
// Tests for sanitization, link rewriting, and resource URL handling.

#include <cassert>
#include <string>
#include <vector>
#include "std/html_renderer_std.h"

using namespace UnidictCoreStd;

void test_basic_sanitize() {
    HtmlRendererStd renderer;

    // Safe HTML should pass through
    std::string safe = "<div><p>Hello world</p></div>";
    std::string result = renderer.sanitize(safe);
    assert(result.find("<div>") != std::string::npos);
    assert(result.find("<p>") != std::string::npos);

    // Dangerous scripts should be removed
    std::string dangerous = "<div><script>alert('xss')</script><p>Hello</p></div>";
    result = renderer.sanitize(dangerous);
    assert(result.find("<script>") == std::string::npos);
    assert(result.find("alert") == std::string::npos);
    assert(result.find("<p>") != std::string::npos);  // safe tag remains

    // Event handlers should be stripped
    std::string events = "<div onclick=\"bad()\" onmouseover=\"evil()\">Content</div>";
    result = renderer.sanitize(events);
    assert(result.find("onclick") == std::string::npos);
    assert(result.find("onmouseover") == std::string::npos);
}

void test_strip_tags() {
    HtmlRendererStd renderer;

    std::string html = "<div><p>Hello <strong>world</strong></p></div>";
    std::string text = renderer.strip_tags(html);
    assert(text.find("<div>") == std::string::npos);
    assert(text.find("<p>") == std::string::npos);
    assert(text.find("Hello") != std::string::npos);
    assert(text.find("world") != std::string::npos);
}

void test_extract_text() {
    HtmlRendererStd renderer;

    std::string html = "<div><p>Hello <em>world</em>!</p><ul><li>item1</li><li>item2</li></ul></div>";
    std::string text = renderer.extract_text(html);
    assert(text.find("Hello") != std::string::npos);
    assert(text.find("world") != std::string::npos);
    assert(text.find("item1") != std::string::npos);
    assert(text.find("item2") != std::string::npos);
}

void test_link_rewriting() {
    HtmlRendererStd renderer;

    std::string html = R"(
        <div>
            <a href="entry://hello">hello</a>
            <a href="bword://world">world</a>
            <a href="http://example.com">external</a>
        </div>
    )";

    std::string rewritten = renderer.rewrite_links(html, "test_dict");
    assert(rewritten.find("unidict://lookup?word=hello") != std::string::npos);
    assert(rewritten.find("unidict://lookup?word=world") != std::string::npos);
    assert(rewritten.find("http://example.com") != std::string::npos);  // external preserved
}

void test_cross_reference_detection() {
    HtmlRendererStd renderer;

    // entry:// protocol
    assert(renderer.is_cross_reference_link("entry://hello"));
    assert(renderer.is_cross_reference_link("entry://hello_world"));

    // bword:// protocol
    assert(renderer.is_cross_reference_link("bword://test"));

    // Not cross-reference
    assert(!renderer.is_cross_reference_link("http://example.com"));
    assert(!renderer.is_cross_reference_link("https://example.com"));
    assert(!renderer.is_cross_reference_link("/relative/path"));
}

void test_safe_url_validation() {
    HtmlRendererStd renderer;

    // Safe URLs
    assert(renderer.is_safe_url("http://example.com"));
    assert(renderer.is_safe_url("https://example.com"));
    assert(renderer.is_safe_url("/relative/path"));
    assert(renderer.is_safe_url("image.png"));

    // Dangerous javascript: URLs
    assert(!renderer.is_safe_url("javascript:alert('xss')"));
    assert(!renderer.is_safe_url("JAVASCRIPT:alert('xss')"));
    assert(!renderer.is_safe_url("javascript:void(0)"));

    // Data URLs (block for security)
    assert(!renderer.is_safe_url("data:text/html,<script>alert(1)</script>"));
}

void test_css_sanitization() {
    HtmlRendererStd renderer;

    // Safe CSS
    std::string safe_css = "color: red; font-size: 14px; margin: 10px;";
    std::string result;
    assert(renderer.sanitize_css_style(safe_css));

    // Dangerous CSS expressions
    std::string dangerous = "width: expression(alert(1));";
    assert(!renderer.sanitize_css_style(dangerous));

    // JavaScript in CSS
    std::string js_css = "background: url(javascript:alert(1));";
    assert(!renderer.sanitize_css_style(js_css));
}

void test_render_with_options() {
    HtmlRendererStd renderer;

    HtmlRenderOptions options;
    options.allow_css = true;
    options.allow_tables = true;
    options.extract_text = true;
    options.dictionary_id = "test_dict";

    std::string html = R"(
        <div>
            <p style="color: blue;">Hello <strong>world</strong></p>
            <table><tr><td>Cell</td></tr></table>
            <img src="pic.png"/>
        </div>
    )";

    RenderedHtml rendered = renderer.render(html, options);

    // HTML output
    assert(rendered.html.find("<p>") != std::string::npos);
    assert(rendered.html.find("style=") != std::string::npos);
    assert(rendered.html.find("<table>") != std::string::npos);

    // Text extraction
    assert(rendered.text.find("Hello") != std::string::npos);
    assert(rendered.text.find("world") != std::string::npos);
    assert(rendered.text.find("Cell") != std::string::npos);

    // Flags
    assert(rendered.has_images);
    assert(!rendered.has_audio);
    assert(!rendered.has_math);
}

void test_tag_whitelist() {
    HtmlRendererStd renderer;

    // Default allowed tags
    assert(renderer.is_tag_allowed("div"));
    assert(renderer.is_tag_allowed("p"));
    assert(renderer.is_tag_allowed("span"));
    assert(renderer.is_tag_allowed("strong"));
    assert(renderer.is_tag_allowed("em"));

    // Dangerous tags not allowed by default
    assert(!renderer.is_tag_allowed("script"));
    assert(!renderer.is_tag_allowed("iframe"));
    assert(!renderer.is_tag_allowed("object"));
    assert(!renderer.is_tag_allowed("embed"));
}

void test_attribute_whitelist() {
    HtmlRendererStd renderer;

    // Safe attributes
    assert(renderer.is_attribute_allowed("class"));
    assert(renderer.is_attribute_allowed("id"));
    assert(renderer.is_attribute_allowed("style"));
    assert(renderer.is_attribute_allowed("href"));
    assert(renderer.is_attribute_allowed("title"));

    // Dangerous event handlers
    assert(!renderer.is_attribute_allowed("onclick"));
    assert(!renderer.is_attribute_allowed("onload"));
    assert(!renderer.is_attribute_allowed("onerror"));
}

void test_custom_tag_configuration() {
    HtmlRendererStd renderer;

    // Add custom tag
    renderer.add_allowed_tag("article");
    assert(renderer.is_tag_allowed("article"));

    // Remove tag
    renderer.remove_allowed_tag("div");
    assert(!renderer.is_tag_allowed("div"));
}

void test_css_property_whitelist() {
    HtmlRendererStd renderer;

    // Safe CSS properties
    assert(renderer.is_css_property_allowed("color"));
    assert(renderer.is_css_property_allowed("font-size"));
    assert(renderer.is_css_property_allowed("margin"));
    assert(renderer.is_css_property_allowed("padding"));

    // Potentially dangerous properties
    assert(!renderer.is_css_property_allowed("behavior"));  // IE only, dangerous
    assert(!renderer.is_css_property_allowed("expression"));  // IE only, dangerous
}

void test_resource_url_rewriting() {
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

void test_custom_link_resolver() {
    HtmlRendererStd renderer;

    renderer.set_link_resolver([](const std::string& word, const std::string& dict_id) {
        return "myapp://lookup/" + dict_id + "/" + word;
    });

    std::string html = R"(<a href="entry://test">link</a>)";

    HtmlRenderOptions options;
    options.dictionary_id = "mydict";

    RenderedHtml rendered = renderer.render(html, options);

    assert(rendered.html.find("myapp://lookup/mydict/test") != std::string::npos);
}

int main() {
    test_basic_sanitize();
    test_strip_tags();
    test_extract_text();
    test_link_rewriting();
    test_cross_reference_detection();
    test_safe_url_validation();
    test_css_sanitization();
    test_render_with_options();
    test_tag_whitelist();
    test_attribute_whitelist();
    test_custom_tag_configuration();
    test_css_property_whitelist();
    test_resource_url_rewriting();
    test_custom_link_resolver();

    return 0;
}
