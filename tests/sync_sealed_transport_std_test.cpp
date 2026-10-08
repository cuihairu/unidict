// B5 传输密封层纯 std 测试（server_plan §7；红线「中转只见密文」）：
// 密封后中转侧不见明文 / 双引擎经密封链收敛 / 篡改与未持钥拒绝 /
// 快照密封往返 / 密钥轮换多版本可解 / 参数与空集直通。
// 传输面用 tests/sync_relay_memory_std.h（内存 relay + 只读观测缝）。
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "sync_relay_memory_std.h"
#include "sync_sealed_transport_std.h"

using namespace UnidictCoreStd;

namespace {

const char* kGid = "abcdefgh12345678";
// 测试定钥（两环同钥 = 同组建组与入组配对完成后的形态）
const std::string kKey32(32, '\x11');

SyncKeyRingStd make_ring() {
    SyncKeyRingStd ring;
    ring.import_key(1, kKey32);
    return ring;
}

// 1) 中转只见密文 + 双引擎收敛：A 经密封链推 op，中转 binlog 无明文、
//    长度足额（版本头+nonce+tag）；B 经密封链拉取开封回放，状态一致
void test_ciphertext_only_and_converge() {
    MemoryRelay relay;
    SyncKeyRingStd ring_a = make_ring();
    SyncKeyRingStd ring_b = make_ring();
    SyncSealedTransportStd sealed_a(relay, ring_a);
    SyncSealedTransportStd sealed_b(relay, ring_b);
    SyncEngineStd a("devA"), b("devB");

    assert(a.enqueue(SyncOpType::AddEntry, "mathematics") == "devA:1");
    assert(a.enqueue(SyncOpType::RecordHistory, "mathematics", "", 1234) ==
           "devA:2");
    std::string err;
    assert(a.sync(sealed_a, kGid, &err));

    // 中转侧：binlog 每条都不含明文、且 ≠ 明文、长度 ≥ 密封开销
    for (const RemoteOpStd& op : relay.binlog_of(kGid)) {
        assert(op.payload.find("mathematics") == std::string::npos);
        assert(op.payload != "mathematics");
        assert(op.payload.size() >= kSyncSealedOverhead);
    }

    assert(b.sync(sealed_b, kGid, &err));
    assert(b.state() == a.state());
    assert(b.state().words.size() == 1);
    assert(b.state().history.size() == 1);
}

// 2) 篡改拒收：中转侧字节被翻，拉取侧整轮失败且 cursor 不动
void test_tamper_rejected() {
    MemoryRelay relay;
    SyncKeyRingStd ring_a = make_ring();
    SyncKeyRingStd ring_b = make_ring();
    SyncSealedTransportStd sealed_a(relay, ring_a);
    SyncSealedTransportStd sealed_b(relay, ring_b);
    SyncEngineStd a("devA"), b("devB");

    assert(a.enqueue(SyncOpType::AddEntry, "hello") == "devA:1");
    std::string err;
    assert(a.sync(sealed_a, kGid, &err));

    relay.binlog_mutable(kGid)[0].payload[10] ^= 0x40;  // 密文中部翻一位
    assert(!b.sync(sealed_b, kGid, &err));
    assert(err.find("op decrypt failed") != std::string::npos);
    assert(b.cursor() == 0);          // 位点不推进
    assert(b.state().words.empty());  // 不落半条
}

// 3) 未持钥（配对未完成）：推/拉/快照读写全拒绝，引擎同步失败
void test_no_key_rejected() {
    MemoryRelay relay;
    SyncKeyRingStd ring_empty;
    SyncKeyRingStd ring_a = make_ring();
    SyncSealedTransportStd sealed_empty(relay, ring_empty);
    SyncSealedTransportStd sealed_a(relay, ring_a);
    SyncEngineStd a("devA");

    assert(a.enqueue(SyncOpType::AddEntry, "hello") == "devA:1");
    std::string err;
    assert(!a.sync(sealed_empty, kGid, &err));
    assert(err.find("group key not configured") != std::string::npos);
    assert(a.outbox_size() == 1);  // 指令留在 outbox，重试语义

    GroupMetaStd meta;
    assert(sealed_empty.meta(kGid, &meta, &err));  // 元信息直通不涉密
    const std::vector<EnqueuedOpStd> none;
    std::vector<std::string> acked;
    assert(!sealed_empty.push_ops(kGid, none, &acked, &err));
    uint64_t cur = 0;
    bool more = false;
    std::vector<RemoteOpStd> ops;
    assert(!sealed_empty.pull_ops(kGid, 0, 10, &ops, &cur, &more, &err));
    uint64_t up = 0;
    std::string snap;
    assert(!sealed_empty.put_snapshot(kGid, 1, "{}", &err));
    assert(!sealed_empty.get_snapshot(kGid, &up, &snap, &err));

    // 配对完成后同一引擎经持钥链可正常出箱
    assert(a.sync(sealed_a, kGid, &err));
    assert(a.outbox_size() == 0);
}

// 4) 快照密封往返：A 达阈值上传快照（密封），新引擎落后位点先跳快照
//    （开封回放）再增量，收敛一致；快照在中转侧同样不见明文
void test_snapshot_roundtrip() {
    MemoryRelay relay;
    SyncKeyRingStd ring_a = make_ring();
    SyncKeyRingStd ring_b = make_ring();
    SyncSealedTransportStd sealed_a(relay, ring_a);
    SyncSealedTransportStd sealed_b(relay, ring_b);
    SyncEngineStd a("devA"), b("devB");

    for (int i = 0; i < 3; ++i) {
        assert(!a.enqueue(SyncOpType::AddEntry, "w" + std::to_string(i))
                    .empty());
    }
    std::string err;
    assert(a.sync(sealed_a, kGid, &err));
    assert(a.maybe_snapshot(sealed_a, kGid, 3, &err));
    assert(relay.snapshot_payload_of(kGid).find("w0") ==
           std::string::npos);  // 快照同样只见密文

    assert(b.sync(sealed_b, kGid, &err));
    assert(b.state() == a.state());
    assert(b.state().words.size() == 3);
}

// 5) 密钥轮换多版本：A 以 v1 推一条后换钥到 v2（测试面确定性注入同一
//    v2 钥 = 换钥协议完成形态；真实路径经 B3-b PAKE 信封）再推一条。
//    B 缺 v2 时整轮失败不推进（不静默丢更）；补注 v2（环内 v1 保留 +
//    v2 当前，轮换口径）后全量可解，收敛一致
void test_key_rotation() {
    const std::string k2(32, '\x22');
    MemoryRelay relay;
    SyncKeyRingStd ring_a = make_ring();
    SyncKeyRingStd ring_b = make_ring();
    SyncSealedTransportStd sealed_a(relay, ring_a);
    SyncSealedTransportStd sealed_b(relay, ring_b);
    SyncEngineStd a("devA"), b("devB");

    assert(a.enqueue(SyncOpType::AddEntry, "alpha") == "devA:1");
    std::string err;
    assert(a.sync(sealed_a, kGid, &err));

    ring_a.import_key(2, k2);  // v2 成为当前钥（v1 保留解在途）
    assert(a.enqueue(SyncOpType::AddEntry, "beta") == "devA:2");
    assert(a.sync(sealed_a, kGid, &err));

    // B 只有 v1：v2 指令解不开 → 整轮失败、位点不动
    assert(!b.sync(sealed_b, kGid, &err));
    assert(err.find("op decrypt failed") != std::string::npos);
    assert(b.cursor() == 0);
    assert(b.state().words.empty());

    ring_b.import_key(2, k2);  // 换钥到达：环内 v1+v2 与轮换口径一致
    assert(b.sync(sealed_b, kGid, &err));
    assert(b.state() == a.state());
    assert(b.state().words.size() == 2);
}

// 5b) rotate 生成新随机钥：版本递增（1→2）、旧版本保留在环，轮换后
//     密封链照常推拉（当前钥密封、按版本头开封）
void test_rotate_new_version() {
    MemoryRelay relay;
    SyncKeyRingStd ring = make_ring();
    SyncSealedTransportStd sealed(relay, ring);
    SyncEngineStd a("devA");

    assert(a.enqueue(SyncOpType::AddEntry, "alpha") == "devA:1");
    std::string err;
    assert(a.sync(sealed, kGid, &err));
    assert(ring.rotate() == 2);
    assert(a.enqueue(SyncOpType::AddEntry, "beta") == "devA:2");
    assert(a.sync(sealed, kGid, &err));  // v1、v2 混排 binlog 自身可回放
    assert(a.state().words.size() == 2);
    assert(a.cursor() == 2);
}

// 6) 直通面：meta/拉取参数/空集不下发/内层失败原样穿透/异钥快照拒收
void test_passthrough() {
    MemoryRelay relay;
    SyncKeyRingStd ring = make_ring();
    SyncSealedTransportStd sealed(relay, ring);
    SyncEngineStd a("devA");

    assert(a.enqueue(SyncOpType::AddEntry, "hello") == "devA:1");
    std::string err;
    assert(a.sync(sealed, kGid, &err));

    GroupMetaStd meta;
    assert(sealed.meta(kGid, &meta, &err));
    assert(meta.latest_seq == 1);

    std::vector<RemoteOpStd> ops;
    uint64_t cur = 0;
    bool more = false;
    assert(sealed.pull_ops(kGid, 0, 7, &ops, &cur, &more, &err));
    assert(relay.last_pull_limit == 7);  // limit/since 原样穿透

    const int calls_before = relay.push_calls;
    std::vector<EnqueuedOpStd> none;
    std::vector<std::string> acked;
    assert(sealed.push_ops(kGid, none, &acked, &err));  // 空集不下发
    assert(relay.push_calls == calls_before);

    // 内层失败原样穿透（故障注入一次）
    relay.fail_pull = 1;
    assert(!sealed.pull_ops(kGid, 0, 7, &ops, &cur, &more, &err));
    assert(relay.fail_pull == 0);

    // 异钥快照拒收：ring_a 落的快照，ring_c（异钥）开封必败
    assert(a.maybe_snapshot(sealed, kGid, 1, &err));
    SyncKeyRingStd ring_c;
    ring_c.import_key(1, std::string(32, '\x33'));
    SyncSealedTransportStd sealed_c(relay, ring_c);
    uint64_t up = 0;
    std::string snap;
    assert(!sealed_c.get_snapshot(kGid, &up, &snap, &err));
    assert(err.find("snapshot decrypt failed") != std::string::npos);
    assert(snap.empty());
}

}  // namespace

int main() {
    test_ciphertext_only_and_converge();
    test_tamper_rejected();
    test_no_key_rejected();
    test_snapshot_roundtrip();
    test_key_rotation();
    test_rotate_new_version();
    test_passthrough();
    std::cout << "sync_sealed_transport_std_test: all passed\n";
    return 0;
}
