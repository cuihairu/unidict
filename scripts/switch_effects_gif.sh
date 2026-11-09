#!/usr/bin/env bash
# 配置开关效果 GIF 流水线（DOC-GIF）：跑离屏沙盒开关对（真实拨动路径），
# 逐对打包双帧 GIF 落 docs/site/config-effects/assets/，供「配置效果」页
# 双语嵌入。开关对本身由 ctest ui_sandbox_switch_effects 回归把关（两帧
# 相同即红），本脚本只做「跑沙盒 + 裁剪 + 合 GIF」。
#
# 用法：scripts/switch_effects_gif.sh [build 目录]（默认 build；沙盒需
# UNIDICT_BUILD_UI_SANDBOX=ON 已构建）。依赖 ImageMagick（magick）。
set -euo pipefail

BUILD_DIR=${1:-build}
BIN="$BUILD_DIR/Release/unidict_ui_sandbox"
ASSETS=docs/site/config-effects/assets
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

[ -x "$BIN" ] || BIN="$BUILD_DIR/unidict_ui_sandbox"
[ -x "$BIN" ] || { echo "sandbox binary not found under $BUILD_DIR" >&2; exit 2; }

echo "== run sandbox switch pass =="
UNIDICT_SANDBOX_SWITCHES=1 \
UNIDICT_DICTS=dictionaries/ccedict-zh-en.json \
  "$BIN" "$WORK"

mkdir -p "$ASSETS"

# 抽屉只占右上 420x535（win.width 1200 时）：单开关对裁抽屉区，lang/theme
# 整窗变化走全幅。delay 80 = 每帧 0.8s，-loop 0 两帧无限交替。
pack() { # <name> <full|drawer>
    local name=$1 mode=$2
    local f0="$WORK/sw_${name}_0.png" f1="$WORK/sw_${name}_1.png"
    [ -f "$f0" ] || { echo "skip $name (not captured on this platform)"; return; }
    if [ "$mode" = full ]; then
        magick "$f0" "$f1" -delay 80 -loop 0 "$ASSETS/$name.gif"
    else
        magick "$f0" -crop 420x535+780+0 +repage "$WORK/g_${name}_0.png"
        magick "$f1" -crop 420x535+780+0 +repage "$WORK/g_${name}_1.png"
        magick "$WORK/g_${name}_0.png" "$WORK/g_${name}_1.png" \
            -delay 80 -loop 0 "$ASSETS/$name.gif"
    fi
    echo "packed $ASSETS/$name.gif"
}

pack clipboard drawer
pack quicklookup drawer
pack selection drawer      # 平台不支持时沙盒跳过，此处自动略过
pack startup drawer
pack dict_enable drawer
pack sync drawer
pack lang full
pack theme full
pack textnorm drawer
pack aicache drawer

echo "== done: $ASSETS =="
ls -la "$ASSETS"
