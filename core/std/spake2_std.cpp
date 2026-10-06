#include "spake2_std.h"

#include <stdexcept>

#include "crypto_std.h"
#include "sha256_std.h"

namespace UnidictCoreStd {

namespace {

// ---- P-256 域算术：8×32 limb little-endian，值域 [0, p) ----
//
// 不用 __int128（MSVC 兼容，与 crypto_std.cpp 的 Poly1305 同口径）。
// 乘法 schoolbook 用 u64 累加（不变量：t[i+j] 与 carry 都 < 2^32，三者
// 相加 ≤ 2^64-1 不溢出），归约统一走 big_norm 的「高位折叠」——
// 2^256 ≡ 2^256 mod p = k2p256ModP，任何超宽 i64 limb 数组反复把
// 高位 limb 按位权折回低位直到收敛。正确性由 RFC 9383 附录 C 全链
// 向量钉死。
struct Fe {
    std::uint32_t v[8];
};

const Fe kP = {{0xffffffff, 0xffffffff, 0xffffffff, 0x00000000,
                0x00000000, 0x00000000, 0x00000001, 0xffffffff}};
const Fe kPMinus2 = {{0xfffffffd, 0xffffffff, 0xffffffff, 0x00000000,
                      0x00000000, 0x00000000, 0x00000001, 0xffffffff}};
const Fe kCurveB = {{0x27d2604b, 0x3bce3c3e, 0xcc53b0f6, 0x651d06b0,
                     0x769886bc, 0xb3ebbd55, 0xaa3a93e7, 0x5ac635d8}};
// 2^256 mod p
const Fe k2p256ModP = {{0x00000001, 0x00000000, 0x00000000, 0xffffffff,
                        0xffffffff, 0xffffffff, 0xfffffffe, 0x00000000}};
const Fe kGx = {{0xd898c296, 0xf4a13945, 0x2deb33a0, 0x77037d81,
                 0x63a440f2, 0xf8bce6e5, 0xe12c4247, 0x6b17d1f2}};
const Fe kGy = {{0x37bf51f5, 0xcbb64068, 0x6b315ece, 0x2bce3357,
                 0x7c0f9e16, 0x8ee7eb4a, 0xfe1a7f9b, 0x4fe342e2}};
// RFC 9383 §4 固定点 M/N（TT 中以未压缩形态出现，此处存分量）
const Fe kMx = {{0x3d8fa12f, 0xafd49733, 0xf3dcab95, 0x3b64e16e,
                 0x2579f299, 0xba9dd724, 0xace46e55, 0x886e2f97}};
const Fe kMy = {{0xa12e2d20, 0xca547d55, 0x19c785e0, 0x5c7be094,
                 0xff02ac8e, 0x4e0b0e65, 0x3e43ce22, 0x5ff35516}};
const Fe kNx = {{0xa1292b49, 0x4b4f98ba, 0x014d49a2, 0x19c629d7,
                 0x38c37707, 0xb04d997f, 0x39c62937, 0xd8bbd6c6}};
const Fe kNy = {{0x656edbe7, 0x64490b1e, 0x34808cd5, 0x4d9bd360,
                 0x7f5168c6, 0x08a63633, 0xbfade450, 0x07d60aa6}};

Fe fe_zero() { return Fe{}; }

Fe fe_one() {
    Fe r{};
    r.v[0] = 1;
    return r;
}

bool fe_is_zero(const Fe& a) {
    for (int i = 0; i < 8; ++i)
        if (a.v[i]) return false;
    return true;
}

bool fe_eq(const Fe& a, const Fe& b) {
    for (int i = 0; i < 8; ++i)
        if (a.v[i] != b.v[i]) return false;
    return true;
}

// a >= b（limb 高位优先）
bool fe_ge(const Fe& a, const Fe& b) {
    for (int i = 7; i >= 0; --i) {
        if (a.v[i] != b.v[i]) return a.v[i] > b.v[i];
    }
    return true;
}

// 条件减 p：输入 < 2p，输出 < p
Fe fe_reduce_once(const Fe& a) {
    if (!fe_ge(a, kP)) return a;
    Fe r;
    std::uint64_t borrow = 0;
    for (int i = 0; i < 8; ++i) {
        const std::uint64_t cur = (std::uint64_t)a.v[i] - kP.v[i] - borrow;
        r.v[i] = (std::uint32_t)(cur & 0xffffffffu);
        borrow = cur >> 63;
    }
    return r;
}

// 任意宽度 u64 limb 数组（24 limb 容量）归约到 [0, p)——全程非负：
//   1) 逐 limb 规范化（carry 向高位传）
//   2) 高位 limb（k>=8）按位权 2^(32k) ≡ 2^(32(k-8))·(2^256 mod p)
//      折回低位，行式乘加（h[k]·c1[j] ≤ (2^32-1)²，加上 <2^32 的
//      低位与 carry 后 ≤ 2^64-1，u64 恰好不溢出），循环到高位清零
//   3) 顶位进位乘 c1 补入
//   4) 条件减 p（折叠后值 < 3p 量级，减两次足够）
// 负数不进这里：减法一律走「加 p 的补元」（fe_sub = a + (p-b)）。
Fe big_norm(std::uint64_t acc[24]) {
    for (int round = 0; round < 16; ++round) {
        for (int k = 0; k < 23; ++k) {
            acc[k + 1] += acc[k] >> 32;
            acc[k] &= 0xffffffffu;
        }
        bool high = false;
        for (int k = 8; k < 24; ++k)
            if (acc[k]) { high = true; break; }
        if (!high) break;
        std::uint64_t h[16] = {};
        for (int k = 8; k < 24; ++k) {
            h[k - 8] = acc[k];
            acc[k] = 0;
        }
        for (int k = 0; k < 16; ++k) {
            if (!h[k]) continue;
            std::uint64_t carry = 0;
            for (int j = 0; j < 8; ++j) {
                const std::uint64_t cur =
                    acc[k + j] + h[k] * k2p256ModP.v[j] + carry;
                acc[k + j] = cur & 0xffffffffu;
                carry = cur >> 32;
            }
            for (int j = 8; k + j < 24 && carry; ++j) {
                const std::uint64_t cur = acc[k + j] + carry;
                acc[k + j] = cur & 0xffffffffu;
                carry = cur >> 32;
            }
        }
    }
    // 收尾：规范化 + 顶位进位（≥2^256 的残量乘 c1 补回）
    std::uint64_t top = 0;
    for (int k = 0; k < 8; ++k) {
        const std::uint64_t cur = acc[k] + top;
        top = cur >> 32;
        acc[k] = cur & 0xffffffffu;
    }
    if (top > 0) {
        std::uint64_t carry = 0;
        for (int j = 0; j < 8; ++j) {
            const std::uint64_t cur = acc[j] + k2p256ModP.v[j] + carry;
            acc[j] = cur & 0xffffffffu;
            carry = cur >> 32;
        }
    }
    Fe r{};
    for (int k = 0; k < 8; ++k) r.v[k] = (std::uint32_t)acc[k];
    return fe_reduce_once(fe_reduce_once(r));
}

// p - b（b ∈ [0, p)）：b=0 返回 0，否则逐 limb 借位减
Fe fe_p_minus(const Fe& b) {
    if (fe_is_zero(b)) return fe_zero();
    Fe r;
    std::uint64_t borrow = 0;
    for (int i = 0; i < 8; ++i) {
        const std::uint64_t cur = (std::uint64_t)kP.v[i] - b.v[i] - borrow;
        r.v[i] = (std::uint32_t)(cur & 0xffffffffu);
        borrow = cur >> 63;
    }
    return r;
}

Fe fe_add(const Fe& a, const Fe& b) {
    std::uint64_t acc[24] = {};
    for (int i = 0; i < 8; ++i)
        acc[i] = (std::uint64_t)a.v[i] + b.v[i];  // 先提升再相加，防 int 回绕
    return big_norm(acc);
}

Fe fe_sub(const Fe& a, const Fe& b) { return fe_add(a, fe_p_minus(b)); }

Fe fe_neg(const Fe& a) { return fe_p_minus(a); }

Fe fe_mul(const Fe& a, const Fe& b) {
    std::uint64_t t[16] = {};
    for (int i = 0; i < 8; ++i) {
        std::uint64_t carry = 0;
        for (int j = 0; j < 8; ++j) {
            const std::uint64_t cur =
                (std::uint64_t)a.v[i] * b.v[j] + t[i + j] + carry;
            t[i + j] = cur & 0xffffffff;
            carry = cur >> 32;
        }
        t[i + 8] = carry;
    }
    std::uint64_t acc[24] = {};
    for (int i = 0; i < 16; ++i) acc[i] = t[i];
    return big_norm(acc);
}

Fe fe_sqr(const Fe& a) { return fe_mul(a, a); }

// 平方乘，MSB→LSB（r=1 起步，首次平方/乘无效但无害）
Fe fe_pow(const Fe& a, const Fe& e) {
    Fe r = fe_one();
    for (int i = 7; i >= 0; --i) {
        for (int bit = 31; bit >= 0; --bit) {
            r = fe_sqr(r);
            if ((e.v[i] >> bit) & 1u) r = fe_mul(r, a);
        }
    }
    return r;
}

Fe fe_inv(const Fe& a) { return fe_pow(a, kPMinus2); }

Fe fe_from_bytes_be(const unsigned char in[32]) {
    Fe r{};
    for (int limb = 0; limb < 8; ++limb) {
        const int off = 28 - limb * 4;  // 大端：高位字节在前，limb7 是最高
        r.v[limb] = ((std::uint32_t)in[off] << 24) | ((std::uint32_t)in[off + 1] << 16) |
                    ((std::uint32_t)in[off + 2] << 8) | (std::uint32_t)in[off + 3];
    }
    return r;
}

void fe_to_bytes_be(unsigned char out[32], const Fe& a) {
    for (int limb = 0; limb < 8; ++limb) {
        const int off = 28 - limb * 4;
        out[off] = (unsigned char)(a.v[limb] >> 24);
        out[off + 1] = (unsigned char)(a.v[limb] >> 16);
        out[off + 2] = (unsigned char)(a.v[limb] >> 8);
        out[off + 3] = (unsigned char)a.v[limb];
    }
}

// ---- Jacobian 点算术（a = -3 专用加倍；Z=0 = 无穷远）----

struct Jac {
    Fe X, Y, Z;
};

Jac pt_infinity() {
    Jac r;
    r.X = fe_one();
    r.Y = fe_one();
    r.Z = fe_zero();
    return r;
}

Jac pt_affine(const Fe& x, const Fe& y) {
    Jac r;
    r.X = x;
    r.Y = y;
    r.Z = fe_one();
    return r;
}

bool pt_is_infinity(const Jac& p) { return fe_is_zero(p.Z); }

// 点取负：(X, Y, Z) → (X, -Y, Z)（Y 坐标取域负——仿射 (x, y) → (x, p-y)）。
// 注意标量取负必须模群阶 n 而非域素数 p，不能拿 fe_neg 当标量负用。
Jac pt_neg(const Jac& p) {
    Jac r;
    r.X = p.X;
    r.Y = fe_neg(p.Y);
    r.Z = p.Z;
    return r;
}

Jac pt_double(const Jac& p) {
    if (pt_is_infinity(p) || fe_is_zero(p.Y)) return pt_infinity();
    const Fe delta = fe_sqr(p.Z);
    const Fe gamma = fe_sqr(p.Y);
    const Fe beta = fe_mul(p.X, gamma);
    const Fe x_m_d = fe_sub(p.X, delta);
    const Fe x_p_d = fe_add(p.X, delta);
    // alpha = 3*(X-delta)*(X+delta)
    const Fe alpha = fe_mul(x_m_d, fe_add(x_p_d, fe_add(x_p_d, x_p_d)));
    const Fe beta4 = fe_add(fe_add(beta, beta), fe_add(beta, beta));
    const Fe x3 = fe_sub(fe_sqr(alpha), fe_add(beta4, beta4));
    const Fe z3 = fe_sub(fe_sub(fe_sqr(fe_add(p.Y, p.Z)), gamma), delta);
    const Fe g2 = fe_sqr(gamma);
    const Fe g4 = fe_add(fe_add(g2, g2), fe_add(g2, g2));  // 4*gamma^2
    const Fe g8 = fe_add(g4, g4);                          // 8*gamma^2（EFD dbl-2001-b）
    const Fe y3 = fe_sub(fe_mul(alpha, fe_sub(beta4, x3)), g8);
    return {x3, y3, z3};
}

Jac pt_add(const Jac& p, const Jac& q) {
    if (pt_is_infinity(p)) return q;
    if (pt_is_infinity(q)) return p;
    const Fe z1z1 = fe_sqr(p.Z);
    const Fe z2z2 = fe_sqr(q.Z);
    const Fe u1 = fe_mul(p.X, z2z2);
    const Fe u2 = fe_mul(q.X, z1z1);
    const Fe s1 = fe_mul(fe_mul(p.Y, q.Z), z2z2);
    const Fe s2 = fe_mul(fe_mul(q.Y, p.Z), z1z1);
    const Fe h = fe_sub(u2, u1);
    const Fe r = fe_sub(s2, s1);
    if (fe_is_zero(h)) {
        if (fe_is_zero(r)) return pt_double(p);
        return pt_infinity();
    }
    const Fe hh = fe_sqr(h);
    const Fe hhh = fe_mul(h, hh);
    const Fe v = fe_mul(u1, hh);
    const Fe x3 = fe_sub(fe_sub(fe_sqr(r), hhh), fe_add(v, v));
    const Fe y3 = fe_sub(fe_mul(r, fe_sub(v, x3)), fe_mul(s1, hhh));
    const Fe z3 = fe_mul(fe_mul(p.Z, q.Z), h);
    return {x3, y3, z3};
}

// 标量乘 MSB→LSB（本协议无侧信道要求，非恒时）
Jac pt_mul_scalar(const Fe& k, const Jac& p) {
    Jac r = pt_infinity();
    for (int i = 7; i >= 0; --i) {
        for (int bit = 31; bit >= 0; --bit) {
            r = pt_double(r);
            if ((k.v[i] >> bit) & 1u) r = pt_add(r, p);
        }
    }
    return r;
}

void pt_to_uncompressed(unsigned char out[65], const Jac& p) {
    out[0] = 0x04;
    const Fe zinv = fe_inv(p.Z);
    const Fe zinv2 = fe_sqr(zinv);
    fe_to_bytes_be(out + 1, fe_mul(p.X, zinv2));
    fe_to_bytes_be(out + 33, fe_mul(p.Y, fe_mul(zinv2, zinv)));
}

// on-curve：y² == x³ - 3x + b
bool fe_on_curve(const Fe& x, const Fe& y) {
    const Fe x2 = fe_sqr(x);
    const Fe x3 = fe_mul(x2, x);
    const Fe rhs = fe_add(fe_sub(x3, fe_add(fe_add(x, x), x)), kCurveB);
    return fe_eq(fe_sqr(y), rhs);
}

// 解析未压缩点（0x04 前缀 + on-curve + 非无穷远）
Jac pt_from_uncompressed(const std::string& enc, const char* what) {
    if (enc.size() != 65 || (unsigned char)enc[0] != 0x04)
        throw std::invalid_argument(std::string(what) +
                                    " must be uncompressed SEC1 (65B, 0x04)");
    const unsigned char* p = reinterpret_cast<const unsigned char*>(enc.data());
    const Fe x = fe_from_bytes_be(p + 1);
    const Fe y = fe_from_bytes_be(p + 33);
    if (!fe_on_curve(x, y)) throw std::invalid_argument(std::string(what) + " not on curve");
    return pt_affine(x, y);
}

Fe scalar_from_bytes(const std::string& s, const char* what) {
    if (s.size() != 32) throw std::invalid_argument(std::string(what) + " must be 32 bytes");
    return fe_from_bytes_be(reinterpret_cast<const unsigned char*>(s.data()));
}

std::string point_to_str(const Jac& p) {
    unsigned char buf[65];
    pt_to_uncompressed(buf, p);
    return std::string(reinterpret_cast<const char*>(buf), 65);
}

std::string scalar_to_str(const Fe& f) {
    unsigned char buf[32];
    fe_to_bytes_be(buf, f);
    return std::string(reinterpret_cast<const char*>(buf), 32);
}

// TT 段：8B LE 长度前缀 || 数据（RFC 9383 §3.3）
void tt_append(std::string& tt, const std::string& data) {
    const std::uint64_t n = data.size();
    for (int i = 0; i < 8; ++i) tt += (char)((n >> (8 * i)) & 0xff);
    tt += data;
}

// 40B 大端大数 mod p：V = hi(前 8B)·2^256 + lo(后 32B) → hi·(2^256 mod p)+lo
Fe scalar40_mod_p(const unsigned char raw[40]) {
    Fe hi{};
    for (int i = 0; i < 2; ++i)  // 前 8B = 2 个 u32 limb（高字在前）
        hi.v[1 - i] = ((std::uint32_t)raw[i * 4] << 24) | ((std::uint32_t)raw[i * 4 + 1] << 16) |
                      ((std::uint32_t)raw[i * 4 + 2] << 8) | (std::uint32_t)raw[i * 4 + 3];
    const Fe lo = fe_from_bytes_be(raw + 8);
    return fe_add(fe_mul(hi, k2p256ModP), lo);
}

Spake2pSecrets secrets_from_tt(const std::string& tt, const std::string& share_p,
                               const std::string& share_v) {
    Sha256HasherStd h;
    h.update(tt);
    const std::string k_main = h.digest_raw();
    const std::string prk = hkdf_sha256_extract("", k_main);
    const std::string k_confirm = hkdf_sha256_expand(prk, "ConfirmationKeys", 64);
    Spake2pSecrets s;
    s.k_shared = hkdf_sha256_expand(prk, "SharedKey", 32);
    s.confirm_prover = hmac_sha256(k_confirm.substr(0, 32), share_v);
    s.confirm_verifier = hmac_sha256(k_confirm.substr(32, 32), share_p);
    return s;
}

// 组装 TT（RFC 9383 §3.3 顺序）——shareP 在前 shareV 在后
std::string build_tt(const Spake2pParams& params, const std::string& share_p,
                     const std::string& share_v, const std::string& z_enc,
                     const std::string& v_enc, const Fe& w0) {
    std::string tt;
    tt_append(tt, params.context);
    tt_append(tt, params.id_prover);
    tt_append(tt, params.id_verifier);
    tt_append(tt, point_to_str(pt_affine(kMx, kMy)));
    tt_append(tt, point_to_str(pt_affine(kNx, kNy)));
    tt_append(tt, share_p);
    tt_append(tt, share_v);
    tt_append(tt, z_enc);
    tt_append(tt, v_enc);
    tt_append(tt, scalar_to_str(w0));
    return tt;
}

}  // namespace

void spake2p_derive_w0_w1(const std::string& password, const std::string& id_prover,
                          const std::string& id_verifier, std::string& w0_out,
                          std::string& w1_out) {
    // PBKDF 输入串与 TT 同风格：8B LE 长度前缀拼接（PBKDF 编码不进
    // ciphersuite，与协议互操作无关）
    std::string msg;
    tt_append(msg, password);
    tt_append(msg, id_prover);
    tt_append(msg, id_verifier);
    // 占位 PBKDF：HMAC-SHA256 迭代展开 96B 取前 80B = w0s(40) || w1s(40)
    const std::string key = "unidict/sync/pbkdf/v1";
    std::string okm;
    std::string t;
    for (unsigned char counter = 1; okm.size() < 80; ++counter) {
        t = hmac_sha256(key, t + msg + std::string(1, (char)counter));
        okm += t;
    }
    const unsigned char* raw = reinterpret_cast<const unsigned char*>(okm.data());
    w0_out = scalar_to_str(scalar40_mod_p(raw));
    w1_out = scalar_to_str(scalar40_mod_p(raw + 40));
}

std::string spake2p_registration_record(const std::string& w1) {
    const Fe s = scalar_from_bytes(w1, "w1");
    return point_to_str(pt_mul_scalar(s, pt_affine(kGx, kGy)));
}

std::string spake2p_random_scalar() {
    for (;;) {
        const std::string r = random_bytes(32);
        const Fe f = fe_from_bytes_be(reinterpret_cast<const unsigned char*>(r.data()));
        if (!fe_ge(f, kP)) return r;  // [0, p)
    }
}

std::string spake2p_prover_share(const std::string& w0, const std::string& x_scalar) {
    const Jac M = pt_affine(kMx, kMy);
    return point_to_str(
        pt_add(pt_mul_scalar(scalar_from_bytes(x_scalar, "x"), pt_affine(kGx, kGy)),
               pt_mul_scalar(scalar_from_bytes(w0, "w0"), M)));
}

std::string spake2p_verifier_share(const std::string& w0, const std::string& y_scalar) {
    const Jac N = pt_affine(kNx, kNy);
    return point_to_str(
        pt_add(pt_mul_scalar(scalar_from_bytes(y_scalar, "y"), pt_affine(kGx, kGy)),
               pt_mul_scalar(scalar_from_bytes(w0, "w0"), N)));
}

Spake2pSecrets spake2p_prover_finish(const Spake2pParams& params, const std::string& w0,
                                     const std::string& w1, const std::string& x_scalar,
                                     const std::string& share_v) {
    const Jac Y = pt_from_uncompressed(share_v, "shareV");
    const Fe w0f = scalar_from_bytes(w0, "w0");
    const Fe w1f = scalar_from_bytes(w1, "w1");
    const Fe xf = scalar_from_bytes(x_scalar, "x");
    // T = Y - w0*N；Z = x*T；V = w1*T（§3.3 Prover 公式；点取负实现减法）
    const Jac T = pt_add(Y, pt_neg(pt_mul_scalar(w0f, pt_affine(kNx, kNy))));
    const std::string share_p = spake2p_prover_share(w0, x_scalar);  // X 重算进 TT
    const std::string z_enc = point_to_str(pt_mul_scalar(xf, T));
    const std::string v_enc = point_to_str(pt_mul_scalar(w1f, T));
    return secrets_from_tt(build_tt(params, share_p, share_v, z_enc, v_enc, w0f), share_p,
                           share_v);
}

Spake2pSecrets spake2p_verifier_finish(const Spake2pParams& params, const std::string& w0,
                                       const std::string& y_scalar,
                                       const std::string& share_p,
                                       const std::string& registration_l) {
    const Jac X = pt_from_uncompressed(share_p, "shareP");
    const Jac L = pt_from_uncompressed(registration_l, "registration L");
    const Fe w0f = scalar_from_bytes(w0, "w0");
    const Fe yf = scalar_from_bytes(y_scalar, "y");
    // T = X - w0*M；Z = y*T；V = y*L（§3.3 Verifier 公式；点取负实现减法）
    const Jac T = pt_add(X, pt_neg(pt_mul_scalar(w0f, pt_affine(kMx, kMy))));
    const std::string share_v = spake2p_verifier_share(w0, y_scalar);  // Y 重算进 TT
    const std::string z_enc = point_to_str(pt_mul_scalar(yf, T));
    const std::string v_enc = point_to_str(pt_mul_scalar(yf, L));
    return secrets_from_tt(build_tt(params, share_p, share_v, z_enc, v_enc, w0f), share_p,
                           share_v);
}

// ---- B3-b 换钥信封 ----

namespace {
const char kGroupKeyAad[] = "unidict/sync/groupkey/v1";
constexpr std::size_t kGroupKeyLen = 32;
}  // namespace

std::string seal_group_key_envelope(const std::string& k_shared, std::uint32_t version,
                                    const std::string& group_key) {
    if (k_shared.size() != kGroupKeyLen)
        throw std::invalid_argument("k_shared must be 32 bytes");
    if (group_key.size() != kGroupKeyLen)
        throw std::invalid_argument("group key must be 32 bytes");
    const std::string aead_key = hkdf_sha256_expand(k_shared, kGroupKeyAad, 32);
    std::string pt(4, '\0');
    for (int i = 0; i < 4; ++i) pt[i] = (char)((version >> (8 * i)) & 0xff);
    pt += group_key;
    return aead_xchacha20poly1305_seal(aead_key, pt, kGroupKeyAad);
}

bool open_group_key_envelope(const std::string& sealed, const std::string& k_shared,
                             std::uint32_t& version_out, std::string& group_key_out) {
    if (k_shared.size() != kGroupKeyLen)
        throw std::invalid_argument("k_shared must be 32 bytes");
    const std::string aead_key = hkdf_sha256_expand(k_shared, kGroupKeyAad, 32);
    std::string pt;
    if (!aead_xchacha20poly1305_open(sealed, aead_key, kGroupKeyAad, pt)) return false;
    if (pt.size() != 4 + kGroupKeyLen) return false;  // 形状不对 = 拒
    version_out = (std::uint32_t)(unsigned char)pt[0] |
                  ((std::uint32_t)(unsigned char)pt[1] << 8) |
                  ((std::uint32_t)(unsigned char)pt[2] << 16) |
                  ((std::uint32_t)(unsigned char)pt[3] << 24);
    group_key_out.assign(pt, 4, kGroupKeyLen);
    return true;
}

}  // namespace UnidictCoreStd
