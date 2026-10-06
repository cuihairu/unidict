#pragma once

// 对称加密与 KDF 原语（std-only，自实现，无第三方依赖）。
//
// 为什么自写而不是链 libsodium/OpenSSL：core/ 的纪律是纯 C++17 + STL，
// 第三方重依赖一律走适配器（同 sha256_std.h 的口径）。同步的端到端加密
// 需要 AEAD 与 KDF；本文件按 RFC 逐字实现，行为由权威测试向量钉死
// （RFC 8439 / draft-irtf-cfrg-xchacha-03 / RFC 5869，见 tests/
// crypto_std_test.cpp）——自实现的风险由「向量对不上即测试红」兜底，
// 不靠自觉。
//
// 支持面注记：随机数走 std::random_device（glibc/libc++/MSVC 下均落
// CSPRNG）；MinGW 的确定性实现不在支持面（与 CI 矩阵一致）。取随机
// 失败按 random_device 的异常向上抛，绝不静默降级到伪随机。

#include <cstddef>
#include <string>

namespace UnidictCoreStd {

// ---- 随机与擦除 ----

// 密码学随机 n 字节。失败抛异常，不降级。
std::string random_bytes(std::size_t n);

// 最佳努力内存擦除：volatile 写零 + 编译屏障。对抗的是「退出组后堆上
// 密钥仍可读」的常规口径，不承诺对抗换页/休眠快照/冷启动攻击。
void secure_wipe(void* p, std::size_t n);
inline void secure_wipe(std::string& s) {
    if (!s.empty()) secure_wipe(&s[0], s.size());
    s.clear();
}

// ---- KDF ----

// HMAC-SHA256（RFC 2104）。
std::string hmac_sha256(const std::string& key, const std::string& data);

// HKDF-SHA256（RFC 5869）：extract + expand 两步分开暴露（同步配对
// 流程里 extract 一次、expand 多次派生不同用途子钥）。
std::string hkdf_sha256_extract(const std::string& salt, const std::string& ikm);
// len 上限 255*32 字节（RFC 5869 协议上限，超限抛 std::invalid_argument）。
std::string hkdf_sha256_expand(const std::string& prk, const std::string& info,
                               std::size_t len);

// ---- Poly1305 单发原语 ----

// 一段式消息认证：key 32 字节 + 任意长消息，返回 16 字节 tag（原始字节）。
// RFC 8439 §2.5.2 向量直接钉死——AEAD 面的 tag 输入恒为 16 字节整块
// 序列，够不着尾块缓冲分支，原语必须有自己的独立验证入口。
std::string poly1305_tag(const std::string& key32, const std::string& msg);

// ---- AEAD：XChaCha20-Poly1305 ----

// 密封形态：24 字节随机 nonce || 密文 || 16 字节 tag（combined，与
// 主流 AEAD 库的密封形态一致，一次一条指令自包含）。
constexpr std::size_t kXchaChaNonceLen = 24;
constexpr std::size_t kAeadTagLen = 16;
constexpr std::size_t kAeadKeyLen = 32;

// 加密恒成功（只可能因长度参数错误抛 std::invalid_argument）。
std::string aead_xchacha20poly1305_seal(const std::string& key, const std::string& plaintext,
                                        const std::string& aad);
// tag 校验失败返回 false 且 out_plaintext 不动（不输出部分明文）。
// key/nonce 长度错抛 std::invalid_argument。
bool aead_xchacha20poly1305_open(const std::string& sealed, const std::string& key,
                                 const std::string& aad, std::string& out_plaintext);

// ---- 确定性形态（nonce 由调用方给定，输出不带 nonce）----
//
// 上面两个随机 nonce 封装没法对着 RFC/draft 测试向量复现（向量都是
// 定值 nonce），所以把两层各自的原生形态单独暴露：RFC 8439 §2.8.2 向
// 量钉 chacha 形态，draft-irtf-cfrg-xchacha §A.3.1 向量钉 xchacha 形态
// （后者内部 = HChaCha20 派生子钥 + 前者）。密封输出一律「密文 || tag」，
// nonce 不内嵌。tag 校验失败返回 false 且 out_plaintext 不动。

// RFC 8439 原生形态：key 32 字节 + 12 字节 nonce。
std::string aead_chacha20poly1305_seal(const std::string& key, const std::string& nonce12,
                                       const std::string& plaintext, const std::string& aad);
bool aead_chacha20poly1305_open(const std::string& sealed, const std::string& key,
                                const std::string& nonce12, const std::string& aad,
                                std::string& out_plaintext);

// XChaCha 显式 nonce 形态：key 32 字节 + 24 字节 nonce。
std::string xchacha20poly1305_seal_with_nonce(const std::string& key, const std::string& nonce24,
                                              const std::string& plaintext,
                                              const std::string& aad);
bool xchacha20poly1305_open_with_nonce(const std::string& sealed, const std::string& key,
                                       const std::string& nonce24, const std::string& aad,
                                       std::string& out_plaintext);

}  // namespace UnidictCoreStd
