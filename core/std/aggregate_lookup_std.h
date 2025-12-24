// Aggregated lookup results across multiple dictionaries (std-only).
// Provides unified API for fetching and organizing results from multiple
// dictionary sources with grouping, prioritization, and deduplication.

#ifndef UNIDICT_AGGREGATE_LOOKUP_STD_H
#define UNIDICT_AGGREGATE_LOOKUP_STD_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>

namespace UnidictCoreStd {

// Forward declarations
class DictionaryManagerStd;

// Source of a dictionary entry
struct EntrySource {
    std::string dictionary_id;
    std::string dictionary_name;
    int priority = 0;           // lower = higher priority
    std::string category;        // e.g., "en-en", "en-zh", "technical"
    bool is_enabled = true;
};

// Single dictionary entry with metadata
struct AggregatedEntry {
    std::string word;
    std::string definition;     // rendered HTML/sanitized text
    std::string pronunciation;  // IPA/phonetic
    std::string part_of_speech;
    std::vector<std::string> examples;
    EntrySource source;

    // Relevance score (higher = more relevant)
    double relevance_score = 0.0;

    // For deduplication
    std::string definition_hash;  // hash of definition for duplicate detection

    // Additional metadata
    std::unordered_map<std::string, std::string> metadata;
};

// Group of entries for the same word (from different dictionaries)
struct EntryGroup {
    std::string word;
    std::vector<AggregatedEntry> entries;
    double max_relevance = 0.0;
    int dict_count = 0;

    // Best entry (highest relevance)
    const AggregatedEntry* best_entry = nullptr;
};

// Dictionary group/profile
struct DictionaryProfile {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> dictionary_ids;
    int priority = 0;
    bool is_default = false;
};

// Lookup options
struct LookupOptions {
    std::vector<std::string> enabled_dictionaries;  // empty = all enabled
    std::vector<std::string> enabled_profiles;      // empty = all
    bool deduplicate_definitions = true;
    bool merge_similar_entries = true;
    double similarity_threshold = 0.85;  // for merging
    int max_results_per_dictionary = -1;  // -1 = unlimited
    int max_total_results = -1;          // -1 = unlimited
    bool sort_by_relevance = true;
    bool include_disabled = false;
};

// Aggregation result
struct AggregationResult {
    std::string query_word;
    std::vector<EntryGroup> groups;
    std::vector<AggregatedEntry> all_entries;  // flattened, sorted
    int total_matches = 0;
    int dictionaries_queried = 0;
    int dictionaries_with_matches = 0;
    double query_time_ms = 0.0;

    // Statistics by dictionary
    std::unordered_map<std::string, int> match_counts_by_dict;

    // Get best matching entry
    const AggregatedEntry* get_best() const {
        return all_entries.empty() ? nullptr : &all_entries[0];
    }

    // Get entries from a specific dictionary
    std::vector<const AggregatedEntry*> get_from_dictionary(const std::string& dict_id) const;
};

// Dictionary aggregator
class DictionaryAggregator {
public:
    DictionaryAggregator();
    explicit DictionaryAggregator(DictionaryManagerStd* manager);
    ~DictionaryAggregator() = default;

    // Set dictionary manager
    void set_dictionary_manager(DictionaryManagerStd* manager);

    void unregister_dictionary(const std::string& id);
    bool has_dictionary(const std::string& id) const;

    // Dictionary management
    void set_dictionary_priority(const std::string& id, int priority);
    void set_dictionary_enabled(const std::string& id, bool enabled);
    void set_dictionary_category(const std::string& id, const std::string& category);

    // Profile management
    void create_profile(const DictionaryProfile& profile);
    void delete_profile(const std::string& profile_id);
    void set_default_profile(const std::string& profile_id);
    std::vector<DictionaryProfile> get_profiles() const;
    std::vector<DictionaryProfile> get_profiles_for_category(const std::string& category) const;

    // Main lookup function
    AggregationResult lookup(const std::string& word,
                            const LookupOptions& options = {}) const;

    // Prefix/fuzzy lookup
    AggregationResult prefix_lookup(const std::string& prefix,
                                   const LookupOptions& options = {}) const;
    AggregationResult fuzzy_lookup(const std::string& word,
                                  const LookupOptions& options = {}) const;

    // Get all dictionary IDs
    std::vector<std::string> get_dictionary_ids() const;
    std::vector<std::string> get_enabled_dictionary_ids() const;

    // Get dictionary info
    std::vector<EntrySource> get_dictionary_sources() const;
    EntrySource get_dictionary_source(const std::string& id) const;

    // Statistics
    int total_dictionaries() const { return static_cast<int>(dictionaries_.size()); }
    int enabled_dictionaries() const;
    int total_words() const;

private:
    // Internal lookup helpers
    struct LookupContext {
        const LookupOptions* options;
        std::vector<std::string> target_dict_ids;
        std::unordered_map<std::string, EntrySource> sources;
    };

    std::vector<AggregatedEntry> perform_lookup(const std::string& word,
                                               const LookupContext& ctx) const;
    std::vector<AggregatedEntry> perform_prefix_lookup(const std::string& prefix,
                                                      const LookupContext& ctx) const;
    std::vector<AggregatedEntry> perform_fuzzy_lookup(const std::string& word,
                                                     const LookupContext& ctx) const;

// Simple result builder for quick aggregated lookups
class AggregatedLookupBuilder {
public:
    AggregatedLookupBuilder() = default;
    ~AggregatedLookupBuilder() = default;

    // Add a single entry
    void add_entry(AggregatedEntry entry);

    // Add entries from a dictionary
    void add_entries(const std::string& dictionary_id,
                    const std::vector<std::string>& words,
                    const std::vector<std::string>& definitions);

    // Set priorities
    void set_dictionary_priority(const std::string& id, int priority);

    // Build final result
    AggregationResult build(const std::string& query_word) const;

    // Clear all entries
    void clear();

private:
    std::vector<AggregatedEntry> entries_;
    std::unordered_map<std::string, int> priorities_;
    std::unordered_map<std::string, std::string> dictionary_names_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_AGGREGATE_LOOKUP_STD_H
