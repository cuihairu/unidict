#!/bin/bash

# Unidict Performance Benchmark Script
# 测试各种搜索模式和操作的执行时间

set -e

DICT_PATH="examples/dict.json"
# CLI 可执行文件定位：CLI_PATH 环境变量优先；否则按已知构建布局探测
#（运行时产物统一收敛到 <builddir>/Release/，见根 CMakeLists 输出目录设置）
if [ -z "${CLI_PATH:-}" ]; then
    for cand in ./build/Release/unidict_cli_std \
                ./build-std/Release/unidict_cli_std \
                ./build/cli-std/unidict_cli_std \
                ./build-std/cli-std/unidict_cli_std; do
        if [ -x "$cand" ]; then CLI_PATH="$cand"; break; fi
    done
fi
ITERATIONS=10

if [ ! -f "$CLI_PATH" ]; then
    echo "错误: CLI未找到，请先构建项目"
    echo "运行: cmake --build build"
    exit 1
fi

if [ ! -f "$DICT_PATH" ]; then
    echo "错误: 测试词典文件未找到: $DICT_PATH"
    exit 1
fi

export UNIDICT_DICTS="$DICT_PATH"

echo "=== Unidict 性能基准测试 ==="
echo "词典文件: $DICT_PATH"
echo "CLI路径: $CLI_PATH"
echo "测试迭代次数: $ITERATIONS"
echo

# 函数：运行基准测试
run_benchmark() {
    local test_name="$1"
    local command="$2"
    local iterations="${3:-$ITERATIONS}"

    echo "测试: $test_name"
    echo "命令: $command"

    local total_user=0
    local total_sys=0
    local total_real=0

    for i in $(seq 1 $iterations); do
        local output=$(/usr/bin/time -p bash -c "$command" 2>&1)
        local user=$(echo "$output" | grep "user" | awk '{print $2}')
        local sys=$(echo "$output" | grep "sys" | awk '{print $2}')
        local real=$(echo "$output" | grep "real" | awk '{print $2}')

        total_user=$(echo "$total_user + $user" | bc -l)
        total_sys=$(echo "$total_sys + $sys" | bc -l)
        total_real=$(echo "$total_real + $real" | bc -l)
    done

    local avg_user=$(echo "scale=3; $total_user / $iterations" | bc -l)
    local avg_sys=$(echo "scale=3; $total_sys / $iterations" | bc -l)
    local avg_real=$(echo "scale=3; $total_real / $iterations" | bc -l)

    echo "  平均用户时间: ${avg_user}s"
    echo "  平均系统时间: ${avg_sys}s"
    echo "  平均实际时间: ${avg_real}s"
    echo

    # 记录结果到文件
    echo "$test_name,${avg_user},${avg_sys},${avg_real}" >> benchmark_results.csv
}

# 函数：测试内存使用（GNU time -v 输出 KB；BSD time -l 输出字节——统一归 KB）
run_memory_test() {
    local test_name="$1"
    local command="$2"

    echo "内存测试: $test_name"

    local output max_res max_kb
    if /usr/bin/time -v true >/dev/null 2>&1; then
        output=$(/usr/bin/time -v bash -c "$command" 2>&1)
        max_res=$(echo "$output" | grep -i "maximum resident set size" | awk '{print $NF}')
        max_kb=$max_res
    else
        output=$(/usr/bin/time -l bash -c "$command" 2>&1)
        max_res=$(echo "$output" | grep -i "maximum resident set size" | awk '{print $1}')
        max_kb=$(echo "scale=0; $max_res / 1024" | bc -l)
    fi

    # 转换为MB
    local max_mb=$(echo "scale=2; $max_kb / 1024" | bc -l)
    echo "  最大内存使用: ${max_mb}MB"
    echo "  最大内存(KB): $max_kb"
    echo

    echo "$test_name,$max_kb,$max_mb" >> memory_results.csv
}

# 检查bc命令是否可用
if ! command -v bc &> /dev/null; then
    echo "错误: 需要bc命令进行数值计算"
    exit 1
fi

# 创建结果文件
echo "Test,UserTime,SysTime,RealTime" > benchmark_results.csv
echo "Test,MaxResidentKB,MaxResidentMB" > memory_results.csv

# 1. 基本搜索测试
echo "1. 基本搜索测试"
run_benchmark "精确匹配" "$CLI_PATH --mode exact hello" 20
run_benchmark "前缀搜索" "$CLI_PATH --mode prefix h" 20
run_benchmark "模糊搜索" "$CLI_PATH --mode fuzzy helo" 10
run_benchmark "通配符搜索" "$CLI_PATH --mode wildcard h*o" 10

# 2. 系统操作测试
echo "2. 系统操作测试"
run_benchmark "列出词典" "$CLI_PATH --list-dicts" 5
run_benchmark "索引计数" "$CLI_PATH --index-count" 5
run_benchmark "显示词汇" "$CLI_PATH --dump-words 50" 5

# 3. 索引操作测试
echo "3. 索引操作测试"
run_benchmark "索引保存" "$CLI_PATH --index-save /tmp/test.index --mode exact hello" 5
run_benchmark "索引加载" "$CLI_PATH --index-load /tmp/test.index --mode exact hello" 10

# 4. 内存测试
echo "4. 内存使用测试"
run_memory_test "精确匹配内存" "$CLI_PATH --mode exact hello"
run_memory_test "前缀搜索内存" "$CLI_PATH --mode prefix h"
run_memory_test "模糊搜索内存" "$CLI_PATH --mode fuzzy helo"

# 5. 全文搜索测试（如果支持）
echo "5. 全文搜索测试"
run_benchmark "全文搜索" "$CLI_PATH --mode fulltext greeting" 10

# 6. 缓存性能测试
echo "6. 缓存性能测试"
run_benchmark "清除缓存" "$CLI_PATH --clear-cache" 3
run_benchmark "缓存大小" "$CLI_PATH --cache-size" 5

# 清理临时文件
rm -f /tmp/test.index

echo "=== 测试完成 ==="
echo "结果已保存到:"
echo "  - benchmark_results.csv (时间基准)"
echo "  - memory_results.csv (内存使用)"
echo
echo "显示结果:"
echo "  cat benchmark_results.csv"
echo "  cat memory_results.csv"

# 显示摘要
if [ -f benchmark_results.csv ]; then
    echo
    echo "=== 性能摘要 ==="
    echo "时间基准结果 (CSV格式):"
    column -t -s, benchmark_results.csv
fi

if [ -f memory_results.csv ]; then
    echo
    echo "内存使用摘要 (CSV格式):"
    column -t -s, memory_results.csv
fi