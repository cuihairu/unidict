#!/usr/bin/env bash
# 一键拉取开源示例词典（下载资产不入库——.gitignore 挡 dictionaries/downloaded/）。
#
# 拉取清单（URL 均经实测，格式为解析器直接可吃的形态）：
#   1. ECDICT 简明英汉增强版   stardict(.ifo/.idx/.dict)  MIT          ~340 万词  70MB zip
#   2. WikDict zh-en           stardict 裸 idx/dict       CC BY-SA 4.0  ~1 万词    1.0MB zip
#   3. WikDict en-zh           stardict 裸 idx/dict       CC BY-SA 4.0  ~1 万词    1.6MB zip
#   4. FreeDict eng-deu        stardict(idx 需 gunzip)    GPL-3.0       ~26 万词  27MB tar.xz
#   5. FreeDict eng-zho        stardict(idx 需 gunzip)    GPL-3.0       ~1 万词   1.7MB tar.xz
# 外加仓库内置 dictionaries/ccedict-zh-en.json（CC-CEDICT 汉英 12.5 万词），
# 合计 6 个词典，下载完成即可开箱体验。
#
# 用法：
#   scripts/fetch_sample_dicts.sh                 # 全部拉取
#   scripts/fetch_sample_dicts.sh wikdict-zh-en   # 只拉指定的（见下方 CASE 名）
#   UNIDICT_DICTS="$(scripts/fetch_sample_dicts.sh --print-env)" build-std/Release/unidict_cli_std ...
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${UNIDICT_DICT_DOWNLOAD_DIR:-$ROOT/dictionaries/downloaded}"
mkdir -p "$DEST"

# Git Bash / 精简 runner 可能没有 unzip：回退到 python zipfile
unzip_or_python() { # <zip> <dest>
    if command -v unzip >/dev/null 2>&1; then
        unzip -q "$1" -d "$2"
    else
        python3 - "$1" "$2" <<'PY'
import sys, zipfile
zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])
PY
    fi
}

# 各字典：名字 → URL + 解包方式。数据集按 CC BY-SA / MIT / GPL-3.0 分发，
# 署名与许可文本见 dictionaries/downloaded/ATTRIBUTION.md（拉取时生成）。
fetch() {
    local name="$1" url="$2"
    if ls "$DEST/$name"/.done >/dev/null 2>&1; then
        echo "[skip] $name（已存在，删除 $DEST/$name 可重拉）"
        return 0
    fi
    echo "[fetch] $name"
    local tmp="$DEST/.tmp.$name"
    rm -rf "$tmp" && mkdir -p "$tmp"
    # GitHub TLS 在部分网络环境下间歇握手失败：重试 3 次退避
    local ok=0
    for attempt in 1 2 3; do
        if curl -fL --retry 2 --connect-timeout 20 -o "$tmp/pkg" "$url"; then ok=1; break; fi
        echo "  retry $attempt/3 failed for $name" >&2
        sleep $((attempt * 3))
    done
    if [[ $ok != 1 ]]; then echo "[FAIL] $name 下载失败（网络或 URL 失效），跳过" >&2; rm -rf "$tmp"; return 1; fi

    local out="$DEST/$name"
    mkdir -p "$out"
    case "$name" in
        ecdict-stardict)
            unzip_or_python "$tmp/pkg" "$tmp/x"
            # zip 内目录层级不定：把 ifo/idx/dict 三件套找出来平铺
            local ifo; ifo="$(find "$tmp/x" -name '*.ifo' | head -1)"
            if [[ -z $ifo ]]; then echo "[FAIL] ecdict 包内未找到 .ifo" >&2; rm -rf "$tmp" "$out"; return 1; fi
            local base; base="${ifo%.ifo}"
            cp "$base".ifo "$base".idx "$base".dict "$out/" 2>/dev/null || {
                # .dict.dz 形态：解压 dz 得裸 dict（解析器自动落缓存，但直接摊平更省心）
                cp "$base".ifo "$base".idx "$out/"
                local dz; dz="$(find "$tmp/x" -name '*.dict.dz' | head -1)"
                [[ -n $dz ]] && gunzip -kc "$dz" > "$out/$(basename "${dz%.dz}")"
            }
            ;;
        wikdict-*|freedict-*)
            mkdir -p "$tmp/x"
            case "$name" in
                wikdict-*) unzip_or_python "$tmp/pkg" "$tmp/x" ;;
                freedict-*) tar -xJf "$tmp/pkg" -C "$tmp/x" ;;
            esac
            # 平铺三件套；freedict 的 .idx.gz 解出裸 idx（解析器只认裸 .idx）
            local ifo; ifo="$(find "$tmp/x" -name '*.ifo' | head -1)"
            if [[ -z $ifo ]]; then echo "[FAIL] $name 包内未找到 .ifo" >&2; rm -rf "$tmp" "$out"; return 1; fi
            local base; base="${ifo%.ifo}"
            cp "$base".ifo "$out/"
            if [[ -f "$base.idx" ]]; then cp "$base.idx" "$out/"
            elif [[ -f "$base.idx.gz" ]]; then gunzip -kc "$base.idx.gz" > "$out/$(basename "$base").idx"; fi
            if [[ -f "$base.dict" ]]; then cp "$base.dict" "$out/"
            elif [[ -f "$base.dict.dz" ]]; then gunzip -kc "$base.dict.dz" > "$out/$(basename "$base").dict"; fi
            ;;
    esac
    rm -rf "$tmp"
    touch "$out/.done"
    echo "[ok] $name → $out"
}

FAILED=0
WANTED="${1:-all}"
want() { [[ $WANTED == all || $WANTED == "$1" ]]; }

if want ecdict-stardict; then fetch ecdict-stardict "https://github.com/skywind3000/ECDICT/releases/download/1.0.28/ecdict-stardict-28.zip" || FAILED=1; fi
if want wikdict-zh-en; then fetch wikdict-zh-en "https://download.wikdict.com/dictionaries/stardict/wikdict-zh-en.zip" || FAILED=1; fi
if want wikdict-en-zh; then fetch wikdict-en-zh "https://download.wikdict.com/dictionaries/stardict/wikdict-en-zh.zip" || FAILED=1; fi
if want freedict-eng-deu; then fetch freedict-eng-deu "https://download.freedict.org/dictionaries/eng-deu/1.9-fd1/freedict-eng-deu-1.9-fd1.stardict.tar.xz" || FAILED=1; fi
if want freedict-eng-zho; then fetch freedict-eng-zho "https://download.freedict.org/dictionaries/eng-zho/2025.11.23/freedict-eng-zho-2025.11.23.stardict.tar.xz" || FAILED=1; fi

cat > "$DEST/ATTRIBUTION.md" <<'EOF'
# 下载词典署名与许可

| 目录 | 词典 | 许可 | 来源 |
|---|---|---|---|
| ecdict-stardict | ECDICT 简明英汉增强版（~340 万词） | MIT（仓库口径；数据聚合自多个开放源） | https://github.com/skywind3000/ECDICT |
| wikdict-zh-en / wikdict-en-zh | WikDict 汉英/英汉（Wiktionary 派生） | CC BY-SA 4.0（经 DBnary） | https://download.wikdict.com |
| freedict-eng-deu | FreeDict 英德 | GPL-3.0 | https://freedict.org |
| freedict-eng-zho | FreeDict 英中 | GPL-3.0 | https://freedict.org |
| （仓库内置 ../ccedict-zh-en.json） | CC-CEDICT 汉英 12.5 万词 | CC BY-SA 4.0，署名见 ../CC-CEDICT-ATTRIBUTION.md | https://www.mdbg.net/chinese/export/cedict/ |
EOF

if [[ "${1:-}" == --print-env ]]; then
    # 打印 UNIDICT_DICTS 值（内置 CC-CEDICT + 所有已拉取的 stardict）
    local_env="$ROOT/dictionaries/ccedict-zh-en.json"
    for d in "$DEST"/*/; do
        [[ -f "$d/.done" ]] || continue
        ifo="$(find "$d" -maxdepth 1 -name '*.ifo' | head -1)"
        [[ -n $ifo ]] && local_env="$local_env:$(dirname "$ifo")/$(basename "$ifo")"
    done
    echo "$local_env"
    exit 0
fi

echo
echo "完成。使用："
echo "  UNIDICT_DICTS=\"\$(scripts/fetch_sample_dicts.sh --print-env)\" build-std/Release/unidict_cli_std 词典"
echo "  或： build-std/Release/unidict_cli_std --scan-dir $DEST -m prefix car"
[[ $FAILED == 1 ]] && { echo "（部分字典下载失败，见上方日志；重跑本脚本只补缺）" >&2; exit 1; }
exit 0
