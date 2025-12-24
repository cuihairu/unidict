// Aggregated lookup unit tests (std-only).
// Tests for multi-dictionary result aggregation, deduplication, and relevance scoring.

#include <cassert>
#include <string>
#include <vector>
#include "std/aggregate_lookup_std.h"

using namespace UnidictCoreStd;

void test_definition_hash() {
    // Same definition should produce same hash
    std::string def1 = "A definition of the word.";
    std::string def2 = "A definition of the word.";

    std::string hash1 = DictionaryAggregator::calculate_definition_hash(def1);
    std::string hash2 = DictionaryAggregator::calculate_definition_hash(def2);

    assert(!hash1.empty());
    assert(hash1 == hash2);

    // Different definition should produce different hash
    std::string def3 = "A different definition.";
    std::string hash3 = DictionaryAggregator::calculate_definition_hash(def3);

    assert(hash1 != hash3);
}

void test_definition_similarity() {
    // Identical definitions
    double sim1 = DictionaryAggregator::definition_similarity(
        "The cat sat on the mat.",
        "The cat sat on the mat."
    );
    assert(sim1 > 0.9);

    // Similar definitions (one word different)
    double sim2 = DictionaryAggregator::definition_similarity(
        "The cat sat on the mat.",
        "The cat sat on a mat."
    );
    assert(sim2 > 0.7);

    // Completely different
    double sim3 = DictionaryAggregator::definition_similarity(
        "The cat sat on the mat.",
        "Hello world programming."
    );
    assert(sim3 < 0.3);

    // Empty strings
    double sim4 = DictionaryAggregator::definition_similarity("", "");
    assert(sim4 == 0.0);
}

void test_entry_grouping() {
    DictionaryAggregator aggregator;

    std::vector<AggregatedEntry> entries;

    // Add entries for same word from different dictionaries
    AggregatedEntry e1;
    e1.word = "hello";
    e1.definition = "Definition 1";
    e1.source.dictionary_id = "dict1";
    e1.source.priority = 1;
    e1.relevance_score = 0.9;
    entries.push_back(e1);

    AggregatedEntry e2;
    e2.word = "hello";
    e2.definition = "Definition 2";
    e2.source.dictionary_id = "dict2";
    e2.source.priority = 2;
    e2.relevance_score = 0.8;
    entries.push_back(e2);

    AggregatedEntry e3;
    e3.word = "world";
    e3.definition = "Definition 3";
    e3.source.dictionary_id = "dict1";
    e3.source.priority = 1;
    e3.relevance_score = 0.7;
    entries.push_back(e3);

    auto groups = aggregator.group_entries(entries);

    assert(groups.size() == 2);

    // Check hello group
    assert(groups[0].word == "hello");
    assert(groups[0].dict_count == 2);
    assert(groups[0].entries.size() == 2);
    assert(groups[0].max_relevance == 0.9);
    assert(groups[0].best_entry != nullptr);
    assert(groups[0].best_entry->source.dictionary_id == "dict1");
}

void test_aggregated_lookup_builder() {
    AggregatedLookupBuilder builder;

    // Add individual entry
    AggregatedEntry e1;
    e1.word = "test";
    e1.definition = "Test definition";
    e1.source.dictionary_id = "dict1";
    e1.source.dictionary_name = "Dictionary 1";
    e1.source.priority = 1;
    e1.relevance_score = 0.95;
    builder.add_entry(e1);

    // Add entries in bulk
    builder.add_entries("dict2", {"word1", "word2"}, {"def1", "def2"});

    // Set priorities
    builder.set_dictionary_priority("dict1", 1);
    builder.set_dictionary_priority("dict2", 2);

    // Build result
    AggregationResult result = builder.build("test");

    assert(result.query_word == "test");
    assert(result.total_matches == 3);  // 1 individual + 2 bulk
    assert(result.all_entries.size() == 3);
    assert(result.groups.size() >= 2);

    // Check best entry
    const AggregatedEntry* best = result.get_best();
    assert(best != nullptr);
    assert(best->relevance_score >= 0.95);
}

void test_relevance_calculation() {
    // DictionaryAggregator needs DictionaryManagerStd, skip for now
    // Test could be expanded when actual lookup is implemented
}

void test_deduplication() {
    // This tests the deduplication logic in aggregate_lookup_std.cpp
    // Actual implementation would require DictionaryManagerStd setup
}

void test_lookup_options() {
    LookupOptions options;

    // Defaults
    assert(options.deduplicate_definitions == true);
    assert(options.merge_similar_entries == true);
    assert(options.similarity_threshold == 0.85);
    assert(options.max_results_per_dictionary == -1);
    assert(options.max_total_results == -1);
    assert(options.sort_by_relevance == true);
    assert(options.include_disabled == false);

    // Custom values
    options.deduplicate_definitions = false;
    options.max_results_per_dictionary = 10;
    options.max_total_results = 100;

    assert(options.deduplicate_definitions == false);
    assert(options.max_results_per_dictionary == 10);
    assert(options.max_total_results == 100);
}

void test_dictionary_profile() {
    DictionaryProfile profile;

    profile.id = "english_learner";
    profile.name = "English Learner";
    profile.description = "Dictionaries for English learners";
    profile.dictionary_ids = {"oxford", "cambridge", "longman"};
    profile.priority = 1;
    profile.is_default = true;

    assert(profile.id == "english_learner");
    assert(profile.dictionary_ids.size() == 3);
    assert(profile.is_default);
}

void test_entry_source() {
    EntrySource source;

    source.dictionary_id = "oxford";
    source.dictionary_name = "Oxford English Dictionary";
    source.priority = 1;
    source.category = "en-en";
    source.is_enabled = true;

    assert(source.dictionary_id == "oxford");
    assert(source.priority == 1);
    assert(source.category == "en-en");
    assert(source.is_enabled);
}

void test_aggregated_entry() {
    AggregatedEntry entry;

    entry.word = "test";
    entry.definition = "A test entry.";
    entry.pronunciation = "/tɛst/";
    entry.part_of_speech = "noun";
    entry.examples = {"This is a test.", "Another example."};

    entry.source.dictionary_id = "test_dict";
    entry.source.dictionary_name = "Test Dictionary";
    entry.source.priority = 1;
    entry.relevance_score = 0.95;
    entry.definition_hash = "abc123";

    entry.metadata["key1"] = "value1";
    entry.metadata["key2"] = "value2";

    assert(entry.word == "test");
    assert(entry.examples.size() == 2);
    assert(entry.relevance_score == 0.95);
    assert(entry.metadata.size() == 2);
}

void test_aggregation_result() {
    AggregationResult result;

    result.query_word = "hello";
    result.total_matches = 5;
    result.dictionaries_queried = 3;
    result.dictionaries_with_matches = 2;
    result.query_time_ms = 42.5;

    result.match_counts_by_dict["dict1"] = 3;
    result.match_counts_by_dict["dict2"] = 2;

    assert(result.query_word == "hello");
    assert(result.total_matches == 5);
    assert(result.dictionaries_queried == 3);
    assert(result.match_counts_by_dict.size() == 2);

    // Empty result has no best entry
    assert(result.get_best() == nullptr);
}

void test_aggregation_result_with_entries() {
    AggregationResult result;

    result.query_word = "test";

    AggregatedEntry e1;
    e1.word = "test";
    e1.definition = "Definition 1";
    e1.source.dictionary_id = "dict1";
    e1.relevance_score = 0.9;
    result.all_entries.push_back(e1);

    AggregatedEntry e2;
    e2.word = "test";
    e2.definition = "Definition 2";
    e2.source.dictionary_id = "dict2";
    e2.relevance_score = 0.95;
    result.all_entries.push_back(e2);

    // Get best entry (highest relevance)
    const AggregatedEntry* best = result.get_best();
    assert(best != nullptr);
    assert(best->relevance_score == 0.95);
    assert(best->source.dictionary_id == "dict2");
}

void test_get_entries_from_dictionary() {
    AggregationResult result;

    result.query_word = "test";

    // Add entries from different dictionaries
    AggregatedEntry e1;
    e1.word = "test";
    e1.definition = "Def 1";
    e1.source.dictionary_id = "dict1";
    result.all_entries.push_back(e1);

    AggregatedEntry e2;
    e2.word = "test";
    e2.definition = "Def 2";
    e2.source.dictionary_id = "dict1";
    result.all_entries.push_back(e2);

    AggregatedEntry e3;
    e3.word = "test";
    e3.definition = "Def 3";
    e3.source.dictionary_id = "dict2";
    result.all_entries.push_back(e3);

    // Get entries from dict1
    auto dict1_entries = result.get_from_dictionary("dict1");
    assert(dict1_entries.size() == 2);

    // Get entries from dict2
    auto dict2_entries = result.get_from_dictionary("dict2");
    assert(dict2_entries.size() == 1);

    // Get entries from non-existent dict
    auto dict3_entries = result.get_from_dictionary("dict3");
    assert(dict3_entries.size() == 0);
}

void test_aggregated_lookup_builder_clear() {
    AggregatedLookupBuilder builder;

    AggregatedEntry e;
    e.word = "test";
    e.definition = "test def";
    builder.add_entry(e);

    builder.add_entries("dict1", {"word1"}, {"def1"});

    assert(!builder.build("test").all_entries.empty());

    builder.clear();

    assert(builder.build("test").all_entries.empty());
}

void test_dictionary_priority() {
    EntrySource s1, s2, s3;

    s1.dictionary_id = "dict1";
    s1.priority = 3;

    s2.dictionary_id = "dict2";
    s2.priority = 1;

    s3.dictionary_id = "dict3";
    s3.priority = 2;

    // Lower number = higher priority
    assert(s2.priority < s1.priority);
    assert(s3.priority > s2.priority);
}

void test_enabled_dictionaries_filter() {
    LookupOptions options;

    // Enable specific dictionaries
    options.enabled_dictionaries = {"dict1", "dict2"};

    assert(options.enabled_dictionaries.size() == 2);
    assert(options.enabled_dictionaries[0] == "dict1");
}

void test_max_results_limits() {
    LookupOptions options;

    options.max_results_per_dictionary = 5;
    options.max_total_results = 20;

    assert(options.max_results_per_dictionary == 5);
    assert(options.max_total_results == 20);
}

int main() {
    test_definition_hash();
    test_definition_similarity();
    test_entry_grouping();
    test_aggregated_lookup_builder();
    test_lookup_options();
    test_dictionary_profile();
    test_entry_source();
    test_aggregated_entry();
    test_aggregation_result();
    test_aggregation_result_with_entries();
    test_get_entries_from_dictionary();
    test_aggregated_lookup_builder_clear();
    test_dictionary_priority();
    test_enabled_dictionaries_filter();
    test_max_results_limits();

    return 0;
}
