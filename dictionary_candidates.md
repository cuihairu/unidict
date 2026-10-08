# Open Source Dictionary Candidates for Unidict

Based on code analysis of `/home/cui/workspaces/unidict/core/`, the following dictionary formats are natively supported:

**Supported Formats (from code):**
- **Stardict** – `.stardict` format (core/stardict_parser.h/.cpp)
- **MDict** – `.mdx`/`.dtxt` format (core/mdict_parser.h/.cpp)
- **JSON** – `{"name", "description", "entries": [{"word", "definition"}, ...]}` (examples/dict.json)
- **EPUB** – zip container with XHTML entries (std/epub_parser_std.*)
- **CSV** – `word<separator>definition` per line, auto-detect tab/comma/semicolon/pipe (std/csv_parser_std.*)
- **DSL** – ABBYY Lingvo DSL format (std/dsl_parser_std.*)

---

## Candidate Table (6+ Entries)

| # | Name | Format | License | Entries | Download URL | Direct Support | Conversion Needed |
|---|------|--------|---------|---------|--------------|----------------|-------------------|
| 1 | **CC-CEDICT** (汉英词典) | Text (CC-CEDICT syntax) → JSON | CC BY-SA 4.0 | ~125K | <https://cc-cedict.org/editor/editor.php?handler=Download> (`cedict_1_0_ts_utf-8_mdbg.zip` or `.txt.gz`) | ✅ **JSON** – `tools/build_ccedict_dict.py` converts raw CC-CEDICT text to Unidict JSON format directly | None needed; built-in converter |
| 2 | **ECDICT** (简明英汉增强版) | CSV (13 fields, UTF-8) | MIT | ~76K base / ~3.4M ultimate | <https://github.com/skywind3000/ECDICT/releases> (`ecdict.csv` or `ecdict-ultimate-csv.zip`) | ✅ **CSV** – `std/csv_parser_std.cpp` parses `word,definition` pairs directly. ECDICT CSV has lemma, pos, phonetics, definitions, etc.; map first two fields to word+definition | None needed; natively supported |
| 3 | **Open English WordNet** (Open English Wordnet) | JSON / XML (LMF) / RDF/Turtle | CC BY 4.0 (Open) / WordNet License (Princeton) | ~150K | <https://github.com/globalwordnet/english-wordnet/releases> (`english-wordnet-2025-json.zip`) or <https://en-word.net/downloads> | ✅ **JSON** – export format matches Unidict `{"word", "definition"}` structure directly | None needed; JSON export available |
| 4 | **FreeDict** (Free bilingual dictionaries) | Stardict (.stardict) | Mix: GPL, CC, others | Varies by dict (hundreds of entries each) | <https://freedict.org/downloads/> (StarDict format) | ✅ **Stardict** – `core/stardict_parser.cpp` / `adapters/qt/stardict_parser.h` natively supported | None needed; direct Stardict support |
| 5 | **GCIDE** (GNU Collaborative International Dictionary) | XML (GCIDE_XML) / plain text | GPL v3 | ~217K | <https://gcide.gnu.org.ua/download> (`gcide-0.54.tar.gz`) or <https://github.com/scillidan/share_gcide> | ❌ No direct format. Requires converter to JSON or Stardict. XML structure differs from Unidict format. | Yes – custom converter needed |
| 6 | **Wiktionary derivatives** (Kaikki / wiktextract) | JSON / XML / CSV | CC BY-SA 4.0 | Varies (hundreds of thousands) | <https://github.com/kaikki/wiktextract> (exports to JSON/CSV/TSV) | ✅ **JSON** or **CSV** – wiktextract can export in Unidict-compatible formats | None needed if JSON/CSV export used |
| 7 | **CEDIC T** (older CEDICT) | Text (same as CC-CEDICT) | Varies (often NC-restricted) | ~100K | <https://cedict.org/> | ✅ **JSON** – same converter as CC-CEDICT (`tools/build_ccedict_dict.py`) | None needed; same as #1 but license may be more restrictive |

---

## Summary of Direct vs. Needed Conversion

| Format | Natively Eaten ✅ | Needs Converter ❌ | Converter Available? |
|--------|-------------------|---------------------|----------------------|
| JSON (Unidict schema) | All JSON-exportable dictionaries (WordNet, Wiktionary, CC-CEDICT→JSON) | – | Built-in (`tools/build_ccedict_dict.py`) |
| CSV (word\|definition) | ECDICT CSV, simple CSV dictionaries | GCIDE text, complex DSL | `std/csv_parser_std.cpp` handles simple `word,definition` |
| Stardict | FreeDict dictionaries, ECDICT→Stardict builds | – | `core/stardict_parser.h` fully supports |
| MDict | Some commercial/MDict-built dictionaries | – | `core/mdict_parser.h` fully supports |
| EPUB | EPUB-format literary dictionaries | – | `std/epub_parser_std.cpp` supports zip+XHTML |
| DSL (ABBYY) | ABBYY Lingvo format dictionaries | – | `std/dsl_parser_std.cpp` supports DSL markup |

---

## Recommendations

1. **ECDICT** – Best choice if you want a large English-Chinese dictionary with MIT license and CSV format is directly supported. No conversion needed; just place `ecdict.csv` (or the smaller `ecdict.mini.csv` for testing) alongside your app and load it via the CSV parser.

2. **CC-CEDICT** – Best choice for Chinese-English. Use the built-in `tools/build_ccedict_dict.py` to convert the downloaded `cedict_ts.u8` to `dictionaries/ccedict-zh-en.json`, then load via the JSON parser. License is CC BY-SA 4.0, permissive for most uses.

3. **Open English WordNet** – Best for an English-English lexical database. JSON export matches Unidict schema exactly; CC BY 4.0 license is very permissive.

4. **FreeDict** – Good for bilingual dictionaries in Stardict format. Verify each sub-dictionary's license before use (GPL some, CC others).

5. **Avoid GCIDE** unless you write a custom converter; its GPL license and XML format require additional work.

6. **Wiktionary derivatives** – viable if you need multilingual coverage; ensure you comply with CC BY-SA 4.0 attribution requirements.

---

## How to Load a Dictionary in Unidict

### JSON (most common)
```cpp
// Load from JSON file matching examples/dict.json schema
DictionaryParserJson parser;
parser.load_dictionary("path/to/dict.json");
```

### CSV (simple word/definition pairs)
```cpp
CsvParserStd parser;
parser.load_dictionary("path/to/dict.csv"); // auto-detects comma/tab/semicolon/pipe
```

### Stardict
```cpp
StardictParser parser;  // from core/
parser.load_dict_file("path/to/dict.stardict");
```