#include "crypto_std.h"

#include <stdexcept>

#include <random>

#include "sha256_std.h"

namespace UnidictCoreStd {

namespace {

inline std::uint32_t rotl32(std::uint32_t v, int c) {
    return (v << c) | (v >> (32 - c));
}

inline std::uint32_t le32(const unsigned char* p) {
    return (std::uint32_t)p[0] | ((std::uint32_t)p[1] << 8) |
           ((std::uint32_t)p[2] << 16) | ((std::uint32_t)p[3] << 24);
}

inline void put_le32(unsigned char* p, std::uint32_t v) {
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
}

inline void put_le64(unsigned char* p, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = (unsigned char)((v >> (8 * i)) & 0xff);
}

// ---- ChaCha20 核心（RFC 8439）----

// 10 个双轮（列轮 + 对角轮），block 与 HChaCha20 共用
void chacha_rounds(std::uint32_t w[16]) {
    auto quarter = [](std::uint32_t* q, int a, int b, int c, int d) {
        q[a] += q[b]; q[d] ^= q[a]; q[d] = rotl32(q[d], 16);
        q[c] += q[d]; q[b] ^= q[c]; q[b] = rotl32(q[b], 12);
        q[a] += q[b]; q[d] ^= q[a]; q[d] = rotl32(q[d], 8);
        q[c] += q[d]; q[b] ^= q[c]; q[b] = rotl32(q[b], 7);
    };
    for (int i = 0; i < 10; ++i) {
        quarter(w, 0, 4, 8, 12); quarter(w, 1, 5, 9, 13);
        quarter(w, 2, 6, 10, 14); quarter(w, 3, 7, 11, 15);
        quarter(w, 0, 5, 10, 15); quarter(w, 1, 6, 11, 12);
        quarter(w, 2, 7, 8, 13); quarter(w, 3, 4, 9, 14);
    }
}

// RFC 8439 §2.3 state：4 常数 + 8 key 字 + 1 counter + 3 nonce 字；
// 20 轮后逐字加回初始 state 再小端序列化
void chacha20_block(const std::uint32_t key[8], std::uint32_t counter,
                    const unsigned char nonce12[12], unsigned char out[64]) {
    const std::uint32_t st[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
        key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7],
        counter, le32(nonce12), le32(nonce12 + 4), le32(nonce12 + 8)};
    std::uint32_t w[16];
    for (int i = 0; i < 16; ++i) w[i] = st[i];
    chacha_rounds(w);
    for (int i = 0; i < 16; ++i) put_le32(out + 4 * i, w[i] + st[i]);
}

// ---- HChaCha20（draft-irtf-cfrg-xchacha §2.2）----
// 16 字节 nonce 占 state 字 12-15；20 轮后取字 0-3 与 12-15 两块做子钥，
// 不加初始 state
void hchacha20(const std::uint32_t key[8], const unsigned char nonce16[16],
               unsigned char subkey[32]) {
    const std::uint32_t st[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
        key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7],
        le32(nonce16), le32(nonce16 + 4), le32(nonce16 + 8), le32(nonce16 + 12)};
    std::uint32_t w[16];
    for (int i = 0; i < 16; ++i) w[i] = st[i];
    chacha_rounds(w);
    for (int i = 0; i < 4; ++i) put_le32(subkey + 4 * i, w[i]);
    for (int i = 0; i < 4; ++i) put_le32(subkey + 16 + 4 * i, w[12 + i]);
}

// ---- Poly1305（RFC 8439 §2.5）----
// 26-bit 域拆分（donna-32 风格）：MSVC 没有 __int128，用 u64 累加器
// （最大项 2^26 * (2^26+5) * 5 < 2^59，加法链不溢出）

constexpr std::uint32_t kMask26 = 0x3ffffff;

struct Poly1305Ctx {
    std::uint32_t r[5];
    std::uint32_t h[5];
    std::uint32_t pad[4];
    unsigned char buf[16];
    std::size_t buf_len = 0;
};

void poly1305_init(Poly1305Ctx& c, const unsigned char key[32]) {
    const std::uint32_t t0 = le32(key), t1 = le32(key + 4);
    const std::uint32_t t2 = le32(key + 8), t3 = le32(key + 12);
    // RFC 8439 的 130-bit clamp 掩码在 26-bit 拆分下的常量形态
    c.r[0] = t0 & 0x3ffffff;
    c.r[1] = ((t0 >> 26) | (t1 << 6)) & 0x3ffff03;
    c.r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ff;
    c.r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fff;
    c.r[4] = (t3 >> 8) & 0x00fffff;
    for (int i = 0; i < 5; ++i) c.h[i] = 0;
    c.pad[0] = le32(key + 16);
    c.pad[1] = le32(key + 20);
    c.pad[2] = le32(key + 24);
    c.pad[3] = le32(key + 28);
    c.buf_len = 0;
}

// 吸收整 16 字节块（hibit：满块 0x01000000，补位尾块 0）
void poly1305_blocks(Poly1305Ctx& c, const unsigned char* m, std::size_t len,
                     std::uint32_t hibit) {
    const std::uint32_t s1 = c.r[1] * 5, s2 = c.r[2] * 5;
    const std::uint32_t s3 = c.r[3] * 5, s4 = c.r[4] * 5;
    while (len >= 16) {
        c.h[0] += le32(m) & kMask26;
        c.h[1] += (le32(m + 3) >> 2) & kMask26;
        c.h[2] += (le32(m + 6) >> 4) & kMask26;
        c.h[3] += (le32(m + 9) >> 6) & kMask26;
        c.h[4] += (le32(m + 12) >> 8) | hibit;
        std::uint64_t d0 = (std::uint64_t)c.h[0] * c.r[0] + (std::uint64_t)c.h[1] * s4 +
                           (std::uint64_t)c.h[2] * s3 + (std::uint64_t)c.h[3] * s2 +
                           (std::uint64_t)c.h[4] * s1;
        std::uint64_t d1 = (std::uint64_t)c.h[0] * c.r[1] + (std::uint64_t)c.h[1] * c.r[0] +
                           (std::uint64_t)c.h[2] * s4 + (std::uint64_t)c.h[3] * s3 +
                           (std::uint64_t)c.h[4] * s2;
        std::uint64_t d2 = (std::uint64_t)c.h[0] * c.r[2] + (std::uint64_t)c.h[1] * c.r[1] +
                           (std::uint64_t)c.h[2] * c.r[0] + (std::uint64_t)c.h[3] * s4 +
                           (std::uint64_t)c.h[4] * s3;
        std::uint64_t d3 = (std::uint64_t)c.h[0] * c.r[3] + (std::uint64_t)c.h[1] * c.r[2] +
                           (std::uint64_t)c.h[2] * c.r[1] + (std::uint64_t)c.h[3] * c.r[0] +
                           (std::uint64_t)c.h[4] * s4;
        std::uint64_t d4 = (std::uint64_t)c.h[0] * c.r[4] + (std::uint64_t)c.h[1] * c.r[3] +
                           (std::uint64_t)c.h[2] * c.r[2] + (std::uint64_t)c.h[3] * c.r[1] +
                           (std::uint64_t)c.h[4] * c.r[0];
        std::uint64_t cc = d0 >> 26; c.h[0] = (std::uint32_t)d0 & kMask26;
        d1 += cc; cc = d1 >> 26; c.h[1] = (std::uint32_t)d1 & kMask26;
        d2 += cc; cc = d2 >> 26; c.h[2] = (std::uint32_t)d2 & kMask26;
        d3 += cc; cc = d3 >> 26; c.h[3] = (std::uint32_t)d3 & kMask26;
        d4 += cc; cc = d4 >> 26; c.h[4] = (std::uint32_t)d4 & kMask26;
        c.h[0] += (std::uint32_t)cc * 5;
        cc = c.h[0] >> 26; c.h[0] &= kMask26;
        c.h[1] += (std::uint32_t)cc;
        m += 16;
        len -= 16;
    }
}

void poly1305_update(Poly1305Ctx& c, const unsigned char* m, std::size_t len) {
    if (c.buf_len) {
        const std::size_t take = (16 - c.buf_len < len) ? (16 - c.buf_len) : len;
        for (std::size_t i = 0; i < take; ++i) c.buf[c.buf_len + i] = m[i];
        c.buf_len += take;
        m += take;
        len -= take;
        if (c.buf_len == 16) {
            poly1305_blocks(c, c.buf, 16, 0x01000000);
            c.buf_len = 0;
        }
    }
    const std::size_t whole = len & ~static_cast<std::size_t>(15);
    if (whole) {
        poly1305_blocks(c, m, whole, 0x01000000);
        m += whole;
        len -= whole;
    }
    for (std::size_t i = 0; i < len; ++i) c.buf[c.buf_len++] = m[i];
}

void poly1305_finish(Poly1305Ctx& c, unsigned char tag[16]) {
    if (c.buf_len) {
        unsigned char tail[16] = {};
        for (std::size_t i = 0; i < c.buf_len; ++i) tail[i] = c.buf[i];
        tail[c.buf_len] = 1;
        poly1305_blocks(c, tail, 16, 0);
    }
    // 完全进位
    std::uint32_t cc = c.h[1] >> 26; c.h[1] &= kMask26; c.h[2] += cc;
    cc = c.h[2] >> 26; c.h[2] &= kMask26; c.h[3] += cc;
    cc = c.h[3] >> 26; c.h[3] &= kMask26; c.h[4] += cc;
    cc = c.h[4] >> 26; c.h[4] &= kMask26; c.h[0] += cc * 5;
    cc = c.h[0] >> 26; c.h[0] &= kMask26; c.h[1] += cc;
    // h + -p：算 h+5，第 131 位没立起来（g4 借位下溢）说明 h < p，选 h；
    // 否则选 g = h - p
    std::uint32_t g0 = c.h[0] + 5; cc = g0 >> 26; g0 &= kMask26;
    std::uint32_t g1 = c.h[1] + cc; cc = g1 >> 26; g1 &= kMask26;
    std::uint32_t g2 = c.h[2] + cc; cc = g2 >> 26; g2 &= kMask26;
    std::uint32_t g3 = c.h[3] + cc; cc = g3 >> 26; g3 &= kMask26;
    std::uint32_t g4 = c.h[4] + cc - (1u << 26);
    const std::uint32_t mask = (g4 >> 31) - 1;
    const std::uint32_t inv = ~mask;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    c.h[0] = (c.h[0] & inv) | g0;
    c.h[1] = (c.h[1] & inv) | g1;
    c.h[2] = (c.h[2] & inv) | g2;
    c.h[3] = (c.h[3] & inv) | g3;
    c.h[4] = (c.h[4] & inv) | g4;
    // 5×26 位 limb 重打包成连续 128 位（4×32 位字）——tag 是 h 的纯小端
    // 位流，limb 之间不能留 6 位空洞；旧 h4 的 bit128/129 由 32 位截断
    // 自然丢掉（mod 2^128）
    c.h[0] = (c.h[0]) | (c.h[1] << 26);
    c.h[1] = (c.h[1] >> 6) | (c.h[2] << 20);
    c.h[2] = (c.h[2] >> 12) | (c.h[3] << 14);
    c.h[3] = (c.h[3] >> 18) | (c.h[4] << 8);
    // h += pad（一次性出 16 字节 tag）
    std::uint64_t f = (std::uint64_t)c.h[0] + c.pad[0]; c.h[0] = (std::uint32_t)f;
    f = (std::uint64_t)c.h[1] + c.pad[1] + (f >> 32); c.h[1] = (std::uint32_t)f;
    f = (std::uint64_t)c.h[2] + c.pad[2] + (f >> 32); c.h[2] = (std::uint32_t)f;
    f = (std::uint64_t)c.h[3] + c.pad[3] + (f >> 32); c.h[3] = (std::uint32_t)f;
    put_le32(tag, c.h[0]);
    put_le32(tag + 4, c.h[1]);
    put_le32(tag + 8, c.h[2]);
    put_le32(tag + 12, c.h[3]);
}

// ---- AEAD 组装（RFC 8439 §2.8）----

void key_words_of(const std::string& key32, std::uint32_t out[8]) {
    for (int i = 0; i < 8; ++i)
        out[i] = le32(reinterpret_cast<const unsigned char*>(key32.data()) + 4 * i);
}

// keystream：counter 从 1 起逐块异或（block 0 留给 Poly1305 钥）
void chacha20_xor(const std::string& key32, const std::string& nonce12,
                  const std::string& in, std::string& out) {
    std::uint32_t kw[8];
    key_words_of(key32, kw);
    const unsigned char* n12 =
        reinterpret_cast<const unsigned char*>(nonce12.data());
    out.clear();
    out.reserve(in.size());
    std::uint32_t counter = 1;
    unsigned char ks[64];
    for (std::size_t off = 0; off < in.size(); off += 64) {
        chacha20_block(kw, counter++, n12, ks);
        const std::size_t take = (in.size() - off < 64) ? (in.size() - off) : 64;
        for (std::size_t i = 0; i < take; ++i)
            out += static_cast<char>(static_cast<unsigned char>(in[off + i]) ^ ks[i]);
    }
}

// Poly1305 钥 = counter 0 块的前 32 字节；mac 输入 =
// aad || pad16 || ct || pad16 || le64(aad_len) || le64(ct_len)
// （分段喂 update，不拼大字符串）
void chacha20poly1305_tag(const std::string& key32, const std::string& nonce12,
                          const std::string& ct, const std::string& aad,
                          unsigned char tag[16]) {
    std::uint32_t kw[8];
    key_words_of(key32, kw);
    const unsigned char* n12 =
        reinterpret_cast<const unsigned char*>(nonce12.data());
    unsigned char pk[64];
    chacha20_block(kw, 0, n12, pk);

    Poly1305Ctx c;
    poly1305_init(c, pk);
    auto feed = [&c](const std::string& s) {
        poly1305_update(c, reinterpret_cast<const unsigned char*>(s.data()), s.size());
    };
    feed(aad);
    std::string pad((16 - (aad.size() % 16)) % 16, '\0');
    feed(pad);
    feed(ct);
    pad.assign((16 - (ct.size() % 16)) % 16, '\0');
    feed(pad);
    unsigned char lens[16];
    put_le64(lens, (std::uint64_t)aad.size());
    put_le64(lens + 8, (std::uint64_t)ct.size());
    poly1305_update(c, lens, sizeof(lens));
    poly1305_finish(c, tag);
}

// XChaCha：HChaCha20(key, nonce[0..16]) 派生子钥 + 4 零字节拼 nonce 后
// 8 字节 = RFC 8439 的 12 字节 nonce
void derive_xchacha(const std::string& key32, const std::string& nonce24,
                    std::string& subkey_out, std::string& nonce12_out) {
    std::uint32_t kw[8];
    key_words_of(key32, kw);
    const unsigned char* n24 =
        reinterpret_cast<const unsigned char*>(nonce24.data());
    unsigned char sub[32];
    hchacha20(kw, n24, sub);
    subkey_out.assign(reinterpret_cast<const char*>(sub), 32);
    secure_wipe(sub, sizeof(sub));
    nonce12_out.assign(4, '\0');
    nonce12_out.append(nonce24, 16, 8);
}

// ---- HMAC-SHA256（RFC 2104）----

std::string hmac_sha256_impl(const std::string& key, const std::string& data) {
    unsigned char k[32] = {};
    if (key.size() > 32) {
        // 长钥先散列到 32 字节（RFC 2104 口径）
        Sha256HasherStd h;
        h.update(key);
        const std::string kr = h.digest_raw();
        for (int i = 0; i < 32; ++i) k[i] = (unsigned char)kr[i];
    } else {
        for (std::size_t i = 0; i < key.size(); ++i) k[i] = (unsigned char)key[i];
    }
    unsigned char ipad[64], opad[64];
    for (int i = 0; i < 64; ++i) {
        const unsigned char kb = (i < 32) ? k[i] : (unsigned char)0;
        ipad[i] = (unsigned char)(kb ^ 0x36);
        opad[i] = (unsigned char)(kb ^ 0x5c);
    }
    Sha256HasherStd inner;
    inner.update(ipad, sizeof(ipad));
    inner.update(data);
    const std::string inner_raw = inner.digest_raw();
    Sha256HasherStd outer;
    outer.update(opad, sizeof(opad));
    outer.update(inner_raw);
    return outer.digest_raw();
}

}  // namespace

// ---- 公共 API ----

std::string random_bytes(std::size_t n) {
    std::string out(n, '\0');
    if (n == 0) return out;
    std::random_device rd;
    // 按 32 位值的小端逐字节铺，与机器字节序无关
    constexpr std::size_t kChunk = sizeof(std::random_device::result_type);
    for (std::size_t i = 0; i < n; i += kChunk) {
        const auto v = rd();
        unsigned char bytes[kChunk];
        for (std::size_t b = 0; b < kChunk; ++b)
            bytes[b] = (unsigned char)((v >> (8 * b)) & 0xff);
        const std::size_t take = (n - i < kChunk) ? (n - i) : kChunk;
        for (std::size_t b = 0; b < take; ++b) out[i + b] = (char)bytes[b];
    }
    return out;
}

void secure_wipe(void* p, std::size_t n) {
    volatile unsigned char* v = reinterpret_cast<volatile unsigned char*>(p);
    while (n--) *v++ = 0;
}

std::string hmac_sha256(const std::string& key, const std::string& data) {
    return hmac_sha256_impl(key, data);
}

std::string hkdf_sha256_extract(const std::string& salt, const std::string& ikm) {
    // salt 传空串 = RFC 5869 的缺省全零 HashLen salt（HMAC 零填充 key
    // 与显式全零 key 等价）
    return hmac_sha256_impl(salt, ikm);
}

std::string hkdf_sha256_expand(const std::string& prk, const std::string& info,
                               std::size_t len) {
    if (len > 255u * 32u) throw std::invalid_argument("hkdf expand length > 255*HashLen");
    std::string okm;
    std::string t;
    okm.reserve(len + 32);
    for (unsigned char counter = 1; okm.size() < len; ++counter) {
        t = hmac_sha256_impl(prk, t + info + std::string(1, (char)counter));
        okm += t;
    }
    okm.resize(len);
    return okm;
}

std::string poly1305_tag(const std::string& key32, const std::string& msg) {
    if (key32.size() != kAeadKeyLen)
        throw std::invalid_argument("poly1305 key must be 32 bytes");
    Poly1305Ctx c;
    poly1305_init(c, reinterpret_cast<const unsigned char*>(key32.data()));
    if (!msg.empty())
        poly1305_update(c, reinterpret_cast<const unsigned char*>(msg.data()), msg.size());
    unsigned char tag[16];
    poly1305_finish(c, tag);
    return std::string(reinterpret_cast<const char*>(tag), sizeof(tag));
}

std::string aead_chacha20poly1305_seal(const std::string& key, const std::string& nonce12,
                                       const std::string& plaintext, const std::string& aad) {
    if (key.size() != kAeadKeyLen) throw std::invalid_argument("aead key must be 32 bytes");
    if (nonce12.size() != 12) throw std::invalid_argument("chacha nonce must be 12 bytes");
    std::string ct;
    chacha20_xor(key, nonce12, plaintext, ct);
    unsigned char tag[16];
    chacha20poly1305_tag(key, nonce12, ct, aad, tag);
    ct.append(reinterpret_cast<const char*>(tag), 16);
    return ct;
}

bool aead_chacha20poly1305_open(const std::string& sealed, const std::string& key,
                                const std::string& nonce12, const std::string& aad,
                                std::string& out_plaintext) {
    if (key.size() != kAeadKeyLen) throw std::invalid_argument("aead key must be 32 bytes");
    if (nonce12.size() != 12) throw std::invalid_argument("chacha nonce must be 12 bytes");
    if (sealed.size() < kAeadTagLen) return false;
    const std::string ct = sealed.substr(0, sealed.size() - kAeadTagLen);
    const unsigned char* tag = reinterpret_cast<const unsigned char*>(
        sealed.data()) + sealed.size() - kAeadTagLen;
    unsigned char tag_calc[16];
    chacha20poly1305_tag(key, nonce12, ct, aad, tag_calc);
    unsigned char diff = 0;
    for (std::size_t i = 0; i < kAeadTagLen; ++i) diff |= (unsigned char)(tag[i] ^ tag_calc[i]);
    if (diff != 0) return false;
    chacha20_xor(key, nonce12, ct, out_plaintext);
    return true;
}

std::string xchacha20poly1305_seal_with_nonce(const std::string& key, const std::string& nonce24,
                                              const std::string& plaintext,
                                              const std::string& aad) {
    if (key.size() != kAeadKeyLen) throw std::invalid_argument("aead key must be 32 bytes");
    if (nonce24.size() != kXchaChaNonceLen)
        throw std::invalid_argument("xchacha nonce must be 24 bytes");
    std::string subkey, nonce12;
    derive_xchacha(key, nonce24, subkey, nonce12);
    std::string body = aead_chacha20poly1305_seal(subkey, nonce12, plaintext, aad);
    secure_wipe(subkey);
    return body;
}

bool xchacha20poly1305_open_with_nonce(const std::string& sealed, const std::string& key,
                                       const std::string& nonce24, const std::string& aad,
                                       std::string& out_plaintext) {
    if (key.size() != kAeadKeyLen) throw std::invalid_argument("aead key must be 32 bytes");
    if (nonce24.size() != kXchaChaNonceLen)
        throw std::invalid_argument("xchacha nonce must be 24 bytes");
    std::string subkey, nonce12;
    derive_xchacha(key, nonce24, subkey, nonce12);
    const bool ok = aead_chacha20poly1305_open(sealed, subkey, nonce12, aad, out_plaintext);
    secure_wipe(subkey);
    return ok;
}

std::string aead_xchacha20poly1305_seal(const std::string& key, const std::string& plaintext,
                                        const std::string& aad) {
    if (key.size() != kAeadKeyLen) throw std::invalid_argument("aead key must be 32 bytes");
    const std::string nonce = random_bytes(kXchaChaNonceLen);
    return nonce + xchacha20poly1305_seal_with_nonce(key, nonce, plaintext, aad);
}

bool aead_xchacha20poly1305_open(const std::string& sealed, const std::string& key,
                                 const std::string& aad, std::string& out_plaintext) {
    if (key.size() != kAeadKeyLen) throw std::invalid_argument("aead key must be 32 bytes");
    if (sealed.size() < kXchaChaNonceLen + kAeadTagLen) return false;
    const std::string nonce = sealed.substr(0, kXchaChaNonceLen);
    return xchacha20poly1305_open_with_nonce(sealed.substr(kXchaChaNonceLen), key, nonce,
                                             aad, out_plaintext);
}

}  // namespace UnidictCoreStd
