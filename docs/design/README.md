> Language: **English** | [简体中文](./README.zh.md)

# Design drafts (not a committed direction)

Everything in this folder is **design-stage material**. Nothing here has been
agreed for implementation; no code in the repository implements these docs.

- `sync-engine.md` — cloud-grade sync engine design (accounts, incremental
  sync, E2E encryption). Status: design freeze only. Current shipped sync is
  the local file-level merge in `adapters/qt/sync_service_qt` (no cloud, no
  account), kept as the account-free fallback.
- `text-normalization.md` — text normalization design notes.

Rule of engagement (same as `docs/server_plan.md`): **do not start
implementation from these drafts without an explicit product decision.**
Multi-device cloud sync and the dictionary server are parked items on the
roadmap ("deferred — user-driven only"); the product baseline stays
local-first. See `docs/product-principles.md` for the positioning and
`docs/CURRENT_FEATURE_MATRIX.md` for what actually ships today.
