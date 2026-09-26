#include "std/ripemd128_std.h"

#include <cstring>

namespace UnidictCoreStd {
namespace {

// ---- RIPEMD-128 规范常量表（rmd128.txt） ----

// 左路消息字选择顺序
constexpr uint8_t kR[64] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
    7,  4, 13,  1, 10,  6, 15,  3, 12,  0,  9,  5,  2, 14, 11,  8,
    3, 10, 14,  4,  9, 15,  8,  1,  2,  7,  0,  6, 13, 11,  5, 12,
    1,  9, 11, 10,  0,  8, 12,  4, 13,  3,  7, 15, 14,  5,  6,  2,
};

// 右路消息字选择顺序（与 kR 逐项相加为 63）
constexpr uint8_t kRp[64] = {
    5, 14,  7,  0,  9,  2, 11,  4, 13,  6, 15,  8,  1, 10,  3, 12,
    6, 11,  3,  7,  0, 13,  5, 10, 14, 15,  8, 12,  4,  9,  1,  2,
   15,  5,  1,  3,  7, 14,  6,  9, 11,  8, 12,  2, 10,  0,  4, 13,
    8,  6,  4,  1,  3, 11, 15,  0,  5, 12,  2, 13,  9,  7, 10, 14,
};

// 左路循环左移位数
constexpr uint8_t kS[64] = {
    11, 14, 15, 12,  5,  8,  7,  9, 11, 13, 14, 15,  6,  7,  9,  8,
     7,  6,  8, 13, 11,  9,  7, 15,  7, 12, 15,  9, 11,  7, 13, 12,
    11, 13,  6,  7, 14,  9, 13, 15, 14,  8, 13,  6,  5, 12,  7,  5,
    11, 12, 14, 15, 14, 15,  9,  8,  9, 14,  5,  6,  8,  6,  5, 12,
};

// 右路循环左移位数
constexpr uint8_t kSp[64] = {
     8,  9,  9, 11, 13, 15, 15,  5,  7,  7,  8, 11, 14, 14, 12,  6,
     9, 13, 15,  7, 12,  8,  9, 11,  7,  7, 12,  7,  6, 15, 13, 11,
     9,  7, 15, 11,  8,  6,  6, 14, 12, 13,  5, 14, 13, 13,  7,  5,
    15,  5,  8, 11, 14, 14,  6, 14,  6,  9, 12,  9, 12,  5, 15,  8,
};

inline uint32_t rotl(uint32_t x, unsigned s) {
    return (x << s) | (x >> (32u - s));
}

inline uint32_t add32(uint32_t a, uint32_t b) { return a + b; }
inline uint32_t add32(uint32_t a, uint32_t b, uint32_t c) { return a + b + c; }
inline uint32_t add32(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return a + b + c + d;
}
inline uint32_t add32(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    return a + b + c + d + e;
}

// 轮函数 f_j
inline uint32_t f(int j, uint32_t x, uint32_t y, uint32_t z) {
    if (j < 16) return x ^ y ^ z;
    if (j < 32) return (x & y) | (~x & z);
    if (j < 48) return (x | ~y) ^ z;
    return (x & z) | (y & ~z);
}

// 轮常数 K_j
inline uint32_t k(int j) {
    if (j < 16) return 0x00000000u;
    if (j < 32) return 0x5A827999u;
    if (j < 48) return 0x6ED9EBA1u;
    return 0x8F1BBCDCu;
}

// 轮常数 K'_j
inline uint32_t kp(int j) {
    if (j < 16) return 0x50A28BE6u;
    if (j < 32) return 0x5C4DD124u;
    if (j < 48) return 0x6D703EF3u;
    return 0x00000000u;
}

inline uint32_t load_le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline void store_le32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

}  // namespace

Ripemd128Digest ripemd128(const uint8_t* data, size_t len) {
    uint32_t h0 = 0x67452301u;
    uint32_t h1 = 0xEFCDAB89u;
    uint32_t h2 = 0x98BADCFEu;
    uint32_t h3 = 0x10325476u;

    // 填充：先 0x80，再补 0x00 直到 len ≡ 56 (mod 64)，最后 8 字节
    // 放原始比特长度的小端表示。至少补 1 字节。
    const size_t total = ((len + 8) / 64 + 1) * 64;
    // 直接在栈上拼消息（词典密钥派生只会喂 4~20 字节，这里上限 64+8 即可，
    // 但为了通用性用动态分配）
    std::string buf;
    buf.resize(total);
    if (len && data) {
        std::memcpy(&buf[0], data, len);
    }
    buf[len] = 0x80;
    // buf[len+1 .. total-9] 已经是 0（resize 后的值未指定，用显式清零）
    for (size_t i = len + 1; i + 8 <= total; ++i) {
        buf[i] = 0;
    }
    const uint64_t bitlen = static_cast<uint64_t>(len) * 8u;
    for (int i = 0; i < 8; ++i) {
        buf[total - 8 + i] = static_cast<uint8_t>((bitlen >> (8 * i)) & 0xFF);
    }

    for (size_t off = 0; off < total; off += 64) {
        uint32_t x[16];
        for (int i = 0; i < 16; ++i) {
            x[i] = load_le32(reinterpret_cast<const uint8_t*>(buf.data()) + off + 4 * i);
        }
        uint32_t a = h0, b = h1, c = h2, d = h3;
        uint32_t ap = h0, bp = h1, cp = h2, dp = h3;
        for (int j = 0; j < 64; ++j) {
            const uint32_t t = rotl(
                add32(a, f(j, b, c, d), x[kR[j]], k(j)), kS[j]);
            a = d;
            d = c;
            c = b;
            b = t;
            const uint32_t tp = rotl(
                add32(ap, f(63 - j, bp, cp, dp), x[kRp[j]], kp(j)), kSp[j]);
            ap = dp;
            dp = cp;
            cp = bp;
            bp = tp;
        }
        // 与 MD4 家族同构的线性组合
        const uint32_t t = add32(h1, c, dp);
        h1 = add32(h2, d, ap);
        h2 = add32(h3, a, bp);
        h3 = add32(h0, b, cp);
        h0 = t;
    }

    Ripemd128Digest out{};
    store_le32(out.data() + 0, h0);
    store_le32(out.data() + 4, h1);
    store_le32(out.data() + 8, h2);
    store_le32(out.data() + 12, h3);
    return out;
}

Ripemd128Digest ripemd128(const std::string& data) {
    return ripemd128(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::string to_hex(const Ripemd128Digest& d) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(d.size() * 2);
    for (const uint8_t b : d) {
        out.push_back(kHex[b >> 4]);
        out.push_back(kHex[b & 0x0F]);
    }
    return out;
}

}  // namespace UnidictCoreStd
