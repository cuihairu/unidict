// B2 客户端同步引擎纯 std 测试：指令生成与超限、推送/离线/重试去重、
// 双引擎收敛与服务端序确定性、分块拉取与重启续传、快照空洞跳变、
// 压缩计数、回显幂等、持久化往返、确定性序列化、gid/失败路径、limit
// 钳制、转义往返、畸形 payload 与截断状态文件、无 ack 防死循环。
// 传输面用 tests/sync_relay_memory_std.h（内存 relay + 故障注入）。
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "sync_relay_memory_std.h"

using namespace UnidictCoreStd;

namespace {

namespace fs = std::filesystem;

const char* kGid = "abcdefgh12345678";  // 16 字符，过 §3 校验的最短形

std::string scratch_dir() {
    return (fs::temp_directory_path() / "unidict_sync_engine_test").string();
}

// save_state 落盘再抽 "state": 区段——与引擎内部 serialize_state 同源，
// 用于跨引擎逐字节比对（serialize_state 是实现细节，不经头文件暴露）
std::string state_section_of(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    assert(in.good());
    const std::string body((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const std::string key = "\"state\":";
    const size_t pos = body.find(key);
    assert(pos != std::string::npos);
    // state 是文档最后一个成员：掐掉文档收尾的 '}' 即区段原文
    return body.substr(pos + key.size(), body.size() - pos - key.size() - 1);
}

// 全批不 ack 的 stub：协议下不该发生（正常响应必带 ack），引擎必须
// 收口不挂死；同时可喂任意畸形/未知指令测回放容忍
class ScriptedTransport : public SyncTransportStd {
public:
    std::vector<RemoteOpStd> scripted;  // pull_ops 返回的内容
    bool ack_empty = false;             // true：push 成功但 acked 为空
    GroupMetaStd meta_data;

    bool meta(const std::string&, GroupMetaStd* out, std::string*) override {
        *out = meta_data;
        return true;
    }
    bool push_ops(const std::string& gid,
                  const std::vector<EnqueuedOpStd>& ops,
                  std::vector<std::string>* acked, std::string*) override {
        (void)gid;
        if (!ack_empty) {
            for (const auto& op : ops) acked->push_back(op.op_id);
        }
        return true;
    }
    bool pull_ops(const std::string&, uint64_t since, size_t limit,
                  std::vector<RemoteOpStd>* out, uint64_t* cursor,
                  bool* has_more, std::string*) override {
        (void)limit;
        *out = scripted;
        *cursor = out->empty() ? since : out->back().seq;
        *has_more = false;
        return true;
    }
    bool put_snapshot(const std::string&, uint64_t, const std::string&,
                      std::string* err) override {
        if (err) *err = "no snapshot put";
        return false;
    }
    bool get_snapshot(const std::string&, uint64_t*, std::string*,
                      std::string* err) override {
        if (err) *err = "no snapshot";
        return false;
    }
};

// T1 指令生成：op_id 规则、本地立即生效、超限拒绝、随机设备号形状
void test_enqueue_and_limits() {
    SyncEngineStd a("devA");
    const std::string id1 = a.enqueue(SyncOpType::AddEntry, "apple");
    assert(id1 == "devA:1");
    assert(a.local_seq() == 1);
    assert(a.outbox_size() == 1);
    assert(a.state().words.size() == 1 && a.state().words[0] == "apple");

    const std::string id2 =
        a.enqueue(SyncOpType::UpdateNote, "apple", "fruit");
    assert(id2 == "devA:2");
    assert(a.state().notes.at("apple") == "fruit");

    ScriptedTransport t;
    t.meta_data.latest_seq = 2;
    std::string err;
    assert(a.sync(t, kGid, &err));  // 全推出去
    assert(a.outbox_size() == 0);
    assert(a.state().history.empty() && a.state().prefs.empty());

    const std::string big(256 * 1024 + 1, 'x');
    assert(a.enqueue(SyncOpType::UpdateNote, "apple", big) == "");
    assert(a.last_error() == "payload too large");
    assert(a.local_seq() == 2);  // 拒绝的指令不占本地序
    assert(a.outbox_size() == 0);

    // 随机设备号：128 位十六进制
    SyncEngineStd anon;
    assert(anon.device_id().size() == 32);
    for (char c : anon.device_id()) {
        assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }

    // clear() 复位到默认态
    SyncVocabStateStd st = a.state();
    assert(!st.words.empty());
    st.clear();
    assert(st.words.empty() && st.notes.empty() && st.tags.empty() &&
           st.history.empty() && st.prefs.empty());
    assert(st == SyncVocabStateStd());
}

// T2 推送失败 → outbox 原样保留 → 重试成功且服务端不重复（幂等键）
void test_push_offline_retry_dedup() {
    SyncEngineStd a("devA");
    MemoryRelay relay;
    a.enqueue(SyncOpType::AddEntry, "alpha");
    a.enqueue(SyncOpType::AddEntry, "beta");
    a.enqueue(SyncOpType::RecordHistory, "alpha", "", 42);

    relay.fail_push = 1;
    std::string err;
    assert(!a.sync(relay, kGid, &err));
    assert(err == "push failed (injected)");
    assert(a.outbox_size() == 3);  // 未确认指令全保留
    assert(a.cursor() == 0);

    assert(a.sync(relay, kGid, &err));  // 重试即续传
    assert(a.outbox_size() == 0);
    GroupMetaStd meta;
    assert(relay.meta(kGid, &meta, &err));
    assert(meta.latest_seq == 3);  // 幂等键保证不重复入 binlog

    // ack 丢失场景：同 op_id 再推一次 → duplicate ack，binlog 不涨
    SyncEngineStd replay("devA");
    replay.enqueue(SyncOpType::AddEntry, "alpha");
    assert(replay.sync(relay, kGid, &err));
    assert(relay.meta(kGid, &meta, &err));
    assert(meta.latest_seq == 3);
}

// T3 双引擎收敛：各自离线产生指令，互相同步后状态一致（服务端序）
void test_two_engine_convergence() {
    SyncEngineStd a("devA"), b("devB");
    MemoryRelay relay;
    a.enqueue(SyncOpType::AddEntry, "alpha");
    b.enqueue(SyncOpType::AddEntry, "beta");
    a.enqueue(SyncOpType::UpdateNote, "alpha", "from A");
    b.enqueue(SyncOpType::UpdateNote, "alpha", "from B");  // 并发同字段
    b.enqueue(SyncOpType::AddTag, "beta", "hard");
    a.enqueue(SyncOpType::SetPref, "theme", "dark");

    std::string err;
    // 第一轮：a 先推（b 尚未入组），b 推完并全量回放 6 条；a 还没见
    // 到 b 的指令
    assert(a.sync(relay, kGid, &err));
    assert(b.sync(relay, kGid, &err));
    assert(b.state().words.size() == 2);
    // 服务端序后写胜：from B 的 note seq 更高，覆盖 from A
    assert(b.state().notes.at("alpha") == "from B");
    // 第二轮：a 拉到 b 的增量 → 两边收敛
    assert(a.sync(relay, kGid, &err));
    assert(a.state() == b.state());
    assert(a.state().notes.at("alpha") == "from B");
    assert(b.state().tags.at("beta") == std::vector<std::string>{"hard"});
    assert(b.state().prefs.at("theme") == "dark");

    // 再来一轮：B 的更晚指令（更高 seq）反超，两边仍收敛
    b.enqueue(SyncOpType::UpdateNote, "alpha", "from B again");
    assert(b.sync(relay, kGid, &err));  // b 先推上 seq7
    assert(a.sync(relay, kGid, &err));  // a 再拉增量
    assert(a.state() == b.state());
    assert(a.state().notes.at("alpha") == "from B again");

    // 确定性序列化：同状态 → 状态区段逐字节相同
    const std::string p1 = scratch_dir() + "/conv_a.json";
    const std::string p2 = scratch_dir() + "/conv_b.json";
    assert(a.save_state(p1, &err));
    assert(b.save_state(p2, &err));
    assert(state_section_of(p1) == state_section_of(p2));
}

// T4 分块拉取（limit 分页）+ 引擎持久化后的断点续传
void test_paged_pull_and_resume() {
    SyncEngineStd a("devA");
    MemoryRelay relay;
    for (const char* w : {"w1", "w2", "w3", "w4", "w5", "w6", "w7"}) {
        a.enqueue(SyncOpType::AddEntry, w);
    }
    std::string err;
    assert(a.sync(relay, kGid, &err));

    SyncEngineStd b("devB");
    b.set_pull_limit(2);
    const int pulls_before_b = relay.pull_calls;
    assert(b.sync(relay, kGid, &err));
    assert(relay.pull_calls - pulls_before_b == 4);  // 7 条按 limit=2 分 4 页
    assert(relay.last_pull_limit == 2);
    assert(b.state() == a.state());
    assert(b.cursor() == 7);

    // 重启续传：落盘 → 新引擎加载 → 只拉新增量
    const std::string path = scratch_dir() + "/resume_state.json";
    assert(b.save_state(path, &err));
    a.enqueue(SyncOpType::AddEntry, "w8");
    a.enqueue(SyncOpType::RecordHistory, "w8", "", 7);
    assert(a.sync(relay, kGid, &err));

    SyncEngineStd b2("devB");
    assert(b2.load_state(path, &err));
    assert(b2.device_id() == "devB");
    assert(b2.cursor() == 7);
    assert(b2.state() == b.state());
    const int pulls_before = relay.pull_calls;
    assert(b2.sync(relay, kGid, &err));
    assert(b2.state() == a.state());
    assert(b2.cursor() == 9);
    assert(relay.pull_calls == pulls_before + 1);  // 增量一页拉完
}

// T5 快照空洞跳变：新设备位点落后快照覆盖位时先取快照再增量（§2.8）
void test_snapshot_jump() {
    SyncEngineStd a("devA");
    MemoryRelay relay;
    for (const char* w : {"s1", "s2", "s3", "s4", "s5"}) {
        a.enqueue(SyncOpType::AddEntry, w);
    }
    std::string err;
    assert(a.sync(relay, kGid, &err));
    assert(a.maybe_snapshot(relay, kGid, 1, &err));
    GroupMetaStd meta;
    assert(relay.meta(kGid, &meta, &err));
    assert(meta.snapshot_up_to_seq == 5);

    // a 又推 2 条，然后新设备 C（cursor=0）进场
    a.enqueue(SyncOpType::AddEntry, "s6");
    a.enqueue(SyncOpType::AddEntry, "s7");
    assert(a.sync(relay, kGid, &err));

    SyncEngineStd c("devC");
    assert(c.sync(relay, kGid, &err));
    assert(relay.last_pull_since == 5);  // 快照跳变后从覆盖位增量
    assert(c.state() == a.state());
    assert(c.cursor() == 7);

    // 快照取不到（故障注入）：失败保位点，重试走完整跳变路径
    SyncEngineStd d("devD");
    relay.fail_snapshot_get = 1;
    assert(!d.sync(relay, kGid, &err));
    assert(err == "snapshot get failed (injected)");
    assert(d.cursor() == 0);
    assert(d.sync(relay, kGid, &err));
    assert(d.state() == a.state());
    assert(d.cursor() == 7);
}

// T6 压缩计数：未达阈值不动，达标上传并清零；上传失败计数保留
void test_snapshot_threshold() {
    SyncEngineStd a("devA");
    MemoryRelay relay;
    a.enqueue(SyncOpType::AddEntry, "t1");
    a.enqueue(SyncOpType::AddEntry, "t2");
    std::string err;
    assert(a.sync(relay, kGid, &err));
    assert(a.applied_since_snapshot() == 2);

    assert(a.maybe_snapshot(relay, kGid, 3, &err));  // 未达阈值
    GroupMetaStd meta;
    assert(relay.meta(kGid, &meta, &err));
    assert(meta.snapshot_up_to_seq == 0);
    assert(a.applied_since_snapshot() == 2);

    a.enqueue(SyncOpType::AddEntry, "t3");
    assert(a.sync(relay, kGid, &err));
    assert(a.applied_since_snapshot() == 3);
    assert(a.maybe_snapshot(relay, kGid, 3, &err));  // 达标
    assert(relay.meta(kGid, &meta, &err));
    assert(meta.snapshot_up_to_seq == 3);
    assert(a.applied_since_snapshot() == 0);  // 清零

    // 上传失败：计数保留，重试成功
    a.enqueue(SyncOpType::AddEntry, "t4");
    assert(a.sync(relay, kGid, &err));
    relay.fail_snapshot_put = 1;
    assert(!a.maybe_snapshot(relay, kGid, 1, &err));
    assert(err == "snapshot put failed (injected)");
    assert(a.applied_since_snapshot() == 1);
    assert(a.maybe_snapshot(relay, kGid, 1, &err));
    assert(a.applied_since_snapshot() == 0);
}

// T7 回显幂等 + 各型指令对缺失对象的幂等语义
void test_echo_idempotent() {
    SyncEngineStd a("devA");
    MemoryRelay relay;
    std::string err;
    a.enqueue(SyncOpType::AddEntry, "hello");
    assert(a.sync(relay, kGid, &err));
    assert(a.state().words.size() == 1);  // 回显不加重
    assert(a.sync(relay, kGid, &err));
    assert(a.state().words.size() == 1);

    a.enqueue(SyncOpType::RecordHistory, "hello", "", 42);
    assert(a.sync(relay, kGid, &err));
    assert(a.state().history.size() == 1);
    a.enqueue(SyncOpType::RecordHistory, "hello", "", 42);  // 同 (w,ts)
    assert(a.state().history.size() == 1);                  // 本地即去重
    assert(a.sync(relay, kGid, &err));
    assert(a.state().history.size() == 1);  // 回显仍去重

    // 标签增删（现存词）：加→在，删→空表收口删键
    a.enqueue(SyncOpType::AddTag, "hello", "g1");
    assert(a.state().tags.at("hello") == std::vector<std::string>{"g1"});
    a.enqueue(SyncOpType::RemoveTag, "hello", "g1");
    assert(a.state().tags.count("hello") == 0);  // 空标签表收口

    // 对缺失对象的操作：全部幂等无副作用
    const SyncVocabStateStd before = a.state();
    a.enqueue(SyncOpType::RemoveEntry, "ghost");
    a.enqueue(SyncOpType::UpdateNote, "ghost", "x");
    a.enqueue(SyncOpType::AddTag, "ghost", "g");
    a.enqueue(SyncOpType::RemoveTag, "ghost", "g");
    assert(a.sync(relay, kGid, &err));
    assert(a.state() == before);

    // 删现存词：词条移除，历史保留
    a.enqueue(SyncOpType::RemoveEntry, "hello");
    assert(a.sync(relay, kGid, &err));
    assert(a.state().words.empty());
    assert(a.state().history.size() == 1);
}

// T8 持久化往返：字段全量落盘/加载；推一半断电再续传；坏文件口径
void test_state_roundtrip() {
    const std::string dir = scratch_dir();
    fs::create_directories(dir);
    const std::string path = dir + "/engine_state.json";

    SyncEngineStd a("devX");
    a.enqueue(SyncOpType::AddEntry, "persistence");
    a.enqueue(SyncOpType::AddEntry, "roundtrip");
    a.enqueue(SyncOpType::UpdateNote, "roundtrip", "line1\nline2 \"q\"");
    MemoryRelay relay;
    relay.fail_push = 1;
    std::string err;
    assert(!a.sync(relay, kGid, &err));  // 断电点：outbox 未清
    assert(a.outbox_size() == 3);
    assert(a.save_state(path, &err));

    SyncEngineStd b("placeholder");
    assert(b.load_state(path, &err));
    assert(b.device_id() == "devX");
    assert(b.local_seq() == 3);
    assert(b.cursor() == 0);
    assert(b.outbox_size() == 3);
    assert(b.applied_since_snapshot() == 0);
    assert(b.state() == a.state());

    // 加载后续传：outbox 里的指令推上服务端
    assert(b.sync(relay, kGid, &err));
    assert(b.outbox_size() == 0);
    GroupMetaStd meta;
    assert(relay.meta(kGid, &meta, &err));
    assert(meta.latest_seq == 3);
    assert(b.state() == a.state());  // 回放回显不改状态

    // 文件缺失 / 版本不符
    std::string err2;
    SyncEngineStd c;
    assert(!c.load_state(dir + "/missing.json", &err2));
    assert(err2 == "state file does not exist");
    const std::string bad1 = dir + "/bad_version.json";
    {
        std::ofstream o(bad1);
        o << "{\"version\":2}";
    }
    assert(!c.load_state(bad1, &err2));
    assert(err2 == "unsupported state version");

    // 截断文件：可读前缀照常加载（version/device_id 在就生效），
    // 未闭合区段按缺失处理——不崩、不误加载半截状态
    const std::string bad2 = dir + "/truncated_head.json";
    {
        std::ofstream o(bad2);
        o << "{\"ver";
    }
    SyncEngineStd d1;
    assert(!d1.load_state(bad2, &err2));  // version 读不到 → 拒载
    const std::string bad3 = dir + "/truncated_mid.json";
    {
        std::ofstream o(bad3);
        o << "{\"version\":1,\"device_id\":\"devT\",\"local_seq\":5,"
             "\"cursor\":2,\"applied_since_snapshot\":1,\"outbox\":[";
    }
    SyncEngineStd d2;
    assert(d2.load_state(bad3, &err2));
    assert(d2.device_id() == "devT");
    assert(d2.local_seq() == 5 && d2.cursor() == 2);
    assert(d2.outbox_size() == 0);  // 未闭合区段整体丢弃

    // 截断到 state 区段中段：state 整体按缺失处理，可读前缀生效
    const std::string bad4 = dir + "/truncated_state.json";
    {
        std::ofstream o(bad4);
        o << "{\"version\":1,\"device_id\":\"devS\",\"state\":{\"words\":[\"a\"";
    }
    SyncEngineStd d3;
    assert(d3.load_state(bad4, &err2));
    assert(d3.device_id() == "devS");
    assert(d3.state().words.empty());

    // 写失败：路径是目录；嵌套父目录不存在则先建
    std::string err3;
    assert(!a.save_state(dir, &err3));
    assert(err3 == "cannot write state file");
    fs::remove_all(dir + "/sub");  // scratch 目录跨轮留存：先清再建
    assert(a.save_state(dir + "/sub/deep/state.json", &err3));
    assert(fs::exists(dir + "/sub/deep/state.json"));
}

// T9 确定性 + 转义往返：同指令流 → 同状态串；特殊字符无损
void test_deterministic_serialization() {
    SyncEngineStd a("devA"), b("devB");
    MemoryRelay relay;
    // 引号/反斜杠/全部五类控制字符转义
    const std::string tricky = "q\"\\nl\t\n\r\b\f/r x";
    a.enqueue(SyncOpType::AddEntry, tricky);
    a.enqueue(SyncOpType::UpdateNote, tricky, tricky);
    a.enqueue(SyncOpType::AddTag, tricky, "t\"g");
    a.enqueue(SyncOpType::SetPref, "k\"y", tricky);
    a.enqueue(SyncOpType::RecordHistory, tricky, "", 99);  // history 入状态
    std::string err;
    assert(a.sync(relay, kGid, &err));
    assert(b.sync(relay, kGid, &err));
    assert(b.state() == a.state());
    assert(b.state().notes.at(tricky) == tricky);
    assert(b.state().prefs.at("k\"y") == tricky);
    assert(b.state().tags.at(tricky) == std::vector<std::string>{"t\"g"});
    assert(b.state().history.size() == 1 &&
           b.state().history[0].second == 99);

    // 状态串跨引擎逐字节一致
    const std::string p1 = scratch_dir() + "/tricky_a.json";
    const std::string p2 = scratch_dir() + "/tricky_b.json";
    assert(a.save_state(p1, &err));
    assert(b.save_state(p2, &err));
    assert(state_section_of(p1) == state_section_of(p2));

    // save→load 咬合 serialize/parse：无损往返
    SyncEngineStd c("devC");
    assert(c.load_state(p1, &err));
    assert(c.state() == a.state());
}

// T10 gid 校验、meta 失败、err 空指针、空组
void test_gid_and_meta_failures() {
    assert(SyncEngineStd::valid_gid("abcdefgh12345678"));
    assert(SyncEngineStd::valid_gid(std::string(64, 'a')));
    assert(!SyncEngineStd::valid_gid("short"));
    assert(!SyncEngineStd::valid_gid(std::string(15, 'a')));
    assert(!SyncEngineStd::valid_gid(std::string(65, 'a')));
    assert(!SyncEngineStd::valid_gid("abcdefgh1234567+"));  // 非法字符

    SyncEngineStd a("devA");
    MemoryRelay relay;
    std::string err;
    assert(!a.sync(relay, "bad-gid", &err));
    assert(err == "invalid group id");

    relay.fail_meta = 1;
    assert(!a.sync(relay, kGid, &err));
    assert(err == "meta failed (injected)");

    // err 传 nullptr 也要安全走完失败分支
    relay.fail_meta = 1;
    assert(!a.sync(relay, kGid, nullptr));

    // 空组：meta 成功、无快照、拉不到东西 → 成功空转
    SyncEngineStd b("devB");
    MemoryRelay empty_relay;
    assert(b.sync(empty_relay, kGid, &err));
    assert(b.cursor() == 0 && b.state().words.empty());
}

// T11 limit 钳制 + 拉取中途失败重试
void test_pull_limit_clamp_and_pull_failure() {
    SyncEngineStd a("devA");
    MemoryRelay relay;
    a.enqueue(SyncOpType::AddEntry, "one");
    std::string err;
    assert(a.sync(relay, kGid, &err));

    SyncEngineStd b("devB");
    b.set_pull_limit(5000);  // 协议上限 1000
    assert(b.sync(relay, kGid, &err));
    assert(relay.last_pull_limit == 1000);

    // 拉取中途失败：cursor 不动，重试完整拉回
    SyncEngineStd c("devC");
    c.set_pull_limit(1);
    a.enqueue(SyncOpType::AddEntry, "two");
    a.enqueue(SyncOpType::AddEntry, "three");
    assert(a.sync(relay, kGid, &err));
    relay.fail_pull = 1;
    assert(!c.sync(relay, kGid, &err));
    assert(err == "pull failed (injected)");
    assert(c.cursor() == 0);
    assert(c.sync(relay, kGid, &err));
    assert(c.state() == a.state());
}

// 回放容忍：畸形 payload 忽略、未知指令类型忽略、cursor 照常推进；
// 全批无 ack 的 stub 不挂死且 outbox 保留
void test_malformed_and_no_ack() {
    SyncEngineStd a("devA");
    ScriptedTransport t;
    RemoteOpStd bad1;
    bad1.seq = 1;
    bad1.op_id = "devZ:1";
    bad1.payload = "nonsense";
    RemoteOpStd bad2;
    bad2.seq = 2;
    bad2.op_id = "devZ:2";
    bad2.payload = "{\"t\"";  // 截断对象
    RemoteOpStd unknown;
    unknown.seq = 3;
    unknown.op_id = "devZ:3";
    unknown.payload = "{\"t\":\"future\",\"w\":\"x\"}";  // 未知类型忽略
    RemoteOpStd good;
    good.seq = 4;
    good.op_id = "devZ:4";
    good.payload = "{\"t\":\"add\",\"w\":\"cherry\"}";
    t.scripted = {bad1, bad2, unknown, good};
    t.meta_data.latest_seq = 4;

    std::string err;
    assert(a.sync(t, kGid, &err));
    assert(a.state().words.size() == 1 && a.state().words[0] == "cherry");
    assert(a.cursor() == 4);  // 坏指令也推进位点（不重拉）

    // 全批无 ack：协议外响应，引擎必须收口不挂死且 outbox 保留
    ScriptedTransport silent;
    silent.ack_empty = true;
    a.enqueue(SyncOpType::AddEntry, "durian");
    assert(a.sync(silent, kGid, &err));
    assert(a.outbox_size() == 1);
    assert(a.state().words.size() == 2);  // 本地仍生效
}

// 状态文件手工畸形：缺区段/坏元素逐路覆盖
void test_hand_crafted_state_files() {
    const std::string dir = scratch_dir();
    fs::create_directories(dir);
    std::string err;

    struct Case {
        const char* name;
        const char* body;
    };
    const std::vector<Case> cases = {
        // 缺 notes/tags/history/prefs 区段：只有 words 生效
        {"missing_sections.json",
         "{\"version\":1,\"device_id\":\"devM\",\"local_seq\":0,"
         "\"cursor\":0,\"applied_since_snapshot\":0,\"outbox\":[],"
         "\"state\":{\"words\":[\"a\"]}}"},
        // 键无冒号：find_section 防御路
        {"no_colon.json", "{\"version\":1,\"state\":{\"words\"}}"},
        // 值非容器：find_section 防御路
        {"scalar_section.json", "{\"version\":1,\"state\":{\"words\":5}}"},
        // history 畸形元素：非串头 / 空数组 / 无 ts
        {"bad_history.json",
         "{\"version\":1,\"state\":{\"history\":[[5],[],[\"w\"]]}}"},
        // notes 成员键后无冒号（区段闭合但成员残缺）
        {"member_no_colon.json",
         "{\"version\":1,\"state\":{\"notes\":{\"a\" \"b\"}}}"},
        // notes 值是对象（形态不符：对象 token 不入表）
        {"object_note.json",
         "{\"version\":1,\"state\":{\"notes\":{\"a\":{\"b\":1}}}}"},
        // outbox 元素缺 op_id：整条丢弃
        {"bad_outbox.json",
         "{\"version\":1,\"device_id\":\"devO\",\"local_seq\":1,"
         "\"cursor\":0,\"applied_since_snapshot\":0,"
         "\"outbox\":[{\"payload\":\"{\\\"t\\\":\\\"add\\\",\\\"w\\\":"
         "\\\"x\\\"}\"}],\"state\":{}}"},
        // \\b \\f \\r \\t 转义解码（json_escape 不产出它们，只在
        // 手工文件/外部快照里出现）
        {"escaped_controls.json",
         "{\"version\":1,\"state\":{\"notes\":{\"w\":\"a\\bb\\fc\\rd"
         "\\te\\\"f\\\\g\"}}}"},
    };
    for (const Case& c : cases) {
        const std::string path = dir + "/" + c.name;
        {
            std::ofstream o(path);
            o << c.body;
        }
        SyncEngineStd loader;
        // 全部按"畸形部分空处理"成功加载，不崩、不报错
        assert(loader.load_state(path, &err));
        assert(err.empty());
    }
    // 定向断言
    SyncEngineStd m;
    assert(m.load_state(dir + "/missing_sections.json", &err));
    assert(m.state().words == std::vector<std::string>{"a"});
    assert(m.state().notes.empty() && m.state().history.empty());
    SyncEngineStd h;
    assert(h.load_state(dir + "/bad_history.json", &err));
    assert(h.state().history.size() == 1);  // ["w"] 无 ts → ts=0
    assert(h.state().history[0].first == "w" &&
           h.state().history[0].second == 0);
    SyncEngineStd o;
    assert(o.load_state(dir + "/bad_outbox.json", &err));
    assert(o.device_id() == "devO");
    assert(o.outbox_size() == 0);  // 缺 op_id 的指令不进 outbox
    SyncEngineStd esc;
    assert(esc.load_state(dir + "/escaped_controls.json", &err));
    assert(esc.state().notes.at("w") == "a\bb\fc\rd\te\"f\\g");
}

}  // namespace

int main() {
    test_enqueue_and_limits();
    test_push_offline_retry_dedup();
    test_two_engine_convergence();
    test_paged_pull_and_resume();
    test_snapshot_jump();
    test_snapshot_threshold();
    test_echo_idempotent();
    test_state_roundtrip();
    test_deterministic_serialization();
    test_gid_and_meta_failures();
    test_pull_limit_clamp_and_pull_failure();
    test_malformed_and_no_ack();
    test_hand_crafted_state_files();
    std::cout << "sync_engine_std_test: all assertions passed\n";
    return 0;
}
