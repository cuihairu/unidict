#!/usr/bin/env python3
"""CC-CEDICT → unidict JSON 词典转换器（BUG-004 内置词典资产）。

输入：MDBG 导出的 cedict_ts.u8（https://www.mdbg.net/chinese/export/cedict/）
行格式：`繁體 简体 [pin1yin1] /释义1/释义2/`
输出：dictionaries/ccedict-zh-en.json（紧凑、项目词典格式）

许可：CC-CEDICT 以 CC BY-SA 4.0 发布；本文件是其改编产物，同样以
CC BY-SA 4.0 分发（dictionaries/CC-CEDICT-ATTRIBUTION.md 署名）。

词头策略：仅取简体词头（繁体查询不命中是内置版的既定取舍，需要
繁体的用户可自行导入 CC-CEDICT 源文件，署名文件里有出处）。释义
格式：`[拼音] 释义1; 释义2`。
"""

import json
import re
import sys
from pathlib import Path

LINE_RE = re.compile(r"^(\S+) (\S+) \[([^\]]+)\] /(.*)/$")


def convert(src: Path, dst: Path) -> int:
    entries = []
    skipped = 0
    for line in src.read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        m = LINE_RE.match(line)
        if not m:
            skipped += 1
            continue
        _trad, simp, pinyin, defs = m.groups()
        definition = f"[{pinyin}] " + "; ".join(
            d.strip() for d in defs.split("/") if d.strip()
        )
        entries.append({"word": simp, "definition": definition})
    doc = {
        "name": "CC-CEDICT 汉英词典",
        "description": "CC-CEDICT (MDBG) 内置版 · CC BY-SA 4.0 · "
                       "https://www.mdbg.net/chinese/cedict/",
        "entries": entries,
    }
    dst.write_text(
        json.dumps(doc, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )
    if skipped:
        print(f"warning: {skipped} malformed lines skipped", file=sys.stderr)
    return len(entries)


if __name__ == "__main__":
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/cedict_ts.u8")
    dst = (
        Path(sys.argv[2])
        if len(sys.argv) > 2
        else Path(__file__).resolve().parent.parent / "dictionaries" / "ccedict-zh-en.json"
    )
    n = convert(src, dst)
    print(f"entries={n} size={dst.stat().st_size} -> {dst}")
