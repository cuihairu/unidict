#!/usr/bin/env bash
# 符号分离（docs/crash_report_plan.md §6 分发口径）：主程序去符号、
# 符号文件留档不进分发包。Linux ELF+DWARF（objcopy 三步）；
# Windows 的 .pdb / macOS 的 .dSYM 天然分离，无须本脚本。
#
# 用法：scripts/symbols_split.sh <可执行文件> [归档目录]
#   产出：<exe>.debug 符号文件（默认在 exe 同目录；给了归档目录则
#         拷贝归档并带时间戳，报障时按版本取符号还原）
set -euo pipefail

if [ $# -lt 1 ]; then
    echo "用法: $0 <可执行文件> [归档目录]" >&2
    exit 1
fi

EXE=$1
if [ ! -f "$EXE" ]; then
    echo "错误: 找不到 $EXE" >&2
    exit 1
fi

DEBUG_FILE="$EXE.debug"

# 1. 抽符号（原 DWARF 进 .debug 文件）
objcopy --only-keep-debug "$EXE" "$DEBUG_FILE"
# 2. 主程序去调试段（符号信息只剩 debuglink 名）
strip --strip-debug "$EXE"
# 3. 挂回链：还原工具顺 debuglink 找回符号文件
objcopy --add-gnu-debuglink="$DEBUG_FILE" "$EXE"

if [ $# -ge 2 ]; then
    ARCHIVE_DIR=$2
    mkdir -p "$ARCHIVE_DIR"
    STAMP=$(date +%Y%m%d_%H%M%S)
    cp "$DEBUG_FILE" "$ARCHIVE_DIR/$(basename "$EXE").$STAMP.debug"
    echo "符号已归档: $ARCHIVE_DIR/$(basename "$EXE").$STAMP.debug"
fi

echo "符号分离完成:"
echo "  主程序(去符号): $EXE"
echo "  符号文件(留档): $DEBUG_FILE"
