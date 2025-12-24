// Cross-reference link handling unit tests (std-only).
// Tests for link parsing, navigation history, and word variations.

#include <cassert>
#include <string>
#include "std/cross_reference_std.h"

using namespace UnidictCoreStd;

void test_parse_entry_link() {
    CrossReferenceManager manager;

    // Basic entry:// link
    ParsedLink link1 = manager.parse_link("entry://hello");
    assert(link1.protocol == "entry");
    assert(link1.target_word == "hello");
    assert(link1.dictionary_id.empty());

    // With dictionary
    ParsedLink link2 = manager.parse_link("entry://world@oxford");
    assert(link2.protocol == "entry");
    assert(link2.target_word == "world");
    assert(link2.dictionary_id == "oxford");

    // Complex word with underscores
    ParsedLink link3 = manager.parse_link("entry://hello_world_test");
    assert(link3.target_word == "hello_world_test");
}

void test_parse_bword_link() {
    CrossReferenceManager manager;

    // Basic bword:// link
    ParsedLink link1 = manager.parse_link("bword://test");
    assert(link1.protocol == "bword");
    assert(link1.target_word == "test");

    // With dictionary
    ParsedLink link2 = manager.parse_link("bword://example@longman");
    assert(link2.protocol == "bword");
    assert(link2.target_word == "example");
    assert(link2.dictionary_id == "longman");
}

void test_parse_at_link() {
    CrossReferenceManager manager;

    // @@@LINK= format
    ParsedLink link1 = manager.parse_link("@@@LINK=alternative");
    assert(link1.protocol == "@@@LINK");
    assert(link1.target_word == "alternative");

    // With spaces
    ParsedLink link2 = manager.parse_link("@@@LINK= two words ");
    assert(link2.target_word == "two words");  // trimmed
}

void test_parse_web_link() {
    CrossReferenceManager manager;

    // HTTP link
    ParsedLink link1 = manager.parse_link("http://example.com");
    assert(link1.protocol == "http");
    assert(link1.target_word == "http://example.com");
    assert(link1.is_external);

    // HTTPS link
    ParsedLink link2 = manager.parse_link("https://example.com/page");
    assert(link2.protocol == "https");
    assert(link2.is_external);

    // FTP link
    ParsedLink link3 = manager.parse_link("ftp://files.example.com/file.zip");
    assert(link3.protocol == "ftp");
    assert(link3.is_external);
}

void test_parse_malformed_link() {
    CrossReferenceManager manager;

    // Empty link
    ParsedLink link1 = manager.parse_link("");
    assert(link1.protocol.empty());
    assert(link1.target_word.empty());

    // No protocol
    ParsedLink link2 = manager.parse_link("just_a_word");
    assert(link2.protocol.empty());

    // Incomplete entry://
    ParsedLink link3 = manager.parse_link("entry://");
    assert(link3.protocol == "entry");
    assert(link3.target_word.empty());
}

void test_navigation_history() {
    CrossReferenceManager manager;

    // Initial state
    assert(!manager.can_go_back());
    assert(!manager.can_go_forward());
    assert(manager.current_position() == -1);
    assert(manager.history_size() == 0);

    // Add first entry
    manager.navigate_to("word1", "dict1");
    assert(manager.current_position() == 0);
    assert(manager.history_size() == 1);
    assert(!manager.can_go_back());
    assert(!manager.can_go_forward());

    // Add second entry
    manager.navigate_to("word2", "dict1");
    assert(manager.current_position() == 1);
    assert(manager.history_size() == 2);
    assert(manager.can_go_back());
    assert(!manager.can_go_forward());

    // Add third entry
    manager.navigate_to("word3", "dict1");
    assert(manager.current_position() == 2);
    assert(manager.history_size() == 3);

    // Go back
    HistoryEntry back = manager.go_back();
    assert(back.word == "word2");
    assert(manager.current_position() == 1);
    assert(manager.can_go_back());
    assert(manager.can_go_forward());

    // Go back again
    HistoryEntry back2 = manager.go_back();
    assert(back2.word == "word1");
    assert(manager.current_position() == 0);
    assert(!manager.can_go_back());
    assert(manager.can_go_forward());

    // Go forward
    HistoryEntry forward = manager.go_forward();
    assert(forward.word == "word2");
    assert(manager.current_position() == 1);
    assert(manager.can_go_back());
    assert(manager.can_go_forward());
}

void test_navigation_branching() {
    CrossReferenceManager manager;

    // A -> B -> C
    manager.navigate_to("A", "");
    manager.navigate_to("B", "");
    manager.navigate_to("C", "");
    assert(manager.history_size() == 3);

    // Go back to B
    manager.go_back();  // to B
    assert(manager.current_position() == 1);

    // Navigate to D from B (A -> B -> D, C is removed)
    manager.navigate_to("D", "");
    assert(manager.history_size() == 3);  // C removed
    assert(manager.can_go_back());
    assert(!manager.can_go_forward());

    // History should be: A, B, D
    auto history = manager.get_history();
    assert(history.size() == 3);
    assert(history[0].word == "A");
    assert(history[1].word == "B");
    assert(history[2].word == "D");
}

void test_history_max_size() {
    CrossReferenceManager manager;
    manager.set_max_history_size(5);

    // Add 7 entries
    for (int i = 0; i < 7; ++i) {
        manager.navigate_to("word" + std::to_string(i), "");
    }

    // Should only keep last 5
    assert(manager.history_size() == 5);
    assert(manager.current_position() == 4);

    auto history = manager.get_history();
    assert(history[0].word == "word2");  // First two removed
    assert(history[4].word == "word6");
}

void test_clear_history() {
    CrossReferenceManager manager;

    manager.navigate_to("word1", "");
    manager.navigate_to("word2", "");
    manager.navigate_to("word3", "");

    assert(manager.history_size() == 3);

    manager.clear_history();

    assert(manager.history_size() == 0);
    assert(manager.current_position() == -1);
    assert(!manager.can_go_back());
    assert(!manager.can_go_forward());
}

void test_resolve_link() {
    CrossReferenceManager manager;

    // entry:// link
    std::string resolved1 = manager.resolve_link("entry://hello", "dict1");
    assert(resolved1.find("hello") != std::string::npos);
    assert(resolved1.find("dict1") != std::string::npos);

    // bword:// link
    std::string resolved2 = manager.resolve_link("bword://world", "dict2");
    assert(resolved2.find("world") != std::string::npos);

    // External link (pass through)
    std::string resolved3 = manager.resolve_link("http://example.com", "dict1");
    assert(resolved3 == "http://example.com");
}

void test_word_variations() {
    WordVariationManager variations;

    // Add variations using vector
    variations.add_variations("word", {"words", "wording", "worded"});
    variations.add_variations("cat", {"cats", "kitten"});
    variations.add_variations("child", {"children", "child's"});

    // Find variations
    auto vars1 = variations.get_variations("word");
    assert(!vars1.empty());
    assert(vars1.size() >= 3);

    // No variations
    auto vars3 = variations.get_variations("nonexistent");
    assert(vars3.empty());
}

void test_html_link_rewriter() {
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

    // entry:// should become unidict://lookup
    assert(rewritten.find("unidict://lookup?word=hello") != std::string::npos);
    assert(rewritten.find("entry://") == std::string::npos);

    // bword:// should become unidict://lookup
    assert(rewritten.find("unidict://lookup?word=world") != std::string::npos);
    assert(rewritten.find("bword://") == std::string::npos);

    // External links preserved
    assert(rewritten.find("http://example.com") != std::string::npos);
}

void test_link_pattern_factory() {
    // Create various link types
    std::string entry_link = LinkPatternFactory::create_entry_link("test", "mydict");
    assert(entry_link.find("entry://test") != std::string::npos);

    std::string bword_link = LinkPatternFactory::create_bword_link("example");
    assert(bword_link.find("bword://example") != std::string::npos);

    std::string internal_link = LinkPatternFactory::create_internal_link("target");
    assert(internal_link.find("@@@LINK=target") != std::string::npos);

    std::string file_link = LinkPatternFactory::create_file_link("/path/to/file.png");
    assert(file_link.find("file://") != std::string::npos);

    // Detect link types
    assert(LinkPatternFactory::detect_link_type("entry://hello") == LinkType::ENTRY);
    assert(LinkPatternFactory::detect_link_type("bword://test") == LinkType::BWORD);
    assert(LinkPatternFactory::detect_link_type("http://example.com") == LinkType::HTTP);
    assert(LinkPatternFactory::detect_link_type("@@@LINK=word") == LinkType::INTERNAL);
}

void test_parse_dictionary_specific_link() {
    CrossReferenceManager manager;

    // Link with dictionary specified
    ParsedLink link = manager.parse_link("entry://computer@longman");
    assert(link.target_word == "computer");
    assert(link.dictionary_id == "longman");
    assert(link.has_dictionary);
}

void test_link_with_special_characters() {
    CrossReferenceManager manager;

    // Words with hyphens
    ParsedLink link1 = manager.parse_link("entry://well-known");
    assert(link1.target_word == "well-known");

    // Words with apostrophes
    ParsedLink link2 = manager.parse_link("entry://don't");
    assert(link2.target_word == "don't");

    // Words with accents (if encoding supported)
    ParsedLink link3 = manager.parse_link("entry://café");
    assert(link3.target_word == "café");
}

void test_navigation_at_boundaries() {
    CrossReferenceManager manager;

    manager.navigate_to("A", "");
    manager.navigate_to("B", "");

    // Go back to start
    manager.go_back();  // to A
    assert(!manager.can_go_back());

    // Try to go back when at start (should return empty entry)
    HistoryEntry empty = manager.go_back();
    assert(empty.word.empty());

    // Go forward
    manager.go_forward();  // to B
    assert(!manager.can_go_forward());

    // Try to go forward when at end
    HistoryEntry empty2 = manager.go_forward();
    assert(empty2.word.empty());
}

void test_current_entry() {
    CrossReferenceManager manager;

    // No current entry
    HistoryEntry current = manager.current_entry();
    assert(current.word.empty());

    manager.navigate_to("test", "dict");
    current = manager.current_entry();
    assert(current.word == "test");
    assert(current.dictionary_id == "dict");
}

void test_variations_are_related() {
    WordVariationManager variations;

    variations.add_variations("run", {"running", "ran", "runs"});

    // Check if variations are detected
    assert(variations.are_variations("run", "running"));
    assert(variations.are_variations("run", "ran"));

    // Not variations
    assert(!variations.are_variations("run", "walk"));
}

int main() {
    test_parse_entry_link();
    test_parse_bword_link();
    test_parse_at_link();
    test_parse_web_link();
    test_parse_malformed_link();
    test_navigation_history();
    test_navigation_branching();
    test_history_max_size();
    test_clear_history();
    test_resolve_link();
    test_word_variations();
    test_html_link_rewriter();
    test_link_pattern_factory();
    test_parse_dictionary_specific_link();
    test_link_with_special_characters();
    test_navigation_at_boundaries();
    test_current_entry();
    test_variations_are_related();

    return 0;
}
