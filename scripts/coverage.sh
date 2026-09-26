#!/usr/bin/env bash
# Unidict 覆盖率闸门：配置 coverage 构建 → 跑全部 ctest → gcovr 出汇总与
# 逐文件缺口 → 按阈值判定退出码。退出码是唯一可信的闸门信号。
#
# 两种模式：
#   （默认）   core/  std-only 纯逻辑层。Qt 全关，构 quickest，不碰 Qt。
#   --qt       Qt 层。core/ + adapters/qt/ + qmlui/ + gui/ + cli/，
#              需要能编 Qt 的环境；跑测试时强制 offscreen 平台插件。
#
# 用法：
#   scripts/coverage.sh                    # core/，lines 阈值 100
#   scripts/coverage.sh --qt               # Qt 层，lines 阈值 100
#   scripts/coverage.sh --threshold 95     # 临时放宽（CI 未达标时探路用）
#   scripts/coverage.sh --branches         # 额外打印分支覆盖明细
#   scripts/coverage.sh --html             # 额外生成 HTML 报告
#   scripts/coverage.sh --no-build         # 复用已有构建树，不重新编译
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE=core
THRESHOLD=100
SHOW_BRANCHES=0
MAKE_HTML=0
DO_BUILD=1

for arg in "$@"; do
    case "$arg" in
        --qt) MODE=qt; shift ;;
        --threshold) THRESHOLD="${2:?--threshold 需要一个数值}"; shift ;;
        --threshold=*) THRESHOLD="${arg#*=}"; shift ;;
        --branches) SHOW_BRANCHES=1; shift ;;
        --html) MAKE_HTML=1; shift ;;
        --no-build) DO_BUILD=0; shift ;;
        -h|--help) sed -n '2,16p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "未知参数: $arg（--help 看用法）" >&2; exit 2 ;;
    esac
done

command -v gcovr >/dev/null 2>&1 || {
    echo "需要 gcovr：pip install gcovr" >&2
    exit 1
}

if [[ "$MODE" == "qt" ]]; then
    BUILD_DIR="${ROOT}/build-qtcov"
    LABEL="Qt 层"
    # Qt 层的排除项：真·平台/UI 代码。列在这里是为了让排除可审计——
    # 任何一条被加进来都必须写明理由，todo.md「Qt 层缺口」有对应表格。
    #   gui/main.cpp               QWidget 接线，无可断言的纯逻辑
    #   gui/audio_recorder.cpp(.h)  Qt Multimedia 设备采集；仓库纪律明令
    #   gui/pcm_playback.cpp          "测试里不出现任何音频设备假设"，
    #   gui/waveform_widget.cpp      offscreen 环境会假绿
    #   qmlui/main.cpp             QML 应用引导
    #   qmlui/global_hotkeys.cpp   Windows RegisterHotKey 专属
    #   qmlui/startup_launcher.cpp Windows 注册表专属
    QT_EXCLUDES=(
        "${ROOT}/gui/main.cpp"
        "${ROOT}/gui/audio_recorder"
        "${ROOT}/gui/pcm_playback"
        "${ROOT}/gui/waveform_widget"
        "${ROOT}/qmlui/main.cpp"
        "${ROOT}/qmlui/global_hotkeys"
        "${ROOT}/qmlui/startup_launcher"
    )
    QT_FILTERS=(
        --filter "${ROOT}/core/"
        --filter "${ROOT}/adapters/qt/"
        --filter "${ROOT}/qmlui/"
        --filter "${ROOT}/gui/"
        --filter "${ROOT}/cli/"
    )
else
    BUILD_DIR="${ROOT}/build-cov"
    LABEL="core/ (std-only)"
    QT_EXCLUDES=()
    QT_FILTERS=(--filter "${ROOT}/core/")
fi

# --exclude-lines-by-pattern '^\s*\}[;]?\s*$'：只由右花括号（可带分号）
# 单独成行的行不携带任何可执行语句——多行 lambda / 变量声明的收尾都是这
# 形态。但 gcc 仍给它分配一个"不可执行"基本块，gcov 输出里标成 '====='，
# gcovr 默认当成 0 次执行的漏行。这类行永远补不上（没有测试能让一个 '}'
# 执行），留在分母里只会让 100% 变成不可能达成的目标。
BRACE_EXCL='^\s*\}[;]?\s*$'

# 组装 gcovr 参数数组（排除项在有值时才加，避免空参数被当成路径）
GCOVR_ARGS=(-r "${ROOT}")
for f in "${QT_FILTERS[@]}"; do GCOVR_ARGS+=("$f"); done
GCOVR_ARGS+=(--exclude "${BUILD_DIR}")
for e in "${QT_EXCLUDES[@]}"; do GCOVR_ARGS+=(--exclude "${e}"); done
GCOVR_ARGS+=(--exclude-lines-by-pattern "${BRACE_EXCL}")

if [[ "$DO_BUILD" -eq 1 ]]; then
    echo "==> 配置 coverage 构建 (${BUILD_DIR}, 模式 ${MODE})"
    if [[ "$MODE" == "qt" ]]; then
        : "${QT_PREFIX:=${CMAKE_PREFIX_PATH:-}}"
        if [[ -z "$QT_PREFIX" ]]; then
            echo "需要 Qt：请设置 CMAKE_PREFIX_PATH 或 QT_PREFIX 指向 Qt 6" >&2
            exit 1
        fi
        cmake -B "${BUILD_DIR}" -S "${ROOT}" \
            -DCMAKE_PREFIX_PATH="${QT_PREFIX}" \
            -DBUILD_TYPE=RelWithDebInfo \
            -DUNIDICT_ENABLE_COVERAGE=ON \
            >/dev/null
    else
        cmake -B "${BUILD_DIR}" -S "${ROOT}" \
            -DUNIDICT_BUILD_QT_CORE=OFF \
            -DUNIDICT_BUILD_ADAPTER_QT=OFF \
            -DUNIDICT_BUILD_QT_APPS=OFF \
            -DUNIDICT_BUILD_QT_TESTS=OFF \
            -DBUILD_TYPE=RelWithDebInfo \
            -DUNIDICT_ENABLE_COVERAGE=ON \
            >/dev/null
    fi
    echo "==> 编译"
    cmake --build "${BUILD_DIR}" -j"$(nproc)" >/dev/null
fi

# 旧一轮的 .gcda 必须先清掉，否则 gcov 把两轮数据累加，
# 报出"这一行上一轮跑过"的假覆盖，缺口被静默抹平。
find "${BUILD_DIR}" -name '*.gcda' -delete 2>/dev/null || true

# coverage 构建是 -O0 + 插桩，比普通构建慢一到两个数量级；机器稍有负载
# 就可能撞上 ctest 默认的 1500s 单测超时，把"机器忙"误报成"测试挂了"。
# 这里给足余量——真挂的测试会自己 assert 失败，不会靠超时暴露。
if [[ "$MODE" == "qt" ]]; then
    echo "==> 跑 Qt 测试（offscreen；覆盖率数据由这一步产生）"
    QT_QPA_PLATFORM=offscreen ctest --test-dir "${BUILD_DIR}" \
        --timeout 3600 --output-on-failure
else
    echo "==> 跑 std 测试（覆盖率数据由这一步产生）"
    ctest --test-dir "${BUILD_DIR}" --timeout 3600 --output-on-failure
fi

echo "==> 收集覆盖率（${LABEL}）"
REPORT="${BUILD_DIR}/coverage.txt"
( cd "${BUILD_DIR}" && gcovr "${GCOVR_ARGS[@]}" \
    --print-summary --txt 2>/dev/null | tee "${REPORT}" )

if [[ "$SHOW_BRANCHES" -eq 1 ]]; then
    echo
    echo "==> 分支明细（仅趋势跟踪，不设阈值，理由见 todo.md）"
    ( cd "${BUILD_DIR}" && gcovr "${GCOVR_ARGS[@]}" --txt-metric branch 2>/dev/null )
fi

if [[ "$MAKE_HTML" -eq 1 ]]; then
    echo
    echo "==> HTML 报告"
    ( cd "${BUILD_DIR}" && gcovr "${GCOVR_ARGS[@]}" \
        --html-details "${BUILD_DIR}/html" \
        --html-title "Unidict ${LABEL} coverage" 2>/dev/null )
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
echo "==> 阈值判定: ${LABEL} lines ${LINE_PCT}% (要求 >= ${THRESHOLD}%), functions ${FUNC_PCT}%"
if awk "BEGIN{exit !(${LINE_PCT} >= ${THRESHOLD})}"; then
    echo "PASS"
else
    echo "FAIL: ${LABEL} 行覆盖率 ${LINE_PCT}% 未达 ${THRESHOLD}%" >&2
    echo "      逐文件缺口见上方表格 Missing 列" >&2
    exit 1
fi
