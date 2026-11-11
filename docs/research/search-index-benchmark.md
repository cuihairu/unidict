> Language: **English** | [简体中文](../zh/research/search-index-benchmark.md)

# Search Index Benchmark: Import-time Indexing in Dictionary Clients

> Survey date 2026-11-03. Purpose: root-cause evidence for the "search broken
> right after import" report, and best-practice comparison for unidict's
> import → index → search pipeline.
> Sources: GoldenDict-ng official docs/architecture page and public commit
> history (github.com/xiaoyifang/goldendict-ng), mdict-utils / mdict4j MDX
> format documentation, StarDict format spec, Eudic product behavior reports
> (GoldenDict-ng issue #1711 user comparison).

## I. Root Cause in unidict (measured, real dictionary files)

Reproduce: `add_dictionary()` returns → immediately query, **without** any
explicit `build_index()` call (the pre-fix import contract left the trie
build to every caller).

Harness: 24-source core build, real files from `dictionaries/` —
wikdict-en-zh StarDict (25,397 entries), CC-CEDICT JSON (121,401 entries),
ECDICT StarDict (191 MB, ~2 M entries).

| Query path | right after import (pre-fix) | after `build_index()` |
|---|---|---|
| `exact_search("hello")` | 1 hit ✔ | 1 hit ✔ |
| `prefix_search("hel")` | **0 hits ✘** (empty trie) | 5 hits |
| `prefix_search("你好")` (CC-CEDICT) | **0 hits ✘** | 1 hit |
| `fuzzy_search` / `wildcard_search` | works ✔ (`word_index_` populated at add) | works ✔ |
| first `full_text_search` | **4.2 s** (25 k entries) / **3.6 s** (CC-CEDICT) / ECDICT: harness **killed after > 35 min** — the first post-import search never completed | same cost on first call (lazy build, cache after) |

Root causes (file:line at HEAD a51d1a3):

1. **Prefix trie not built at import** — `core/std/index_engine_std.cpp`
   `add_word()` populated `word_index_`/`dict_` but left `trie_` empty;
   `build_index()` was a separate manual step. Callers who forget it get a
   silently dead prefix layer. Concrete forgotten caller:
   `gui/main.cpp:175` `loadDefaultDictionaryLocations()` — on restart, when
   all dictionaries are restored from the persisted state file and nothing
   new is added, `if (added)` is false and `build_index()` never runs.
   `search_grouped`/`suggest_corrections` self-healed lazily, but public
   `prefix_search` (dictionary_manager_std.cpp:556), `search_all` layer 1
   (:333) and the GUI suggest-as-you-type calls (gui/main.cpp:686/711/1239,
   qmlui/lookup_adapter.cpp:84/806/837) did **not** — symptom: search box
   returns nothing / suggestions empty after import or restart.
2. **Full-text index fully lazy** — `ft_index_.reset()` on every
   `add_dictionary` (dictionary_manager_std.cpp:48); first full-text query
   paid the whole inverted-index build inline (measured above). On
   ECDICT-scale dictionaries this is a minutes-long "search hangs" symptom.

Fix (this change set):

- `add_word()` now maintains the trie incrementally (import = indexed);
  `remove_word()` removes from the trie in lock-step (no ghost candidates).
- `add_dictionary` marks the prefix index clean; all public query surfaces
  (`prefix_search`, `search_all` layer 1) self-heal via
  `ensure_prefix_index_built()` if a dirty state ever occurs (`load_index`
  wholesale replace).
- New `warm_fulltext_index()` (manager, idempotent): import-finish paths in
  both GUIs (`gui/main.cpp` add-file/add-dir handlers,
  `qmlui/lookup_adapter.cpp` `finishImport()`) build the inverted index at
  import completion — after the progress dialog closes, every query path is
  instant. CLI keeps lazy build (single-shot invocations must not pay the
  build).

## II. How established clients do it

| Aspect | GoldenDict-ng | Eudic (欧路) | MDict ecosystem (mdict-utils / mdict4j) | StarDict format | unidict (after fix) |
|---|---|---|---|---|---|
| Headword index at import | Parses into in-memory index per format; B-tree-ish index file per dictionary (`IdxHeader`/`Chunks`/`BtreeIndex`, xiaoyifang.github.io/goldendict-ng/architecture) | Converts imported third-party dictionaries into its own internal library format at import time (reported "30 GB of dictionaries, ready to use on open", GoldenDict-ng issue #1711) | MDX file itself **is** the index: sorted key blocks + key index (block-level binary search); readers build a flat key list on load (`readmdict.py` `_read_key_block`) | `.idx` ships pre-sorted → binary search, no import-time build needed | Incremental trie + hash index built inside `add_word()` — `add_dictionary` return = searchable |
| Full-text index | **Background thread pool** (`FtsIndexing` via `QtConcurrent`), starts right after program start / dict add, per-dictionary progress in status line; per-dict enable toggle; can be turned off for huge dicts | Not exposed; lookup-first product | No FTS — ecosystem treats MDX as headword-only; FTS is tooling territory (e.g. pyglossary-derived pipelines) | Not in format; GoldenDict builds it externally | Eager synchronous build at import-finish (`warm_fulltext_index`), parallel tokenization (`build_from_documents`) |
| FTS persistence | Persistent per-dict on-disk index (Xapian database, `*_FTS_x` folders); incremental resume — "indexing is incremental, it will not start over" (maintainer in #1711); known bloat complaint: 60 GB dicts → >100 GB index | N/A (internal format) | N/A | N/A | UDFT1/2/3 on-disk cache with dictionary-set signature (`fulltext_signature()`, TV/NV version salts) — load-on-match, rebuild on mismatch |
| Prefix search | Wildcard/prefix via Xapian term iterators; CJK uses n-gram term generator | Instant (internal indexed format) | Headword-sorted blocks → prefix = block-range scan | Sorted .idx → prefix binary-search range | Trie walk; fold-key normalization profiles (Tolerant/Conservative/Exact) |
| Fuzzy search | Lemma/synonym + Xapian fuzzy | Internal | absent | absent | Edit distance ≤ 2 over `word_index_` (O(N) scan — acceptable at 2 M entries? no: future work, see §IV) |
| Failure mode when index is stale | Signature/ID check (`needToRebuildIndex` on dictionary file list id); "Schedule for reindex" | opaque | signature = file itself | file set is the index | UDFT signature mismatch → auto rebuild; `kTokenizerVersion`/`kFoldKeyVersion` salts invalidate on behavior change |

## III. Conclusions adopted

1. **Import must leave the dictionary searchable** — Eudic's "import =
   internal indexed format" and GoldenDict-ng's "index starts immediately"
   both make indexing part of ingestion, never a manual second step.
   unidict now does the same for the word-level indexes synchronously
   (cheap: trie/hash), matching its existing in-file parsers (MDX/StarDict
   also just "load the shipped index").
2. **Full-text is a separate, heavier index with its own lifecycle** —
   GoldenDict-ng deliberately runs it in the background and persists it
   per dictionary; nobody builds FTS inline in the import dialog. unidict's
   `warm_fulltext_index()` at import-finish is the honest synchronous
   version; the lazy path remains as fallback for startup loads (CLI,
   state restore) so startup stays fast.
3. **Persistence with a correctness signature** is the consensus mechanism
   (GoldenDict dictionary-file-set id; unidict UDFT signature with
   tokenizer/fold versions) — keep invalidating on any behavior change.
4. **Per-format reality check**: MDX and StarDict already carry sorted
   headword indexes in-file; "import-time indexing" for them is only the
   cross-dictionary merge (trie/hash), not re-indexing content.

## IV. Known follow-ups (deliberately out of scope here)

- FTS warm at import is synchronous: on multi-GB dictionaries the import
  finish step will block for the build duration. GoldenDict-ng's answer is
  a background worker with progress + per-dict toggle; port when dictionaries
  of that size become a real workload (manager is not yet thread-safe for
  concurrent build+mutate — see TECH_DEBT/todo `ensure_postings` note).
- `fuzzy_search` is a full-map edit-distance scan; at ECDICT scale a
  BK-tree or n-gram candidate filter is the standard next step.
- ECDICT-scale first-load measurement: harness was killed after **> 35
  minutes** without completing the first post-import search (191 MB source);
  index-persistence (UDFT) load path is
  the mitigation and exists, but GUIs do not yet auto-prefetch it on
  startup (CLI exposes `--fulltext-index-load`).
