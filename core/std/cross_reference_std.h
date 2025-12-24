// Cross-reference link handling for dictionary entries (std-only).
// Manages inter-dictionary and intra-dictionary word references,
// navigation history (back/forward), and link resolution.

#ifndef UNIDICT_CROSS_REFERENCE_STD_H
#define UNIDICT_CROSS_REFERENCE_STD_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <deque>

namespace UnidictCoreStd {

// Types of cross-reference links
enum class LinkType {
    INTERNAL,      // Same dictionary (@@@LINK= format)
    EXTERNAL,      // Different dictionary
    ENTRY,         // entry:// protocol
    HTTP,          // http:// or https://
    FILE,          // file:// resource
    SOUND,         // sound:// audio resource
    BWORD,         // bword:// (GoldenDict compatible)
    UNKNOWN
};

// Parsed link information
struct ParsedLink {
    LinkType type = LinkType::UNKNOWN;
    std::string target_word;
    std::string target_dictionary_id;
    std::string raw_url;
    std::string fragment;        // #fragment part
    std::unordered_map<std::string, std::string> params;  // Query parameters
    bool is_valid = false;
};

// Navigation history entry
struct HistoryEntry {
    std::string word;
    std::string dictionary_id;   // empty if any/all
    std::string displayed_definition;  // For back navigation restoration
    uint64_t timestamp = 0;
    int scroll_position = 0;     // For restoring scroll position

    // For differentiating entries
    std::string hash() const {
        return word + "\0" + dictionary_id;
    }
};

// Navigation state
struct NavigationState {
    std::deque<HistoryEntry> back_stack;
    std::deque<HistoryEntry> forward_stack;
    HistoryEntry current;

    bool can_go_back() const { return !back_stack.empty(); }
    bool can_go_forward() const { return !forward_stack.empty(); }

    void clear() {
        back_stack.clear();
        forward_stack.clear();
        current = HistoryEntry();
    }

    size_t total_history() const {
        return back_stack.size() + 1 + forward_stack.size();
    }
};

// Link resolver callback
using LinkResolverCallback = std::function<std::string(const std::string& target_word,
                                                       const std::string& dictionary_id)>;

// Cross-reference link manager
class CrossReferenceManager {
public:
    CrossReferenceManager();
    ~CrossReferenceManager() = default;

    // Link parsing
    ParsedLink parse_link(const std::string& link) const;
    std::string format_link(const ParsedLink& link) const;
    bool is_cross_reference(const std::string& url) const;

    // Link resolution
    std::string resolve_link(const std::string& link,
                            const std::string& current_dictionary_id = "") const;
    std::string resolve_link(const ParsedLink& link,
                            const std::string& current_dictionary_id = "") const;

    // Set custom resolver
    void set_link_resolver(LinkResolverCallback resolver) {
        link_resolver_ = std::move(resolver);
    }

    // Navigation history
    void navigate_to(const std::string& word, const std::string& dictionary_id = "");
    void navigate_to(const HistoryEntry& entry);

    HistoryEntry go_back();
    HistoryEntry go_forward();
    bool can_go_back() const { return navigation_.can_go_back(); }
    bool can_go_forward() const { return navigation_.can_go_forward(); }

    const HistoryEntry& current_entry() const { return navigation_.current; }
    const NavigationState& navigation_state() const { return navigation_; }

    // History management
    void clear_history();
    std::vector<HistoryEntry> get_history() const;
    void set_max_history_size(size_t max_size) { max_history_size_ = max_size; }

    // Current context
    void set_current_dictionary(const std::string& dictionary_id) {
        current_dictionary_id_ = dictionary_id;
    }
    std::string get_current_dictionary() const { return current_dictionary_id_; }

    // Link rewriting for HTML
    std::string rewrite_links_in_html(const std::string& html,
                                     const std::string& dictionary_id = "") const;

    // Export/Import history
    std::string export_history() const;  // JSON format
    bool import_history(const std::string& json);

private:
    // Internal link resolution helpers
    std::string resolve_entry_link(const std::string& target,
                                  const std::string& current_dict) const;
    std::string resolve_internal_link(const std::string& target) const;
    std::string resolve_bword_link(const std::string& target) const;

    // Navigation helpers
    void add_to_back_stack(const HistoryEntry& entry);
    void trim_history();

    // URL parsing helpers
    static std::string extract_protocol(const std::string& url);
    static std::unordered_map<std::string, std::string> parse_query_params(
        const std::string& query);
    static std::string url_decode(const std::string& s);

    NavigationState navigation_;
    std::string current_dictionary_id_;
    size_t max_history_size_ = 100;
    LinkResolverCallback link_resolver_;
};

// HTML link rewriter for cross-reference conversion
class HtmlLinkRewriter {
public:
    explicit HtmlLinkRewriter(CrossReferenceManager* manager)
        : manager_(manager) {}

    // Rewrite all links in HTML to internal lookup format
    std::string rewrite_for_lookup(const std::string& html,
                                  const std::string& dictionary_id = "") const;

    // Extract all cross-reference links from HTML
    std::vector<ParsedLink> extract_links(const std::string& html) const;

    // Convert internal lookup links back to displayable URLs
    std::string rewrite_for_display(const std::string& html) const;

private:
    CrossReferenceManager* manager_;
};

// Factory for creating commonly used link patterns
class LinkPatternFactory {
public:
    // Create internal dictionary link (@@@LINK= format)
    static std::string create_internal_link(const std::string& target_word);

    // Create entry:// protocol link
    static std::string create_entry_link(const std::string& target_word,
                                        const std::string& dictionary_id = "");

    // Create bword:// protocol link (GoldenDict compatible)
    static std::string create_bword_link(const std::string& target_word,
                                        const std::string& dictionary_id = "");

    // Create file:// resource link
    static std::string create_file_link(const std::string& resource_path);

    // Create sound:// audio link
    static std::string create_sound_link(const std::string& audio_path);

    // Create HTTP link
    static std::string create_http_link(const std::string& url);

    // Detect link type from URL
    static LinkType detect_link_type(const std::string& url);

    // Validate link format
    static bool is_valid_link(const std::string& url);
};

// Utility class for managing word variations and synonyms
class WordVariationManager {
public:
    WordVariationManager() = default;
    ~WordVariationManager() = default;

    // Add variations for a word
    void add_variations(const std::string& word, const std::vector<std::string>& variations);

    // Get all variations for a word
    std::vector<std::string> get_variations(const std::string& word) const;

    // Check if two words might be variations of each other
    bool are_variations(const std::string& a, const std::string& b) const;

    // Find the canonical form of a word
    std::string get_canonical_form(const std::string& word) const;

    // Load variations from file (one line: word,var1,var2,...)
    bool load_from_file(const std::string& file_path);

    // Export variations to file
    bool save_to_file(const std::string& file_path) const;

private:
    std::unordered_map<std::string, std::vector<std::string>> variations_;
    std::unordered_map<std::string, std::string> canonical_;  // word -> canonical form
};

} // namespace UnidictCoreStd

#endif // UNIDICT_CROSS_REFERENCE_STD_H
