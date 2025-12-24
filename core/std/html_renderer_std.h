// HTML rendering engine for dictionary entries (std-only).
// Provides secure HTML subset rendering with CSS support, resource rewriting,
// and cross-reference link handling for professional dictionary experience.

#ifndef UNIDICT_HTML_RENDERER_STD_H
#define UNIDICT_HTML_RENDERER_STD_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <memory>

namespace UnidictCoreStd {

// Forward declarations
class ResourceResolverStd;

// HTML element types (safe subset for dictionary rendering)
enum class HtmlElementType {
    // Text structure
    DIV, SPAN, P, BR, HR,
    // Headings
    H1, H2, H3, H4, H5, H6,
    // Lists
    UL, OL, LI, DL, DT, DD,
    // Tables
    TABLE, THEAD, TBODY, TR, TH, TD,
    // Text formatting
    B, I, U, S, STRONG, EM, MARK, SMALL, SUB, SUP,
    // Semantic
    CODE, PRE, BLOCKQUOTE, ABBR, ACRONYM,
    // Media (placeholder for resolved resources)
    IMG, AUDIO, VIDEO, SOURCE,
    // Dictionary-specific
    ENTRY, DEFINITION, EXAMPLE, PHONETIC, POS,  // Part of Speech
    // Custom container
    DIV_CLASS, SPAN_CLASS  // with custom class attribute
};

// HTML attribute types (safe subset)
enum class HtmlAttributeType {
    ID, CLASS, STYLE,
    HREF, TARGET, TITLE, ALT, SRC,
    COLSPAN, ROWSPAN, ALIGN, VALIGN,
    DATA_DICT, DATA_WORD,  // For cross-reference links
    LANG, DIR              // Language and text direction
};

// CSS property types (safe subset)
enum class CssPropertyType {
    // Text & Fonts
    CSS_COLOR, CSS_BACKGROUND_COLOR, CSS_FONT_FAMILY, CSS_FONT_SIZE, CSS_FONT_WEIGHT,
    CSS_FONT_STYLE, CSS_TEXT_DECORATION, CSS_TEXT_ALIGN, CSS_TEXT_INDENT, CSS_LINE_HEIGHT,
    // Spacing
    CSS_MARGIN, CSS_MARGIN_TOP, CSS_MARGIN_RIGHT, CSS_MARGIN_BOTTOM, CSS_MARGIN_LEFT,
    CSS_PADDING, CSS_PADDING_TOP, CSS_PADDING_RIGHT, CSS_PADDING_BOTTOM, CSS_PADDING_LEFT,
    // Layout
    CSS_WIDTH, CSS_HEIGHT, CSS_MAX_WIDTH, CSS_MAX_HEIGHT, CSS_DISPLAY, CSS_FLOAT, CSS_CLEAR,
    CSS_POSITION, CSS_TOP, CSS_RIGHT, CSS_BOTTOM, CSS_LEFT, CSS_OVERFLOW,
    // Borders
    CSS_BORDER, CSS_BORDER_TOP, CSS_BORDER_RIGHT, CSS_BORDER_BOTTOM, CSS_BORDER_LEFT,
    CSS_BORDER_COLOR, CSS_BORDER_STYLE, CSS_BORDER_WIDTH, CSS_BORDER_RADIUS,
    // Other
    CSS_OPACITY, CSS_VISIBILITY
};

// Sanitized HTML token
struct HtmlToken {
    enum Type { TEXT, ELEMENT_START, ELEMENT_END, SELF_CLOSING, COMMENT };
    Type type;
    std::string value;           // text content or element name
    std::unordered_map<std::string, std::string> attributes;
    size_t position = 0;         // position in original content
};

// Rendered HTML output
struct RenderedHtml {
    std::string html;           // sanitized HTML string
    std::string text;           // plain text extraction
    std::vector<std::string> linked_words;  // cross-reference targets
    std::unordered_map<std::string, std::string> resources;  // original -> resolved URLs
    bool has_math = false;      // contains mathematical notation
    bool has_audio = false;     // contains audio elements
    bool has_images = false;    // contains image elements
};

// Rendering options
struct HtmlRenderOptions {
    bool allow_css = true;
    bool allow_tables = true;
    bool allow_media = true;
    bool resolve_links = true;   // resolve cross-reference links
    bool extract_text = true;    // extract plain text
    std::string base_url;        // base URL for relative resources
    std::string dictionary_id;   // current dictionary context
    std::function<std::string(const std::string&, const std::string&)> link_resolver;
        // custom link resolver (word, dictionary_id) -> lookup_url
};

// HTML sanitizer and renderer
class HtmlRendererStd {
public:
    HtmlRendererStd();
    explicit HtmlRendererStd(std::shared_ptr<ResourceResolverStd> resource_resolver);
    ~HtmlRendererStd() = default;

    // Main rendering function
    RenderedHtml render(const std::string& html, const HtmlRenderOptions& options = {}) const;

    // Sanitization
    std::string sanitize(const std::string& html) const;
    std::string strip_tags(const std::string& html) const;
    std::string extract_text(const std::string& html) const;

    // Link handling
    std::string rewrite_links(const std::string& html, const std::string& dictionary_id) const;
    std::string resolve_cross_reference(const std::string& link, const std::string& dictionary_id) const;

    // Resource URL rewriting
    std::string rewrite_resource_urls(const std::string& html,
                                     const std::unordered_map<std::string, std::string>& url_map) const;

    // Configuration
    void add_allowed_tag(const std::string& tag);
    void remove_allowed_tag(const std::string& tag);
    void add_allowed_attribute(const std::string& attr);
    void add_allowed_css_property(const std::string& property);
    void set_max_text_length(size_t max_length) { max_text_length_ = max_length; }

    // Validation
    bool is_tag_allowed(const std::string& tag) const;
    bool is_attribute_allowed(const std::string& attr) const;
    bool is_css_property_allowed(const std::string& property) const;

    // Custom link resolver
    void set_link_resolver(std::function<std::string(const std::string&, const std::string&)> resolver) {
        custom_link_resolver_ = std::move(resolver);
    }

private:
    // Tokenization
    std::vector<HtmlToken> tokenize(const std::string& html) const;

    // Sanitization helpers
    bool sanitize_token(HtmlToken& token) const;
    bool sanitize_attribute(const std::string& name, std::string& value) const;
    bool sanitize_css_style(std::string& style) const;
    std::string decode_html_entities(const std::string& s) const;
    std::string encode_html_entities(const std::string& s) const;

    // URL validation
    bool is_safe_url(const std::string& url) const;
    bool is_javascript_url(const std::string& url) const;
    std::string normalize_url(const std::string& url) const;

    // Cross-reference detection (e.g., @@@LINK=, <a href="entry://word">)
    bool is_cross_reference_link(const std::string& url) const;
    std::string extract_link_target(const std::string& url) const;

    // Whitelists
    std::unordered_set<std::string> allowed_tags_;
    std::unordered_set<std::string> allowed_attributes_;
    std::unordered_set<std::string> allowed_css_properties_;
    std::unordered_set<std::string> allowed_protocols_;

    // Resource resolver (for .mdd, StarDict resources)
    std::shared_ptr<ResourceResolverStd> resource_resolver_;

    // Custom link resolver
    std::function<std::string(const std::string&, const std::string&)> custom_link_resolver_;

    // Limits
    size_t max_text_length_ = 100000;  // 100KB default
    size_t max_nesting_depth_ = 32;
};

// Resource resolver for dictionary resources (images, audio, etc.)
class ResourceResolverStd {
public:
    struct ResourceInfo {
        std::string local_path;     // path to cached/local file
        std::string mime_type;
        size_t size = 0;
        bool is_cached = false;
        bool is_external = false;   // loaded from internet
    };

    ResourceResolverStd() = default;
    virtual ~ResourceResolverStd() = default;

    // Resolve a resource URL to local file path
    virtual ResourceInfo resolve(const std::string& url, const std::string& dictionary_id) = 0;

    // Check if resource exists
    virtual bool exists(const std::string& url, const std::string& dictionary_id) = 0;

    // Get data URL for embedding (data:image/png;base64,...)
    virtual std::string get_data_url(const std::string& url, const std::string& dictionary_id);

    // Cache management
    virtual bool preload_resources(const std::vector<std::string>& urls, const std::string& dictionary_id);
    virtual void clear_cache(const std::string& dictionary_id = "");
    virtual std::vector<std::string> get_cached_resources() const;

    // Resource URL patterns
    virtual bool is_dictionary_resource(const std::string& url) const;
    virtual std::string extract_resource_key(const std::string& url) const;
};

// Default resource resolver implementation
class DefaultResourceResolverStd : public ResourceResolverStd {
public:
    DefaultResourceResolverStd();

    // Resource resolution from .mdd files or StarDict resource directories
    ResourceInfo resolve(const std::string& url, const std::string& dictionary_id) override;
    bool exists(const std::string& url, const std::string& dictionary_id) override;
    std::string get_data_url(const std::string& url, const std::string& dictionary_id) override;
    bool preload_resources(const std::vector<std::string>& urls, const std::string& dictionary_id) override;
    void clear_cache(const std::string& dictionary_id = "") override;
    std::vector<std::string> get_cached_resources() const override;
    bool is_dictionary_resource(const std::string& url) const override;
    std::string extract_resource_key(const std::string& url) const override;

    // Register dictionary resource directory
    void register_dictionary(const std::string& dictionary_id, const std::string& resource_path);
    void unregister_dictionary(const std::string& dictionary_id);

    // Set cache directory
    void set_cache_directory(const std::string& cache_dir);
    std::string get_cache_directory() const { return cache_dir_; }

private:
    std::string find_resource_file(const std::string& key, const std::string& dictionary_id) const;
    std::string normalize_key(const std::string& key) const;

    std::unordered_map<std::string, std::string> dictionary_resources_;  // dict_id -> resource_path
    std::string cache_dir_;
};

// Factory for creating renderer with resource resolver
class HtmlRendererFactory {
public:
    static std::unique_ptr<HtmlRendererStd> create_with_defaults();
    static std::unique_ptr<HtmlRendererStd> create_with_resource_resolver(
        std::shared_ptr<ResourceResolverStd> resolver);
    static std::unique_ptr<HtmlRendererStd> create_strict();  // minimal allowed tags
    static std::unique_ptr<HtmlRendererStd> create_permissive();  // more permissive for trusted dicts
};

} // namespace UnidictCoreStd

#endif // UNIDICT_HTML_RENDERER_STD_H
