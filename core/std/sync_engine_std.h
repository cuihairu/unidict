// B2 客户端同步引擎（server_plan §7）：指令生成（设备号+本地序 op_id）、
// 增量推拉、离线位点续传、确定性回放、快照与本地压缩。传输面是窄接口
// SyncTransportStd（HTTP/WebDAV/LAN 绑定归 B5）；指令/回放语义对齐
// server/sync_relay/PROTOCOL.md v1 与 server_plan §6 指令流模型。
// 红线：引擎是惰性库——不配置 gid、不调 sync() 就没有任何数据外发；
// 默认关闭的开关面在 B7 UI。

#ifndef UNIDICT_SYNC_ENGINE_STD_H
#define UNIDICT_SYNC_ENGINE_STD_H

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace UnidictCoreStd {

// 待推指令（payload = 规范指令 JSON；转 base64 是传输层职责）
struct EnqueuedOpStd {
    std::string op_id;    // "<device_id>:<local_seq>"（PROTOCOL §1 幂等键）
    std::string payload;
};

// 拉取侧远端指令（seq 升序）
struct RemoteOpStd {
    uint64_t seq = 0;
    std::string op_id;
    std::string device_id;
    long long ts = 0;
    std::string payload;
};

// 组元信息（PROTOCOL §2.3）
struct GroupMetaStd {
    uint64_t latest_seq = 0;
    uint64_t snapshot_up_to_seq = 0;
};

// 传输面：协议端点的最小映射。实现方负责 JSON/base64/HTTP（B5）；
// 语义必须满足 PROTOCOL §2/§4：组内 seq 单调无空洞、op_id 幂等、
// 拉取按 seq 升序。契约验收口径 = B1 契约符合性测试。
class SyncTransportStd {
public:
    virtual ~SyncTransportStd();

    virtual bool meta(const std::string& gid, GroupMetaStd* out,
                      std::string* err) = 0;
    // acked = assigned ∪ duplicate_op_ids——两者对引擎同义：服务端已有
    virtual bool push_ops(const std::string& gid,
                          const std::vector<EnqueuedOpStd>& ops,
                          std::vector<std::string>* acked, std::string* err) = 0;
    virtual bool pull_ops(const std::string& gid, uint64_t since, size_t limit,
                          std::vector<RemoteOpStd>* out, uint64_t* cursor,
                          bool* has_more, std::string* err) = 0;
    virtual bool put_snapshot(const std::string& gid, uint64_t up_to_seq,
                              const std::string& payload, std::string* err) = 0;
    virtual bool get_snapshot(const std::string& gid, uint64_t* up_to_seq,
                              std::string* payload, std::string* err) = 0;
};

// 指令类型（§6 同步面的规范集：生词本/笔记/标签/历史/偏好）
enum class SyncOpType {
    AddEntry,
    RemoveEntry,
    UpdateNote,
    AddTag,
    RemoveTag,
    RecordHistory,
    SetPref,
};

// 物化状态：回放终点，也是快照内容。全规范序——words/tags 字典序、
// history 按 (ts, word)——指令语义可交换且幂等，指令集相同（回放次序
// 无论怎样交错）即收敛到同一状态，序列化逐字节确定。
struct SyncVocabStateStd {
    std::vector<std::string> words;                          // 生词本，字典序
    std::map<std::string, std::string> notes;                // word → note
    std::map<std::string, std::vector<std::string>> tags;    // word → tags 字典序
    std::vector<std::pair<std::string, long long>> history;  // 按 (ts, word) 序
    std::map<std::string, std::string> prefs;                // key → value

    void clear();
    bool operator==(const SyncVocabStateStd& other) const;
};

// 同步引擎：一台设备的指令生成 + 推拉 + 回放 + 持久化。
// 本地指令 enqueue 即生效并进 outbox；回放对自身回显幂等（同字段并发
// 更新按服务端序生效——全序，无静默丢更）。
class SyncEngineStd {
public:
    static constexpr size_t kMaxOpsPerPost = 256;       // PROTOCOL §3
    static constexpr size_t kMaxPayloadBytes = 256 * 1024;
    static constexpr size_t kDefaultPullLimit = 200;    // PROTOCOL §2.5 缺省
    static constexpr size_t kMaxPullLimit = 1000;       // PROTOCOL §2.5 上限

    // device_id 为空时生成随机十六进制标识（128 位）；显式传入用于测试
    // 与多设备模拟
    explicit SyncEngineStd(
        const std::string& device_id = std::string());

    // 指令生成：本地立即生效 + 进 outbox，返回 op_id；payload 超限返回
    // ""（last_error 说明）。参数按类型取用：
    //   AddEntry/RemoveEntry/RecordHistory: a=word（hist 用 ts）
    //   UpdateNote: a=word, b=note          AddTag/RemoveTag: a=word, b=tag
    //   SetPref: a=key, b=value
    std::string enqueue(SyncOpType type, const std::string& a = std::string(),
                        const std::string& b = std::string(),
                        long long ts = 0);

    // 同步一轮：推 outbox → 按 PROTOCOL §2.8 拉取规则重放（位点落后
    // 快照覆盖位时先取快照跳变再增量）。传输失败返回 false；outbox 与
    // cursor 保持一致，重试即离线续传。
    bool sync(SyncTransportStd& transport, const std::string& gid,
              std::string* err);

    // 快照与压缩：本轮回放计数达阈值即上传快照（up_to_seq=cursor）并清
    // 计数。binlog 服务端裁剪（PROTOCOL §5）不在客户端职责内。
    bool maybe_snapshot(SyncTransportStd& transport, const std::string& gid,
                        size_t applied_threshold, std::string* err);

    // 持久化（离线续传的根）：device_id/本地序/cursor/outbox/状态一起
    // 落盘；load 后继续 sync 即断点续传。
    bool save_state(const std::string& path, std::string* err);
    bool load_state(const std::string& path, std::string* err);

    // —— 观测面（B7 同步状态页：组水位 vs 本机水位，server_plan §6）——
    const std::string& device_id() const { return device_id_; }
    uint64_t local_seq() const { return local_seq_; }
    uint64_t cursor() const { return cursor_; }
    size_t outbox_size() const { return outbox_.size(); }
    uint64_t applied_since_snapshot() const { return applied_since_snapshot_; }
    const SyncVocabStateStd& state() const { return state_; }
    const std::string& last_error() const { return last_error_; }
    void set_pull_limit(size_t limit) { pull_limit_ = limit; }

    // PROTOCOL §3：^[A-Za-z0-9_-]{16,64}$
    static bool valid_gid(const std::string& gid);

private:
    void apply(const std::string& payload);  // 单条确定性回放（不认识→忽略）
    bool push_outbox(SyncTransportStd& transport, const std::string& gid,
                     std::string* err);
    bool pull_and_replay(SyncTransportStd& transport, const std::string& gid,
                         std::string* err);

    std::string device_id_;
    uint64_t local_seq_ = 0;
    uint64_t cursor_ = 0;
    uint64_t applied_since_snapshot_ = 0;
    size_t pull_limit_ = kDefaultPullLimit;
    std::vector<EnqueuedOpStd> outbox_;
    SyncVocabStateStd state_;
    std::string last_error_;
};

}  // namespace UnidictCoreStd

#endif  // UNIDICT_SYNC_ENGINE_STD_H
