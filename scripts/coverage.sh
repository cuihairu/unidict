#!/usr/bin/env bash
# Unidict core/ 覆盖率闸门：配置 coverage 构建 → 跑全部 std ctest →
# gcovr 出汇总与逐文件缺口 → 按阈值判定退出码。
#
# 只覆盖 core/（std-only 纯逻辑），不覆盖 Qt 层：gui/qmlui 碰音频设备与
# 窗口，CI offscreen 环境会假绿，且仓库纪律要求"纯逻辑与平台代码物理
# 分离"——平台层的覆盖率交给各自的 Qt Test，不在这里凑数字。
#
# 用法：
#   scripts/coverage.sh                    # 默认 lines 阈值 100
#   scripts/coverage.sh --threshold 95     # 临时放宽（CI 未达标时探路用）
#   scripts/coverage.sh --branches         # 额外打印分支覆盖明细
#   scripts/coverage.sh --html             # 额外生成 HTML 报告到 build-cov/html
#   scripts/coverage.sh --no-build         # 复用已有 build-cov，不重新编译
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-cov"
THRESHOLD=100
SHOW_BRANCHES=0
MAKE_HTML=0
DO_BUILD=1

for arg in "$@"; do
    case "$arg" in
        --threshold) THRESHOLD="${2:?--threshold 需要一个数值}"; shift ;;
        --threshold=*) THRESHOLD="${arg#*=}"; shift ;;
        --branches) SHOW_BRANCHES=1; shift ;;
        --html) MAKE_HTML=1; shift ;;
        --no-build) DO_BUILD=0; shift ;;
        -h|--help) sed -n '2,20p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "未知参数: $arg（--help 看用法）" >&2; exit 2 ;;
    esac
done

command -v gcovr >/dev/null 2>&1 || {
    echo "需要 gcovr：pip install gcovr" >&2
    exit 1
}

if [[ "$DO_BUILD" -eq 1 ]]; then
    echo "==> 配置 coverage 构建 (${BUILD_DIR})"
    cmake -B "${BUILD_DIR}" -S "${ROOT}" \
        -DUNIDICT_BUILD_QT_CORE=OFF \
        -DUNIDICT_BUILD_ADAPTER_QT=OFF \
        -DUNIDICT_BUILD_QT_APPS=OFF \
        -DUNIDICT_BUILD_QT_TESTS=OFF \
        -DBUILD_TYPE=RelWithDebInfo \
        -DUNIDICT_ENABLE_COVERAGE=ON \
        >/dev/null
    echo "==> 编译"
    cmake --build "${BUILD_DIR}" -j"$(nproc)" >/dev/null
fi

# 旧一轮的 .gcda 必须先清掉，否则 gcov 把两轮数据累加，
# 报出"这一行上一轮跑过"的假覆盖，缺口被静默抹平。
find "${BUILD_DIR}" -name '*.gcda' -delete 2>/dev/null || true

echo "==> 跑 std 测试（覆盖率数据由这一步产生）"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo "==> 收集覆盖率（core/）"
# --exclude-lines-by-pattern '^\s*\}[;]?\s*$'：只由右花括号（可带分号）
# 单独成行的行不携带任何可执行语句——多行 lambda / 变量声明的收尾都是这
# 形态。但 gcc 仍给它分配一个"不可执行"基本块，gcov 输出里标成 '====='，
# gcovr 默认当成 0 次执行的漏行。这类行永远补不上（没有测试能让一个 '}'
# 执行），留在分母里只会让 100% 变成不可能达成的目标。
REPORT="${BUILD_DIR}/coverage.txt"
( cd "${BUILD_DIR}" && gcovr -r "${ROOT}" \
    --filter "${ROOT}/core/" \
    --exclude "${BUILD_DIR}" \
    --exclude-lines-by-pattern '^\s*\}[;]?\s*$' \
    --print-summary \
    --txt 2>/dev/null | tee "${REPORT}" )

if [[ "$SHOW_BRANCHES" -eq 1 ]]; then
    echo
    echo "==> 分支明细（仅趋势跟踪，不设阈值，理由见 todo.md）"
    ( cd "${BUILD_DIR}" && gcovr -r "${ROOT}" \
        --filter "${ROOT}/core/" \
        --exclude "${BUILD_DIR}" \
        --exclude-lines-by-pattern '^\s*\}[;]?\s*$' \
        --txt-metric branch 2>/dev/null )
fi

if [[ "$MAKE_HTML" -eq 1 ]]; then
    echo
    echo "==> HTML 报告"
    ( cd "${BUILD_DIR}" && gcovr -r "${ROOT}" \
        --filter "${ROOT}/core/" \
        --exclude "${BUILD_DIR}" \
        --exclude-lines-by-pattern '^\s*\}[;]?\s*$' \
        --html-details "${BUILD_DIR}/html" \
        --html-title "Unidict core/ coverage" 2>/dev/null )
    echo "    ${BUILD_DIR}/html/index.html"
fi

# 阈值判定：读 gcovr 汇总里的 "lines: NN.N% (a out of b)"。
# 不靠颜色、不靠人眼看表格——退出码是唯一可信的闸门信号。
LINE_PCT="$(grep -oP 'lines:\s+\K[0-9.]+(?=%)' "${REPORT}" | tail -1)"
FUNC_PCT="$(grep -oP 'functions:\s+\K[0-9.]+(?=%)' "${REPORT}" | tail -1)"

if [[ -z "${LINE_PCT}" ]]; then
    echo "无法从覆盖率报告解析 lines 百分比" >&2
    exit 1
fi

echo
echo "==> 阈值判定: lines ${LINE_PCT}% (要求 >= ${THRESHOLD}%), functions ${FUNC_PCT}%"
if awk "BEGIN{exit !(${LINE_PCT} >= ${THRESHOLD})}"; then
    echo "PASS"
else
    echo "FAIL: core/ 行覆盖率 ${LINE_PCT}% 未达 ${THRESHOLD}%" >&2
    echo "      逐文件缺口见上方表格 Missing 列" >&2
    exit 1
fi
