> Language: **English** | [简体中文](./pronunciation-plan.md)

# Pronunciation Practice Plan (Pronunciation Correction · Shadow Reading · Global Pronunciation)

> Status: M1-M10 all implemented (M1-M9 records in each milestone section; M10 model
> bootstrap download closed out on 2026-10-10). The first half of this document is the
> finalized direction and phased breakdown from 2026-09; milestone sections are filled in
> incrementally as delivery proceeds.
> Related: roadmap "Voice Features → Pronunciation practice & scoring".

## Background and Goals

The progress of AI speech in recent years has made "offline pronunciation correction" feasible
for the first time as a personal project: open-source ASR models (zipformer/paraformer family)
plus ONNX inference runtimes are now small enough to ship with a dictionary application, and
phoneme-level scoring (GOP-family algorithms) can reach usable accuracy without a cloud.

Unidict's pronunciation practice aims to solve three things:

1. **Pronunciation correction** - the user reads a word/sentence and the app locally provides
   phoneme-level scoring and error localization (which sound was mispronounced), not just a
   vague percentage score.
2. **Shadow reading mode** - TTS demonstration → user reads along → scoring → try again,
   for daily use of "learning pronunciation from the dictionary".
3. **Global pronunciation** - the same word's demonstration, phonetic transcription, and
   scoring reference all follow the accent (US/UK/AU etc.) instead of assuming the whole
   world reads American English.

The iron rule stays: **offline first**. Core scoring must be runnable locally; cloud AI is
only an optional enhancement (using the existing AI external command bridge pattern, with no
cloud SDK hard-wired in).

## Technology Selection

### Scoring Engine: Local ONNX Approach as Primary

| Approach | Accuracy | Offline | Size | Verdict |
|------|------|------|------|------|
| Azure pronunciation evaluation | High | No | 0 | Violates offline-first; excluded as "primary" approach |
| Kaldi + GOP | Good | Yes | Large, heavy toolchain | Too high engineering cost; not selected |
| **sherpa-onnx + open-source CTC models for alignment + GOP scoring** | Usable | Yes | Models 50-200MB | **Selected** |
| Full self-implementation (train own acoustic model) | - | Yes | - | Unrealistic for a personal project; not selected |

Decision rationale: sherpa-onnx is an Apache-2.0 pure inference runtime (k2-fsa ecosystem,
clean C++ API, cross Win/macOS/Linux); it does not itself carry pronunciation evaluation but
provides CTC ASR models and force-alignment capability. The classic **GOP (Goodness of
Pronunciation)** algorithm - first force-align to obtain each phoneme's frame segment, then
compare "target phoneme vs actual acoustic likelihood" - is implemented at this layer by
ourselves; the pure algorithmic code can enter `core/` unit tests. That is: **alignment uses
the off-the-shelf runtime, scoring algorithm is self-contained**.

Cloud enhancement (long-term optional): the same recording can be sent to the external AI
command bridge for "more human-like commentary"; architecturally it is just another
implementation of the evaluation interface and does not block the main line.

### Audio Capture: Qt Multimedia

- `QAudioSource`, 16kHz / 16bit / mono PCM - the standard input for speech models; the
  capture layer records to this spec, no extra cleverness.
- Microphone permission: Windows uses the system settings (system prompt on first access);
  macOS requires adding `NSMicrophoneUsageDescription` to the Info.plist (no macOS packaging
  yet, but recorded here in the docs for now).
- **CI offscreen environments have no audio devices**: all capture/playback code is locked in
  the GUI layer and does not enter `core/`; tests only cover pure logic (see architecture
  linkage section).

### TTS: Continue Using Qt TextToSpeech

The "demonstration audio" for shadow reading uses the existing TTS pipeline directly (qmlui
already connected, gui not yet - plan is to land the pronunciation practice panel first in the
Qt Widgets GUI, and add it then). The multi-locale voices that come with system TTS engines
are the first demonstration source for "global pronunciation".

### Layered Architecture (Aligned with core's No-Heavy-Dependency Principle)

```
core/std/            scoring algorithms (pure logic): GOP scoring, phoneme mapping, score normalization
adapters/pron/       sherpa-onnx adapter: model loading, force-alignment
gui/                 pronunciation practice panel: recording, playback, waveform, score display, shadow loop
```

- `core/` only sees interfaces (e.g. `PronunciationScorer` pure virtual) and pure algorithms,
  and does not pull in ONNX headers - the same discipline as core without Qt: heavy dependencies
  go through adapters.
- Model files **do not enter git** (same iron rule as dictionary assets); at runtime they live
  in the data directory, with first-use guided download (source written in the docs, hash
  verification).
- CI only tests `core/` pure logic and interface mocks; the adapter layer does compile
  verification without running models.

## Phased Milestones

**M1 Audio Capture Infrastructure** (completed 2026-09-24, d82a4a2)
`QAudioSource` encapsulation (start/stop/fetch PCM) + simple waveform drawing + recording
playback. This step has independent value: it also lays the groundwork for future "voice
search". Landing discipline: pure logic (pcm_util.h waveform downsampling + WAV
wrap/parse) and the Qt shell are physically separated; only pure logic enters tests - Qt
full and std-only dual-form ctest both contain 9 sets of pcm_util test cases; capture uses
pull mode + 40ms timer polling (QIODevice readyRead timing varies widely across platforms;
polling is the dumbest but most reliable approach); 30-second recording cap with circuit
breaker.

**M2 Shadow Reading Loop MVP** (completed 2026-09-24, 6f7bcb3)
TTS reads the entry → user clicks button to record shadow reading → own recording and TTS
replayed in sequence ("compare" button), self-assessment by ear. The pitfalls of UI and
recording layer are all stepped on in this phase: QTextToSpeech lazy construction (zero cost
when engine plugin is missing), 5-second fallback timer for the compare flow (engine death
does not hang the button), `unique_ptr<T>` member explicit destructor placed in .cpp.

**M3a Scoring Pure Logic Kernel** (completed 2026-09-24)
ARPAbet 39-phoneme table, Needleman-Wunsch global alignment (gap 0.75, same-class substitution
0.5 / cross-class 1), `alignment_similarity` ((n_match+0.5·n_sub-0.25·n_ins)/target_len),
word score aggregation (0.7·mean + 0.3·min, the worst phoneme must not hide). All in
`core/std/pronunciation_score_std.*`, pure std enters CTest - scoring semantics are pinned
first, then M3b's sherpa-onnx adapter feeds data per the interface. Pitfall: write test
assertions by hand-deriving values first; identities like `0.7*mean + 0.3*min ≥ 0.7*mean`
will backfire if you use "bad phoneme pushes score down" as the assertion (must compare
against an all-good word).

**M3 Local Scoring MVP** (core step; English only first, main scenario is Chinese users
learning English)
Originally planned sherpa-onnx → on 2026-09-24 decided after actual measurement:
onnxruntime + wav2vec2-espeak-ctc (see "open question 2" above); IPA→ARPAbet mapping landed
as `espeak_arpabet_std`.

**M3b Inference Shell and Pipeline Wired Through** (implemented 2026-09-25, real-model
end-to-end verification in progress)
- **Layering**: `core/std/` pure logic fully in place - `pron_vocab_std` (HF vocab.json
  strict parsing: UTF-8/escapes/surrogate pairs, repeated ids and sparse holes rejected,
  blank candidates `<pad>/[PAD]/<blk>/|` recognized), `pron_wave_std` (int16→float +
  zero-mean/unit-variance normalization + silence guardrail + 16kHz/mono/16bit WAV
  reject-on-validation), `ctc_logits_std.h` (IEEE fp16→fp32 bit-level conversion + max-subtracted
  log_softmax, in-place safe); `adapters/pron/onnx_pron_scorer` (onnxruntime C++ inference shell,
  Pimpl keeps ORT headers out of the header, load-failure fallback semantics align with M2).
- **Tests**: Constant fake model fixture (614 bytes, hand-written protobuf generated by
  `scripts/gen_fake_ctc_fixture.py`, no python onnx package dependency) lets
  load→inference→tensor parsing→fp16→log_softmax→CTC alignment→GOP full chain enter
  CTest (`test_onnx_pron_scorer`); pure logic additionally has three independent test groups
  pron_vocab/pron_wave/ctc_logits. The log_softmax test caught the "exp then subtract
  log_sum" implementation bug on the spot - empirical proof of the value of test-first.
- **CMake**: `UNIDICT_BUILD_PRON=ON` is required to pull the onnxruntime 1.30.0 prebuilt
  package (URL overridable, supports pre-placed archive, IMPORTED target + build-tree rpath);
  when OFF it is completely excluded from the build. CLI `unidict_cli_std --pron-score` is
  compiled in under the same switch, allowing end-to-end scoring without a GUI.
- **Lessons learned**: parallel segmented downloads (Range slice concatenation) can produce
  archives that "have the right size but broken bytes" on slow, high-loss networks - download
  integrity must be verified with tar -tzf / sha256, especially for the 635MB model asset
  (LFS pointer carries sha256, verifiable).

**M3b Real-Model End-to-End Verification** (passed 2026-09-25)
Model sadda-speech/wav2vec2-espeak-ctc (sha256 verification passed) + real human
pronunciation (Wiktionary En-us-cat.ogg / En-us-dog.ogg, ffmpeg converted to 16k/mono/16bit):
- **Positive**: cat audio scored "K AE T" → K=0.978 / AE=0.650 / T=0.942,
  word score 0.795; frame-level argmax cleanly lands on k(40ms)/æ(120ms)/t(340ms),
  confidence corresponds one-to-one with GOP score - pipeline correct, score interpretable.
- **Negative**: cat audio scored "D AO G" → word score 0.000, discrimination is sufficient.
- **Known phenomenon** (not a bug): dog audio scored against its own phonetic transcription
  only gets 0.032 - frame diagnostics (CLI --pron-dump) show d→t voicing confusion,
  final g→ŋ assimilation, vowel evidence scattered across multi-lingual symbols. This is
  exactly the pre-planned "variant tolerance" tuning item: D/T, G/NG same-class variant
  pairs and AO's ɔ/ɑ variant (cot-caught merger) are incorporated into the M4 variant
  tolerance table (GOP takes max(log p(primary key), log p(variant))).
- Diagnostics entry: `unidict_cli_std --pron-score <wav> --pron-dump` for per-frame
  top-1 class and log p (tuning infrastructure for M4 phoneme localization).

**M4 Phoneme-Level Localization + Shadow Reading Integration**
Sentence-level alignment scoring, difference phoneme highlight display, shadow reading loop
integrated with scoring, vocabulary book linkage (words with unstable pronunciation
automatically tagged / entered into review queue - this is the real differentiator for
integrating with dictionary-based learning).

**M4 Implementation Records (from 2026-09-26)**
- **Variant tolerance table** (`core/std/pron_variants_std`): GOP takes max(primary key,
  variant) - alignment is still pinned to the primary key class (localization semantics
  unchanged); if a variant's evidence is stronger within the interval, it is recorded as
  the variant. First batch of entries collected by "natural speech processes / accent
  variants": T/D→ɾ (flap, latter/ladder merger), L/N→l̩/n̩ (syllabic consonants), ER→ɜ
  (non-rhotic), AH→ə/ɐ, AO→ɔ/ɑ (cot-caught merger, the only planned cross-domain
  exception), AA→ɑ/ɒ, EH→e, IH→ᵻ, IY→i, OW→o/oː, UW→u. **Explicitly not collected**:
  G→ŋ (a real process but only word-final; a position-blind table would also forgive
  "goal misread"; wait for position-aware variants) and all voicing / place-of-articulation
  confusions (those are errors, not variants). Real-model regression: cat 0.795 unchanged
  (lossless), dog 0.032→0.042 (d→t is a real error, deducted per design - dog's problem is
  voicing and too-short vowel segment, should not be whitewashed). Test pitfall: in synthetic
  data, if the primary key has no evidence in any frame, Viterbi alignment position degrades
  to arbitrary tie-breaking - real models always have quality for the primary key; tests must
  leave weak evidence for the primary key to have well-defined alignment semantics.
- **Dictionary IPA Text → ARPAbet** (`core/std/ipa_to_arpabet_std`): prerequisite for shadow
  reading integration - GUI/CLI directly scores entry phonetic transcriptions, no hand
  copying needed. Dual mode: whitespace-tokenized all-legal ARPAbet (tolerates stress digits
  "AE1") passes directly; otherwise UTF-8 path uses espeak phoneme table longest-prefix
  match (`match_espeak_prefix`, includes əl/ɑːɹ merged expansions, l̩ syllabic symbols);
  stress/segmentation/tie bar (U+0361/035C, after removal tʃ naturally matches a single
  phoneme) annotations skipped, unlisted code points rejected as a whole - rather not
  score than score with a truncated sequence. CLI adds `--pron-ipa` (mutually exclusive
  with `--pron-phones`), real-model verification that "ˈkæt" and "K AE T" score identically.
  Engineering lesson: in test literals containing \x escapes, \x followed by a hex character
  (e/d are both) is greedily consumed as one byte; phonetic transcription literals must
  always be written as native UTF-8.
- **GUI Panel Wired to Scoring** (gui/pronunciation_panel, M4 closure): the pronunciation
  practice panel adds a "score" button; shadow reading is upgraded from "human-ear
  comparison" to "machine per-phoneme comparison". Wiring key points:
  - Build gate `UNIDICT_GUI_PRON` (only linked when UNIDICT_BUILD_PRON is ON, pulls in
    unidict_pron + Qt6::Concurrent); when OFF the score button does not appear at all,
    the panel stays at M2 shadow reading without scoring - same as cli-std's "feature
    absent rather than link failure";
  - Target phonemes come from definition text: dictionaries have no independent pronunciation
    field and `pronunciation` has no entry point to fill it - add `core/std`
    `extract_phonetic_text` (full tests): strip HTML tags then scan the first 256 bytes
    for `/…/` or `[…]` fields; pure-ASCII candidates only accept legal ARPAbet ("hello" happens
    to consist entirely of single-letter phonemes, and "parseable" ordinary text like URL
    domains is always rejected), non-ASCII candidates must pass strict IPA parsing as a
    whole. Headword first; if empty, look through the first 5 definition sources in order;
    if extraction fails (empty / non-English IPA / HTML-entity-encoded transcription - a
    known limitation of only stripping tags without decoding entities), the button is
    disabled with a tooltip explaining the reason, silently falling back to M2;
  - Model path convention `~/.cache/unidict-models/wav2vec2-espeak-ctc/
    {model.onnx,vocab.json}`, overridable by `UNIDICT_PRON_MODEL`/`UNIDICT_PRON_VOCAB`
    environment variables; on load failure scoring is permanently disabled (no repeated
    attempts), status bar explains and shadow reading mode is kept;
  - Inference runs in a QtConcurrent background thread; the task value-copies
    PCM/phonemes/scorer and the lambda does not touch `this` (the panel can be closed at any
    time during scoring); model loaded once and reused;
  - Display as word score + per-phoneme GOP + weakest phoneme callout (the minimal viable
    form of "what to practice", no candy coloring); recordings <0.5 seconds do not enter
    scoring (not even one phoneme can be cut out, the score is meaningless);
  - Layering discipline unchanged: the panel is a platform shell and enters no tests; all
    testable logic of the scoring kernel is in core/std (already covered by M3a/M3b/M4).

**M5 Global Pronunciation (implemented)**
- Layering approach: scoring layer = core/std `extract_phonetic_variants` extracts British and
  American phonetic transcription fields from definitions (`PhoneticFields`, dictionary
  convention British-first American-second: `英 […] 美 […]`, `UK /…/ US /…/`); demonstration
  layer = GUI accent selector switches `QTextToSpeech::setLocale` (en-GB/en-US); display
  layer = bilingual dictionary definitions themselves already render dual fields, the
  repo's job is to extract for scoring, not to duplicate rendering.
- `PhoneticFields` semantics: british is the first field that passes parsing, american is the
  first field after british that differs from british - when British and American are
  homophonous ([kæt] [kæt]), only british is recorded, not forced (accent switching is
  meaningless); when a repeated homophone is followed by a different field, skip the
  duplicate and take the different one. Fields are still subject to all
  `extract_phonetic_text` guardrails (256-byte window, nested delimiter rejection, pure-ASCII
  only accepts ARPAbet, strict IPA parsing), and bad UTF-8 candidates are rejected and the
  scan continues.
- GUI (gui/pronunciation_panel): accent dropdown (BrE/AmE), selection persisted via
  QSettings("pron/accent"); free practice (no entry) hides the entire row. Switching accent
  = TTS locale change + scoring retarget (`retargetScoring`) + old score results hidden
  (scores based on the old reference are no longer applicable). When the selected accent's
  field is missing or fails to parse, fall back to the other field (single-field dictionaries
  should not turn off scoring entirely because of accent switching); the score button tooltip
  carries the raw text of the actually-effective field (cross-accent fallback is visible to
  the user). Non-PRON builds keep the accent selection (demonstration layer is unrelated to
  scoring); the scoring path is still gated by UNIDICT_GUI_PRON.
- gui/main.cpp: headword first; if empty, look through the first 5 definition sources in
  order; take the first source that has a british field and adopt both fields as a whole
  (no cross-source stitching), pass both fields to the panel.
- Known limitations: single-field dictionaries switching accent only affects the demonstration
  TTS locale, evaluation reference unchanged; when the TTS engine has no voice for the
  corresponding locale, Qt falls back to the default voice (demonstration accent is "best
  effort approximation"); HTML entity-encoded transcriptions are still not captured (known
  limitation carried over from M4); accent enumeration currently only en-GB/en-US (en-AU
  waits for real need; the data model PhoneticFields has only British/American slots).

**M6 Confusion Localization (implemented)**
- Motivation: M4's "weakest phoneme callout" only answers "what to practice"; users want to
  know "what I actually pronounced" (the known-risk section predicted phoneme-level
  post-processing would take more effort than the algorithm itself - this is the first step).
- Implementation: `PhoneGopResult::confused_with`. For each phoneme, score_word takes argmax
  of per-class averaged evidence over its forced interval (excluding blank - silence is not
  "what I pronounced as"), and records it only when it clearly beats the scoring evidence
  (log domain > 0.7, roughly 2x probability) and can map back to a different ARPAbet. Merged
  expansions joined with spaces (ɑːɹ → "AA R").
- Four no-report gates: primary key / tolerated variants naturally cannot pass the margin
  (mean ≤ best_mean, a genuine flap won't be reported as "pronounced as ɾ"); free variants
  mapping back to the same ARPAbet (ASCII g vs primary key ɡ) don't count as errors;
  multi-lingual symbols outside the vocabulary (French/German vowels) don't map back to
  ARPAbet, reporting symbols the user can't understand is meaningless; insufficient margin
  (adjacent-class jitter magnitude 0.1-0.3) is not reported - high-scoring phonemes are
  naturally quiet.
- Display: GUI per-phoneme inline arrow ("T 0.13→ER") + weakest phoneme callout with "sounds
  like X"; CLI --pron-score line-ending `-> ER`.
- Test lessons (second time stepping on it): in synthetic data, when the primary key has no
  evidence in any frame, Viterbi placement is arbitrary tie-breaking - T may be placed on the
  ʌ frame and confusion then reports the wrong object. Give the primary key weak evidence
  (-2, real-model form) for placement to be deterministic. Same pitfall as the flap test,
  cross-linked in comments.

**M7 Position-Aware Variants (implemented)**
- M4's deferred word-final g→ŋ assimilation (M3b real-model evidence that dog's final sound is
  harshly deducted) lands: `arpabet_variants(arpabet, word_final)` carries a position gate -
  word-final-limited variants only enter scoring candidates when the target phoneme is at the
  end of the sequence. "Word-final" is judged by the end of the scored target sequence (today
  only single words are scored; sentence-level scoring will need to revisit this semantics).
- Variants split into two categories (`PhoneVariant::notable`): silent = phonetic realizations
  of the same phoneme (flap ɾ, syllabic l̩ - only participates in scoring max, not reported,
  reporting it equals reporting "you pronounced it correctly"); notable = phoneme-level
  substitutions tolerated by policy (ŋ is a phoneme in the NG domain, not a realization of
  g) - beyond score max tolerance, the user must also see it.
- Actual reporting goes through `PhoneGopResult::realized_as`: when a notable variant's interval
  evidence clearly beats the primary key (same 0.7 nats threshold as confusion localization),
  its ARPAbet is recorded. Mutually exclusive with confused_with and visually distinguished:
  → is a mispronunciation (already deducted), ≈ is a tolerated actual pronunciation (scored
  as it, not deducted). Display: GUI per-phoneme "G 0.93≈NG" + footnote
  "≈ = native variant (not deducted)" (without explanation it reads as "a bit wrong");
  CLI line-ending `~ NG (tolerated)`.
- Two sides of position semantics: word-final G read as ŋ = tolerated + actually reported;
  non-word-final G read as ŋ (goal-style) = no variant candidate, low score + confusion →NG
  still reported - a position-blind table cannot separate these two, which is exactly why M4
  deferred it.
- Gray zone (variant lead <0.7): scoring tolerance as usual (max has no threshold, flap
  precedent), actual reporting silent - what is reported must be a definite "you pronounced
  it as that".

**M8 Vocabulary Book Linkage (implemented)**
- Motivation: shadow reading scoring and the vocabulary book were previously disconnected -
  after scoring, a word sinks to the bottom with no entry point for "which words still need
  practice". This is the "real differentiator for integrating with dictionary learning" from
  M4's original plan, added after M7 closure.
- Implementation (layering unchanged): core/std `pron_review_std` pure rules -
  `word_score_unstable` (word score < 0.6 judged unstable, valid domain [0,1] is not judged,
  NaN/negative sentinels always false) + `next_pron_review_tags` (returns new tag set when
  change is needed; when state is unchanged or score is invalid, returns nullopt so the
  upper layer avoids writing to disk); GUI pronunciation_panel does vocabulary book
  read/write and status-bar copy in the scoring completion callback.
- Tags are **state** not records: unstable gets the "pronunciation unstable" tag, when it
  recovers the tag is automatically removed, only our own tag is added or removed (existing
  tags and order are untouched, duplicate occurrences cleaned up together). The collection
  panel's group filter dropdown aggregates all vocabulary tags - tagging enters the practice
  list entry point.
- 0.6 threshold rationale: real-model cat normal pronunciation word score 0.795 leaves
  headroom without false positives; a single phoneme clearly collapsing (about 0.2 while the
  rest are 0.9) drops the word score to around 0.5 and must enter; the gray zone 0.6-0.8
  does not fuss - the tag only collects definite "not yet stable".
- Assumptions and boundaries (decided this round):
  1. Only link **words already in the vocabulary book** - the vocabulary book is the user's
     collection semantics, we don't collect words for the user; when not collected, after
     scoring the status bar prompts "automatically tag after collecting", and it takes
     effect once collected.
  2. "Enter review queue" is not in this milestone: the widgets GUI has no review queue
     (qmlui LearningManager is a separate store, the panel does not cross modules); tags +
     group filtering carry the practice list entry point; the real queue and the
     "relative-to-own-history" risk item (see below) are designed together.
  3. CLI --pron-score has no entry context (wav + phonetic transcription parameters), does not
     link.
- Tests: `tests/pron_review_std_test` (threshold boundaries, add/delete/idempotent/
  no-write-to-disk, out-of-domain scores don't touch tags, existing tags preserve order); the
  panel is a platform shell and per design does not enter tests; the linked vocabulary book
  read/write reuses the DataStore interfaces already covered by existing tests.

**M9 Practice History and Relativization (implemented)**
- Motivation: the "non-native accent baseline" marked in two places in the "known risks"
  section - GOP absolute scores are inherently strict for heavy-accent users, scoring should
  be compared against one's own progress, not use absolute scores as grades. M8 only reached
  tags (its assumption 2 explicitly left "real queue + relativization" to this round); users
  could see "unstable" but not "improving", so the tags had no payoff.
- Implementation (layering unchanged): core/std `pron_history_std` pure rules -
  `next_pron_attempt` (records one practice attempt: latest/best/count/time + delta from last),
  `pron_trend` (progress determination), `pron_practice_queue` (practice list ordering);
  persistent records `PronRecordStd` stored with vocabulary/notes in the same store file
  (`pron_records` section), GUI panel does read/write and copy. DataStore side added a Qt
  bridge and `UnidictCore::DataStore` forwarding in sync.
- Relativization: delta = this attempt - last attempt, dead zone `kPronFlatDelta` = 0.02.
  A single GOP word score has ±0.02 magnitude jitter (recording noise, accent on the model's
  edge); calling this magnitude "improved" is self-deception. Displayed as
  "+0.12 vs last (0.85) / flat / first record (nth attempt)" - progress is visible so users
  know continuing to practice is useful.
- Practice list: only collects records whose "most recent attempt is still unstable"
  (threshold reuses M8's `kUnstableWord Score`; the tag says "not yet stable", the list
  should practice what's not yet stable), order low score → long untrained → word name
  lexicographic (ordering keeps the UI from jumping and results testable). Records with
  unknown time (`last_at` = 0 or caller can't get the clock) are treated as "longest
  untrained" and sorted first: those whose training recency is unknown are treated as the
  most in need of practice. Panel top row "To practice: cat 0.42 · dog 0.55" (max 5 entries,
  the entire row is hidden when the list is empty), the full list still goes through the
  collection panel filtered by the "pronunciation unstable" tag.
- Bad data discipline (same "rather not modify than guess" stance as M8): empty word name /
  out-of-domain / NaN scores do not enter history (recording a fake record is worse than not
  recording; it pollutes the baseline and list); records modified on disk
  (`best_score` < `last_score`) are realigned by this attempt; unknown timestamps (now ≤
  0) keep the old timestamp rather than erasing it.
- Assumptions and boundaries (decided this round):
  1. History is **not limited to collected words**: having practiced is one's own practice
     trajectory; M8's tag discipline is unchanged (tags still only attach to vocabulary book
     entries, not collecting words for the user).
  2. Records are per-word, **not per-accent**: switching British/American accent changes the
     scoring reference (different phonetic transcription fields), so history is mixed. The
     same learner's same tongue; finer division has limited value; per-accent bucketing
     would need one more storage dimension, done only when real need appears.
  3. The panel's "to practice" is a reminder not a queue: not clickable, doesn't change the
     scoring target (top 5), the drill-down entry point is still the collection panel - the
     real review queue needs cross-module unified store semantics, designed together with the
     "relative-to-own-history" risk item.
  4. CLI `--pron-score` does not record history (only wav + phonetic transcription, no entry
     context, same assumption as M8).
  5. The store section name is `pron_records` rather than a short name: that file's section
     lookup is substring-positioned (known limitation recorded in 1ee2ba1/71b9289); a literal
     like `"pron":` in a definition (pron. abbreviation is not rare) would cause the short
     name to be misidentified. This scenario is pinned in tests.
- Tests: `tests/pron_history_std_test` (first/cumulative/trend dead zone/bad data/unknown
  clock/list filtering and three orderings/unknown time treated as longest untrained),
  `tests/data_store_std_pron` (upsert/case-insensitive/empty word name ignored/cross-instance
  round-trip/hand-written data file fault tolerance: old-format no section, records without
  keys skipped, missing fields take defaults, non-numeric fields don't crash, integers/
  negatives/trailing garbage decimals). The round-trip test forced out a real defect: word
  scores are real numbers while the existing reader only accepted integers, so `0.795` would
  be read as `0` (the record becomes "never wrong") - added `obj_num` all the way through.
  M8's "valid word score" definition is simultaneously extracted as `word_score_valid` for
  tags and history to share.
- Pitfall: the trend dead zone boundary was caught by tests with floating-point noise -
  `0.52 - 0.5` in binary floating point is `0.020000000000000018`, which side it falls on is
  decided by rounding. Tests were changed to assert both sides of the dead zone (0.019/0.03)
  and the boundary is written as an explicit comment: this is exactly why the dead zone
  exists (a band 0.02 wide should suppress this magnitude), not a defect.

**M10 Model Bootstrap Download (completed 2026-10-10)**
- Motivation: the 635MB fp16 model is not in the repo (size red line TD-152), so users get
  "M2 shadow reading without scoring" - before first use, the model must be self-fetchable,
  otherwise the scoring surface equals non-existent. Per the "technology selection" section's
  established stance: first-use guided download, source written in docs, hash verification.
- Implementation: CLI entry `--pron-fetch-model` (download + sha256 verification to disk).
  HTTP transport lives in the shell - core/std does not pull network dependencies, only
  answers "what to do next" (download/resume/verify/land); the shell writes URL, output path,
  and Range offset into a curl config file (quote-escaped, paths containing spaces/semicolons
  will not be swallowed by the shell), external `curl` does the actual fetch -
  `continue-at` resumes from offset for resumable downloads, bad packages re-downloaded. At
  runtime `--pron-model` points to the landed path; when the model is missing/corrupt, the
  panel disables scoring with a tooltip explaining the reason, silently falling back to M2
  (same as M4 closure stance).
- Dependency handling: curl availability pre-check (`curl --version` probe, Windows uses
  `>nul`), on absence gives targeted guidance (install curl or manually download assets to
  the model directory) and return 5 same-family exit codes - no more making Windows users
  guess at truncated exit codes (cmd 9009&0xff colliding with real error codes) (TD-153
  closed).

Long-term (no commitment): community real-human pronunciation library (Forvo-style
crowdsourcing) - involves copyright and network services, only occupies a roadmap slot.

## Known Risks and Pitfalls (Experience Predictions)

- **Model size and download**: English CTC models 50-200MB; download management + hash
  verification + resumable downloads must be done, and failure must cleanly fall back to
  "shadow reading without scoring" (M2 capability).
- **CI false-green risk**: once audio/model-related code gets mixed into the tested path,
  the offscreen environment degrades in all sorts of strange ways. Countermeasure: physically
  separate pure logic from platform code, no audio device assumptions in tests.
- **Score interpretability**: just giving one total score won't satisfy users; GOP's
  phoneme-level post-processing (mapping to user-understandable "this sound was pronounced
  as that sound") takes more effort than the algorithm itself; M4 reserves ample time.
- **Non-native accent baseline**: GOP is inherently strict for heavy-accent users; scoring
  must be relativized against the user's own history (compared with one's own progress), not
  absolute scores - designed together when M4 introduces history curves.
- **Qt Multimedia version differences**: Qt6's QAudioSource/QMediaPlayer API is completely
  different from Qt5, with small changes between 6.2/6.4/6.5; encapsulate uniformly in the
  adapter, do not touch the API directly in the UI layer.

## Open Questions (Clarify Before Implementation)

1. sherpa-onnx as vendored sub-source or system dependency/package management? Lean toward
   FetchContent vendored (version locked, offline-build friendly), CMake option controls
   `UNIDICT_BUILD_PRON=OFF` so it is completely excluded from the build.
2. English evaluation model selection (zipformer CTC versions) decided at M3 startup by
   actual measurement; the docs only lock the hard requirement "CTC + force-alignment
   capable".

   **2026-09-24 M3b selection measurement conclusion**:
   - `sherpa-onnx-zipformer-ctc-en-2023-10-02` (k2 official 383MB): downloaded and
     tokens.txt verified - **BPE subwords (`▁THE`/`ING`), not phonemes**; force alignment
     to ARPAbet is out of the question, excluded. Lesson: in k2's official model list, "CTC"
     does not equal "phoneme-level"; you must download and look at tokens to know.
   - sherpa-onnx public API **does not expose frame-level CTC logits** (only token-level
     timestamps); even with a phoneme model you'd have to modify upstream source - without
     GOP's frame-level posterior, everything is blocked.
   - Final choice: **`sadda-speech/wav2vec2-espeak-ctc`** (HuggingFace,
     Apache-2.0): an fp16 ONNX conversion of facebook/wav2vec2-lv-60-espeak-cv-ft,
     **output is frame-level CTC logits (1,T,392), 50 frames/second**, input
     16kHz mono raw waveform (model has its own CNN front-end - the kaldi fbank
     bit-level compatibility pitfall is entirely sidestepped), vocabulary espeak IPA
     phonemes (392 including multi-lingual). The author uses it as a phone-level forced
     aligner, purpose exactly matching.
   - Cost: **fp16 onnx 635MB**, exceeding original estimate (50-200MB) - acceptable as a
     user-optional download; int8 quantization (~320MB) listed as follow-up optimization,
     not blocking MVP; espeak IPA → ARPAbet mapping table hardcoded (both are English phoneme
     full sets, one-to-one correspondence basically holds); unmapped ones go through M3a's
     cost 1 fallback as illegal phonemes.
   - Inference uses onnxruntime prebuilt library (FetchContent pulls the official release
     package + IMPORTED target), sherpa-onnx is not pulled at all - its only value to us
     (decoding/streaming/language model) is entirely unused in the scoring scenario.
   - **Mapping domain known pitfalls (recorded before actual measurement)**:
     `arpabet_to_espeak("T")` takes primary key "t", but native American butter t flaps
     to ɾ - when model frame evidence lands in the ɾ class, T's GOP is wrongly deducted
     (the more native the user reads, the harsher the deduction). Variant tolerance (T's
     GOP takes max(log p(t), log p(ɾ))) is listed as a post-M3b-measurement tuning item;
     same-class variant pairs (ER's ɚ/ɜː etc.) inventoried together.
3. The pronunciation practice panel lands first in the Qt Widgets GUI (in the same window as
   existing learning features); whether the QML side follows in sync is decided after the
   Widgets version is validated.
