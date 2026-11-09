// 并发 search 竞态回归（深审 P2：ensure_postings 的 const_cast 懒解码
// 非线程安全）。UDFT3 装载后的索引全部懒解码——并发 search 会同时进
// 同一 PostingEntry：一路 clear+重建 vec，另一路正迭代读 → 错误结果/崩。
// 修法：解码互斥 + 解码一次后翻 compressed 并释放 buf（后续调用走纯读
// 快路）。本测试压不出红（竞态窗口时序依赖）时仍是并发正确性金标准：
// 每线程每轮结果必须与单线程基线逐条一致。
#include <atomic>
#include <cassert>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#include <iostream>

#include "std/fulltext_index_std.h"

using namespace UnidictCoreStd;

int main() {
    namespace fs = std::filesystem;
    fs::path idx = fs::current_path() / "build-local" / "ft_race_test.udft";
    fs::create_directories(idx.parent_path());

    // 足量文档拉长解码窗口：大 posting 的首次懒解码耗时越长，clear+重建
    // 与迭代读的交叠概率越高；barrier 让 8 线程同一瞬间开跑，第一枪就
    // 全部撞进同词的未解码窗口
    FullTextIndexStd ft;
    std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> docs;
    const std::string words[] = {"bravo", "charlie", "delta",
                                 "echo", "foxtrot", "golf", "hotel"};
    const size_t nwords = sizeof(words) / sizeof(words[0]);
    for (int d = 0; d < 5000; ++d) {
        std::string text = "alpha ";
        for (size_t w = 0; w <= d % nwords + 8; ++w)
            text += words[(d + w) % nwords] + std::string(" ");
        docs.emplace_back(text, FullTextIndexStd::DocRef{0, d});
    }
    ft.build_from_documents(docs);
    ft.save(idx.string());

    FullTextIndexStd loaded;
    assert(loaded.load(idx.string()));
    assert(loaded.version() == 3);  // UDFT3：懒解码路径在测

    const std::string queries[] = {"alpha", "bravo delta", "echo hotel",
                                   "golf charlie bravo", "hotel"};
    const size_t nqueries = sizeof(queries) / sizeof(queries[0]);
    std::vector<std::vector<FullTextIndexStd::DocRef>> baseline;
    for (size_t qi = 0; qi < nqueries; ++qi)
        baseline.push_back(loaded.search(queries[qi], 20));

    // 新实例重放：baseline 不得由已解码状态污染（全部 fast path 后结果
    // 必须与首趟一致）
    FullTextIndexStd replay;
    assert(replay.load(idx.string()));

    volatile bool bad = false;
    const size_t nthreads = 8;
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> pool;
    for (size_t t = 0; t < nthreads; ++t) {
        pool.emplace_back([&, t]() {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (!go.load(std::memory_order_relaxed))
                std::this_thread::yield();
            // 第一枪齐射同一个大词（未解码窗口最大），再混打全查询集
            for (int round = 0; round < 200; ++round) {
                for (size_t qi = 0; qi < nqueries; ++qi) {
                    const FullTextIndexStd& src = (round == 0) ? loaded : replay;
                    auto got = src.search(queries[qi], 20);
                    const auto& want = baseline[qi];
                    if (got.size() != want.size()) { bad = true; return; }
                    for (size_t i = 0; i < got.size(); ++i)
                        if (got[i].dict != want[i].dict
                            || got[i].word != want[i].word) { bad = true; return; }
                }
            }
        });
    }
    // 全员就位后开跑（未解码窗口齐射）
    while (ready.load() < static_cast<int>(nthreads))
        std::this_thread::yield();
    go.store(true, std::memory_order_relaxed);
    for (auto& th : pool) th.join();
    assert(!bad);
    std::cout << "OK\n";
    return 0;
}
