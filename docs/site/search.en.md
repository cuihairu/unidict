> Language: **English** | [简体中文](./search.md)

# Looking Up Words

Unidict search has six modes, and the CLI and desktop GUI share one and the same engine; the GUI adds a layer of **aggregated lookup** on top — one query returns definitions from multiple dictionaries.

## CLI: How to Choose a Mode

::: code-group

```bash [Exact]
# 默认模式：词形完全匹配
unidict_cli_std -d dict.mdx hello
```

```bash [Prefix]
# 给开头就往下补——适合边输边看候选
unidict_cli_std -d dict.mdx -m prefix -p inter
```

```bash [Fuzzy]
# 拼写接近的词都出来——记不清拼写时用
unidict_cli_std -d dict.ifo -m fuzzy -p helo
```

```bash [Full-text]
# 在释义正文里搜，倒排索引 + TF/IDF 排序
unidict_cli_std -d dict.mdx -m fulltext --pattern "annual meeting"
```

```bash [Wildcard / regex]
unidict_cli_std -d dict.mdx --wildcard "hel*o"
unidict_cli_std -d dict.mdx --regex "^h.*o$"
```

:::

`-d` accepts StarDict's `.ifo`, MDict's `.mdx`, DSL, JSON, CSV, and any other supported format (see the [format matrix](/dictionaries)).

## Desktop GUI

Search box in the left column + an "Automatic" mode dropdown: switch between exact/prefix/fuzzy/full-text modes; the three tabs at the top of the query area — Results / History / Vocabulary Book — switch between looking up words, flipping through records, and saving vocabulary. Aggregated results are displayed grouped by dictionary, with each definition carrying Read-aloud / Favorite / Copy actions and forward/backward paging (navigation history within an entry).

## Dictionary Order and Multiple Dictionaries

You can load multiple dictionaries at once (`UNIDICT_DICTS` separates multiple paths with a colon or semicolon, or import several files in the GUI); aggregated lookups merge by dictionary priority; in the desktop GUI, each dictionary's enabled state and order are persisted automatically and restored on the next launch.

## Search Details Quick Reference

- Chinese queries are normalized full-width → half-width, with full-width punctuation folded; Latin words get accent folding (`café` ↔ `cafe`)
- Full-text search segments Chinese by per-CJK-character + character-bigram tokenization (finer word-segmentation improvements are on the [roadmap](/server#里程碑))
- The CLI is scoped to pure lookup and diagnostics: it writes no history and has no vocabulary book entry; learning management lives in the desktop GUI and Android
