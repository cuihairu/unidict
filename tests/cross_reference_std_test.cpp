// Cross-reference link handling unit tests (std-only).
// Tests for link parsing, navigation history, and link rewriting.

#include <cassert>
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

int main() {
    test_parse_links();
    test_navigation_history();
    test_history_max_size();
    test_resolve_link_default_and_custom();
    test_html_link_rewriter();
    test_link_pattern_factory();
    test_word_variations();
    return 0;
}
