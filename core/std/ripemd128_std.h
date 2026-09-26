// RIPEMD-128（Dobbertin/Bosselaers/Preneel 1996 的 RIPEMD-128 变体）。
//
// 为什么需要它：MDict 的"加密"不是 DES/AES/Blowfish，而是以 RIPEMD-128
// 派生密钥、配合一个半字节交换的 XOR 流水（见 mdict_crypto_std.h）。没有
// RIPEMD-128 就解不开任何一份真实的加密 MDX。
//
// 为什么自己实现而不引 OpenSSL：core/std 必须无 Qt、无外部加密库；而且
// OpenSSL 3.x 默认 provider 里已经**没有** RIPEMD-128（实测本机
// `openssl dgst -ripemd128` 直接报 Unknown option，Python 的 hashlib 也
// 只剩 ripemd160），依赖它等于依赖一个绝大多数发行版都不提供的算法。
//
// 实现照 RIPEMD-128 公开规范（r/s/rp/sp 轮常量表与 f/K/Kp 轮函数）写成，
// 由 tests/ripemd128_std_test.cpp 用规范给出的测试向量锚定：
//   RIPEMD-128("The quick brown fox jumps over the lazy dog")
//     = 3fa9b57f053c053fbe2735b2380db596
// 空串与 "a"/"abc" 的向量也一并钉住。

#ifndef UNIDICT_RIPEMD128_STD_H
#define UNIDICT_RIPEMD128_STD_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace UnidictCoreStd {

// 摘要固定 16 字节。
using Ripemd128Digest = std::array<uint8_t, 16>;

// data 为空指针时视作空输入（等价于对 "" 求摘要）。
Ripemd128Digest ripemd128(const uint8_t* data, size_t len);
Ripemd128Digest ripemd128(const std::string& data);

// 十六进制小写输出，便于测试与日志。
std::string to_hex(const Ripemd128Digest& d);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_RIPEMD128_STD_H
