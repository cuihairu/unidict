#pragma once

// SPAKE2+（RFC 9383），ciphersuite = P-256-SHA256-HKDF-SHA256-HMAC-SHA256。
// server_plan §7 B3-b 配对换钥的 PAKE 面：短时效配对码只作一次性出组
// 凭证，双方以配对码为口令跑 SPAKE2+ 得到认证过的共享密钥，再用它密封
// 传输组钥（见 sync_crypto_std.h 的换钥信封）——短码不直接派生密钥，
// 中转/窃听者只见 share 与密文，密钥不出端。
//
// 实现口径与 crypto_std.h 一致：纯 C++17 + STL 自实现（P-256 域/点算术
// 8×32 limb，无 __int128——MSVC 兼容），行为由 RFC 9383 附录 C 权威测试
// 向量钉死（tests/spake2_std_test.cpp），向量对不上即测试红。
//
// 编码约定：标量与域元素一律 32 字节大端（与 SEC1/hex 直观对应）；点
// 编码一律未压缩 SEC1（0x04 || x || y，65 字节）——RFC 9383 向量的
// share/Z/V/TT 全部采用该形态。

#include <cstdint>
#include <string>

namespace UnidictCoreStd {

// ---- 离线注册：w0/w1 派生 + Verifier 注册记录 ----

// 由口令（配对码归一化形态）与双方身份派生 w0/w1（各 32B 大端，mod p）。
// PBKDF 占位口径：RFC RECOMMENDED scrypt/Argon2id 不在自实现面（重依赖
// 纪律），以 HMAC-SHA256 迭代展开 80 字节替代——配对码 TTL≤600s、单次
// 消费，爆破窗口短，迭代 HMAC 已抬高单次成本；升级归后续批次。PBKDF
// 不进 ciphersuite（§4），此口径不影响协议互操作与向量校验。
void spake2p_derive_w0_w1(const std::string& password, const std::string& id_prover,
                          const std::string& id_verifier, std::string& w0_out,
                          std::string& w1_out);

// Verifier 侧注册记录 L = w1*P（65B 未压缩）——服务器只存 L 与 w0，
// 不存口令派生物全量（augmented 口径：记录泄露不可离线爆破出 w1）。
// w1 长度非 32B 抛 std::invalid_argument。
std::string spake2p_registration_record(const std::string& w1);

// 随机标量（32B 大端，重采样保证 < p）——share/finish 的随机数来源。
std::string spake2p_random_scalar();

// ---- 在线认证：share 阶段（各自计算并发送） ----

// Prover：X = x*P + w0*M（65B 未压缩）。x/w0 非 32B 抛。
std::string spake2p_prover_share(const std::string& w0, const std::string& x_scalar);
// Verifier：Y = y*P + w0*N（65B 未压缩）。y/w0 非 32B 抛。
std::string spake2p_verifier_share(const std::string& w0, const std::string& y_scalar);

// ---- 在线认证：finish 阶段（收到对端 share 后） ----

struct Spake2pParams {
    std::string context;      // 应用定制串（进 TT；本仓配对流程用固定常量）
    std::string id_prover;    // Prover 身份（进 TT）
    std::string id_verifier;  // Verifier 身份（进 TT）
};

struct Spake2pSecrets {
    std::string k_shared;         // 32B 认证过的共享密钥——应用级派生来源
    std::string confirm_prover;   // 32B HMAC(K_confirmP, shareV)，Prover 发出
    std::string confirm_verifier; // 32B HMAC(K_confirmV, shareP)，Verifier 发出
};

// 两侧均返回本端的确认标签；对端标签到货后与本端重算值比对
// （compare_prover/compare_verifier 语义：Prover 验收 confirm_verifier，
// Verifier 验收 confirm_prover）。share 非法（长度/前缀/不在曲线上）抛
// std::invalid_argument——RFC §3.3 的群成员校验在解析处执行。
Spake2pSecrets spake2p_prover_finish(const Spake2pParams& params, const std::string& w0,
                                     const std::string& w1, const std::string& x_scalar,
                                     const std::string& share_v);
Spake2pSecrets spake2p_verifier_finish(const Spake2pParams& params, const std::string& w0,
                                       const std::string& y_scalar,
                                       const std::string& share_p,
                                       const std::string& registration_l);

// ---- B3-b 换钥信封：配对完成后组钥的密封传输 ----

// 确认通过后，双方以 K_shared 派生一次性 AEAD 钥（HKDF-SHA256，info 绑
// 用途域），把组钥（32B + 版本）密封传给新设备；对端解封后经
// SyncKeyRingStd::import_key 注入。sealed = nonce24 || ct || tag16，
// 明文 = 版本 4B LE || 组钥 32B；tag 校验失败返回 false 且出参不动。
// k_shared 非 32B / 组钥非 32B 抛 std::invalid_argument。
std::string seal_group_key_envelope(const std::string& k_shared, std::uint32_t version,
                                    const std::string& group_key);
bool open_group_key_envelope(const std::string& sealed, const std::string& k_shared,
                             std::uint32_t& version_out, std::string& group_key_out);

}  // namespace UnidictCoreStd
