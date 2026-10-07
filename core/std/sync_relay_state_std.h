// 同步中转协议 v1 契约语义层（server_plan §7 B5 / PROTOCOL.md）。
//
// 只含组状态与全部契约规则（定序/幂等/位点/快照/限额/持久化），不含
// 任何传输与 JSON——HTTP 壳在 server/sync_relay/cpp/，把请求解成这里
// 的结构再进来。与 dev 参考实现（server/sync_relay/dev/）同一契约：
// 契约符合性测试 dev/test_relay_protocol.py 是两种实现共同的验收口径。
//
// 红线：中转只见密文——payload 对本层是不透明 base64 串，不解析不索引。

#ifndef UNIDICT_SYNC_RELAY_STATE_STD_H
#define UNIDICT_SYNC_RELAY_STATE_STD_H

#include <cstdint>
#include <mutex>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace UnidictCoreStd {

// PROTOCOL.md §2 错误形态：HTTP 状态码 + 机器可读码
class SyncRelayErrorStd : public std::runtime_error {
public:
    SyncRelayErrorStd(int status, const std::string& code)
        : std::runtime_error(code), status_(status), code_(code) {}
    int status() const { return status_; }
    const std::string& code() const { return code_; }

private:
    int status_;
    std::string code_;
};

// 追加请求侧的指令（客户端提供的三个不透明字段）
struct SyncRelayOpIn {
    std::string op_id;
    std::string device_id;
    std::string payload;  // base64 密文，原样存储
};

// binlog 侧的指令（含服务端定序的 seq 与到达时间）
struct SyncRelayOp {
    std::int64_t seq = 0;
    std::int64_t ts = 0;
    std::string op_id;
    std::string device_id;
    std::string payload;
};

struct SyncRelayAssign {
    std::string op_id;
    std::int64_t seq = 0;
};

struct SyncRelayAppendResult {
    std::vector<SyncRelayAssign> assigned;
    std::vector<std::string> duplicate_op_ids;
};

struct SyncRelayMeta {
    std::string gid;
    std::int64_t latest_seq = 0;
    std::int64_t op_count = 0;
    std::int64_t snapshot_up_to_seq = 0;  // 无快照为 0
    std::int64_t created_at = 0;
};

struct SyncRelayPull {
    std::string gid;
    std::vector<SyncRelayOp> ops;
    std::int64_t cursor = 0;  // 组内当前最大 seq（不是本次返回的最大值）
    bool has_more = false;
};

struct SyncRelaySnapshot {
    std::int64_t up_to_seq = 0;
    std::string payload;
};

// PROTOCOL.md §2.1 探活应答字段（无状态，供传输壳直接引用）
constexpr const char* kRelayServiceName = "unidict-sync-relay";
constexpr int kRelayProtocolVersion = 1;

// 组状态机 + 全部契约语义。一把互斥锁串行化全部读写——组内定序全序
// 无重号由此保证（PROTOCOL.md §4「进程内串行」口径）。
class SyncRelayStateStd {
public:
    // data_dir 非空时状态原子落盘到 <data_dir>/relay_state.json（构造
    // 时加载，坏文件按空起），空串 = 纯内存形态（自测/临时用途）
    explicit SyncRelayStateStd(const std::string& data_dir = {});

    SyncRelayMeta create_group(const std::string& gid);   // 幂等：已存在原样成功
    SyncRelayMeta group_meta(const std::string& gid);

    // 组不存在自动建组（PROTOCOL §2.4，离线重试零往返）；整批原子：
    // 任一 op 非法则整批不落（先全量校验再应用）
    SyncRelayAppendResult append_ops(const std::string& gid,
                                     const std::vector<SyncRelayOpIn>& ops);

    // seq 严格大于 since、按 seq 升序、至多 limit 条；负位点按 0 容错，
    // limit 钳制 [1,1000]
    SyncRelayPull pull_ops(const std::string& gid, std::int64_t since,
                           std::int64_t limit);

    // 快照覆盖 ≤ up_to_seq 的全部指令状态，最后写入者胜；返回 up_to_seq
    std::int64_t put_snapshot(const std::string& gid, std::int64_t up_to_seq,
                              const std::string& payload);
    SyncRelaySnapshot get_snapshot(const std::string& gid);

private:
    struct Group {
        std::int64_t created_at = 0;
        std::int64_t latest_seq = 0;
        std::vector<SyncRelayOp> ops;  // ops[i].seq == i+1（v1 无裁剪）
        bool has_snapshot = false;
        std::int64_t snapshot_up_to_seq = 0;
        std::int64_t snapshot_ts = 0;
        std::string snapshot_payload;
    };

    // 契约校验（PROTOCOL §3 限额；抛 SyncRelayErrorStd）
    static void must_gid(const std::string& gid);
    static void must_op(const SyncRelayOpIn& op);
    static void must_payload(const std::string& payload);

    Group& must_group(const std::string& gid);  // 404 group_not_found
    void persist_locked();  // 落盘（锁内调用）；失败抛 500 persist_failed

    std::mutex mutex_;
    std::map<std::string, Group> groups_;  // 有序：落盘内容逐字节确定
    std::string data_dir_;                 // 空 = 纯内存
    std::string state_path_;
};

}  // namespace UnidictCoreStd

#endif  // UNIDICT_SYNC_RELAY_STATE_STD_H
