> Language: **English** | [简体中文](/dictionaries)

# Dictionary Management and Import Formats

## Import Format Matrix

| Format | Extension | Notes |
|---|---|---|
| StarDict | `.ifo/.idx/.dict(/.dict.dz)` | The most common format for English-Chinese and bilingual reference works; both `.dz` compression and defenses against corrupted files are supported |
| MDict | `.mdx` (+ `.mdd` resources) | The dominant distribution format for Chinese reference works such as the Xinhua Dictionary and the Modern Chinese Dictionary; multiple block layouts, SimpleXOR encryption, images/audio inside `.mdd`, and GB18030-encoded dictionaries are all supported |
| DSL | `.dsl` | AbaDict-family dictionaries |
| JSON / CSV / TSV / plain text | See below | Homemade word banks, UTF-8 |
| EPUB | `.epub` | Dictionary-style e-books |

For an encrypted MDict there are two ways to supply the password: the `--mdict-password <pw>` argument or the `UNIDICT_MDICT_PASSWORD` environment variable.

Homemade word-bank formats: JSON (word → array of definition objects), CSV/TSV (word in the first column, definitions in the remaining columns), plain text (word and definition separated by an agreed delimiter). See `examples/dict.json` in the repository for an example.

## Loading Dictionaries: Environment Variables

Both the CLI and the desktop GUI built from source honor the environment variables:

```bash
UNIDICT_DICTS="dict.json" unidict_cli_std hello            # 单本
UNIDICT_DICTS="a.ifo:b.mdx:/path/c.mdx" unidict_cli_std …  # 多本（路径分隔符冒号）
```

Overview of the common environment variables:

| Variable | Purpose |
|---|---|
| `UNIDICT_DICTS` | Dictionary list (multiple paths, separator-joined) |
| `UNIDICT_DICT_DIR` | Dictionary directory (whole-directory scan) |
| `UNIDICT_DATA_DIR` / `UNIDICT_CACHE_DIR` | Data and cache directories |
| `UNIDICT_MDICT_PASSWORD` | Default MDict password |

## Desktop GUI

If a `dict.json` sits in the same directory at startup, it is loaded automatically (the daily build packages are set up this way). The dictionary management entry supports importing files and scanning directories; enable/disable state, ordering, and enabled flags of dictionaries are persisted automatically and restored after a restart.

## Android

Dictionary management page: import `.ifo/.mdx` and similar files into the app's private directory via the system file picker (SAF), with enable/disable, delete, and entry-count views. First launch ships demo dictionaries in three formats and runs an automatic smoke test (`M2-SMOKE-OK`).

## Current State of Chinese Reference Works

To install Chinese dictionaries such as the Xinhua Dictionary or the Modern Chinese Dictionary: the `.mdx` format **works today**. The current state and roadmap of Chinese-specific search surfaces (pinyin lookup, radical/stroke count, traditional-simplified conversion) are covered in [server roadmap - Chinese format surface](/en/server#chinese-dictionary-format-surface-summary).
