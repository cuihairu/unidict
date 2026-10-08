// B2 测试缝：内存 relay——按 server/sync_relay/PROTOCOL.md v1 语义实现
// SyncTransportStd（组自动创建、seq 从 1 单调、op_id 幂等去重、拉取
// seq 升序分页、快照位点校验）。故障注入用次数计数器（N>0 时前 N 次
// 调用失败并置 err），驱动引擎失败路径与重试续传。
#ifndef UNIDICT_SYNC_RELAY_MEMORY_STD_H
#define UNIDICT_SYNC_RELAY_MEMORY_STD_H

#include "sync_engine_std.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace UnidictCoreStd {

class MemoryRelay : public SyncTransportStd {
public:
    // —— 故障注入（次数制：>0 时逐次消耗，耗尽后恢复正常）——
    int fail_meta = 0;
    int fail_push = 0;
    int fail_pull = 0;
    int fail_snapshot_put = 0;
    int fail_snapshot_get = 0;

    // —— 观测面 ——
    size_t last_pull_limit = 0;
    uint64_t last_pull_since = 0;
    int push_calls = 0;
    int pull_calls = 0;

    bool meta(const std::string& gid, GroupMetaStd* out,
              std::string* err) override {
        if (fail_meta > 0 && fail_meta--) {
            if (err) *err = "meta failed (injected)";
            return false;
        }
        const Group& g = groups_[gid];
        out->latest_seq = g.binlog.size();
        out->snapshot_up_to_seq = g.snapshot_up_to_seq;
        return true;
    }

    bool push_ops(const std::string& gid,
                  const std::vector<EnqueuedOpStd>& ops,
                  std::vector<std::string>* acked, std::string* err) override {
        if (fail_push > 0 && fail_push--) {
            if (err) *err = "push failed (injected)";
            return false;
        }
        ++push_calls;
        Group& g = groups_[gid];  // PROTOCOL §2.2：POST 自动建组
        for (const EnqueuedOpStd& op : ops) {
            if (g.seen_op_ids.count(op.op_id)) {
                acked->push_back(op.op_id);  // duplicate_op_ids → 幂等 ack
                continue;
            }
            g.seen_op_ids.insert(op.op_id);
            RemoteOpStd remote;
            remote.seq = ++g.next_seq;  // §2.1：组内 seq 从 1 单调
            remote.op_id = op.op_id;
            remote.device_id =
                op.op_id.substr(0, op.op_id.find(':'));  // 幂等键前缀
            remote.ts = 0;  // 内存 relay 不模拟服务端时钟
            remote.payload = op.payload;
            g.binlog.push_back(remote);
            acked->push_back(op.op_id);
        }
        return true;
    }

    bool pull_ops(const std::string& gid, uint64_t since, size_t limit,
                  std::vector<RemoteOpStd>* out, uint64_t* cursor,
                  bool* has_more, std::string* err) override {
        if (fail_pull > 0 && fail_pull--) {
            if (err) *err = "pull failed (injected)";
            return false;
        }
        ++pull_calls;
        last_pull_limit = limit;
        last_pull_since = since;
        const Group& g = groups_[gid];
        out->clear();
        for (const RemoteOpStd& op : g.binlog) {
            if (op.seq <= since) continue;
            if (out->size() >= limit) break;
            out->push_back(op);
        }
        *cursor = out->empty() ? since : out->back().seq;
        *has_more = !g.binlog.empty() && g.binlog.back().seq > *cursor;
        return true;
    }

    bool put_snapshot(const std::string& gid, uint64_t up_to_seq,
                      const std::string& payload, std::string* err) override {
        if (fail_snapshot_put > 0 && fail_snapshot_put--) {
            if (err) *err = "snapshot put failed (injected)";
            return false;
        }
        Group& g = groups_[gid];
        // PROTOCOL §2.6：1 ≤ x ≤ latest（x=0 拒绝，防空洞快照）
        if (up_to_seq == 0 || up_to_seq > g.binlog.size()) {
            if (err) *err = "snapshot up_to_seq out of range";
            return false;
        }
        g.snapshot_up_to_seq = up_to_seq;
        g.snapshot_payload = payload;
        return true;
    }

    bool get_snapshot(const std::string& gid, uint64_t* up_to_seq,
                      std::string* payload, std::string* err) override {
        if (fail_snapshot_get > 0 && fail_snapshot_get--) {
            if (err) *err = "snapshot get failed (injected)";
            return false;
        }
        const Group& g = groups_[gid];
        if (g.snapshot_up_to_seq == 0) {
            if (err) *err = "no snapshot";
            return false;
        }
        *up_to_seq = g.snapshot_up_to_seq;
        *payload = g.snapshot_payload;
        return true;
    }

    // 只读观测缝（密封层测试用：中转侧落库内容断言）
    const std::vector<RemoteOpStd>& binlog_of(const std::string& gid) const {
        return groups_.at(gid).binlog;
    }

    // 篡改注入缝（翻密文字节用；非 const 路径与观测面分开命名）
    std::vector<RemoteOpStd>& binlog_mutable(const std::string& gid) {
        return groups_[gid].binlog;
    }

    const std::string& snapshot_payload_of(const std::string& gid) const {
        return groups_.at(gid).snapshot_payload;
    }

private:
    struct Group {
        uint64_t next_seq = 0;
        uint64_t snapshot_up_to_seq = 0;
        std::string snapshot_payload;
        std::set<std::string> seen_op_ids;
        std::vector<RemoteOpStd> binlog;
    };
    std::map<std::string, Group> groups_;
};

}  // namespace UnidictCoreStd

#endif  // UNIDICT_SYNC_RELAY_MEMORY_STD_H
