> Language: **English** | [简体中文](./product-principles.md)

# Unidict Product Principles

> Phase 2 · 2026-10-04 · Paired with [architecture-boundaries.md](architecture-boundaries.md):
> this file defines "what to do / what not to do"; the boundary file defines "how to preserve these structurally in code".
> Current-state check: docs/CURRENT_ARCHITECTURE.md, docs/CURRENT_FEATURE_MATRIX.md (2026-10-04 audit baseline).

## 1. Positioning

**A personal dictionary -- fast to look up, clear to read, clear to hear, sayable out loud, correct in writing.**

- Is: a personal dictionary. A local reference reader that the user owns the data for: format-friendly, fast search, fully offline, optional readout and pronunciation, vocabulary as personal notes.
- Is not: a learning platform, a vocabulary-memorization app, a browser, a feed, a social product.
- Do not replicate other products; do not add features just to add features -- new features first pass the "feature gate" (§8).

## 2. Five User Values

| Value | Meaning | Current anchor (CURRENT_FEATURE_MATRIX section) |
|---|---|---|
| Fast lookup | Type-to-lookup, six search modes (exact/prefix/fuzzy/wildcard/regex/fulltext), multi-dictionary aggregation with grouped dedup | §2 |
| Clear to read | Folded grouped definitions, sanitized rendering, cross references (synonyms/collocations/example sentences), adjustable font/theme | §3 |
| Clear to hear | Local TTS playable offline; optional online human voice (US/UK/AU accents, off by default, sends only the queried word) | §4 |
| Sayable out loud | Optional local pronunciation scoring (GOP per-phoneme feedback, variant tolerance, relativized practice history); **not installed by default** | §4 |
| Correct in writing | Vocabulary + tags + notes (Markdown) + history pinning; CSV export (the data is yours) | §5 |

## 3. Local-first Is a Hard Constraint

- Offline must be fully usable, **including right after the first install**: lookup, aggregation, rendering, history, vocabulary, notes make no network requests.
- Network can only be an enhancement: online pronunciation, AI (optional), a future dictionary-library service.
- One lookup produces no network request (default). The current state already complies (core has no network stack); a violation is architectural debt.

## 4. The User Owns the Data

- **Dictionary files are the user's assets**: loaded from local paths (`UNIDICT_DICTS`/scanned directories), and the repository commits no dictionary data at all.
- **Vocabulary/history/notes are the user's data**: persisted on the local machine, in open formats (JSON/CSV) so they can migrate, and exportable.
- Never sync the contents of dictionary files; the Server (if it ever comes) syncs only user data (account/device semantics).
- Do not distribute copyrighted dictionaries; developers making their own dictionaries is the differentiation path.

## 5. AI and Server Are Both Optional

- **AI**: optional, provider swappable (local / OpenAI-compatible / free / custom); no built-in paywall; failure does not block lookup.
- **Server**: limited to four things -- account, dictionary catalog & distribution, device/sync; all skippable.
- **Settings collapse to two**: Account (cross-device sync surface) vs Device (local preferences); defaults good enough that no configuration is needed.

## 6. Do Not Make Users Configure What the Software Can Infer

- Example: dictionary grouping/priority can be arranged automatically; accents have a default (US→UK→AU); indexes build lazily in the background; corrupt dictionaries are isolated automatically.
- Every setting must pass this question: "Can the software infer this value itself?" If yes, it should not be a setting.
- Current-state check: setting keys have converged to `ui/*` and `pron/*`, `voice/*` (F1 audit §5); the risk of inventing free-form key names is in TECH_DEBT TD-122.

## 7. Forbidden Zone (What Not to Do)

Ads · forced login · feeds · leaderboards · check-ins · achievements · gamified courses · dark patterns.

Current-state check: learning_manager gamification dead code (achievements/motivational copy/daily goals) was retired and deleted as a whole class with P-7 (2026-10-10, TD-113 checked); the §11 four-skill data model lands in DataStoreStd via set_vocabulary_skill, with no live path for achievements/gamification and no residual API.

## 8. Feature Gate (Three Questions)

1. Does it serve one of the five senses (§2)?
2. Does it not break local-first (§3)?
3. Has the current state been audited first -- not reinventing (check the corresponding row in CURRENT_FEATURE_MATRIX.md and the historical conclusions in TECH_DEBT.md)?

If any of the three fails, do not add it. If it was added anyway, register it in TECH_DEBT.

## 9. Performance Budget

- Perceptual dimensions: startup, first lookup, aggregation, rendering, index build. Archived at P50/P95/P99 (the benchmark.sh infrastructure already exists, at the repo root; TD-134 closed -- CSV output produced as the run goes, 38a1355).
- "Fast lookup" is a positioning promise; a performance regression is a bug.

## 10. Pronunciation Is a First-Class Citizen, Scoring Is an Optional Learning Add-on

- TTS and online pronunciation enter the core experience; **pronunciation scoring is downgraded to optional**: not built by default (`UNIDICT_BUILD_PRON` domain off), model not in the repo, self-download (M10), not in the default install package (TD-152 observation point).

## 11. Data Model Reservations

- Vocabulary evolves toward "personal notes" (favorites + tags + notes + state semantics, e.g. "pronunciation unstable");
- Reserve **four-skill LearningState (listening/speaking/reading/writing) field slots** -- so that we do not churn migrations, not so that we course-ify;
- Review does not go into level/difficulty marketing: keep the schedule simple and predictable so the user can understand it.

## 12. Known Deviations from the Current State (Convergence Checklist)

- ~~Gamification dead-code API and storage (TD-113)~~ (2026-10-10 recheck checked: learning_manager retired as a whole class, the §11 four-skill model has landed in DataStoreStd -- deviation resolved).
- ~~Platform hotkeys/autostart Windows-only (TD-151)~~ (2026-10-13 recheck: deviation resolved -- F9/F10 closed out hotkeys and launch-on-boot on three desktop platforms; the residual Wayland session hotkey is transferred to pro_dictionary_gap P1).
- Review UI has no live path (TD-114) -- once the data model is settled, decide whether to add it against the §8 gate.
