// cross_reference_std 分支缺口补测（真实缺边 22 条，第六大簇）。
//
// 缺边构成：url_decode 的尾部截断 %xx 臂、url_encode 的 -_.~ 保留
// 子边与非保留字符编码臂、bword 查询参数无 dict= 的臂、
// is_cross_reference 对合法非交叉引用类型（file/sound/http）的全假
// 短路边、is_valid_link 全空白词假臂、export_history 的 back/forward
// 多条目逗号臂、import_history 的 extract_object/extract_array 五个
// npos 臂与无 word 条目跳过、are_variations 的单侧未知词回落臂、
// load_from_file 的空白片段/空白行臂。全部真实输入驱动。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "std/cross_reference_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path base_dir() {
    fs::path d = fs::current_path() / "build-local" / "xrbr";
    fs::remove_all(d);  // hermetic：清掉上轮（可能中途夭折）的遗留状态
    fs::create_directories(d);
    return d;
}

static fs::path write_bytes(const fs::path& p, const std::string& body) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << body;
    assert(out.good());
    return p;
}

// T1 解析/编码矩阵：截断 %xx、encode 保留集、bword 查询参数
static void test_parse_encode_matrix() {
    CrossReferenceManager m;
    // 尾部截断 %xx：i+2 < len 假臂，'%' 原样保留
    auto t = m.parse_link("bword://a%4");
    assert(t.type == LinkType::BWORD);
    assert(t.target_word == "a%4");
    // 完整 %xx：真臂正常解码（编码往返）
    t = m.parse_link("bword://a%20b");
    assert(t.target_word == "a b");
    // '+' 转空格（解码臂）
    t = m.parse_link("bword://a+b");
    assert(t.target_word == "a b");
    // url_encode 保留集：alnum/-/_/.~/ 全走一遍（经 format_link 的
    // bword 分支）
    auto p = m.parse_link("bword://a b~c-d_e.f");
    assert(p.target_word == "a b~c-d_e.f");
    assert(m.format_link(p) == "bword://a%20b~c-d_e.f");
    // bword 查询参数：有参数但无 dict= → target_dictionary_id 保持空
    t = m.parse_link("bword://word?x=1&y=2");
    assert(t.target_word == "word");
    assert(t.target_dictionary_id.empty());
    assert(t.params.at("x") == "1");
    // 带 dict= 的正向臂
    t = m.parse_link("bword://word?dict=d1&x=1");
    assert(t.target_dictionary_id == "d1");
    // 无参数值片段（parse_query_params 的无 '=' 臂）
    t = m.parse_link("bword://word?flag");
    assert(t.params.at("flag").empty());
}

// T2 is_cross_reference 与 is_valid_link 的类型矩阵
static void test_cross_reference_classification() {
    CrossReferenceManager m;
    // 合法交叉引用三类
    assert(m.is_cross_reference("@@@LINK=word"));
    assert(m.is_cross_reference("entry://word"));
    assert(m.is_cross_reference("bword://word"));
    // 合法但非交叉引用（file/sound/http）：三个比较全假 → false
    assert(!m.is_cross_reference("file://pic.png"));
    assert(!m.is_cross_reference("sound://a.mp3"));
    assert(!m.is_cross_reference("http://example.com"));
    // 非法（空 target）
    assert(!m.is_cross_reference("entry://"));
    assert(!m.is_cross_reference(""));
    // is_valid_link：全空白词 = UNKNOWN 但无有效内容 → 假
    assert(!LinkPatternFactory::is_valid_link("   "));
    assert(!LinkPatternFactory::is_valid_link("\t\n\r"));
    assert(!LinkPatternFactory::is_valid_link(""));
    assert(LinkPatternFactory::is_valid_link("word"));
    assert(LinkPatternFactory::is_valid_link("entry://w"));
}

// T3 HTML 重写：交叉引用被改写、非交叉引用保持原样
static void test_html_rewrite() {
    CrossReferenceManager m;
    HtmlLinkRewriter rw(&m);
    // entry:// 链接 → #lookup: 改写
    std::string html =
        "<p><a href=\"entry://alpha\">a</a>"
        "<a href=\"file://pic.png\">p</a></p>";
    std::string out = rw.rewrite_for_lookup(html, "d1");
    assert(out.find("#lookup:alpha") != std::string::npos);
    assert(out.find("file://pic.png") != std::string::npos);
    // 无链接 HTML 原样返回
    assert(rw.rewrite_for_lookup("<p>plain</p>", "d1") == "<p>plain</p>");
    // extract_links 取全部合法链接（不筛交叉引用类型）
    auto links = rw.extract_links(html);
    assert(links.size() == 2);
    assert(links[0].type == LinkType::ENTRY);
    assert(links[0].target_word == "alpha");
    assert(links[1].type == LinkType::FILE);
}

// T4 导航历史 export/import 矩阵
static void test_history_export_import() {
    fs::path dir = base_dir();
    CrossReferenceManager m;
    // 五次导航 → back 四条；回退两次 → back 两条 + forward 两条。
    // export 的 back/forward 数组都进多条目逗号臂
    m.navigate_to("a", "d1");
    m.navigate_to("b", "d1");
    m.navigate_to("c", "d1");
    m.navigate_to("d", "d1");
    m.navigate_to("e", "d1");
    m.go_back();
    m.go_back();
    assert(m.can_go_forward());
    assert(m.can_go_back());
    const NavigationState& nav = m.navigation_state();
    assert(nav.back_stack.size() == 2);
    assert(nav.forward_stack.size() == 2);
    std::string exported = m.export_history();
    assert(exported.find(", ") != std::string::npos);
    // 往返导入
    {
        CrossReferenceManager m2;
        assert(m2.import_history(exported));
        assert(m2.current_entry().word == "c");
        assert(m2.navigation_state().back_stack.size() == 2);
        assert(m2.navigation_state().forward_stack.size() == 2);
    }
    // import 矩阵：current 标记缺失
    {
        CrossReferenceManager m2;
        assert(!m2.import_history("{}"));
        assert(!m2.import_history("\"current\": null"));
    }
    // back/forward 数组缺失与形态畸变：current 仍合法 → 导入成功
    {
        CrossReferenceManager m2;
        assert(m2.import_history("{\"current\": {\"word\": \"w\"}}"));
        assert(m2.current_entry().word == "w");
        assert(m2.navigation_state().back_stack.empty());
    }
    {
        CrossReferenceManager m2;
        // "back" 后无 '['（数组标记 npos）
        assert(m2.import_history("{\"current\": {\"word\": \"w\"}, \"back\": {}}"));
        assert(m2.navigation_state().back_stack.empty());
    }
    // current 对象无 word：解析空 → 清空并拒绝
    {
        CrossReferenceManager m2;
        assert(!m2.import_history("{\"current\": {\"dict\": \"d\"}}"));
        assert(m2.current_entry().word.empty());
    }
    // 栈内无 word 条目被跳过（数组非空但条目全废）
    {
        CrossReferenceManager m2;
        assert(m2.import_history(
            "{\"current\": {\"word\": \"w\"}, \"back\": [{}, {}]}"));
        assert(m2.current_entry().word == "w");
        assert(m2.navigation_state().back_stack.empty());
    }
    // 嵌套对象：extract_object 扫描经过内层 '}'（depth 2→1 减不到 0）
    {
        CrossReferenceManager m2;
        assert(m2.import_history(
            "{\"current\": {\"nested\": {\"word\": \"x\"}}}"));
        assert(m2.current_entry().word == "x");
    }
    // 嵌套数组：extract_array 扫描经过内层 ']'（depth 2→1 减不到 0）
    {
        CrossReferenceManager m2;
        assert(m2.import_history(
            "{\"current\": {\"word\": \"w\"}, "
            "\"forward\": [[], {\"word\": \"f\"}]}"));
        assert(m2.current_entry().word == "w");
    }
}

// T5 变体管理器：单侧未知回落 + 文件载入空白矩阵
static void test_variation_manager() {
    fs::path dir = base_dir();
    WordVariationManager vm;
    vm.add_variations("color", {"hue", "tint"});
    assert(vm.are_variations("hue", "tint"));
    assert(vm.get_canonical_form("HUE") == "color");
    // 单侧未知：ca 命中 cb 未命中 → 回落小写比较假
    assert(!vm.are_variations("hue", "unheard-word"));
    // 单侧未知（反向）：ca 未命中 cb 命中 → 同一回落臂
    assert(!vm.are_variations("unheard-word", "hue"));
    assert(vm.get_canonical_form("unheard-word") == "unheard-word");
    // 文件载入：注释行、含空白与空片段的行、纯空白行、单词条
    fs::path f = write_bytes(dir / "var.txt",
                             "# comment line\n"
                             "color, hue , tint ,,\n"
                             "   \n"
                             "plain\n");
    WordVariationManager vm2;
    assert(vm2.load_from_file(f.string()));
    auto v = vm2.get_variations("COLOR");
    assert(v.size() == 2);
    assert(v[0] == "hue");
    assert(v[1] == "tint");
    assert(vm2.get_variations("plain").empty());
    assert(vm2.get_canonical_form("plain") == "plain");
    // 不存在的文件
    WordVariationManager vm3;
    assert(!vm3.load_from_file((dir / "missing.txt").string()));
    // 保存往返
    fs::path out = dir / "var_out.txt";
    assert(vm.save_to_file(out.string()));
    WordVariationManager vm4;
    assert(vm4.load_from_file(out.string()));
    assert(vm4.are_variations("hue", "tint"));
}

int main() {
    test_parse_encode_matrix();
    test_cross_reference_classification();
    test_html_rewrite();
    test_history_export_import();
    test_variation_manager();
    std::cout << "OK\n";
    return 0;
}
