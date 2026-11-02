> Language: **English** | [简体中文](../zh/design/README.md)

# Design drafts (not a committed direction)

Everything in this folder is **design-stage material**, and neither design has
been agreed for implementation in its full form. The two designs are,
however, implemented in reduced form: the B series (binlog relay, dynamic
passphrase pairing, E2E ciphertext, off by default) is the shipped realization
of `sync-engine.md`, and the three-profile text-norm engine (delivered
2026-10-16) is the shipped realization of `text-normalization.md`.

- `sync-engine.md` — cloud-grade sync engine design (accounts, incremental
  sync, E2E encryption). Status: design freeze only; shipped in reduced form
  as the B series — `sync_manager_qt` + `core/std/sync_engine_std` (binlog
  command stream, off by default), with the local file-level merge in
  `adapters/qt/sync_service_qt` (no cloud, no account) kept as the
  account-free fallback.
- `text-normalization.md` — text normalization design notes; implemented as
  the three-profile text-norm engine (`core/std/text_norm_std.*`).

Rule of engagement (same as `docs/server_plan.md`): **do not start
implementation from these drafts without an explicit product decision.**
Multi-device cloud sync and the dictionary server are parked items on the
roadmap ("deferred — user-driven only"); the product baseline stays
local-first. See `docs/product-principles.md` for the positioning and
`docs/CURRENT_FEATURE_MATRIX.md` for what actually ships today.
