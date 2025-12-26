// MDict .mdd resource file parser (std-only).
// Handles extraction and caching of resources (images, audio, etc.)
// from MDict resource files (.mdd) for dictionary rendering.

#ifndef UNIDICT_MDD_RESOURCE_STD_H
#define UNIDICT_MDD_RESOURCE_STD_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>

namespace UnidictCoreStd {

// Resource entry in .mdd file
struct MddResourceEntry {
    std::string key;            // normalized resource key (e.g., "images/hello.png")
    uint64_t offset = 0;        // offset in .mdd file
    uint64_t size = 0;          // compressed size
    uint64_t uncompressed_size = 0;  // uncompressed size (0 if not compressed)
    uint32_t block_id = 0;      // block ID (for multi-block .mdd files)
    bool is_compressed = false;
};

// .mdd file header info
struct MddHeaderInfo {
    std::string magic;          // \x1b\x23\x45 or similar
    uint32_t header_len = 0;
    uint32_t version = 0;
    uint32_t num_blocks = 0;    // number of resource blocks
    uint64_t total_size = 0;
    bool encrypted = false;
    std::string encryption_type;
};

// Cache entry for extracted resource
struct CachedResource {
    std::string key;
    std::string local_path;     // path to cached file
    std::string mime_type;
    size_t size = 0;
    uint64_t last_used = 0;     // timestamp
    uint64_t access_count = 0;
};

// MDD resource parser
class MddResourceParser {
public:
    MddResourceParser();
    ~MddResourceParser();

    // Load .mdd file
    bool load(const std::string& mdd_path);
    void unload();
    bool is_loaded() const { return loaded_; }

    // Resource lookup
    bool has_resource(const std::string& key) const;
    std::vector<uint8_t> get_resource(const std::string& key) const;
    std::string get_resource_as_string(const std::string& key) const;

    // Resource metadata
    MddResourceEntry get_resource_info(const std::string& key) const;
    std::vector<std::string> list_resources(const std::string& prefix = "") const;
    int resource_count() const { return static_cast<int>(resources_.size()); }

    // Cache management
    bool extract_to_cache(const std::string& key, const std::string& cache_dir);
    bool extract_all_to_cache(const std::string& cache_dir, int max_count = -1);

    // Header info
    const MddHeaderInfo& header_info() const { return header_; }

    // File path
    std::string file_path() const { return mdd_path_; }

    // Static utility for MIME type detection
    static std::string detect_mime_type(const std::string& key);

private:
    // Parse .mdd header
    bool parse_header();
    bool parse_v1_header();
    bool parse_v2_header();
    bool parse_simplekv_fallback();

    // Parse resource blocks
    bool parse_resource_blocks();
    bool parse_single_block();
    bool parse_multi_block();

    // Decompression
    bool decompress_resource(const MddResourceEntry& entry, std::vector<uint8_t>& out) const;

    // Key normalization
    static std::string normalize_key(const std::string& key);

    // Read from file
    bool read_bytes(uint64_t offset, size_t size, std::vector<uint8_t>& out) const;
    std::string read_string(uint64_t offset, size_t size) const;

    bool loaded_ = false;
    std::string mdd_path_;
    std::unordered_map<std::string, MddResourceEntry> resources_;
    std::vector<std::string> resource_keys_;  // for ordered iteration
    MddHeaderInfo header_;

    // File handle
    mutable std::FILE* file_ = nullptr;
};

// Resource cache manager
class MddResourceCache {
public:
    MddResourceCache();
    explicit MddResourceCache(const std::string& cache_dir);

    // Set cache directory
    void set_cache_directory(const std::string& cache_dir);
    std::string get_cache_directory() const { return cache_dir_; }

    // Cache operations
    bool cache_resource(const std::vector<uint8_t>& data, const std::string& key,
                       const std::string& mime_type);
    bool cache_resource(const std::string& data, const std::string& key,
                       const std::string& mime_type);

    std::string get_cached_path(const std::string& key) const;
    bool is_cached(const std::string& key) const;
    std::vector<uint8_t> get_from_cache(const std::string& key) const;

    // Cache management
    void clear_cache(const std::string& prefix = "");
    void prune_by_size(size_t max_bytes);
    void prune_by_age(uint64_t max_age_seconds);
    void prune_by_access(uint64_t min_access_count = 1);

    // Cache statistics
    size_t get_cache_size() const;
    int get_cached_count() const;
    std::vector<CachedResource> get_cache_info() const;

    // Persist cache metadata
    bool save_metadata(const std::string& meta_path) const;
    bool load_metadata(const std::string& meta_path);

    // Access helpers (for MddResourceManager)
    void update_access_time(const std::string& key);
    void increment_access_count(const std::string& key);

private:
    // Cache file path generation
    std::string get_cache_file_path(const std::string& key) const;

    std::string cache_dir_;
    std::unordered_map<std::string, CachedResource> cache_meta_;

    // Cache limits
    size_t max_cache_size_ = 100 * 1024 * 1024;  // 100MB default
    uint64_t max_cache_age_ = 30 * 24 * 3600;    // 30 days default
};

// Integrated MDD resource manager (parser + cache)
class MddResourceManager {
public:
    MddResourceManager();
    ~MddResourceManager() = default;

    // Load .mdd file
    bool load_mdd(const std::string& mdd_path, const std::string& dictionary_id);
    bool unload_mdd(const std::string& dictionary_id);
    bool has_mdd(const std::string& dictionary_id) const;

    // Resource retrieval (with caching)
    std::string get_resource_path(const std::string& key, const std::string& dictionary_id);
    std::vector<uint8_t> get_resource_data(const std::string& key, const std::string& dictionary_id);
    bool has_resource(const std::string& key, const std::string& dictionary_id) const;

    // List resources
    std::vector<std::string> list_resources(const std::string& dictionary_id,
                                            const std::string& prefix = "") const;

    // Cache management
    void set_cache_directory(const std::string& cache_dir);
    void clear_cache(const std::string& dictionary_id = "");
    void prune_cache(size_t max_bytes);

    // Statistics
    size_t get_total_cache_size() const;
    int get_total_resource_count() const;

private:
    struct DictionaryResources {
        std::unique_ptr<MddResourceParser> parser;
        std::string mdd_path;
    };

    std::unordered_map<std::string, DictionaryResources> dictionaries_;
    std::unique_ptr<MddResourceCache> cache_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_MDD_RESOURCE_STD_H
