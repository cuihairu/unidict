> Language: **English** | [简体中文](./text-normalization.md)

# Query-key text normalization strategy (Unicode / case / diacritic folding, configurable)

> Status: strategy frozen (2026-09-28). The **core implementation has
> landed** (`core/std/text_norm_std` v2, table-driven, no ICU dependency);
> this document codifies the strategy, the configuration surface, and the
> test matrix, and marks every item as "delivered / completed this round /
> deferred".
> Local revision 2026-10-16: the three-tier profile
> (Tolerant/Conservative/Exact) is implemented and exposed in the qmlui
> settings surface (48eff28); the section 4 "reserved tiers" proposal is
> re-recorded as delivered, with a deviation note.
> Related: [roadmap](../roadmap.md) "Core Architecture -> Text
> normalization strategy"; background requirement `docs/pro_dictionary_gap.md`
> P0 (2); same-class precedent: [sync-engine](./sync-engine.md).

## 1. Problem and goals

Query-key normalization fixes one class of failure: "the word is clearly in
the dictionary, yet the lookup fails". Query words users copy from
Word/web/PDF carry typographic characters (curly quote U+2019, em dash,
zero-width space, BOM, soft hyphen); multilingual entries carry case and
diacritics (Москва/москва, café/cafe, ς/σ); full-width and half-width
input gets mixed. All of it must fold into **the same key**.

**Goals**

1. Different spellings -> same key (folding is lossy: é and e sharing a
   key is exactly the point);
2. A configuration surface that can turn parts of the folding off per
   scenario (`TextNorm::Options`, seven switches);
3. Pure std, table-driven, no ICU (the core's no-heavy-dependency red
   line);
4. Rule changes must invalidate the persisted index cache
   (`kFoldKeyVersion`).

**Non-goals**

- Display-text normalization (display uses the original text; keys are
  only for matching);
- Tokenization/stemming (that is the query layer's business);
- Full NFKD compatibility decomposition (see the section 6 deferred
  items).

## 2. Folding strategy: the rule pipeline

`fold_key(s, opt)` executes in order (step 2 is unconditional; the rest
are gated by the Options switches):

| # | Step | Switch | Backing table |
|---|---|---|---|
| 1 | Drop invisible characters (zero-width / BOM / soft hyphen / variation selectors / tag block) | `strip_invisible` | `kInvisible` |
| 1' | Drop combining marks (see the section 3.1 coverage ranges) | `strip_combining` | `kCombining` |
| 2 | Full-width ASCII (U+FF01-FF5E) and full-width space (U+3000) -> half-width | unconditional | inline mapping |
| 3 | Punctuation/quotes/dashes/fractions -> ASCII equivalents | `fold_punctuation` | `kPunct` |
| 4 | Accented/pinyin-toned letters -> base letters (é->e, ǐ->i, ß->ss) | `fold_diacritics` | `kFold` (generated) |
| 5 | Case folding (ASCII/Latin-1/Cyrillic/Greek/Latin Ext. Add.) | `fold_case` | `kCaseOffset`/`kCaseOddEven` (generated) |
| 6 | Unicode whitespace -> U+0020 | (rides with kPunct/kSpace) | `kPunct`+`kSpace` |
| 7 | Trim leading/trailing whitespace | `trim` | inline |

Ordering semantics: diacritic folding (4) runs before case folding (5) --
Ẁ lives in the diacritic table (NFKD = w + combining mark), so it folds to
w rather than to ẁ.

**Table generation and validation pipeline** (hand-writing is guaranteed
to be wrong; we have been bitten twice already, so everything is
scripted):

```text
scripts/gen_text_norm_fold.py   → core/std/text_norm_fold_std.inc（重音表）
scripts/gen_text_norm_tables.py → core/std/text_norm_case_std.inc（大小写表）
scripts/check_text_norm_tables.py  # 升序/不重叠/与 Python str.lower() 逐码点一致
scripts/gen_text_norm_test.py   → tests/text_norm_std_pro_test.cpp（端到端矩阵）
```

## 3. Boundary rulings (decided item by item)

### 3.1 Combining characters

Policy: **strip combining marks, keep the base letter** -- independent of
whether the input is precomposed (NFC): `e + U+0301` and `é` fold to the
same key. Coverage ranges: Latin U+0300-036F, Cyrillic U+0483-0489, Hebrew
niqqud U+0591-05C7, Arabic harakat U+064B-065F and U+06D6-06ED, Indic
vowel signs (matra as standalone code points, e.g. का -> क), and symbol
combining from U+20D0 on. **Trade-off**: enumerate by range rather than
"the entire Mn category" -- without a Unicode property database the
general category cannot be decided at runtime; the generator script
exports from Python `unicodedata`, so a new requirement only adds a range
and re-runs the pipeline.

### 3.2 RTL (right-to-left scripts)

RTL is a **rendering-layer** concern. Key normalization does not reorder
bytes and adds no directional marks: the Hebrew/Arabic **base characters
are kept verbatim** (alef stays alef); only the pointing (niqqud/harakat)
is stripped -- the same word with and without pointing must share one key
(שָׁלוּם -> שלום). U+200E/200F (LRM/RLM) count as invisible characters
and are stripped.

### 3.3 Surrogate pairs and the astral planes

UTF-8 has no surrogate pairs; the equivalent boundary here is
**multi-byte sequence integrity**:

- A valid 4-byte sequence (U+10000-10FFFF: CJK Extension B, mathematical
  alphanumerics, emoji) is decoded as a whole and passed through verbatim
  when no table entry matches;
- Invalid sequences are **passed through byte by byte -- no correction,
  no dropped bytes**: surrogate-range CESU-8 (`\xed\xa0\x80`), overlong
  encodings (`\xc0\x80`), truncation, stray continuation bytes, and
  **out-of-range code points** (`\xf4\x90\x80\x80` = U+110000, illegal
  per RFC 3629 -- this round added the decoder's reject branch; it was
  previously decoded as if valid);
- Desync recovery: a valid 4-byte sequence immediately following an
  invalid byte is still decoded as a whole (byte-by-byte pass-through
  consumes only 1 byte and does not swallow subsequent sequences);
- **Invisible** characters in the astral planes are also stripped:
  Variation Selectors Supplement U+E0100-E01EF (CJK compatibility
  ideograph variants, which genuinely occur in dictionary data) and the
  tag block U+E0000-E007F.

### 3.4 Full-width/half-width

U+FF01-FF5E -> U+0021-FF5E (+0xFEE0 in reverse), U+3000 -> U+0020.
**In effect unconditionally, with no switch** (section 2, step 2): this is
the only "character rewriting" rule. The rationale: full/half-width is an
input-method artifact, not a semantic distinction, and keeping a switch
would merely create two incompatible key spaces (with half the folds on
and half off inside one index, lookups would never reconcile); every other
fold has a legitimate strict-matching scenario and therefore can be
switched off, but this one does not get one.

## 4. Configuration surface (profile)

**Delivered**: the `TextNorm::Options` seven switches (`fold_case` /
`fold_diacritics` / `fold_punctuation` / `strip_invisible` /
`strip_combining` / `fold_ligatures` / `trim`), all on by default; the
tier is carried by the `TextNorm::Profile` enum
(Tolerant/Conservative/Exact), with `Options` as its value carrier, and
the two are mapped at a single point by `options_for_profile`.

**Current call sites** (the profile is dispatched at a single point
through the manager; engine and parsers apply the same rule):

| Call site | Profile source | Notes |
|---|---|---|
| `TextNorm::options_for_profile` | tier 0/1/2 -> seven switches | the single tier mapping point; out-of-range enum values clamp to Tolerant |
| `DictionaryManagerStd::set_norm_profile` | settings `textnorm/profile` | clamp the int tier to `[0,2]`, then sync the engine and every loaded parser; newly loaded dictionaries are stamped with the current tier |
| `IndexEngineStd::set_norm_options` | same as above | headwords are stored verbatim in `dict_`; clear `word_index_`/trie and refill under the new tier, **without reloading the dictionary file** |
| json/stardict/mdict parsers | same as above | the fold fallback cache is built lazily under the current tier; switching tiers marks it dirty and rebuilds it |
| manager dedup points | same as above | in-group headword dedup, full-text fallback dedup, suggestion-candidate dedup |
| `dictionary_manager_std` full-text index signature | `kFoldKeyVersion` | rule change -> `NV=` changes -> the old cache is invalidated and rebuilt (the full-text index is not keyed by tier, so it is unaffected by the profile) |

**Three-tier profile (implemented and exposed in the UI, 2026-10-16 /
48eff28)**:

| Tier | Switches turned off | Semantics |
|---|---|---|
| Tolerant (0, default) | none | the current `fold_key`: `Hello`/`Ｈｅｌｌｏ`/`École` fold to one key and hit one another |
| Conservative (1) | `fold_diacritics` + `strip_combining` | diacritics and combining marks are preserved (é stays distinct from e); typographic noise still folds (`ﬁ -> fi`, `’ -> '`, full-width -> half-width) |
| Exact (2) | all off | the key is the verbatim bytes; differences in form never collide |

UI surface: three radio buttons under the qmlui Appearance tab "Query-key
normalization", persisted to `textnorm/profile`, settled before the
startup load (main.cpp calls it before `loadDictionariesFromEnv`, so the
load applies it in one shot); switching tiers takes effect immediately,
with no app restart.

> **Deviation from this document's original proposal (recorded as
> happened)**: this section's original "reserved tiers" proposal was two
> tiers -- Strict (only `fold_diacritics`+`fold_ligatures` off) / Lenient
> (all on). What was actually delivered is three tiers, and Conservative
> turns off `fold_diacritics`+`strip_combining` while **keeping**
> `fold_ligatures`: ligature expansion is pure typographic normalization
> (`ﬁ` should hit `fi` anyway), whereas it is precisely diacritic folding
> and combining-mark stripping that make accented forms collide with base
> forms -- so the two switch on and off together, and ligatures stay on.

## 5. Table-driven test matrix

Three layers with a division of labor: `tests/text_norm_std_test.cpp`
covers basic behavior; `tests/text_norm_profile_test.cpp` covers tier
mapping (the three tiers' switch sets + out-of-range clamping) and
cross-tier behavioral semantics (how the same input's key shape differs
across the three tiers); `tests/text_norm_std_pro_test.cpp` (**a generated
file**, 14 groups) is the end-to-end matrix, whose assertion form is
uniformly "paired inputs -> same key" plus verbatim pass-through when
switches are off. The tier consumer surface carries its own separate
assertions: engine refill (`index_engine_std_test`),
json/stardict/mdict parser fold fallbacks (the three parser tests),
manager tier clamping and `exact_search` semantics
(`dictionary_manager_std_test`), adapter round trips
(`lookup_adapter_test`), and real-click UI (`ui_click_audit` S38).

| Group | Boundary | Representative assertions |
|---|---|---|
| 1 | Non-Latin case | Москва = москва; the whole Cyrillic block code point by code point; ς = σ; Georgian across 0x2000 |
| 2 | Typographic punctuation | don’t -> don't; all 7 members of the dash family fold; ™ -> (tm) |
| 3 | Invisible characters | the zero-width trio / BOM / soft hyphen / VS16 / tag block U+E0000 |
| 4 | Combining marks | niqqud / harakat / Devanagari matra, each paired to one key |
| 5 | Ligatures | ﬁ -> fi, ﬅ -> st |
| 6 | Unicode whitespace | NBSP / em space, at the edges and in the middle |
| 7 | Diacritics | café, ß -> ss, ǐ -> yi |
| 8 | Non-Latin base text | Chinese / kana / Arabic **kept verbatim** |
| 9 | Invalid UTF-8 | stray continuation bytes / truncation / CESU-8 surrogate range / overlong encodings / 0xFF |
| 10 | Options seven switches | each switch off means verbatim; with all off, full-width still folds |
| 11 | Odd/even offset | Ѡ -> ѡ (even +1) / ѡ verbatim |
| 12 | Full-width space | U+3000 at the edges -> trim / in the middle -> U+0020 |
| 13 | Empty input | empty string / pure whitespace / pure BOM -> empty key |
| 14 | **Astral planes (added this round)** | U+20000/𝕏/😀 passed through; U+E0100 stripped; `\xf4\x90\x80\x80` out-of-range passed through byte by byte; after `\xff` desync, the following 4-byte decode is unaffected |

**Engineering guardrails**: the test file is generated by
`gen_text_norm_test.py` and **must not be hand-edited**, and byte literals
must not be hand-written inside the generator (measured this round: the
`b` in `\x80b` was greedily swallowed by the escape as a VT, so the
assertion always blew up after the trailing trim -- a byte-by-byte
`blit()` helper has been added, with a comment left in place); any table
change requires re-running `check_text_norm_tables.py` (reconciliation
against Python `str.lower()`).

## 6. Delivered / completed this round / deferred

**Delivered (before this round)**: every step of the section 2 pipeline;
the section 3.1-3.2 boundaries; section 3.3 valid 4-byte handling and
invalid pass-through; section 4 Options and version invalidation; section
5 matrix groups 1-13; and the generation and validation pipeline.

**Completed this round**: the decoder's out-of-range rejection
(`cp > 0x10FFFF`, exact per RFC 3629); section 5 group 14, the astral
plane matrix (valid pass-through / astral invisible stripping /
out-of-range pass-through / desync recovery); and the `blit()` guardrail
in `gen_text_norm_test.py`.

**Deferred (judgment recorded, to avoid re-litigating)**

- **Full NFKD compatibility decomposition**: the current tables cover the
  characters that actually occur in dictionaries (diacritics, ligatures,
  tones); a full NFKD table runs to tens of thousands of rows and drags in
  decomposition-order semantics (sensitive to combining order), for low
  marginal benefit and high size and maintenance cost. Add ranges on
  demand once real unfound entries show up (the pipeline is ready; adding
  a table takes effect immediately).
- ~~**UI-exposed profile configuration** (section 4 reserved tiers)~~ ->
  delivered (2026-10-16 / 48eff28, the section 4 three-tier table;
  including engine refill and parser fold-cache switching in step with the
  tier change, not just the query path).
- **Original-text span mapping for search highlighting** (post-fold byte
  offsets -> original offsets): a query-layer feature; current
  highlighting goes through substring matching against the original text
  and is unaffected.
