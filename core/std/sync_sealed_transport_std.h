// B5 传输密封层（server_plan §7；红线「中转只见密文」的客户端执行点）：
// SyncTransportStd 装饰器——payload 在引擎与真实传输之间密封/开封，组
// 密钥环（B3 SyncKeyRingStd）以引用驻留本层，密钥不出端。
//
// 分层口径：引擎 payload = 规范指令 JSON 明文 → 本层 XChaCha20-Poly1305
// 密封（版本头+AAD 绑定）→ 传输层（HTTP/LAN 绑定）base64 上线。即中转
// 看到的是 base64 密文，任何形态（自建中转/官方托管/局域网直传）同一份
// 密封链，行为完全一致。
//
// 解不开的指令/快照不跳过不丢弃——整轮同步失败、离线重试（宁可不同步，
// 不静默丢更；密钥轮换补齐历史版本后可解）。未持钥（配对未完成）一律
// 拒绝下发/接收，绝不落一条明文上链。
#ifndef UNIDICT_SYNC_SEALED_TRANSPORT_STD_H
#define UNIDICT_SYNC_SEALED_TRANSPORT_STD_H

#include <string>
#include <vector>

#include "sync_crypto_std.h"
#include "sync_engine_std.h"

namespace UnidictCoreStd {

class SyncSealedTransportStd : public SyncTransportStd {
public:
    // inner 与 keys 生命周期由调用方保证长于本对象（引擎单线程驱动口径，
    // 与 sync_engine_std 一致）
    SyncSealedTransportStd(SyncTransportStd& inner, const SyncKeyRingStd& keys)
        : inner_(inner), keys_(keys) {}

    bool meta(const std::string& gid, GroupMetaStd* out,
              std::string* err) override;
    bool push_ops(const std::string& gid,
                  const std::vector<EnqueuedOpStd>& ops,
                  std::vector<std::string>* acked, std::string* err) override;
    bool pull_ops(const std::string& gid, uint64_t since, size_t limit,
                  std::vector<RemoteOpStd>* out, uint64_t* cursor,
                  bool* has_more, std::string* err) override;
    bool put_snapshot(const std::string& gid, uint64_t up_to_seq,
                      const std::string& payload, std::string* err) override;
    bool get_snapshot(const std::string& gid, uint64_t* up_to_seq,
                      std::string* payload, std::string* err) override;

private:
    SyncTransportStd& inner_;
    const SyncKeyRingStd& keys_;
};

}  // namespace UnidictCoreStd

#endif  // UNIDICT_SYNC_SEALED_TRANSPORT_STD_H
