#include "sync_sealed_transport_std.h"

namespace UnidictCoreStd {

bool SyncSealedTransportStd::meta(const std::string& gid, GroupMetaStd* out,
                                  std::string* err) {
    // 元信息不含 payload，直通
    return inner_.meta(gid, out, err);
}

bool SyncSealedTransportStd::push_ops(const std::string& gid,
                                      const std::vector<EnqueuedOpStd>& ops,
                                      std::vector<std::string>* acked,
                                      std::string* err) {
    if (!keys_.has_current()) {
        if (err) *err = "group key not configured (pairing pending)";
        return false;
    }
    if (ops.empty()) return true;  // 引擎空 outbox 不下发，语义直通
    // has_current 守卫在前，seal_command 的「未持钥」抛错面不可达
    // （SyncKeyRingStd 契约），密封段不包 try/catch
    std::vector<EnqueuedOpStd> sealed;
    sealed.reserve(ops.size());
    for (const EnqueuedOpStd& op : ops) {
        sealed.push_back(op);
        sealed.back().payload = keys_.seal_command(op.payload);
    }
    return inner_.push_ops(gid, sealed, acked, err);
}

bool SyncSealedTransportStd::pull_ops(const std::string& gid, uint64_t since,
                                      size_t limit,
                                      std::vector<RemoteOpStd>* out,
                                      uint64_t* cursor, bool* has_more,
                                      std::string* err) {
    if (!keys_.has_current()) {
        if (err) *err = "group key not configured (pairing pending)";
        return false;
    }
    if (!inner_.pull_ops(gid, since, limit, out, cursor, has_more, err)) {
        return false;
    }
    // 就地开封；任一条解不开即整轮失败（cursor 不推进，离线重试口径）
    for (RemoteOpStd& op : *out) {
        std::string plain;
        if (!keys_.open_command(op.payload, plain)) {
            if (err)
                *err = "op decrypt failed (seq " + std::to_string(op.seq) +
                       ", op_id " + op.op_id + ")";
            out->clear();
            return false;
        }
        op.payload = plain;
    }
    return true;
}

bool SyncSealedTransportStd::put_snapshot(const std::string& gid,
                                          uint64_t up_to_seq,
                                          const std::string& payload,
                                          std::string* err) {
    if (!keys_.has_current()) {
        if (err) *err = "group key not configured (pairing pending)";
        return false;
    }
    return inner_.put_snapshot(gid, up_to_seq, keys_.seal_command(payload),
                               err);  // has_current 守卫在前，同 push 口径
}

bool SyncSealedTransportStd::get_snapshot(const std::string& gid,
                                          uint64_t* up_to_seq,
                                          std::string* payload,
                                          std::string* err) {
    if (!keys_.has_current()) {
        if (err) *err = "group key not configured (pairing pending)";
        return false;
    }
    if (!inner_.get_snapshot(gid, up_to_seq, payload, err)) return false;
    std::string plain;
    if (!keys_.open_command(*payload, plain)) {
        if (err) *err = "snapshot decrypt failed";
        payload->clear();
        return false;
    }
    *payload = plain;
    return true;
}

}  // namespace UnidictCoreStd
