> Language: **English** | [简体中文](./zh/perf-benchmarks.md)

# Large Dictionary Performance Stress Test Record

> 2026-09-24, regression polishing of prefix completion and fulltext indexing on
> dictionaries at the 100k-word level.
> Conclusion first: **StarDict/Mdict's lowerBound binary search is completely
> stress-free on large dictionaries; the fulltext index's lazy one-time 1.3s build
> is acceptable; the real pitfall was that JsonParser originally fell through to the
> base class's linear prefixSearch, which had to scan the whole table even with no
> match (13ms per call on 100k words) -- now fixed.**

> **Current-state update (2026-10-14 recheck): this document is a historical stress
> test record from 2026-09-24 on the legacy Qt chain; the numbers and mechanism
> descriptions do not describe the current code.**
>
> - The stress test program linked `unidict_core_qt` (the same chain the GUI used
>   then) -- that chain has been retired in whole with TD-101/TD-102 (2026-10-10,
>   927f376). The current GUI main chain runs on std: `DictionaryManagerStd` →
>   `ParserRegistryStd` → the individual `*ParserStd` (docs/CURRENT_ARCHITECTURE.md
>   §3).
> - The `core/json_parser.{h,cpp}` where the "fix below" lives no longer exists
>   (now `core/std/json_parser_std.*`); the QMap `m_lowerWords` + `lowerBound` +
>   `startsWith` mechanism does not exist on the std side -- the std side uses a
>   `std::unordered_map` `lower_words_`, serving only the fold fallback for exact
>   lookup (json_parser_std.cpp:169).
> - Prefix completion on the std side is a **manager-level trie**, not
>   parser-level binary search: `DictionaryManagerStd::prefix_search` →
>   `IndexEngineStd::prefix_search` (trie_ drill-down + `TrieNode::collect` gather,
>   index_engine_std.cpp:112). That is, the "large-dictionary prefix is stress-free"
>   conclusion still holds on the std side, but via a different road.
> - The "guard tests" listed -- `json_parser_lowerbound_path` /
>   `json_parser_large_dict_prefix_fast` -- no longer exist (zero hits tree-wide); the
>   current guard surface is the `dictionary_manager_std_*` family (including
>   grouped/priority/quarantine/state and others) plus the `index_engine_std` side
>   cases.
> - Recording in passing: the std `*ParserStd::find_similar` implementations still
>   scan linearly word by word (json_parser_std.cpp:175 allocates a `lcase()` per
>   word), but there is **no production caller** (the only call sites tree-wide are
>   in tests), so it is not on the completion hot path -- if someone in the future
>   wires it into the completion surface, that would carry the 13ms-per-call defect
>   recorded here back verbatim.

## Why Stress Test

When prefix completion (QCompleter, type-to-lookup) and the fulltext fallback landed, the test data was always a mini dictionary of a few words; whether `prefixSearch` was linear or binary was invisible at that data scale. Real dictionaries (300k-500k words for English-Chinese) are the real exam for this path, so let us run the 100k-word-level data first and see.

## Stress Test Method

A one-off Qt Console program linking `unidict_core_qt` (same chain as the GUI), with a fixed seed generating two 100k-word dictionaries, produced programmatically and not committed (repo iron rule: never commit dictionary assets, and stress test data is no exception):

- **JSON**: 20k from `aaa00000` + 20k from `zzz00000` (creating dense prefix regions) + 60k random 5-9 letter words; definitions use a uniform sentence pattern embedding a shared keyword `kw<N%50>` (each kw appears in exactly 2000 definitions, so fulltext hit counts are predictable).
- **StarDict**: the same word list written in byte order as the three-piece set (.ifo/.idx/.dict, uncompressed), mimicking the writeStarDict in `tests/dictionary_manager_prefix_test.cpp`.

Three segments measured: parser load, single-call average of prefixSearch (200 calls), and the manager-level fulltext lazy build plus hot queries. Outer `/usr/bin/time -v` for RSS.

## Data (before fix, gcc15 -O2, local Linux)

| Metric | Result |
| --- | --- |
| JsonParser loading 100k words | 787 ms |
| StarDictParser loading 100k words | 171 ms |
| JSON prefixSearch, hit prefix 'aaa' (20k hits, stops at 20 collected) | <0.05 ms |
| **JSON prefixSearch, no match 'zzqq' (linear full-table scan)** | **13.1 ms/call** |
| StarDict prefixSearch (lowerBound binary search, including no-match) | all <0.05 ms |
| Fulltext index first build (200k entries, including per-word StarDict decoding) | 1301 ms (one-time lazy) |
| Fulltext hot query 'kw3' | 1.4 ms |
| Fulltext hot query wide word 'definition' | 58.2 ms |
| Peak RSS (200k entries fully in-memory) | ~457 MB |

## Findings and Fix

**Pitfall: JsonParser has no lowercase ordered index.** `m_entries` is a QMap but keyed by the original word form (case-sensitive order); prefix queries need to be case-insensitive, so they had no choice but to fall through to the base class's linear scan of `m_words`, and in the no-match case had to scan the full table. The GUI's textChanged calls this synchronously on every keystroke: 13ms at 100k words is still imperceptible, but a real 500k-word dictionary reaches 60ms+, and fast typing produces noticeable stutter.

The fix mirrors StarDictParser (`core/json_parser.{h,cpp}`):

- Add `QMap<QString, QString> m_lowerWords` (lower(word) → original word), filled in sync with entries during loadDictionary, with later writes to the same lowercase key overwriting (canonical takes the last word form, consistent with StarDict's m_canonicalWords semantics).
- `prefixSearch` override: `lowerBound(p)` lands at the start of the prefix region, `!startsWith` breaks, O(log N + hit count).

After the fix, all JSON prefix scenarios are <0.05 ms, on par with StarDict's binary search.

## Behavior Changes and Guard Tests

- **Output order**: JSON completion changes from "word-list insertion order" to "lowercase key alphabetical order" -- an improvement for the GUI completion popup (alphabetical display); same-lowercase different-word forms (e.g. Hello/hello) change from possibly emitting one entry each to merging into one (the manager layer already has lowercase dedup, so the final result's semantics are unchanged).
- Guard tests (`test_dictionary_manager_prefix`, additive only, never removed):
  - `json_parser_lowerbound_path`: pins down the binary search path semantics (alphabetical order, case-insensitive, canonical later-write overwrite, truncation, no match).
  - `json_parser_large_dict_prefix_fast`: 50k-word JSON no-match prefix query <5ms. The threshold is set very loosely -- a linear fallback of roughly 6.5ms would trip it, while binary search is three orders of magnitude below the threshold, so CI jitter will not false-positive; it only blocks regressions at the "fall back to linear full-table scan" level.

## Residual Observations (known, not touched for now)

- The 1.3s fulltext first build is a one-time lazy cost, occurring on the first fulltext fallback query (in the GUI it shows as the first fallback after an exact miss being slightly slow). If it ever feels bad, the direction is background-thread warmup on the GUI side, not changing the index itself.
- Wide-word (large hit set) hot query at 58ms: TF/IDF has to walk the hit set to score, which is inherent algorithmic cost, still within the imperceptible range.
- 200k entries fully resident at ~457MB: the index and entry copies are doubled in memory. Wait for a real user to report a memory issue before doing paging/trimming; record the baseline first.
