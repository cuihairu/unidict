// 边缘覆盖：allow-list 配置、链接改写、资源 URL 重写、工厂、默认资源解析器
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

#include "std/html_renderer_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

int main() {
    // ===== allow-list 配置接口 =====
    {
        HtmlRendererStd r;
        assert(r.is_tag_allowed("b"));
        assert(!r.is_tag_allowed("script"));
        assert(r.is_attribute_allowed("href"));
        assert(r.is_css_property_allowed("color"));

        r.add_allowed_tag("customtag");
        assert(r.is_tag_allowed("customtag"));
        r.remove_allowed_tag("customtag");
        assert(!r.is_tag_allowed("customtag"));

        r.add_allowed_attribute("data-custom");
        assert(r.is_attribute_allowed("data-custom"));

        r.add_allowed_css_property("custom-prop");
        assert(r.is_css_property_allowed("custom-prop"));

        // 加回去再删掉（remove 路径对 attribute/css 也有 add 侧验证）
        r.add_allowed_tag("b"); // 幂等
        assert(r.is_tag_allowed("b"));
    }

    // ===== render 标志位与实体编码 =====
    {
        HtmlRendererStd r;
        auto out = r.render("<p>hello</p><img src=\"pic.png\" /><video src=\"v.mp4\" /><!-- hidden -->");
        assert(out.has_images);
        assert(out.has_audio);
        assert(out.html.find("hidden") == std::string::npos); // 注释被剥离
        assert(out.text.find("hello") != std::string::npos);
        // 未涉及媒体时标志为假
        auto plain = r.render("<p>text only</p>");
        assert(!plain.has_images && !plain.has_audio);
        // TEXT 节点经实体编码：& 与 > 被转义（裸 < 会进入标签解析，另测）
        auto ent = r.render("Tom & Jerry said 5 > 3");
        assert(ent.html.find("Tom &amp; Jerry said 5 &gt; 3") != std::string::npos);
        // entry:// 链接抽取
        auto links = r.render("<a href=\"entry://cat\">cat</a>");
        assert(links.linked_words.size() == 1);
        assert(links.linked_words[0] == "cat");
    }

    // ===== rewrite_links：@@@LINK 与 entry:// =====
    {
        HtmlRendererStd r;
        // @@@LINK 先替换成 entry:// 链接，紧接着第二阶段又把 entry://
        // 改写成 #lookup:（默认 resolver），所以最终产物里没有 entry://
        auto l1 = r.rewrite_links("@@@LINK=apple", "dict1");
        assert(l1.find("href=\"#lookup:apple\"") != std::string::npos);
        assert(l1.find("data-dict=\"dict1\"") != std::string::npos);
        assert(l1.find(">apple</a>") != std::string::npos);

        auto l2 = r.rewrite_links("<a href=\"entry://foo\">x</a>", "dict1");
        assert(l2.find("href=\"#lookup:foo\"") != std::string::npos);

        // 自定义 link resolver 接管 entry:// 改写
        HtmlRendererStd r2;
        r2.set_link_resolver([](const std::string& word, const std::string&) {
            return "/lookup/" + word;
        });
        auto l3 = r2.rewrite_links("<a href=\"entry://bar\">y</a>", "dict1");
        assert(l3.find("href=\"/lookup/bar\"") != std::string::npos);
    }

    // ===== resolve_cross_reference =====
    {
        HtmlRendererStd r;
        assert(r.resolve_cross_reference("entry://cat", "d") == "#lookup:cat");
        assert(r.resolve_cross_reference("http://example.com", "d") == "http://example.com");

        HtmlRendererStd r2;
        r2.set_link_resolver([](const std::string& word, const std::string& dict) {
            return dict + ":" + word;
        });
        assert(r2.resolve_cross_reference("entry://dog", "mydict") == "mydict:dog");
    }

    // ===== rewrite_resource_urls：url_map 命中与否 =====
    {
        HtmlRendererStd r;
        std::unordered_map<std::string, std::string> url_map{
            {"a.png", "file:///resolved/a.png"}};
        auto out = r.rewrite_resource_urls(
            "<img src=\"a.png\" /><img src=\"b.png\" />", url_map);
        assert(out.find("src=\"file:///resolved/a.png\"") != std::string::npos);
        assert(out.find("src=\"b.png\"") != std::string::npos); // 未命中的原样保留
    }

    // ===== DefaultResourceResolverStd =====
    {
        fs::path res_dir = fs::current_path() / "build-local" / "html_res" / "images";
        fs::create_directories(res_dir);
        // 写一个最小 PNG 头文件（内容不影响 resolver 判定）
        auto png_path = res_dir / "icon.png";
        {
            std::ofstream out(png_path, std::ios::binary | std::ios::trunc);
            out << "\x89PNG\r\n\x1a\n" << "fake-image-data";
        }
        // 带空格文件名，走 %20 解码
        auto sp_path = res_dir / "my img.png";
        {
            std::ofstream out(sp_path, std::ios::binary | std::ios::trunc);
            out << "spaced";
        }

        DefaultResourceResolverStd res;
        assert(res.get_cache_directory().find("unidict") != std::string::npos ||
               res.get_cache_directory().find("tmp") != std::string::npos);
        res.set_cache_directory((fs::current_path() / "build-local" / "html_cache").string());
        assert(res.get_cache_directory().find("html_cache") != std::string::npos);

        // 未注册词典：查不到
        assert(!res.exists("images/icon.png", "nope"));
        assert(res.resolve("images/icon.png", "nope").local_path.empty());

        // 注册后精确命中
        res.register_dictionary("d1", (fs::current_path() / "build-local" / "html_res").string());
        assert(res.exists("images/icon.png", "d1"));
        auto info = res.resolve("images/icon.png", "d1");
        assert(!info.local_path.empty());
        assert(info.mime_type == "image/png");
        assert(info.size > 0);
        assert(info.is_cached);

        // 无扩展名引用 → 扩展名回退命中
        assert(res.exists("images/icon", "d1"));

        // %20 解码命中
        assert(res.exists("images/my%20img.png", "d1"));

        // base64 data URL
        auto data_url = res.get_data_url("images/icon.png", "d1");
        assert(data_url.find("data:image/png;base64,") == 0);
        assert(data_url.size() > std::string("data:image/png;base64,").size());

        // 注册表列表与注销
        auto cached = res.get_cached_resources();
        assert(cached.size() == 1);
        assert(cached[0] == "d1");
        res.unregister_dictionary("d1");
        assert(!res.exists("images/icon.png", "d1"));

        // 重新注册再整体清空
        res.register_dictionary("d2", (fs::current_path() / "build-local" / "html_res").string());
        assert(res.exists("images/icon.png", "d2"));
        res.clear_cache(""); // 空 id 清全部
        assert(res.get_cached_resources().empty());
    }

    // ===== URL 判定辅助（经 resolver 公开接口）=====
    {
        DefaultResourceResolverStd res;
        assert(!res.is_dictionary_resource("entry://word")); // 词典内引用不算资源
        assert(res.is_dictionary_resource("http://x/a.png"));
        assert(res.is_dictionary_resource("a.png"));
        assert(res.extract_resource_key("http://host/a.png") == "host/a.png");
        assert(res.extract_resource_key("a.png") == "a.png");
    }

    // ===== 工厂 =====
    {
        auto d1 = HtmlRendererFactory::create_with_defaults();
        assert(d1 != nullptr);
        auto d2 = HtmlRendererFactory::create_with_resource_resolver(
            std::make_shared<DefaultResourceResolverStd>());
        assert(d2 != nullptr);
        auto strict = HtmlRendererFactory::create_strict();
        assert(strict != nullptr);
        assert(!strict->is_tag_allowed("iframe"));
        auto perm = HtmlRendererFactory::create_permissive();
        assert(perm != nullptr);
        assert(perm->is_tag_allowed("details"));
    }

    std::cout << "OK\n";
    return 0;
}
