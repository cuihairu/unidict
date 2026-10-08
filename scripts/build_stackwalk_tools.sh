#!/usr/bin/env bash
# 编译 minidump 还原工具（docs/crash_report_plan.md §6）：从
# google/breakpad 源码编 dump_syms + minidump_stackwalk 到 build-tools/。
#
# 为什么用 Breakpad 工具而不是 Crashpad 的：Crashpad 自带的
# minidump_stackwalk 只解 unwind 不做符号查表；Breakpad 的
# dump_syms/minidump_stackwalk 是符号化还原的成熟组合（选型已钉：
# Crashpad 采集 + Breakpad 工具还原，两家的 minidump 格式兼容）。
# 两个工具只进宿主机 build-tools/（gitignore），不进产物、不进主
# CMake——避免把 breakpad 拖进应用构建。
#
# 用法：scripts/build_stackwalk_tools.sh [breakpad_commit]
#   默认钉版 6598c9c3（2026-10-01，google/breakpad master HEAD）
set -euo pipefail

PINNED_COMMIT=${1:-6598c9c3}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$ROOT/build-tools"
SRC_DIR="$TOOLS_DIR/breakpad-src"

mkdir -p "$TOOLS_DIR"

# 克隆重试：本环境对 github/googlesource 有间歇性 TLS 干扰
# （握手失败偶发，重试即过），故 ×3
clone_retry() {
    local url=$1 dir=$2
    for _ in 1 2 3; do
        git clone -q "$url" "$dir" && return 0
        rm -rf "$dir"
        sleep 2
    done
    return 1
}

# 源码：钉版克隆（已存在则复用并校验 commit）
if [ ! -d "$SRC_DIR" ]; then
    clone_retry https://github.com/google/breakpad.git "$SRC_DIR"
    git -C "$SRC_DIR" checkout "$PINNED_COMMIT"
fi
CURRENT=$(git -C "$SRC_DIR" rev-parse HEAD)
if [ "$CURRENT" != "$(git -C "$SRC_DIR" rev-parse "$PINNED_COMMIT")" ]; then
    echo "源码 commit 与钉版不符（$CURRENT != $PINNED_COMMIT），重置" >&2
    git -C "$SRC_DIR" checkout "$PINNED_COMMIT"
fi

# breakpad 依赖 linux-syscall-support 头（与 crashpad 同一个库）
LSS_DIR="$TOOLS_DIR/lss-src"
if [ ! -d "$LSS_DIR" ]; then
    clone_retry https://chromium.googlesource.com/linux-syscall-support "$LSS_DIR"
    git -C "$LSS_DIR" checkout e1e7b0a
fi
mkdir -p "$SRC_DIR/third_party/lss"
cp "$LSS_DIR/linux_syscall_support.h" "$SRC_DIR/third_party/lss/"

# 配置编译（GNU autotools；只编两个工具，不跑测试）
cd "$SRC_DIR"
if [ ! -x configure ]; then
    ./autogen.sh
fi
if [ ! -f config.status ]; then
    ./configure --disable-dependency-tracking
fi
make -j"$(nproc 2>/dev/null || echo 4)" \
    src/processor/minidump_stackwalk src/tools/linux/dump_syms/dump_syms

# 收口到 build-tools/ 根（稳定路径，文档与验收脚本引用这里）
cp src/processor/minidump_stackwalk "$TOOLS_DIR/minidump_stackwalk"
cp src/tools/linux/dump_syms/dump_syms "$TOOLS_DIR/dump_syms"

echo "还原工具就绪（宿主机专用，不进产物）:"
echo "  $TOOLS_DIR/dump_syms            # ELF+DWARF → breakpad 符号"
echo "  $TOOLS_DIR/minidump_stackwalk   # minidump + symbols/ → 调用栈"
echo ""
echo "用法见 docs/crash_report_plan.md §6：dump_syms unidict_qml > x.sym →"
echo "按 MODULE 行 uuid 摆进 symbols/<name>/<uuid>/<name>.sym →"
echo "minidump_stackwalk <dump>.dmp symbols/"
