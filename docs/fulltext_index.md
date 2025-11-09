# Full-Text Index Formats (UDFT)

This document describes Unidict's on-disk full‑text index formats, their evolution, and migration/compatibility guidance.

Terminology
- "FT index": the persisted full‑text index for definitions across loaded dictionaries.
- "Signature": a deterministic digest binding the index to the exact dictionary set and their companion files (paths/size/mtime), used to prevent accidental reuse.

Versions

1) UDFT1 (legacy)
- Header: `UDFT1` (5 bytes)
- No signature
- Plain postings: for each term, a list of `(docId, tf)` pairs written as fixed 32‑bit integers
- Status: deprecated; supported at load time for backward compatibility; not written by current versions

2) UDFT2 (signed)
- Header: `UDFT2`
- Signature block: `u32 sig_len`, then `sig_len` bytes of the signature string
- Body: same as UDFT1 (plain postings)
- Signature contents: `hex_fnv64|payload`, where payload encodes for each dictionary
  - `name|word_count|first_word|last_word|` and for each source/companion path: `path|size|mtime#...;`
  - Companion files covered: StarDict `.ifo`, `.idx`, `.dict`/`.dict.dz`; MDict `.mdx` + associated `.mdd` files sharing the same stem
- Use cases: strict validation that the FT index matches the current dictionary set. Recommended for production.

3) UDFT3 (compressed + signed)
- Header: `UDFT3`
- Signature block: same as UDFT2
- Postings: compressed per term
  - For a term with `n` postings, write `u32 n`, then `u32 comp_len`, then `comp_len` bytes
  - Encoding: sort by `docId`, delta‑encode `docId` and varint‑encode both `doc_delta` and `tf` (LEB128‑like)
- Benefits: smaller index size; load supports UDFT1/2/3 transparently

Compatibility & Modes

- Strict: only accept UDFT2/3 and enforce signature match; reject legacy UDFT1 or mismatched signature
- Auto (default): try strict; on failure, attempt to load UDFT1 without signature (prints a notice); still refuses mismatched UDFT2/3
- Loose: accept any version and ignore signature (prints a warning). Use only for ad‑hoc checks.

CLI Flags (std CLI)

- Load/save
  - `--fulltext-index-save <file>`: write UDFT3
  - `--fulltext-index-load <file>`: load UDFT1/2/3
  - `--ft-index-compat strict|auto|loose`: compatibility mode (default: `auto`)

- Upgrade
  - Single file: `--ft-index-upgrade-in <old> --ft-index-upgrade-out <new>`
  - Batch: `--ft-index-upgrade-dir <dir>` (recursive)
    - `--ft-index-upgrade-suffix <suf>` (default: `.v2`)
    - `--ft-index-out-dir <dir>`: write to a separate root, mirroring input tree
    - `--ft-index-filter-ext .index,.idx`: process only matching extensions
    - `--ft-index-force`: overwrite existing outputs
    - `--ft-index-dry-run`: show actions/signature hex prefix, do not write

Recommendations

- Production usage
  - Use `strict` mode to prevent stale/mismatched FT index reuse
  - Regularly save UDFT3 for large dictionaries to speed up next launches

- Migration
  - For old FT indexes: use single‑file or batch upgrade to UDFT3
  - Always load the same dictionary set (paths included) before upgrading so the new signature matches reality

- Troubleshooting
  - If load fails, check `--ft-index-compat` and ensure dictionary files/companion files are unchanged (path/size/mtime)
  - For quick verification use `loose`, but immediately regenerate a signed index in normal workflows

