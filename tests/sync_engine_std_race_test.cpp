// 并发调用竞态回归（深审 P2）：SyncEngineStd 可变状态（outbox_/local_seq_/
// cursor_/state_）无同步，多线程同时 enqueue/sync/观测就是数据竞争。
// 修法：公共入口单互斥串行化（enqueue 等 sync 一轮收工再进 outbox，语义
// 无损）。TSAN 下未修必红；修复后账目必须自洽：入队数 = 本地序 = 回放
// 词数，sync 收敛后 outbox 清空。
// 审核批 2026-11-12 起 sync 另有单飞闸门（锁不跨网络）：并发 sync 被
// 拒绝方按合同重试，收敛断言不变。
#include <atomic>
#include <cassert>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <iostream>

#include "std/sync_engine_std.h"

using namespace UnidictCoreStd;

// 测试双：内存传输（自身互斥——引擎外的共享对象自管，TSAN 只许报引擎）
namespace {
class MemTransport : public SyncTransportStd {
public:
    std::mutex mu;
    std::vector<RemoteOpStd> log;
    uint64_t top = 0;
    std::vector<std::string> acked_ids;

    bool meta(const std::string&, GroupMetaStd* out, std::string*) override {
        std::lock_guard<std::mutex> lk(mu);
        out->latest_seq = top;
        out->snapshot_up_to_seq = 0;
        return true;
    }
    bool push_ops(const std::string&, const std::vector<EnqueuedOpStd>& ops,
                  std::vector<std::string>* acked, std::string*) override {
        std::lock_guard<std::mutex> lk(mu);
        for (const auto& op : ops) {
            RemoteOpStd r;
            r.seq = ++top;
            r.op_id = op.op_id;
            r.payload = op.payload;
            log.push_back(std::move(r));
            acked->push_back(op.op_id);
        }
        return true;
    }
    bool pull_ops(const std::string&, uint64_t since, size_t,
                  std::vector<RemoteOpStd>* out, uint64_t* cursor,
                  bool* has_more, std::string*) override {
        std::lock_guard<std::mutex> lk(mu);
        for (const auto& op : log)
            if (op.seq > since) out->push_back(op);
        *cursor = top;
        *has_more = false;
        return true;
    }
    bool put_snapshot(const std::string&, uint64_t, const std::string&,
                      std::string*) override { return true; }
    bool get_snapshot(const std::string&, uint64_t*, std::string*,
                      std::string*) override { return false; }
};
}  // namespace

int main() {
    const int kEnqueuers = 6;
    const int kOpsPer = 200;
    const int kSyncers = 2;

    SyncEngineStd engine;
    MemTransport transport;
    const std::string gid = "race-group-gid-0001";

    volatile bool bad = false;
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> pool;
    for (int t = 0; t < kEnqueuers; ++t) {
        pool.emplace_back([&, t]() {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (!go.load(std::memory_order_relaxed))
                std::this_thread::yield();
            for (int i = 0; i < kOpsPer; ++i) {
                auto id = engine.enqueue(SyncOpType::AddEntry,
                                         "w" + std::to_string(t) + "_" + std::to_string(i));
                if (id.empty()) { bad = true; return; }
            }
        });
    }
    for (int t = 0; t < kSyncers; ++t) {
        pool.emplace_back([&]() {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (!go.load(std::memory_order_relaxed))
                std::this_thread::yield();
            for (int r = 0; r < 50; ++r) {
                // 单飞闸门（审核批 2026-11-12）：并发 sync 有一方被闸门
                // 拒绝（"sync already in progress"）是合同内结果——重试
                // 到成功；其余失败才算坏
                std::string err;
                for (;;) {
                    if (engine.sync(transport, gid, &err)) break;
                    if (err != "sync already in progress") { bad = true; return; }
                    std::this_thread::yield();
                }
                // 观测面并发读（TSAN 报 shared 状态读与写竞争）
                (void)engine.outbox_size();
                (void)engine.local_seq();
                (void)engine.cursor();
            }
        });
    }
    while (ready.load() < kEnqueuers + kSyncers) std::this_thread::yield();
    go.store(true, std::memory_order_relaxed);
    for (auto& th : pool) th.join();
    assert(!bad);

    // 账目自洽：入队 6*200，sync 一轮推净 outbox（拉回自身回显幂等）
    const int total = kEnqueuers * kOpsPer;
    std::string err;
    int guard = 0;
    while (engine.outbox_size() > 0 && guard++ < 10)
        assert(engine.sync(transport, gid, &err));
    assert(engine.outbox_size() == 0);
    assert(engine.local_seq() == (uint64_t)total);
    assert(engine.state().words.size() == (size_t)total);
    assert(engine.cursor() == transport.top);
    std::cout << "OK\n";
    return 0;
}
