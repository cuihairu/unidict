// MDict .mdd resource file parser implementation (std-only).

#include "mdd_resource_std.h"
#include "path_utils_std.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <ctime>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace fs = std::filesystem;

namespace UnidictCoreStd {

// ============================================================================
// MddResourceParser Implementation
// ============================================================================

namespace {
    // MDD magic bytes
    const uint8_t MDD_MAGIC_V1[] = {0x1b, 0x23, 0x45};  // Older format
    const uint8_t MDD_MAGIC_V2[] = {0x1b, 0x23, 0x01};  // Newer format

    // Block signatures
    const char* BLOCK_SIGNATURE_RBLK = "RBLK";
    const char* BLOCK_SIGNATURE_RBCT = "RBCT";

    // Maximum resource size to load into memory (10MB)
    const size_t MAX_RESOURCE_SIZE = 10 * 1024 * 1024;

    // Big-endian reading helpers
    inline uint16_t be16(const uint8_t* p) {
        return (uint16_t)p[0] << 8 | p[1];
    }

    inline uint32_t be32(const uint8_t* p) {
        return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
               (uint32_t)p[2] << 8 | p[3];
    }

    inline uint64_t be64(const uint8_t* p) {
        return (uint64_t)p[0] << 56 | (uint64_t)p[1] << 48 |
               (uint64_t)p[2] << 40 | (uint64_t)p[3] << 32 |
               (uint64_t)p[4] << 24 | (uint64_t)p[5] << 16 |
               (uint64_t)p[6] << 8 | p[7];
    }

    // Simple decompression using zlib
    bool decompress_zlib(const uint8_t* input, size_t input_len,
                        std::vector<uint8_t>& output) {
#ifdef USE_ZLIB
        z_stream stream = {};
        if (inflateInit2(&stream, 15 + 32) != Z_OK) {
            return false;
        }

        stream.avail_in = static_cast<uInt>(input_len);
        stream.next_in = const_cast<uint8_t*>(input);

        // Estimate output size (start with 2x input)
        output.resize(std::max(input_len * 2, size_t(1024)));
        size_t output_pos = 0;

        int ret;
        do {
            stream.avail_out = static_cast<uInt>(output.size() - output_pos);
            stream.next_out = output.data() + output_pos;

            ret = inflate(&stream, Z_NO_FLUSH);

            if (ret == Z_OK || ret == Z_STREAM_END) {
                output_pos = stream.total_out;
                if (ret == Z_OK && stream.avail_out == 0) {
                    // Need more output space
                    output.resize(output.size() * 2);
                }
            }
        } while (ret == Z_OK);

        inflateEnd(&stream);

        if (ret != Z_STREAM_END) {
            return false;
        }

        output.resize(stream.total_out);
        return true;
#else
        // Fallback: copy input to output (assuming uncompressed)
        output.assign(input, input + input_len);
        return true;
#endif
    }
}

MddResourceParser::MddResourceParser() {
}

bool MddResourceParser::load(const std::string& mdd_path) {
    unload();

    mdd_path_ = mdd_path;
    file_ = std::fopen(mdd_path.c_str(), "rb");
    if (!file_) {
        return false;
    }

    if (!parse_header()) {
        std::fclose(file_);
        file_ = nullptr;
        return false;
    }

    if (!parse_resource_blocks()) {
        std::fclose(file_);
        file_ = nullptr;
        return false;
    }

    loaded_ = true;
    return true;
}

void MddResourceParser::unload() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
    resources_.clear();
    resource_keys_.clear();
    loaded_ = false;
}

MddResourceParser::~MddResourceParser() {
    unload();
}

bool MddResourceParser::parse_header() {
    // Read first 4 bytes to detect magic
    uint8_t magic[4] = {0};
    if (std::fread(magic, 1, 4, file_) != 4) {
        return false;
    }
    std::rewind(file_);

    // Detect version
    if (std::memcmp(magic, MDD_MAGIC_V1, 3) == 0) {
        header_.magic = std::string(reinterpret_cast<char*>(magic), 3);
        return parse_v1_header();
    } else if (std::memcmp(magic, MDD_MAGIC_V2, 3) == 0) {
        header_.magic = std::string(reinterpret_cast<char*>(magic), 3);
        return parse_v2_header();
    }

    return false;
}

bool MddResourceParser::parse_v1_header() {
    // V1 format: magic (3) + header_len (4) + version (4) + ...
    uint8_t buf[12] = {0};
    if (std::fread(buf, 1, 12, file_) != 12) {
        return false;
    }

    header_.header_len = be32(buf);
    header_.version = be32(buf + 4);

    // Skip to end of header
    uint32_t remaining = header_.header_len - 12;
    if (remaining > 0) {
        std::fseek(file_, remaining, SEEK_CUR);
    }

    // Get file size
    std::fseek(file_, 0, SEEK_END);
    header_.total_size = std::ftell(file_);
    std::fseek(file_, header_.header_len, SEEK_SET);

    return true;
}

bool MddResourceParser::parse_v2_header() {
    // V2 format: magic (3) + header_len (2) + version (2) + ...
    uint8_t buf[8] = {0};
    if (std::fread(buf, 1, 8, file_) != 8) {
        return false;
    }

    header_.header_len = be16(buf);
    header_.version = be16(buf + 2);

    // Skip to end of header
    uint32_t remaining = header_.header_len - 8;
    if (remaining > 0) {
        std::fseek(file_, remaining, SEEK_CUR);
    }

    // Get file size
    std::fseek(file_, 0, SEEK_END);
    header_.total_size = std::ftell(file_);
    std::fseek(file_, header_.header_len, SEEK_SET);

    return true;
}

bool MddResourceParser::parse_resource_blocks() {
    // Read initial bytes to detect format
    uint8_t sig[8] = {0};
    long pos = std::ftell(file_);
    if (std::fread(sig, 1, 4, file_) != 4) {
        return false;
    }
    std::fseek(file_, pos, SEEK_SET);

    // Check for RBCT signature (multi-block format)
    if (std::memcmp(sig, BLOCK_SIGNATURE_RBCT, 4) == 0) {
        return parse_multi_block();
    }

    // Otherwise assume single-block format
    return parse_single_block();
}

bool MddResourceParser::parse_single_block() {
    // Single block format: list of { key_len, key, offset, size }
    // This is a simplified format for smaller .mdd files

    long start_pos = std::ftell(file_);
    std::fseek(file_, 0, SEEK_END);
    long end_pos = std::ftell(file_);
    std::fseek(file_, start_pos, SEEK_SET);

    while (std::ftell(file_) < end_pos) {
        // Read key length
        uint8_t len_buf[2];
        if (std::fread(len_buf, 1, 2, file_) != 2) {
            break;
        }
        uint16_t key_len = be16(len_buf);
        if (key_len == 0 || key_len > 1024) {
            break;  // Invalid key length
        }

        // Read key
        std::string key(key_len, '\0');
        if (std::fread(&key[0], 1, key_len, file_) != key_len) {
            break;
        }

        // Read offset and size
        uint8_t entry_buf[16];
        if (std::fread(entry_buf, 1, 16, file_) != 16) {
            break;
        }
        uint64_t offset = be64(entry_buf);
        uint64_t size = be64(entry_buf + 8);

        // Normalize and store
        std::string normalized = normalize_key(key);
        MddResourceEntry entry;
        entry.key = normalized;
        entry.offset = offset;
        entry.size = size;
        entry.is_compressed = false;

        resources_[normalized] = entry;
        resource_keys_.push_back(normalized);
    }

    return !resources_.empty();
}

bool MddResourceParser::parse_multi_block() {
    // Multi-block format: RBCT + num_blocks + repeated { RBLK + comp_len + data }

    // Read RBCT signature
    char sig[4];
    if (std::fread(sig, 1, 4, file_) != 4 || std::memcmp(sig, BLOCK_SIGNATURE_RBCT, 4) != 0) {
        return false;
    }

    // Read number of blocks
    uint8_t buf[4];
    if (std::fread(buf, 1, 4, file_) != 4) {
        return false;
    }
    uint32_t num_blocks = be32(buf);
    header_.num_blocks = num_blocks;

    // Parse each block
    for (uint32_t block_id = 0; block_id < num_blocks; ++block_id) {
        // Read RBLK signature
        if (std::fread(sig, 1, 4, file_) != 4 || std::memcmp(sig, BLOCK_SIGNATURE_RBLK, 4) != 0) {
            break;
        }

        // Read compressed length
        if (std::fread(buf, 1, 4, file_) != 4) {
            break;
        }
        uint32_t comp_len = be32(buf);

        // Read compressed data
        std::vector<uint8_t> comp_data(comp_len);
        if (std::fread(comp_data.data(), 1, comp_len, file_) != comp_len) {
            break;
        }

        // Decompress
        std::vector<uint8_t> block_data;
        if (!decompress_zlib(comp_data.data(), comp_len, block_data)) {
            continue;
        }

        // Parse resource entries from decompressed block
        const uint8_t* p = block_data.data();
        const uint8_t* end = p + block_data.size();

        while (p + 2 <= end) {
            uint16_t key_len = be16(p);
            p += 2;

            if (key_len == 0 || p + key_len > end) {
                break;
            }

            std::string key(reinterpret_cast<const char*>(p), key_len);
            p += key_len;

            if (p + 16 > end) {
                break;
            }

            uint64_t offset = be64(p);
            uint64_t size = be64(p + 8);
            p += 16;

            std::string normalized = normalize_key(key);
            MddResourceEntry entry;
            entry.key = normalized;
            entry.offset = offset;
            entry.size = size;
            entry.block_id = block_id;
            entry.is_compressed = false;

            resources_[normalized] = entry;
            resource_keys_.push_back(normalized);
        }
    }

    return !resources_.empty();
}

bool MddResourceParser::has_resource(const std::string& key) const {
    std::string normalized = normalize_key(key);
    return resources_.find(normalized) != resources_.end();
}

std::vector<uint8_t> MddResourceParser::get_resource(const std::string& key) const {
    std::vector<uint8_t> result;

    std::string normalized = normalize_key(key);
    auto it = resources_.find(normalized);
    if (it == resources_.end()) {
        return result;
    }

    const auto& entry = it->second;

    // Sanity check size
    if (entry.size > MAX_RESOURCE_SIZE) {
        return result;
    }

    // Read raw data
    std::vector<uint8_t> data;
    if (!read_bytes(entry.offset, entry.size, data)) {
        return result;
    }
    result = data;

    // Decompress if needed
    if (entry.is_compressed && !result.empty()) {
        std::vector<uint8_t> decompressed;
        if (decompress_resource(entry, decompressed)) {
            result = std::move(decompressed);
        }
    }

    return result;
}

std::string MddResourceParser::get_resource_as_string(const std::string& key) const {
    auto data = get_resource(key);
    if (data.empty()) {
        return "";
    }
    return std::string(data.begin(), data.end());
}

MddResourceEntry MddResourceParser::get_resource_info(const std::string& key) const {
    std::string normalized = normalize_key(key);
    auto it = resources_.find(normalized);
    if (it != resources_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> MddResourceParser::list_resources(const std::string& prefix) const {
    std::vector<std::string> result;

    for (const auto& key : resource_keys_) {
        if (prefix.empty() || key.find(prefix) == 0) {
            result.push_back(key);
        }
    }

    return result;
}

bool MddResourceParser::extract_to_cache(const std::string& key, const std::string& cache_dir) {
    auto data = get_resource(key);
    if (data.empty()) {
        return false;
    }

    // Create cache directory if needed
    fs::create_directories(cache_dir);

    // Generate cache file path
    std::string cache_path = cache_dir + "/" + key;
    std::replace(cache_path.begin(), cache_path.end(), '/', '-');

    // Write to file
    std::ofstream out(cache_path, std::ios::binary);
    if (!out) {
        return false;
    }

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return out.good();
}

bool MddResourceParser::extract_all_to_cache(const std::string& cache_dir, int max_count) {
    fs::create_directories(cache_dir);

    int count = 0;
    for (const auto& key : resource_keys_) {
        if (max_count > 0 && count >= max_count) {
            break;
        }

        if (extract_to_cache(key, cache_dir)) {
            ++count;
        }
    }

    return count > 0;
}

bool MddResourceParser::decompress_resource(const MddResourceEntry& entry,
                                           std::vector<uint8_t>& out) const {
    std::vector<uint8_t> data;
    if (!read_bytes(entry.offset, entry.size, data)) {
        return false;
    }

    return decompress_zlib(data.data(), data.size(), out);
}

std::string MddResourceParser::normalize_key(const std::string& key) {
    std::string result = key;

    // Convert backslashes to forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');

    // Remove leading slashes
    size_t start = result.find_first_not_of("/");
    if (start != std::string::npos) {
        result = result.substr(start);
    }

    // Remove URL protocol prefixes
    std::vector<std::string> prefixes = {
        "file://", "sound://", "entry://", "bword://", "gxres://", "mdd://"
    };

    for (const auto& prefix : prefixes) {
        if (result.size() >= prefix.size()) {
            std::string lower_prefix = prefix;
            std::transform(lower_prefix.begin(), lower_prefix.end(), lower_prefix.begin(), ::tolower);

            std::string lower_result = result.substr(0, prefix.size());
            std::transform(lower_result.begin(), lower_result.end(), lower_result.begin(), ::tolower);

            if (lower_result == lower_prefix) {
                result = result.substr(prefix.size());
                break;
            }
        }
    }

    // Remove query strings and fragments
    size_t query_pos = result.find('?');
    if (query_pos != std::string::npos) {
        result = result.substr(0, query_pos);
    }

    size_t frag_pos = result.find('#');
    if (frag_pos != std::string::npos) {
        result = result.substr(0, frag_pos);
    }

    // Convert to lowercase for case-insensitive lookup
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);

    return result;
}

std::string MddResourceParser::detect_mime_type(const std::string& key) {
    std::string lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Image types
    if (lower.find(".png") != std::string::npos) return "image/png";
    if (lower.find(".jpg") != std::string::npos || lower.find(".jpeg") != std::string::npos) return "image/jpeg";
    if (lower.find(".gif") != std::string::npos) return "image/gif";
    if (lower.find(".svg") != std::string::npos) return "image/svg+xml";
    if (lower.find(".webp") != std::string::npos) return "image/webp";
    if (lower.find(".bmp") != std::string::npos) return "image/bmp";
    if (lower.find(".ico") != std::string::npos) return "image/x-icon";

    // Audio types
    if (lower.find(".mp3") != std::string::npos) return "audio/mpeg";
    if (lower.find(".wav") != std::string::npos) return "audio/wav";
    if (lower.find(".ogg") != std::string::npos) return "audio/ogg";
    if (lower.find(".m4a") != std::string::npos) return "audio/mp4";
    if (lower.find(".aac") != std::string::npos) return "audio/aac";

    // Video types
    if (lower.find(".mp4") != std::string::npos) return "video/mp4";
    if (lower.find(".webm") != std::string::npos) return "video/webm";
    if (lower.find(".ogv") != std::string::npos) return "video/ogg";

    // Default
    return "application/octet-stream";
}

bool MddResourceParser::read_bytes(uint64_t offset, size_t size, std::vector<uint8_t>& out) const {
    if (!file_) {
        return false;
    }

    std::fseek(file_, static_cast<long>(offset), SEEK_SET);
    out.resize(size);

    return std::fread(out.data(), 1, size, file_) == size;
}

std::string MddResourceParser::read_string(uint64_t offset, size_t size) const {
    std::vector<uint8_t> data;
    if (!read_bytes(offset, size, data)) {
        return "";
    }
    return std::string(data.begin(), data.end());
}

// ============================================================================
// MddResourceCache Implementation
// ============================================================================

MddResourceCache::MddResourceCache() {
    // Set default cache directory
    const char* home = std::getenv("HOME");
    if (home) {
        cache_dir_ = std::string(home) + "/.cache/unidict/mdd_resources";
    } else {
        cache_dir_ = "/tmp/unidict_mdd_cache";
    }
}

MddResourceCache::MddResourceCache(const std::string& cache_dir)
    : cache_dir_(cache_dir) {
}

void MddResourceCache::set_cache_directory(const std::string& cache_dir) {
    cache_dir_ = cache_dir;
    fs::create_directories(cache_dir_);
}

bool MddResourceCache::cache_resource(const std::vector<uint8_t>& data,
                                     const std::string& key,
                                     const std::string& mime_type) {
    if (data.empty()) {
        return false;
    }

    fs::create_directories(cache_dir_);

    std::string cache_path = get_cache_file_path(key);

    // Write to file
    std::ofstream out(cache_path, std::ios::binary);
    if (!out) {
        return false;
    }

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (!out.good()) {
        return false;
    }

    // Update metadata
    CachedResource info;
    info.key = key;
    info.local_path = cache_path;
    info.mime_type = mime_type;
    info.size = data.size();
    info.last_used = std::time(nullptr);
    info.access_count = 1;

    cache_meta_[key] = info;

    return true;
}

bool MddResourceCache::cache_resource(const std::string& data,
                                     const std::string& key,
                                     const std::string& mime_type) {
    std::vector<uint8_t> vec(data.begin(), data.end());
    return cache_resource(vec, key, mime_type);
}

std::string MddResourceCache::get_cached_path(const std::string& key) const {
    auto it = cache_meta_.find(key);
    if (it != cache_meta_.end()) {
        return it->second.local_path;
    }
    return "";
}

bool MddResourceCache::is_cached(const std::string& key) const {
    return cache_meta_.find(key) != cache_meta_.end();
}

std::vector<uint8_t> MddResourceCache::get_from_cache(const std::string& key) const {
    std::string path = get_cached_path(key);
    if (path.empty()) {
        return {};
    }

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return {};
    }

    size_t size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    if (!in.read(reinterpret_cast<char*>(data.data()), size)) {
        return {};
    }

    return data;
}

void MddResourceCache::clear_cache(const std::string& prefix) {
    if (prefix.empty()) {
        // Clear all
        for (const auto& entry : cache_meta_) {
            fs::remove(entry.second.local_path);
        }
        cache_meta_.clear();
    } else {
        // Clear matching entries
        auto it = cache_meta_.begin();
        while (it != cache_meta_.end()) {
            if (it->first.find(prefix) == 0) {
                fs::remove(it->second.local_path);
                it = cache_meta_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void MddResourceCache::prune_by_size(size_t max_bytes) {
    size_t total_size = get_cache_size();
    if (total_size <= max_bytes) {
        return;
    }

    // Sort by last used time (LRU)
    std::vector<CachedResource*> sorted;
    for (auto& entry : cache_meta_) {
        sorted.push_back(&entry.second);
    }

    std::sort(sorted.begin(), sorted.end(),
        [](const CachedResource* a, const CachedResource* b) {
            return a->last_used < b->last_used;
        });

    // Remove oldest entries until under limit
    for (auto* entry : sorted) {
        if (total_size <= max_bytes) {
            break;
        }

        fs::remove(entry->local_path);
        total_size -= entry->size;
        cache_meta_.erase(entry->key);
    }
}

void MddResourceCache::prune_by_age(uint64_t max_age_seconds) {
    uint64_t now = std::time(nullptr);

    auto it = cache_meta_.begin();
    while (it != cache_meta_.end()) {
        if (now - it->second.last_used > max_age_seconds) {
            fs::remove(it->second.local_path);
            it = cache_meta_.erase(it);
        } else {
            ++it;
        }
    }
}

void MddResourceCache::prune_by_access(uint64_t min_access_count) {
    auto it = cache_meta_.begin();
    while (it != cache_meta_.end()) {
        if (it->second.access_count < min_access_count) {
            fs::remove(it->second.local_path);
            it = cache_meta_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t MddResourceCache::get_cache_size() const {
    size_t total = 0;
    for (const auto& entry : cache_meta_) {
        total += entry.second.size;
    }
    return total;
}

int MddResourceCache::get_cached_count() const {
    return static_cast<int>(cache_meta_.size());
}

std::vector<CachedResource> MddResourceCache::get_cache_info() const {
    std::vector<CachedResource> result;
    result.reserve(cache_meta_.size());

    for (const auto& entry : cache_meta_) {
        result.push_back(entry.second);
    }

    return result;
}

std::string MddResourceCache::get_cache_file_path(const std::string& key) const {
    // Generate safe filename from key
    std::string filename = key;

    // Replace special characters
    std::replace(filename.begin(), filename.end(), '/', '-');
    std::replace(filename.begin(), filename.end(), '\\', '-');
    std::replace(filename.begin(), filename.end(), ':', '-');
    std::replace(filename.begin(), filename.end(), '?', '-');
    std::replace(filename.begin(), filename.end(), '*', '-');
    std::replace(filename.begin(), filename.end(), '"', '-');
    std::replace(filename.begin(), filename.end(), '<', '-');
    std::replace(filename.begin(), filename.end(), '>', '-');
    std::replace(filename.begin(), filename.end(), '|', '-');

    return cache_dir_ + "/" + filename;
}

void MddResourceCache::update_access_time(const std::string& key) {
    auto it = cache_meta_.find(key);
    if (it != cache_meta_.end()) {
        it->second.last_used = std::time(nullptr);
    }
}

void MddResourceCache::increment_access_count(const std::string& key) {
    auto it = cache_meta_.find(key);
    if (it != cache_meta_.end()) {
        it->second.access_count++;
    }
}

// ============================================================================
// MddResourceManager Implementation
// ============================================================================

MddResourceManager::MddResourceManager() {
    cache_ = std::make_unique<MddResourceCache>();
}

bool MddResourceManager::load_mdd(const std::string& mdd_path,
                                  const std::string& dictionary_id) {
    auto parser = std::make_unique<MddResourceParser>();
    if (!parser->load(mdd_path)) {
        return false;
    }

    DictionaryResources dict_res;
    dict_res.parser = std::move(parser);
    dict_res.mdd_path = mdd_path;

    dictionaries_[dictionary_id] = std::move(dict_res);
    return true;
}

bool MddResourceManager::unload_mdd(const std::string& dictionary_id) {
    auto it = dictionaries_.find(dictionary_id);
    if (it == dictionaries_.end()) {
        return false;
    }

    dictionaries_.erase(it);
    return true;
}

bool MddResourceManager::has_mdd(const std::string& dictionary_id) const {
    return dictionaries_.find(dictionary_id) != dictionaries_.end();
}

std::string MddResourceManager::get_resource_path(const std::string& key,
                                                  const std::string& dictionary_id) {
    auto dict_it = dictionaries_.find(dictionary_id);
    if (dict_it == dictionaries_.end()) {
        return "";
    }

    auto& parser = dict_it->second.parser;

    // Check cache first
    std::string cached = cache_->get_cached_path(key);
    if (!cached.empty() && fs::exists(cached)) {
        cache_->update_access_time(key);
        cache_->increment_access_count(key);
        return cached;
    }

    // Get from parser and cache
    auto data = parser->get_resource(key);
    if (data.empty()) {
        return "";
    }

    std::string mime_type = MddResourceParser::detect_mime_type(key);
    if (!cache_->cache_resource(data, key, mime_type)) {
        return "";
    }

    return cache_->get_cached_path(key);
}

std::vector<uint8_t> MddResourceManager::get_resource_data(const std::string& key,
                                                           const std::string& dictionary_id) {
    auto dict_it = dictionaries_.find(dictionary_id);
    if (dict_it == dictionaries_.end()) {
        return {};
    }

    return dict_it->second.parser->get_resource(key);
}

bool MddResourceManager::has_resource(const std::string& key,
                                     const std::string& dictionary_id) const {
    auto dict_it = dictionaries_.find(dictionary_id);
    if (dict_it == dictionaries_.end()) {
        return false;
    }

    return dict_it->second.parser->has_resource(key);
}

std::vector<std::string> MddResourceManager::list_resources(
    const std::string& dictionary_id, const std::string& prefix) const {

    auto dict_it = dictionaries_.find(dictionary_id);
    if (dict_it == dictionaries_.end()) {
        return {};
    }

    return dict_it->second.parser->list_resources(prefix);
}

void MddResourceManager::set_cache_directory(const std::string& cache_dir) {
    cache_->set_cache_directory(cache_dir);
}

void MddResourceManager::clear_cache(const std::string& dictionary_id) {
    if (dictionary_id.empty()) {
        cache_->clear_cache();
    } else {
        cache_->clear_cache(dictionary_id + "_");
    }
}

void MddResourceManager::prune_cache(size_t max_bytes) {
    cache_->prune_by_size(max_bytes);
}

size_t MddResourceManager::get_total_cache_size() const {
    return cache_->get_cache_size();
}

int MddResourceManager::get_total_resource_count() const {
    int total = 0;
    for (const auto& entry : dictionaries_) {
        total += entry.second.parser->resource_count();
    }
    return total;
}

} // namespace UnidictCoreStd
