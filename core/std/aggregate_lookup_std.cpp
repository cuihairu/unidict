// Aggregated lookup results implementation (std-only).

#include "aggregate_lookup_std.h"
#include "dictionary_manager_std.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <regex>
#include <cstring>

namespace UnidictCoreStd {

// ============================================================================
// AggregationResult Implementation
// ============================================================================

std::vector<const AggregatedEntry*> AggregationResult::get_from_dictionary(
    const std::string& dict_id) const {

    std::vector<const AggregatedEntry*> result;
    for (const auto& entry : all_entries) {
        if (entry.source.dictionary_id == dict_id) {
            result.push_back(&entry);
        }
    }
    return result;
}

// ============================================================================
// DictionaryAggregator Implementation
// ============================================================================

namespace {
    // Simple string similarity (Jaro-Winkler distance variant)
    double string_similarity(const std::string& a, const std::string& b) {
        if (a == b) return 1.0;
        if (a.empty() || b.empty()) return 0.0;

        size_t len_a = std::min(a.size(), size_t(255));
        size_t len_b = std::min(b.size(), size_t(255));

        // Match distance
        size_t match_distance = std::max(len_a, len_b) / 2 - 1;
        if (match_distance < 0) match_distance = 0;

        std::vector<bool> a_matched(len_a, false);
        std::vector<bool> b_matched(len_b, false);

        size_t matches = 0;
        size_t transpositions = 0;

        // Find matches
        for (size_t i = 0; i < len_a; ++i) {
            size_t start = (i >= match_distance) ? i - match_distance : 0;
            size_t end = std::min(i + match_distance + 1, len_b);

            for (size_t j = start; j < end; ++j) {
                if (b_matched[j] || a[i] != b[j]) continue;
                a_matched[i] = true;
                b_matched[j] = true;
                ++matches;
                break;
            }
        }

        if (matches == 0) return 0.0;

        // Count transpositions
        size_t k = 0;
        for (size_t i = 0; i < len_a; ++i) {
            if (!a_matched[i]) continue;
            while (!b_matched[k]) ++k;
            if (a[i] != b[k]) ++transpositions;
            ++k;
        }

        // Jaro similarity
        double jaro = (matches / (double)len_a +
                      matches / (double)len_b +
                      (matches - transpositions / 2.0) / matches) / 3.0;

        // Winkler modification for common prefix
        size_t prefix = 0;
        size_t max_prefix = std::min({size_t(4), len_a, len_b});
        for (; prefix < max_prefix && a[prefix] == b[prefix]; ++prefix);

        return jaro + prefix * 0.1 * (1.0 - jaro);
    }

    // Simple hash function for deduplication
    std::string simple_hash(const std::string& s) {
        uint64_t h = 1469598103934665603ULL;
        for (unsigned char c : s) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        std::ostringstream oss;
        oss << std::hex << h;
        return oss.str();
    }

    // Strip HTML tags for comparison
    std::string strip_html_tags(const std::string& html) {
        std::string result;
        bool in_tag = false;
        for (char c : html) {
            if (c == '<') in_tag = true;
            else if (c == '>') in_tag = false;
            else if (!in_tag) result += c;
        }
        return result;
    }

    // Tokenize for comparison
    std::vector<std::string> tokenize(const std::string& s) {
        std::vector<std::string> tokens;
        std::string current;
        for (char c : s) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                current += c;
            } else if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        }
        if (!current.empty()) {
            tokens.push_back(current);
        }
        return tokens;
    }
}

DictionaryAggregator::DictionaryAggregator() {
}

void DictionaryAggregator::register_dictionary(const std::string& id,
                                               std::shared_ptr<DictionaryParserStd> parser,
                                               const EntrySource& source) {
    DictionaryEntry entry;
    entry.parser = std::move(parser);
    entry.source = source;
    entry.source.dictionary_id = id;
    dictionaries_[id] = std::move(entry);
}

void DictionaryAggregator::unregister_dictionary(const std::string& id) {
    dictionaries_.erase(id);
}

bool DictionaryAggregator::has_dictionary(const std::string& id) const {
    return dictionaries_.find(id) != dictionaries_.end();
}

void DictionaryAggregator::set_dictionary_priority(const std::string& id, int priority) {
    auto it = dictionaries_.find(id);
    if (it != dictionaries_.end()) {
        it->second.source.priority = priority;
    }
}

void DictionaryAggregator::set_dictionary_enabled(const std::string& id, bool enabled) {
    auto it = dictionaries_.find(id);
    if (it != dictionaries_.end()) {
        it->second.source.is_enabled = enabled;
    }
}

void DictionaryAggregator::set_dictionary_category(const std::string& id,
                                                   const std::string& category) {
    auto it = dictionaries_.find(id);
    if (it != dictionaries_.end()) {
        it->second.source.category = category;
    }
}

void DictionaryAggregator::create_profile(const DictionaryProfile& profile) {
    profiles_[profile.id] = profile;
    if (profile.is_default) {
        default_profile_id_ = profile.id;
    }
}

void DictionaryAggregator::delete_profile(const std::string& profile_id) {
    profiles_.erase(profile_id);
    if (default_profile_id_ == profile_id) {
        default_profile_id_.clear();
    }
}

void DictionaryAggregator::set_default_profile(const std::string& profile_id) {
    if (profiles_.find(profile_id) != profiles_.end()) {
        default_profile_id_ = profile_id;
    }
}

std::vector<DictionaryProfile> DictionaryAggregator::get_profiles() const {
    std::vector<DictionaryProfile> result;
    result.reserve(profiles_.size());
    for (const auto& entry : profiles_) {
        result.push_back(entry.second);
    }
    return result;
}

std::vector<DictionaryProfile> DictionaryAggregator::get_profiles_for_category(
    const std::string& category) const {

    std::vector<DictionaryProfile> result;
    for (const auto& entry : profiles_) {
        bool has_category = false;
        for (const auto& dict_id : entry.second.dictionary_ids) {
            auto it = dictionaries_.find(dict_id);
            if (it != dictionaries_.end() && it->second.source.category == category) {
                has_category = true;
                break;
            }
        }
        if (has_category) {
            result.push_back(entry.second);
        }
    }
    return result;
}

AggregationResult DictionaryAggregator::lookup(const std::string& word,
                                               const LookupOptions& options) const {
    AggregationResult result;
    result.query_word = word;

    // Build lookup context
    LookupContext ctx;
    ctx.options = &options;

    // Determine target dictionaries
    if (!options.enabled_dictionaries.empty()) {
        ctx.target_dict_ids = options.enabled_dictionaries;
    } else if (!options.enabled_profiles.empty()) {
        for (const auto& profile_id : options.enabled_profiles) {
            auto it = profiles_.find(profile_id);
            if (it != profiles_.end()) {
                ctx.target_dict_ids.insert(ctx.target_dict_ids.end(),
                    it->second.dictionary_ids.begin(),
                    it->second.dictionary_ids.end());
            }
        }
    } else {
        // Use all enabled dictionaries
        for (const auto& entry : dictionaries_) {
            if (entry.second.source.is_enabled || options.include_disabled) {
                ctx.target_dict_ids.push_back(entry.first);
            }
        }
    }

    // Get sources
    for (const auto& dict_id : ctx.target_dict_ids) {
        auto it = dictionaries_.find(dict_id);
        if (it != dictionaries_.end()) {
            ctx.sources[dict_id] = it->second.source;
        }
    }

    result.dictionaries_queried = static_cast<int>(ctx.target_dict_ids.size());

    // Perform lookup
    auto entries = perform_lookup(word, ctx);

    // Post-process
    if (options.deduplicate_definitions) {
        entries = deduplicate_entries(std::move(entries), options);
    }

    if (options.sort_by_relevance) {
        entries = sort_by_relevance(std::move(entries));
    }

    result.all_entries = std::move(entries);
    result.groups = group_entries(result.all_entries);
    result.total_matches = static_cast<int>(result.all_entries.size());

    // Calculate statistics
    for (const auto& entry : result.all_entries) {
        result.match_counts_by_dict[entry.source.dictionary_id]++;
    }
    result.dictionaries_with_matches = static_cast<int>(result.match_counts_by_dict.size());

    return result;
}

AggregationResult DictionaryAggregator::prefix_lookup(const std::string& prefix,
                                                      const LookupOptions& options) const {
    AggregationResult result;
    result.query_word = prefix;

    // Build context (same as lookup)
    LookupContext ctx;
    ctx.options = &options;

    if (!options.enabled_dictionaries.empty()) {
        ctx.target_dict_ids = options.enabled_dictionaries;
    } else {
        for (const auto& entry : dictionaries_) {
            if (entry.second.source.is_enabled || options.include_disabled) {
                ctx.target_dict_ids.push_back(entry.first);
            }
        }
    }

    for (const auto& dict_id : ctx.target_dict_ids) {
        auto it = dictionaries_.find(dict_id);
        if (it != dictionaries_.end()) {
            ctx.sources[dict_id] = it->second.source;
        }
    }

    result.dictionaries_queried = static_cast<int>(ctx.target_dict_ids.size());

    auto entries = perform_prefix_lookup(prefix, ctx);

    if (options.deduplicate_definitions) {
        entries = deduplicate_entries(std::move(entries), options);
    }

    if (options.sort_by_relevance) {
        entries = sort_by_relevance(std::move(entries));
    }

    result.all_entries = std::move(entries);
    result.total_matches = static_cast<int>(result.all_entries.size());

    return result;
}

AggregationResult DictionaryAggregator::fuzzy_lookup(const std::string& word,
                                                     const LookupOptions& options) const {
    AggregationResult result;
    result.query_word = word;

    LookupContext ctx;
    ctx.options = &options;

    if (!options.enabled_dictionaries.empty()) {
        ctx.target_dict_ids = options.enabled_dictionaries;
    } else {
        for (const auto& entry : dictionaries_) {
            if (entry.second.source.is_enabled || options.include_disabled) {
                ctx.target_dict_ids.push_back(entry.first);
            }
        }
    }

    for (const auto& dict_id : ctx.target_dict_ids) {
        auto it = dictionaries_.find(dict_id);
        if (it != dictionaries_.end()) {
            ctx.sources[dict_id] = it->second.source;
        }
    }

    result.dictionaries_queried = static_cast<int>(ctx.target_dict_ids.size());

    auto entries = perform_fuzzy_lookup(word, ctx);

    if (options.deduplicate_definitions) {
        entries = deduplicate_entries(std::move(entries), options);
    }

    if (options.sort_by_relevance) {
        entries = sort_by_relevance(std::move(entries));
    }

    result.all_entries = std::move(entries);
    result.total_matches = static_cast<int>(result.all_entries.size());

    return result;
}

std::vector<AggregatedEntry> DictionaryAggregator::perform_lookup(
    const std::string& word, const LookupContext& ctx) const {

    std::vector<AggregatedEntry> result;

    for (const auto& dict_id : ctx.target_dict_ids) {
        auto it = dictionaries_.find(dict_id);
        if (it == dictionaries_.end()) continue;

        const auto& dict_entry = it->second;

        // Lookup word in dictionary
        std::string definition = dict_entry.parser->lookup(word);
        if (definition.empty()) continue;

        AggregatedEntry entry;
        entry.word = word;
        entry.definition = definition;
        entry.source = ctx.sources.at(dict_id);
        entry.definition_hash = calculate_definition_hash(definition);
        entry.relevance_score = calculate_relevance(entry, word);

        result.push_back(std::move(entry));
    }

    return result;
}

std::vector<AggregatedEntry> DictionaryAggregator::perform_prefix_lookup(
    const std::string& prefix, const LookupContext& ctx) const {

    std::vector<AggregatedEntry> result;

    for (const auto& dict_id : ctx.target_dict_ids) {
        auto it = dictionaries_.find(dict_id);
        if (it == dictionaries_.end()) continue;

        const auto& dict_entry = it->second;
        const auto& source = ctx.sources.at(dict_id);

        auto similar_words = dict_entry.parser->find_similar(prefix, 100);

        for (const auto& word : similar_words) {
            if (word.size() < prefix.size()) continue;
            if (word.substr(0, prefix.size()) != prefix) continue;

            std::string definition = dict_entry.parser->lookup(word);
            if (definition.empty()) continue;

            AggregatedEntry entry;
            entry.word = word;
            entry.definition = definition;
            entry.source = source;
            entry.definition_hash = calculate_definition_hash(definition);
            entry.relevance_score = calculate_relevance(entry, prefix);

            result.push_back(std::move(entry));
        }
    }

    return result;
}

std::vector<AggregatedEntry> DictionaryAggregator::perform_fuzzy_lookup(
    const std::string& word, const LookupContext& ctx) const {

    std::vector<AggregatedEntry> result;

    for (const auto& dict_id : ctx.target_dict_ids) {
        auto it = dictionaries_.find(dict_id);
        if (it == dictionaries_.end()) continue;

        const auto& dict_entry = it->second;
        const auto& source = ctx.sources.at(dict_id);

        auto similar_words = dict_entry.parser->find_similar(word, 50);

        for (const auto& similar_word : similar_words) {
            std::string definition = dict_entry.parser->lookup(similar_word);
            if (definition.empty()) continue;

            AggregatedEntry entry;
            entry.word = similar_word;
            entry.definition = definition;
            entry.source = source;
            entry.definition_hash = calculate_definition_hash(definition);

            // Calculate similarity-based relevance
            double similarity = string_similarity(word, similar_word);
            entry.relevance_score = similarity * 0.7 + calculate_relevance(entry, word) * 0.3;

            result.push_back(std::move(entry));
        }
    }

    return result;
}

std::vector<AggregatedEntry> DictionaryAggregator::deduplicate_entries(
    std::vector<AggregatedEntry> entries, const LookupOptions& options) const {

    if (!options.merge_similar_entries) {
        return entries;
    }

    std::vector<AggregatedEntry> result;
    std::unordered_set<std::string> seen_hashes;

    for (auto& entry : entries) {
        bool is_duplicate = false;

        // Check for exact hash match
        if (seen_hashes.find(entry.definition_hash) != seen_hashes.end()) {
            is_duplicate = true;
        }

        // Check for similar definitions
        if (!is_duplicate) {
            for (const auto& existing : result) {
                double sim = definition_similarity(entry.definition, existing.definition);
                if (sim >= options.similarity_threshold) {
                    is_duplicate = true;
                    // Keep the one with higher priority (lower number = higher priority)
                    if (entry.source.priority < existing.source.priority) {
                        // Replace existing with this one
                        // (In practice, we'd want to be smarter here)
                    }
                    break;
                }
            }
        }

        if (!is_duplicate) {
            seen_hashes.insert(entry.definition_hash);
            result.push_back(std::move(entry));
        }
    }

    return result;
}

std::vector<AggregatedEntry> DictionaryAggregator::sort_by_relevance(
    std::vector<AggregatedEntry> entries) const {

    std::sort(entries.begin(), entries.end(),
        [](const AggregatedEntry& a, const AggregatedEntry& b) {
            // Primary: relevance score
            if (std::abs(a.relevance_score - b.relevance_score) > 0.001) {
                return a.relevance_score > b.relevance_score;
            }
            // Secondary: priority (lower = higher priority)
            if (a.source.priority != b.source.priority) {
                return a.source.priority < b.source.priority;
            }
            // Tertiary: dictionary name (lexicographic)
            return a.source.dictionary_name < b.source.dictionary_name;
        });

    return entries;
}

std::vector<EntryGroup> DictionaryAggregator::group_entries(
    const std::vector<AggregatedEntry>& entries) const {

    std::unordered_map<std::string, EntryGroup> groups;

    for (const auto& entry : entries) {
        auto& group = groups[entry.word];
        if (group.word.empty()) {
            group.word = entry.word;
        }
        group.entries.push_back(entry);
        group.dict_count++;
        group.max_relevance = std::max(group.max_relevance, entry.relevance_score);
    }

    // Set best entry for each group
    std::vector<EntryGroup> result;
    result.reserve(groups.size());

    for (auto& pair : groups) {
        auto& group = pair.second;

        if (!group.entries.empty()) {
            // Find entry with highest relevance
            auto best_it = std::max_element(group.entries.begin(), group.entries.end(),
                [](const AggregatedEntry& a, const AggregatedEntry& b) {
                    return a.relevance_score < b.relevance_score;
                });
            group.best_entry = &(*best_it);
        }

        result.push_back(std::move(group));
    }

    // Sort groups by max relevance
    std::sort(result.begin(), result.end(),
        [](const EntryGroup& a, const EntryGroup& b) {
            return a.max_relevance > b.max_relevance;
        });

    return result;
}

double DictionaryAggregator::calculate_relevance(const AggregatedEntry& entry,
                                                 const std::string& query) const {
    double score = 0.5;  // Base score

    // Exact word match bonus
    if (entry.word == query) {
        score += 0.3;
    } else {
        // Partial match based on similarity
        score += string_similarity(entry.word, query) * 0.2;
    }

    // Dictionary priority (lower = higher priority)
    // Map priority 0-10 to bonus 0.2-0
    score += std::max(0.0, (10.0 - entry.source.priority) / 50.0);

    // Category bonus (e.g., prefer en-en for English words)
    // (This would need more sophisticated logic)

    // Definition quality indicators
    std::string stripped = strip_html_tags(entry.definition);
    if (stripped.size() > 20) {
        score += 0.05;  // Substantial definition
    }
    if (stripped.size() > 100) {
        score += 0.05;  // Detailed definition
    }

    // Has examples
    if (!entry.examples.empty()) {
        score += 0.05;
    }

    // Has pronunciation
    if (!entry.pronunciation.empty()) {
        score += 0.03;
    }

    return std::min(1.0, score);
}

std::string DictionaryAggregator::calculate_definition_hash(const std::string& definition) {
    // Normalize definition for hashing
    std::string normalized = strip_html_tags(definition);

    // Remove extra whitespace
    std::string compressed;
    bool in_whitespace = false;
    for (char c : normalized) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!in_whitespace) {
                compressed += ' ';
                in_whitespace = true;
            }
        } else {
            compressed += c;
            in_whitespace = false;
        }
    }

    return simple_hash(compressed);
}

double DictionaryAggregator::definition_similarity(const std::string& a,
                                                   const std::string& b) {
    std::string a_stripped = strip_html_tags(a);
    std::string b_stripped = strip_html_tags(b);

    auto tokens_a = tokenize(a_stripped);
    auto tokens_b = tokenize(b_stripped);

    if (tokens_a.empty() || tokens_b.empty()) {
        return 0.0;
    }

    // Calculate Jaccard similarity
    std::unordered_set<std::string> set_a(tokens_a.begin(), tokens_a.end());
    std::unordered_set<std::string> set_b(tokens_b.begin(), tokens_b.end());

    size_t intersection = 0;
    for (const auto& token : set_a) {
        if (set_b.find(token) != set_b.end()) {
            ++intersection;
        }
    }

    size_t union_size = set_a.size() + set_b.size() - intersection;

    return union_size > 0 ? static_cast<double>(intersection) / union_size : 0.0;
}

std::vector<std::string> DictionaryAggregator::get_dictionary_ids() const {
    std::vector<std::string> ids;
    ids.reserve(dictionaries_.size());
    for (const auto& entry : dictionaries_) {
        ids.push_back(entry.first);
    }
    return ids;
}

std::vector<std::string> DictionaryAggregator::get_enabled_dictionary_ids() const {
    std::vector<std::string> ids;
    for (const auto& entry : dictionaries_) {
        if (entry.second.source.is_enabled) {
            ids.push_back(entry.first);
        }
    }
    return ids;
}

std::vector<EntrySource> DictionaryAggregator::get_dictionary_sources() const {
    std::vector<EntrySource> sources;
    sources.reserve(dictionaries_.size());
    for (const auto& entry : dictionaries_) {
        sources.push_back(entry.second.source);
    }
    return sources;
}

EntrySource DictionaryAggregator::get_dictionary_source(const std::string& id) const {
    auto it = dictionaries_.find(id);
    if (it != dictionaries_.end()) {
        return it->second.source;
    }
    return {};
}

int DictionaryAggregator::enabled_dictionaries() const {
    int count = 0;
    for (const auto& entry : dictionaries_) {
        if (entry.second.source.is_enabled) {
            ++count;
        }
    }
    return count;
}

int DictionaryAggregator::total_words() const {
    int total = 0;
    for (const auto& entry : dictionaries_) {
        total += entry.second.parser->word_count();
    }
    return total;
}

// ============================================================================
// AggregatedLookupBuilder Implementation
// ============================================================================

void AggregatedLookupBuilder::add_entry(AggregatedEntry entry) {
    entries_.push_back(std::move(entry));
}

void AggregatedLookupBuilder::add_entries(const std::string& dictionary_id,
                                         const std::vector<std::string>& words,
                                         const std::vector<std::string>& definitions) {
    if (words.size() != definitions.size()) {
        return;
    }

    for (size_t i = 0; i < words.size(); ++i) {
        AggregatedEntry entry;
        entry.word = words[i];
        entry.definition = definitions[i];
        entry.source.dictionary_id = dictionary_id;
        entry.source.dictionary_name = dictionary_names_[dictionary_id];
        entry.source.priority = priorities_[dictionary_id];
        entry.definition_hash = DictionaryAggregator::calculate_definition_hash(definitions[i]);

        entries_.push_back(std::move(entry));
    }
}

void AggregatedLookupBuilder::set_dictionary_priority(const std::string& id, int priority) {
    priorities_[id] = priority;
}

AggregationResult AggregatedLookupBuilder::build(const std::string& query_word) const {
    AggregationResult result;
    result.query_word = query_word;
    result.all_entries = entries_;

    // Sort by relevance
    std::sort(result.all_entries.begin(), result.all_entries.end(),
        [](const AggregatedEntry& a, const AggregatedEntry& b) {
            if (std::abs(a.relevance_score - b.relevance_score) > 0.001) {
                return a.relevance_score > b.relevance_score;
            }
            return a.source.priority < b.source.priority;
        });

    result.groups = DictionaryAggregator::group_entries(result.all_entries);
    result.total_matches = static_cast<int>(result.all_entries.size());

    return result;
}

void AggregatedLookupBuilder::clear() {
    entries_.clear();
}

} // namespace UnidictCoreStd
