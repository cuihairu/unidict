// Cross-reference link handling unit tests (std-only).
// Tests for link parsing, navigation history, and link rewriting.

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/cross_reference_std.h"

using namespace UnidictCoreStd;

static void test_parse_links() {
    CrossReferenceManager manager;

    // entry://
    {
        ParsedLink link = manager.parse_link("entry://hello");
        assert(link.is_valid);
        assert(link.type == LinkType::ENTRY);
        assert(link.target_word == "hello");
        assert(link.target_dictionary_id.empty());
    }
    {
        ParsedLink link = manager.parse_link("entry://world|oxford");
        assert(link.is_valid);
        assert(link.type == LinkType::ENTRY);
        assert(link.target_word == "world");
        assert(link.target_dictionary_id == "oxford");
    }

    // bword://
    {
        ParsedLink link = manager.parse_link("bword://test");
        assert(link.is_valid);
        assert(link.type == LinkType::BWORD);
        assert(link.target_word == "test");
        assert(link.target_dictionary_id.empty());
    }
    {
        ParsedLink link = manager.parse_link("bword://example?dict=longman");
        assert(link.is_valid);
        assert(link.type == LinkType::BWORD);
        assert(link.target_word == "example");
        assert(link.target_dictionary_id == "longman");
    }
    {
        ParsedLink link = manager.parse_link("bword://hello%20world");
        assert(link.is_valid);
        assert(link.type == LinkType::BWORD);
        assert(link.target_word == "hello world");
    }

    // @@@LINK=
    {
        ParsedLink link = manager.parse_link("@@@LINK=alternative");
        assert(link.is_valid);
        assert(link.type == LinkType::INTERNAL);
        assert(link.target_word == "alternative");
    }

    // web
    {
        ParsedLink link = manager.parse_link("http://example.com");
        assert(link.is_valid);
        assert(link.type == LinkType::HTTP);
        assert(link.target_word == "http://example.com");
    }
    {
        ParsedLink link = manager.parse_link("https://example.com/page");
        assert(link.is_valid);
        assert(link.type == LinkType::HTTP);
    }
    {
        ParsedLink link = manager.parse_link("ftp://files.example.com/file.zip");
        assert(link.is_valid);
        assert(link.type == LinkType::UNKNOWN);
    }

    // malformed / unknown
    {
        ParsedLink link = manager.parse_link("");
        assert(!link.is_valid);
        assert(link.target_word.empty());
    }
    {
        ParsedLink link = manager.parse_link("just_a_word");
        assert(link.is_valid);
        assert(link.type == LinkType::UNKNOWN);
        assert(link.target_word == "just_a_word");
    }
    {
        ParsedLink link = manager.parse_link("entry://");
        assert(!link.is_valid);
        assert(link.type == LinkType::ENTRY);
        assert(link.target_word.empty());
    }
}

static void test_navigation_history() {
    CrossReferenceManager manager;
    assert(!manager.can_go_back());
    assert(!manager.can_go_forward());
    assert(manager.get_history().empty());
    assert(manager.current_entry().timestamp == 0);

    manager.navigate_to("word1", "dict1");
    assert(manager.current_entry().word == "word1");
    assert(manager.current_entry().dictionary_id == "dict1");
    assert(!manager.can_go_back());
    assert(!manager.can_go_forward());

    manager.navigate_to("word2", "dict1");
    assert(manager.current_entry().word == "word2");
    assert(manager.can_go_back());
    assert(!manager.can_go_forward());
    assert(manager.navigation_state().back_stack.size() == 1);
    assert(manager.navigation_state().back_stack.front().word == "word1");

    manager.navigate_to("word3", "dict1");
    assert(manager.current_entry().word == "word3");
    assert(manager.navigation_state().back_stack.size() == 2);
    assert(manager.navigation_state().back_stack.front().word == "word2");
    assert(manager.navigation_state().back_stack[1].word == "word1");

    HistoryEntry back = manager.go_back();
    assert(back.word == "word2");
    assert(manager.current_entry().word == "word2");
    assert(manager.can_go_back());
    assert(manager.can_go_forward());

    HistoryEntry back2 = manager.go_back();
    assert(back2.word == "word1");
    assert(manager.current_entry().word == "word1");
    assert(!manager.can_go_back());
    assert(manager.can_go_forward());

    HistoryEntry forward = manager.go_forward();
    assert(forward.word == "word2");
    assert(manager.current_entry().word == "word2");
    assert(manager.can_go_back());
    assert(manager.can_go_forward());
}

static void test_history_max_size() {
    CrossReferenceManager manager;
    manager.set_max_history_size(5);

    for (int i = 0; i < 7; ++i) {
        manager.navigate_to("word" + std::to_string(i), "");
    }

    assert(manager.current_entry().word == "word6");
    assert(manager.navigation_state().back_stack.size() == 5);
    assert(manager.navigation_state().back_stack.front().word == "word5");
    assert(manager.navigation_state().back_stack.back().word == "word1"); // word0 removed
}

static void test_resolve_link_default_and_custom() {
    CrossReferenceManager manager;

    // Default resolution uses internal lookup format.
    assert(manager.resolve_link("entry://hello", "dict1") == "#lookup:hello");
    assert(manager.resolve_link("entry://hello|oxford", "dict1") == "#lookup:hello");
    assert(manager.resolve_link("bword://world", "dict2") == "#lookup:world");
    assert(manager.resolve_link("@@@LINK=alt", "dict2") == "#lookup:alt");
    assert(manager.resolve_link("http://example.com", "dict1") == "http://example.com");

    // Custom resolver receives dictionary context for entry:// links.
    manager.set_link_resolver([](const std::string& word, const std::string& dict_id) {
        return "unidict://lookup?word=" + word + "&dict=" + dict_id;
    });
    assert(manager.resolve_link("entry://hello", "dict1") == "unidict://lookup?word=hello&dict=dict1");
    assert(manager.resolve_link("entry://hello|oxford", "dict1") == "unidict://lookup?word=hello&dict=oxford");
}

static void test_html_link_rewriter() {
    CrossReferenceManager manager;
    HtmlLinkRewriter rewriter(&manager);

    std::string html = R"(
        <div>
            <a href="entry://hello">Hello</a>
            <a href="bword://world">World</a>
            <a href="http://example.com">External</a>
        </div>
    )";

    std::string rewritten = rewriter.rewrite_for_lookup(html, "test_dict");
    assert(rewritten.find("href=\"#lookup:hello\"") != std::string::npos);
    assert(rewritten.find("href=\"#lookup:world\"") != std::string::npos);
    assert(rewritten.find("http://example.com") != std::string::npos);

    std::string display = rewriter.rewrite_for_display(rewritten);
    assert(display.find("href=\"entry://hello\"") != std::string::npos);
    assert(display.find("href=\"entry://world\"") != std::string::npos);
}

static void test_link_pattern_factory() {
    std::string entry_link = LinkPatternFactory::create_entry_link("test", "mydict");
    assert(entry_link == "entry://test|mydict");

    std::string bword_link = LinkPatternFactory::create_bword_link("example");
    assert(bword_link == "bword://example");

    std::string internal_link = LinkPatternFactory::create_internal_link("target");
    assert(internal_link == "@@@LINK=target");

    std::string file_link = LinkPatternFactory::create_file_link("/path/to/file.png");
    assert(file_link == "file:///path/to/file.png");

    assert(LinkPatternFactory::detect_link_type("entry://hello") == LinkType::ENTRY);
    assert(LinkPatternFactory::detect_link_type("bword://test") == LinkType::BWORD);
    assert(LinkPatternFactory::detect_link_type("http://example.com") == LinkType::HTTP);
    assert(LinkPatternFactory::detect_link_type("@@@LINK=word") == LinkType::INTERNAL);
}

static void test_word_variations() {
    WordVariationManager variations;
    variations.add_variations("run", {"running", "ran", "runs"});

    assert(variations.are_variations("run", "running"));
    assert(variations.are_variations("Run", "ran"));
    assert(!variations.are_variations("run", "walk"));

    assert(variations.get_canonical_form("RUN") == "run");
    assert(variations.get_canonical_form("running") == "run");

    auto vars = variations.get_variations("run");
    assert(vars.size() == 3);
}

static void test_history_export_import_roundtrip() {
    CrossReferenceManager original;
    original.navigate_to("word1", "dict1");
    original.navigate_to("word2", "dict2");
    original.navigate_to("word3", "dict3");
    HistoryEntry current_after_back = original.go_back();
    assert(current_after_back.word == "word2");

    const std::string exported = original.export_history();

    CrossReferenceManager restored;
    assert(restored.import_history(exported));
    assert(restored.current_entry().word == "word2");
    assert(restored.current_entry().dictionary_id == "dict2");
    assert(restored.can_go_back());
    assert(restored.can_go_forward());
    assert(restored.navigation_state().back_stack.size() == 1);
    assert(restored.navigation_state().back_stack.front().word == "word1");
    assert(restored.navigation_state().forward_stack.size() == 1);
    assert(restored.navigation_state().forward_stack.front().word == "word3");

    HistoryEntry restored_back = restored.go_back();
    assert(restored_back.word == "word1");
    HistoryEntry restored_forward = restored.go_forward();
    assert(restored_forward.word == "word2");
}

// ===== 边缘分支补齐：sound/file 解析、format 全类型 roundtrip =====
static void test_parse_link_sound_file_and_format_roundtrip() {
    CrossReferenceManager manager;

    // sound://
    {
        ParsedLink link = manager.parse_link("sound://audio/hello.mp3");
        assert(link.is_valid);
        assert(link.type == LinkType::SOUND);
        assert(link.target_word == "audio/hello.mp3");
    }
    {
        ParsedLink link = manager.parse_link("sound://");  // 空 target → invalid
        assert(!link.is_valid && link.type == LinkType::SOUND);
    }
    // file://
    {
        ParsedLink link = manager.parse_link("file://pics/a.png");
        assert(link.is_valid && link.type == LinkType::FILE);
        assert(link.target_word == "pics/a.png");
    }
    {
        ParsedLink link = manager.parse_link("file://");
        assert(!link.is_valid && link.type == LinkType::FILE);
    }
    {
        ParsedLink link = manager.parse_link("https://example.com/dict");
        assert(link.is_valid && link.type == LinkType::HTTP);
        assert(link.target_word == "https://example.com/dict");
    }

    // format_link 全类型（手工构造 ParsedLink 做 roundtrip）
    {
        ParsedLink link;
        link.type = LinkType::INTERNAL;
        link.target_word = "apple";
        assert(manager.format_link(link) == "@@@LINK=apple");
    }
    {
        ParsedLink link;
        link.type = LinkType::ENTRY;
        link.target_word = "hello";
        assert(manager.format_link(link) == "entry://hello");
        link.target_dictionary_id = "oxford";
        assert(manager.format_link(link) == "entry://hello|oxford");
    }
    {
        ParsedLink link;
        link.type = LinkType::BWORD;
        link.target_word = "a b";
        assert(manager.format_link(link) == "bword://a%20b");
        link.target_dictionary_id = "d1";
        assert(manager.format_link(link) == "bword://a%20b?dict=d1");
    }
    {
        ParsedLink link;
        link.type = LinkType::SOUND;
        link.target_word = "x.mp3";
        assert(manager.format_link(link) == "sound://x.mp3");
        link.type = LinkType::FILE;
        link.target_word = "y.png";
        assert(manager.format_link(link) == "file://y.png");
        link.type = LinkType::HTTP;
        link.target_word = "https://e.com";
        assert(manager.format_link(link) == "https://e.com");
        link.type = LinkType::UNKNOWN;
        link.raw_url = "whatever";
        assert(manager.format_link(link) == "whatever");
    }
}

// ===== resolve 边缘：无效链接、UNKNOWN、resolver 返回空回落 =====
static void test_resolve_link_edge_fallbacks() {
    CrossReferenceManager manager;

    // 空链接直接判无效
    {
        ParsedLink link = manager.parse_link("");
        assert(!link.is_valid);
        assert(!manager.is_cross_reference(""));
    }

    // 无效链接 → ""
    ParsedLink invalid;
    invalid.is_valid = false;
    assert(manager.resolve_link(invalid).empty());
    assert(manager.resolve_link(invalid, "d1").empty());

    // UNKNOWN 且有效：不是交叉引用 → resolve 走 default → ""
    {
        ParsedLink link = manager.parse_link("just-a-word");
        assert(link.is_valid && link.type == LinkType::UNKNOWN);
        assert(!manager.is_cross_reference("just-a-word"));
        assert(manager.resolve_link(link).empty());
        assert(manager.resolve_link(link, "d1").empty());
    }

    // resolver 返回空 → 三个 resolve_* 都回落 #lookup:
    manager.set_link_resolver([](const std::string&, const std::string&) {
        return std::string();
    });
    assert(manager.resolve_link("entry://apple", "d1") == "#lookup:apple");
    assert(manager.resolve_link("@@@LINK=apple") == "#lookup:apple");
    assert(manager.resolve_link("bword://apple") == "#lookup:apple");

    // SOUND/FILE/HTTP 不算交叉引用 → 原样返回
    assert(manager.resolve_link("sound://a.mp3", "d1") == "sound://a.mp3");
    assert(manager.resolve_link("file://a.png", "d1") == "file://a.png");
    assert(manager.resolve_link("http://e.com", "d1") == "http://e.com");

    // resolver 命中：internal / bword 也走自定义结果（entry 已在默认测试覆盖）
    CrossReferenceManager hit;
    hit.set_link_resolver([](const std::string& word, const std::string&) {
        return word == "hit" ? std::string("RESOLVED") : std::string();
    });
    assert(hit.resolve_link("@@@LINK=hit") == "RESOLVED");
    assert(hit.resolve_link("bword://hit") == "RESOLVED");
    assert(hit.resolve_link("entry://hit", "d1") == "RESOLVED");
}

// ===== 导航栈：空栈回退/前进、get_history 顺序、clear =====
static void test_navigation_stack_moves() {
    CrossReferenceManager manager;

    // 空栈 go_back / go_forward 无操作
    assert(!manager.can_go_back() && !manager.can_go_forward());
    assert(manager.go_back().word.empty());
    assert(manager.go_forward().word.empty());

    HistoryEntry a;
    a.word = "a";
    a.dictionary_id = "d1";
    a.timestamp = 1;
    manager.navigate_to(a);
    manager.navigate_to("b", "d2");
    manager.navigate_to("c", "d3");

    assert(manager.can_go_back());
    HistoryEntry cur = manager.go_back();
    assert(cur.word == "b" && cur.dictionary_id == "d2");
    assert(manager.can_go_forward());

    cur = manager.go_forward();
    assert(cur.word == "c" && cur.dictionary_id == "d3");

    // get_history：back 正序 + current + forward 逆序
    manager.go_back();  // current=b, forward=[c]
    const auto hist = manager.get_history();
    assert(hist.size() == 3);
    assert(hist[0].word == "a" && hist[1].word == "b" && hist[2].word == "c");

    manager.clear_history();
    assert(!manager.can_go_back() && !manager.can_go_forward());
    assert(manager.get_history().empty());
    assert(manager.navigation_state().total_history() == 1);
}

// ===== import_history 异常 JSON：缺 current/空 word/不闭合/转义引号 =====
static void test_history_import_edge_cases() {
    CrossReferenceManager manager;

    // 无 current 键 → false
    assert(!manager.import_history(R"({"back": []})"));
    // current 对象存在但无 word → clear + false
    assert(!manager.import_history(R"({"current": {"dict": "d"}})"));
    // current 对象不闭合 → 提取失败 → false
    assert(!manager.import_history(R"({"current": {"word": "x")"));
    // back 数组不闭合 → 提取为空，其余照常成功
    assert(manager.import_history(
        R"({"current": {"word": "x", "dict": "d", "time": 5}, "back": [ {"word": "a"})"));

    // 转义引号走 extract 的 escaped 分支 + forward 裁剪（max 作用于 import 后）
    CrossReferenceManager m2;
    m2.set_max_history_size(1);
    const std::string json =
        "{\"current\": {\"word\": \"a\\\"b\", \"dict\": \"d0\", \"time\": 9}, "
        "\"back\": [{\"word\": \"b1\", \"dict\": \"d1\", \"time\": 8}, "
        "{\"word\": \"b\\\"2\", \"dict\": \"d2\", \"time\": 7}], "
        "\"forward\": [{\"word\": \"f1\", \"dict\": \"d3\", \"time\": 6}, "
        "{\"word\": \"f2\", \"dict\": \"d4\", \"time\": 5}]}";
    assert(m2.import_history(json));
    // regex [^"]* 对转义值截断在 \ 处——实现的实际行为
    assert(m2.current_entry().word == "a\\");
    assert(m2.current_entry().dictionary_id == "d0");
    assert(m2.current_entry().timestamp == 9);
    const NavigationState& nav = m2.navigation_state();
    assert(nav.back_stack.size() == 1);
    assert(nav.back_stack[0].word == "b1");
    assert(nav.forward_stack.size() == 1);
    assert(nav.forward_stack[0].word == "f1");

    // bword://query 裸 key 分支（parse_query_params 无 "=" 的参数）
    ParsedLink queried = manager.parse_link("bword://w?flag&dict=d1");
    assert(queried.is_valid);
    assert(queried.target_dictionary_id == "d1");
    assert(queried.params.find("flag") != queried.params.end());
    assert(queried.params["flag"].empty());

    // url_decode 的 '+' → 空格分支（parse_link 里 target 解码）
    ParsedLink plus = manager.parse_link("bword://a+b");
    assert(plus.is_valid);
    assert(plus.target_word == "a b");
}

// ===== HtmlLinkRewriter：extract_links 混合单双引号 + 无效跳过 + display 反改写 =====
static void test_html_rewriter_extract_and_display() {
    CrossReferenceManager manager;
    HtmlLinkRewriter rewriter(&manager);

    const std::string html =
        "<a href=\"entry://apple\">a</a>"
        "<a href='bword://pear'>b</a>"
        "<a href=\"entry://\">empty</a>";  // 空 target → invalid → 跳过
    const auto links = rewriter.extract_links(html);
    assert(links.size() == 2);
    assert(links[0].type == LinkType::ENTRY && links[0].target_word == "apple");
    assert(links[1].type == LinkType::BWORD && links[1].target_word == "pear");

    // 多处替换（倒序 replace 路径）
    const std::string out = rewriter.rewrite_for_display(
        "<a href=\"#lookup:apple\">a</a> mid <a href=\"#lookup:pear\">b</a>");
    assert(out.find("href=\"entry://apple\"") != std::string::npos);
    assert(out.find("href=\"entry://pear\"") != std::string::npos);
    assert(out.find("#lookup:") == std::string::npos);
    assert(out.find("mid") != std::string::npos);
}

// ===== LinkPatternFactory 全接口 + WordVariationManager 文件 roundtrip =====
static void test_link_factory_and_variations_files() {
    assert(LinkPatternFactory::create_internal_link("a") == "@@@LINK=a");
    assert(LinkPatternFactory::create_entry_link("a", "d") == "entry://a|d");
    assert(LinkPatternFactory::create_entry_link("a") == "entry://a");
    assert(LinkPatternFactory::create_bword_link("a b", "d") == "bword://a%20b?dict=d");
    assert(LinkPatternFactory::create_bword_link("a") == "bword://a");
    assert(LinkPatternFactory::create_file_link("p/f.png") == "file://p/f.png");
    assert(LinkPatternFactory::create_sound_link("s.mp3") == "sound://s.mp3");
    assert(LinkPatternFactory::create_http_link("e.com/x") == "http://e.com/x");
    assert(LinkPatternFactory::create_http_link("https://e.com") == "https://e.com");

    assert(LinkPatternFactory::detect_link_type("@@@LINK=a") == LinkType::INTERNAL);
    assert(LinkPatternFactory::detect_link_type("ENTRY://a") == LinkType::ENTRY);
    assert(LinkPatternFactory::detect_link_type("bword://a") == LinkType::BWORD);
    assert(LinkPatternFactory::detect_link_type("sound://a") == LinkType::SOUND);
    assert(LinkPatternFactory::detect_link_type("file://a") == LinkType::FILE);
    assert(LinkPatternFactory::detect_link_type("http://a") == LinkType::HTTP);
    assert(LinkPatternFactory::detect_link_type("HTTPS://a") == LinkType::HTTP);
    assert(LinkPatternFactory::detect_link_type("word") == LinkType::UNKNOWN);

    assert(!LinkPatternFactory::is_valid_link(""));
    assert(!LinkPatternFactory::is_valid_link("   "));
    assert(LinkPatternFactory::is_valid_link("word"));
    assert(LinkPatternFactory::is_valid_link("http://a.com"));

    // WordVariationManager：未注册词、文件 roundtrip、注释/空白 trim
    WordVariationManager vm;
    assert(vm.get_variations("missing").empty());
    assert(vm.get_canonical_form("Missing") == "missing");
    vm.add_variations("run", {"runs", "running"});

    const std::string dir = "build-local";
    std::filesystem::create_directories(dir);
    const std::string path = dir + "/xref_variations.csv";
    assert(vm.save_to_file(path));
    // 目标目录不存在 → ofstream 打不开 → false
    assert(!vm.save_to_file(dir + "/no_such_dir/x.csv"));

    WordVariationManager vm2;
    assert(!vm2.load_from_file(dir + "/no_such_file.csv"));
    assert(vm2.load_from_file(path));
    assert(vm2.are_variations("RUNS", "running"));
    assert(vm2.get_variations("run").size() == 2);

    {
        std::ofstream out(dir + "/xref_variations_raw.csv");
        out << "# comment line\n"
            << "\n"
            << "  walk ,  walks , strolled \n"
            << "solo\n";
    }
    WordVariationManager vm3;
    assert(vm3.load_from_file(dir + "/xref_variations_raw.csv"));
    assert(vm3.are_variations("walks", "strolled"));
    assert(!vm3.are_variations("walk", "run"));
    assert(vm3.get_variations("solo").empty());
    assert(vm3.get_canonical_form("walks") == "walk");
}

int main() {
    test_parse_links();
    test_navigation_history();
    test_history_max_size();
    test_resolve_link_default_and_custom();
    test_html_link_rewriter();
    test_link_pattern_factory();
    test_word_variations();
    test_history_export_import_roundtrip();
    test_parse_link_sound_file_and_format_roundtrip();
    test_resolve_link_edge_fallbacks();
    test_navigation_stack_moves();
    test_history_import_edge_cases();
    test_html_rewriter_extract_and_display();
    test_link_factory_and_variations_files();
    return 0;
}
