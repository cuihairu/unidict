// HtmlRendererStd 补覆盖：render 挂 resolver 的资源改写、tokenize 属性
// 解析（带空格/引号/unquoted）、不安全 img src 剥除、CSS 白名单 continue、
// 实体解码 miss/无分号、normalize_url 折叠、@@@LINK 交叉引用、
// DefaultResourceResolverStd 的 mime 表/构造回落/试探链，以及
// ResourceResolverStd 基类默认实现。

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "std/html_renderer_std.h"

using namespace UnidictCoreStd;

// Windows (MSVC) 无 POSIX setenv，走 _putenv_s
static void set_env(const char* key, const char* value) {
#if defined(_WIN32)
    _putenv_s(key, value);
#else
    ::setenv(key, value, 1);
#endif
}

// 真正移除变量：置空串在 POSIX 上 getenv 仍返回 ""（非 null），
// 触发不了 resolver 的无 HOME 回落分支
static void unset_env(const char* key) {
#if defined(_WIN32)
    _putenv_s(key, "");
#else
    ::unsetenv(key);
#endif
}

static void write_file(const std::filesystem::path& p, const std::string& content) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << content;
    assert(out.good());
}

// 只实现两个纯虚方法，其余全部走 ResourceResolverStd 默认实现；
// resolve 返回给定文件，可让基类默认 get_data_url 走 file:// 分支
class NullResolver : public ResourceResolverStd {
public:
    explicit NullResolver(std::string existing_file)
        : existing_file_(std::move(existing_file)) {}
    ResourceInfo resolve(const std::string&, const std::string&) override {
        ResourceInfo info;
        info.local_path = existing_file_;
        info.mime_type = "image/png";
        return info;
    }
    bool exists(const std::string&, const std::string&) override { return false; }
private:
    std::string existing_file_;
};

// resolve 落空但 data URL 可用的类型：触发 rewrite_resource_urls 的
// get_data_url 替换分支
class DataOnlyResolver : public ResourceResolverStd {
public:
    ResourceInfo resolve(const std::string&, const std::string&) override { return {}; }
    bool exists(const std::string&, const std::string&) override { return false; }
    std::string get_data_url(const std::string& url, const std::string&) override {
        return "data:application/octet-stream;base64," + url;
    }
    bool is_dictionary_resource(const std::string&) const override { return true; }
    std::string extract_resource_key(const std::string& url) const override { return url; }
};

int main() {
    namespace fs = std::filesystem;

    // ===== ResourceResolverStd 基类默认实现 =====
    {
        auto nr = std::make_shared<NullResolver>(
            (fs::current_path() / "build-local" / "html_cover_res" / "a.png").string());
        assert(nr->preload_resources({"a.png"}, "d"));
        nr->clear_cache("d");   // 默认空实现，不崩即可
        assert(nr->get_cached_resources().empty());
        // 无协议 → 本地资源；带协议 → 非词典资源
        assert(nr->is_dictionary_resource("pic.png"));
        assert(!nr->is_dictionary_resource("http://cdn/pic.png"));
        // 默认 extract_resource_key 原样返回
        assert(nr->extract_resource_key("res/key.png") == "res/key.png");
        // 基类默认 get_data_url：resolve 命中 → file:// 前缀（不读文件内容）
        assert(nr->get_data_url("a.png", "d").find("file://") == 0);
        // resolve 落空时基类默认 get_data_url 回落空串
        auto nr_empty = std::make_shared<NullResolver>("");
        assert(nr_empty->get_data_url("x.png", "d").empty());
    }

    // ===== DefaultResourceResolverStd：构造回落与 cache 目录 =====
    {
        set_env("HOME", "build-local/fake_home");
        DefaultResourceResolverStd with_home;
        assert(with_home.get_cache_directory().find("fake_home") != std::string::npos);
        assert(with_home.get_cache_directory().find("unidict") != std::string::npos);

        unset_env("HOME");
        DefaultResourceResolverStd no_home;
        assert(no_home.get_cache_directory() == "/tmp/unidict_cache");
    }

    // ===== DefaultResourceResolverStd：mime 表 + 试探链 =====
    {
        fs::path dir = fs::current_path() / "build-local" / "html_cover_res";
        fs::create_directories(dir);
        write_file(dir / "a.png", "png");
        write_file(dir / "b.jpg", "jpg");
        write_file(dir / "c.jpeg", "jpeg");
        write_file(dir / "d.gif", "gif");
        write_file(dir / "e.svg", "svg");
        write_file(dir / "f.mp3", "mp3");
        write_file(dir / "g.wav", "wav");
        write_file(dir / "h.ogg", "ogg");
        write_file(dir / "plain", "no-ext");     // 命中但 mime 为空
        write_file(dir / "song.mp3", "audio");   // 无扩展引用 → audio 试探命中

        DefaultResourceResolverStd res;
        res.register_dictionary("d", dir.string());

        struct Case { const char* url; const char* mime; };
        const Case cases[] = {
            {"a.png", "image/png"},   {"b.jpg", "image/jpeg"},
            {"c.jpeg", "image/jpeg"}, {"d.gif", "image/gif"},
            {"e.svg", "image/svg+xml"}, {"f.mp3", "audio/mpeg"},
            {"g.wav", "audio/wav"},   {"h.ogg", "audio/ogg"},
        };
        for (const auto& c : cases) {
            auto info = res.resolve(c.url, "d");
            assert(!info.local_path.empty());
            assert(info.mime_type == c.mime);
            assert(info.is_cached);
            assert(info.size > 0);
            assert(res.exists(c.url, "d"));
        }

        // 无扩展名引用：exact 落空 → 先试图片扩展、再试音频扩展命中
        // （mime 看请求键，键上没扩展名即为空）
        auto probed = res.resolve("song", "d");
        assert(!probed.local_path.empty());
        assert(probed.mime_type.empty());        // 全部落空：exact + 试探链全 miss
        assert(!res.exists("ghost", "d"));

        // 同名目录不是资源：find_resource_file 用 ifstream 探测，部分
        // 文件系统上目录也能"打开"（tellg 报 INT64_MAX），resolve 必须
        // is_regular_file 挡住——否则 get_data_url 按 info.size 的巨值
        // 分配，直接 std::bad_alloc（曾实测炸穿）
        fs::create_directories(dir / "shadow.png");
        assert(res.resolve("shadow.png", "d").local_path.empty());
        assert(!res.exists("shadow.png", "d"));
        assert(res.get_data_url("shadow.png", "d").empty());
        fs::remove_all(dir / "shadow.png");
        // 协议 URL：entry:// 不是资源；http 走资源键但目录里没有
        assert(res.resolve("entry://word", "d").local_path.empty());
        assert(res.resolve("http://cdn/x.png", "d").local_path.empty());

        // data URL：mime 为空（无扩展命中）时回落空串
        assert(res.get_data_url("plain", "d").empty());
        // Default 覆写的 preload_resources：当前实现恒 true
        assert(res.preload_resources({"a.png"}, "d"));
        // 正常 base64：mime 表内扩展名才能进编码——1/2/3 字节把补位三元
        // 运算符的每一侧都走到（1B 两个 pad、2B 一个 pad、3B 无 pad）
        fs::path one = dir / "one.png", two = dir / "two.jpg", three = dir / "three.gif";
        write_file(one, "x");
        write_file(two, "xy");
        write_file(three, "xyz");
        auto d1 = res.get_data_url("one.png", "d");
        auto d2 = res.get_data_url("two.jpg", "d");
        auto d3 = res.get_data_url("three.gif", "d");
        assert(d1.find("data:image/png;base64,") == 0);
        assert(d1.back() == '=');
        assert(d2.find("data:image/jpeg;base64,") == 0);
        assert(d2.back() == '=');
        assert(d3.find("data:image/gif;base64,") == 0);
        assert(d3.back() != '=');
        // 文件在 resolve 后消失：读取失败回落空串
        fs::remove(one);
        assert(res.get_data_url("one.bin", "d").empty());
        fs::remove(two);

        // 注册表与清缓存
        res.register_dictionary("d2", dir.string());
        assert(res.get_cached_resources().size() == 2);
        res.clear_cache("d2");
        assert(res.get_cached_resources().size() == 1);
        res.clear_cache();   // 空 id 全清
        assert(res.get_cached_resources().empty());
        assert(!res.exists("a.png", "d"));   // 已被清空

        // ===== render 挂 resolver：rewrite_resource_urls 的 resolver 分支 =====
        res.register_dictionary("", dir.string());   // render 以空 dict_id 解析
        HtmlRendererStd renderer(std::make_shared<DefaultResourceResolverStd>(res));
        auto out = renderer.render(R"(<img src="a.png">)");
        assert(out.html.find("file://") != std::string::npos);
        assert(out.html.find("a.png") != std::string::npos);
        // resolver 落空且 data URL 也空 → 原样保留
        auto out2 = renderer.render(R"(<img src="ghost.png">)");
        assert(out2.html.find("src=\"ghost.png\"") != std::string::npos);
        // data URL 替换分支（resolve 空、get_data_url 非空）
        HtmlRendererStd data_renderer(std::make_shared<DataOnlyResolver>());
        auto out3 = data_renderer.render(R"(<img src="k.png">)");
        assert(out3.html.find("data:application/octet-stream;base64,k.png") != std::string::npos);
    }

    // ===== sanitize：tokenize 属性解析与不安全 URL 剥除 =====
    {
        HtmlRendererStd renderer;
        // 等号两侧空格 + 双引号
        auto s1 = renderer.sanitize(R"(<a href = "entry://x">y</a>)");
        assert(s1.find("entry://x") != std::string::npos || s1.find("#lookup:") != std::string::npos);
        // 单引号值
        auto s2 = renderer.sanitize("<a href='entry://y'>z</a>");
        assert(s2.find("entry://y") != std::string::npos || s2.find("#lookup:") != std::string::npos);
        // unquoted 值
        auto s3 = renderer.sanitize("<a href=entry://w>q</a>");
        assert(s3.find("entry://w") != std::string::npos || s3.find("#lookup:") != std::string::npos);
        // 孤立 '<'（后无 '>'）：吞到串尾
        auto s4 = renderer.sanitize("a < b");
        assert(s4.find("<") == std::string::npos);
        assert(s4.find("a") != std::string::npos);
        // img src 不安全协议 → src 属性剥除
        auto s5 = renderer.sanitize(R"(<img src="ftp://host/a.png">)");
        assert(s5.find("ftp://") == std::string::npos);
        auto s6 = renderer.sanitize(R"(<img src="vbscript:run">)");
        assert(s6.find("vbscript:") == std::string::npos);
        // 非白名单 CSS 属性 → continue 只留合法项；白名单属性 + 非法属性并存
        auto s7 = renderer.sanitize(R"(<p style="zzz-bogus: 1; color: red">x</p>)");
        assert(s7.find("zzz-bogus") == std::string::npos);
        assert(s7.find("color") != std::string::npos);
    }

    // ===== 白名单动态接口 =====
    {
        HtmlRendererStd renderer;
        renderer.add_allowed_attribute("data-x");
        auto s1 = renderer.sanitize(R"(<span data-x="1">t</span>)");
        assert(s1.find("data-x") != std::string::npos);

        renderer.add_allowed_tag("marquee");
        assert(renderer.is_tag_allowed("marquee"));
        auto s2 = renderer.sanitize("<marquee>m</marquee>");
        assert(s2.find("<marquee>") != std::string::npos);
        renderer.remove_allowed_tag("marquee");
        assert(!renderer.is_tag_allowed("marquee"));
        auto s3 = renderer.sanitize("<marquee>m</marquee>");
        assert(s3.find("<marquee>") == std::string::npos);
    }

    // ===== 实体解码 miss / 无分号 / 编码 =====
    {
        HtmlRendererStd renderer;
        // " & "（无分号）与未知实体：decode 原样带过，输出侧统一编码为 &amp;
        auto s1 = renderer.sanitize("Tom & Jerry &unknownx; &amp; Co");
        assert(s1.find("Tom &amp; Jerry") != std::string::npos);
        assert(s1.find("&amp;unknownx;") != std::string::npos);
        assert(s1.find("&amp; Co") != std::string::npos);
        // 属性值原样保留（title 本就在白名单）
        auto s2 = renderer.sanitize(R"(<p title="a&amp;b">x</p>)");
        assert(s2.find("title=\"a&amp;b\"") != std::string::npos);
        // 属性值里的双引号：decode 解回 " 再统一 encode 成 &quot;
        auto s3 = renderer.sanitize(R"(<p title="say &quot;hi&quot;">x</p>)");
        assert(s3.find("&quot;hi&quot;") != std::string::npos);
        // 文本中的 '>'：不构成标签，encode 成 &gt;（encode 的 '>' 分支本身
        // 不可达：值里出现 '>' 会被 tokenize 当标签结尾截断）
        auto s4 = renderer.sanitize("a > b");
        assert(s4.find("a &gt; b") != std::string::npos);
    }

    // ===== 交叉引用：sanitize 不改写，resolve_cross_reference 才改 =====
    {
        HtmlRendererStd renderer;
        // 外链 href 原样保留（折叠/剥 fragment 不是 sanitize 的职责）
        auto s1 = renderer.sanitize(R"(<a href="http://x.io//p//q#frag">L</a>)");
        assert(s1.find("http://x.io//p//q#frag") != std::string::npos);
        // @@@LINK= / entry:// 在 sanitize 输出里原样保留
        auto s2 = renderer.sanitize(R"(<a href="@@@LINK=pear">P</a>)");
        assert(s2.find("@@@LINK=pear") != std::string::npos);
        auto s3 = renderer.sanitize(R"(<a href="entry://apple">A</a>)");
        assert(s3.find("entry://apple") != std::string::npos);
        // 解析走 resolve_cross_reference：entry:// 与 @@@LINK= 两个提取分支
        assert(renderer.resolve_cross_reference("entry://cat", "dict") == "#lookup:cat");
        assert(renderer.resolve_cross_reference("@@@LINK=pear", "dict") == "#lookup:pear");
        assert(renderer.resolve_cross_reference("http://plain.io", "dict") == "http://plain.io");
        // rewrite_links：中间含 @@@LINK= 的串也取其后目标
        auto l = renderer.rewrite_links("see @@@LINK=orange now", "dict");
        assert(l.find("#lookup:orange") != std::string::npos);
    }

    // ===== Factory =====
    {
        auto strict = HtmlRendererFactory::create_strict();
        auto out = strict->sanitize("<script>x</script><p>ok</p>");
        assert(out.find("<script>") == std::string::npos);
        assert(out.find("<p>") != std::string::npos);

        auto permissive = HtmlRendererFactory::create_permissive();
        auto out2 = permissive->sanitize("<details><summary>s</summary>d</details>");
        assert(out2.find("<details>") != std::string::npos);
        assert(out2.find("<summary>") != std::string::npos);

        auto with_defaults = HtmlRendererFactory::create_with_defaults();
        auto out3 = with_defaults->render("hello");
        assert(out3.html.find("hello") != std::string::npos);
    }

    // ===== 补充：末尾孤立 '<'、实体再转义、小写 @@@link 兜底、
    // 未注册词典查询回落 =====
    {
        HtmlRendererStd r;   // 栈对象：覆盖 ~HtmlRendererStd

        // '<' 为末字符：tokenizer 走 pos+1>=len 的前进分支
        auto out1 = r.render("p <");
        assert(out1.html.find("p") != std::string::npos);

        // &lt; 解码后再转义输出（文本编码的 '<' 分支）
        auto out2 = r.render("x &lt; y");
        assert(out2.html.find("&lt;") != std::string::npos);

        // 小写 @@@link= 命中交叉引用判定（lower 查找），但
        // extract_link_target 的大写 @@@LINK= 查找不命中 → 原样回落
        auto t = r.resolve_cross_reference("@@@link=word", "d");
        assert(t == "#lookup:@@@link=word");
    }
    {
        // 未注册词典 id：resolve 早退回落空；栈对象覆盖基类虚析构
        DefaultResourceResolverStd res;
        assert(res.get_data_url("a.png", "no_such_dict").empty());
        assert(!res.exists("a.png", "no_such_dict"));
    }

    return 0;
}
