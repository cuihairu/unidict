// B3-b SPAKE2+（RFC 9383）权威向量钉死。P-256 域/点算术 + 协议全链
// 不靠自觉：RFC 9383 附录 C 第一组（P-256-SHA256-HKDF-SHA256-HMAC-SHA256）
// 的注册记录 L、两侧 share、K_shared 与确认标签全部逐字节对表，向量
// 对不上这里就红。PBKDF 派生与换钥信封是本仓编排面（不进 ciphersuite），
// 测语义性质与失败矩阵。
#include <stdexcept>

#include <cassert>
#include <cstdio>
#include <string>

#include "std/crypto_std.h"
#include "std/sha256_std.h"
#include "std/spake2_std.h"

using namespace UnidictCoreStd;

namespace {

std::string unhex(const std::string& hex) {
    auto nib = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return (unsigned)(c - '0');
        if (c >= 'a' && c <= 'f') return (unsigned)(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return (unsigned)(c - 'A' + 10);
        assert(false && "bad hex input");
        return 0;
    };
    assert(hex.size() % 2 == 0);
    std::string out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2)
        out += (char)((nib(hex[i]) << 4) | nib(hex[i + 1]));
    return out;
}

template <typename Fn>
bool throws_invalid_argument(Fn&& fn) {
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
    }
    return false;
}

}  // namespace

// 白盒：整个实现 TU 嵌进独立命名空间再包含一份，符号全部本地化，不
// 与 lib 冲突；内部引用的 lib 符号（hmac/hkdf/random_bytes/SHA）经
// using 落到全局 UnidictCoreStd。放在文件作用域——命名空间不能进函数。
namespace wb {
using namespace UnidictCoreStd;
#include "std/spake2_std.cpp"
}

int main() {
    // ---- RFC 9383 附录 C 第一组：全链权威向量 ----
    {
        Spake2pParams params;
        params.context = "SPAKE2+-P256-SHA256-HKDF-SHA256-HMAC-SHA256 Test Vectors";
        params.id_prover = "client";
        params.id_verifier = "server";
        const std::string w0 = unhex(
            "bb8e1bbcf3c48f62c08db243652ae55d3e5586053fca77102994f23ad95491b3");
        const std::string w1 = unhex(
            "7e945f34d78785b8a3ef44d0df5a1a97d6b3b460409a345ca7830387a74b1dba");
        const std::string x = unhex(
            "d1232c8e8693d02368976c174e2088851b8365d0d79a9eee709c6a05a2fad539");
        const std::string y = unhex(
            "717a72348a182085109c8d3917d6c43d59b224dc6a7fc4f0483232fa6516d8b3");
        const std::string l =
            unhex("04eb7c9db3d9a9eb1f8adab81b5794c1f13ae3e225efbe91ea487425854c7fc00f"
                  "00bfedcbd09b2400142d40a14f2064ef31dfaa903b91d1faea7093d835966efd");
        const std::string share_p =
            unhex("04ef3bd051bf78a2234ec0df197f7828060fe9856503579bb1733009042c15c0c1"
                  "de127727f418b5966afadfdd95a6e4591d171056b333dab97a79c7193e341727");
        const std::string share_v =
            unhex("04c0f65da0d11927bdf5d560c69e1d7d939a05b0e88291887d679fcadea75810f"
                  "b5cc1ca7494db39e82ff2f50665255d76173e09986ab46742c798a9a68437b048");
        const std::string k_shared = unhex(
            "0c5f8ccd1413423a54f6c1fb26ff01534a87f893779c6e68666d772bfd91f3e7");
        const std::string confirm_p = unhex(
            "926cc713504b9b4d76c9162ded04b5493e89109f6d89462cd33adc46fda27527");
        const std::string confirm_v = unhex(
            "9747bcc4f8fe9f63defee53ac9b07876d907d55047e6ff2def2e7529089d3e68");

        // 注册记录 L = w1*P
        assert(spake2p_registration_record(w1) == l);
        // share 阶段
        assert(spake2p_prover_share(w0, x) == share_p);
        assert(spake2p_verifier_share(w0, y) == share_v);

        // finish 全链：K_shared + 双确认标签（TT 拼装/KDF/HMAC 都被钉死）
        const Spake2pSecrets sp = spake2p_prover_finish(params, w0, w1, x, share_v);
        const Spake2pSecrets sv =
            spake2p_verifier_finish(params, w0, y, share_p, l);
        assert(sp.k_shared == k_shared);
        assert(sv.k_shared == k_shared);
        assert(sp.confirm_prover == confirm_p);
        assert(sp.confirm_verifier == confirm_v);
        assert(sv.confirm_prover == confirm_p);
        assert(sv.confirm_verifier == confirm_v);
        // 交叉验收语义：Prover 收 confirmV、Verifier 收 confirmP
        assert(sv.confirm_verifier == sp.confirm_verifier);
        assert(sp.confirm_prover == sv.confirm_prover);
    }

    // ---- w0/w1 派生：确定性、身份绑定、模 p 范围 ----
    {
        std::string w0, w1, w0b, w1b;
        spake2p_derive_w0_w1("ABCD-1234", "client", "server", w0, w1);
        spake2p_derive_w0_w1("ABCD-1234", "client", "server", w0b, w1b);
        assert(w0 == w0b && w1 == w1b);              // 同输入同输出
        assert(w0 != w1);                            // 两半独立
        assert(w0.size() == 32 && w1.size() == 32);  // 32B 大端
        std::string w0c, w1c;
        spake2p_derive_w0_w1("ABCD-1234", "client", "other", w0c, w1c);
        assert(w0c != w0);  // 身份进派生（unknown key-share 防线）
        std::string w0d, w1d;
        spake2p_derive_w0_w1("ABCD-1235", "client", "server", w0d, w1d);
        assert(w0d != w0 && w1d != w1);  // 口令进派生
        // 派生出的 w 可直接跑协议（registration 不抛 = 标量域合法）
        assert(!spake2p_registration_record(w1).empty());
    }

    // ---- 随机标量与非法参数面 ----
    {
        const std::string s1 = spake2p_random_scalar();
        const std::string s2 = spake2p_random_scalar();
        assert(s1.size() == 32);
        assert(s1 != s2);
        assert(throws_invalid_argument([] { (void)spake2p_prover_share("31b", std::string(32, '\1')); }));
        assert(throws_invalid_argument([] { (void)spake2p_prover_share(std::string(32, '\1'), "short"); }));
        assert(throws_invalid_argument([] { (void)spake2p_registration_record("nope"); }));

        Spake2pParams params;
        params.context = "ctx";
        params.id_prover = "p";
        params.id_verifier = "v";
        const std::string w = std::string(32, '\x11');
        const std::string bad_share = std::string(65, '\x00');  // 非 0x04 前缀
        assert(throws_invalid_argument([&] {
            (void)spake2p_prover_finish(params, w, w, std::string(32, '\x22'), bad_share);
        }));
        const std::string on_curve_bad =
            unhex("0400000000000000000000000000000000000000000000000000000000000000"
                  "0000000000000000000000000000000000000000000000000000000000000001");
        // 形状合法但不在曲线上 → 拒
        assert(throws_invalid_argument([&] {
            (void)spake2p_verifier_finish(params, w, std::string(32, '\x33'), on_curve_bad,
                                          std::string(65, '\x04'));
        }));
        assert(throws_invalid_argument([&] {
            (void)spake2p_prover_finish(params, w, w, "31byte_scalar", std::string(65, '\x04'));
        }));
    }

    // ---- share 篡改 → K_shared 必变（身份与共享秘密绑定）----
    {
        Spake2pParams params;
        params.context = "unidict/sync/pairing/v1";
        params.id_prover = "dev-a";
        params.id_verifier = "dev-b";
        std::string w0, w1;
        spake2p_derive_w0_w1("PAIR-7777", "dev-a", "dev-b", w0, w1);
        const std::string x = spake2p_random_scalar();
        const std::string y = spake2p_random_scalar();
        const std::string sp = spake2p_prover_share(w0, x);
        const std::string sv = spake2p_verifier_share(w0, y);
        const Spake2pSecrets a = spake2p_prover_finish(params, w0, w1, x, sv);
        const Spake2pSecrets b = spake2p_verifier_finish(params, w0, y, sp,
                                                         spake2p_registration_record(w1));
        assert(a.k_shared == b.k_shared);

        // 对端 share 被改一位 → 几乎必然落曲线外，按 §3.3 群成员校验
        // 在解析处拒绝（invalid_argument）
        std::string sv_t = sv;
        sv_t[10] ^= 0x01;
        assert(throws_invalid_argument(
            [&] { (void)spake2p_prover_finish(params, w0, w1, x, sv_t); }));

        // 换一条合法 share（不同 y）→ K_shared 与确认标签都不同——
        // 共享秘密与对端 share 逐字节绑定
        const std::string y2 = spake2p_random_scalar();
        const std::string sv2 = spake2p_verifier_share(w0, y2);
        const Spake2pSecrets a2 = spake2p_prover_finish(params, w0, w1, x, sv2);
        assert(a2.k_shared != a.k_shared);
        assert(a2.confirm_verifier != a.confirm_verifier);
    }

    // ---- 换钥信封：往返 / 篡改拒 / 错钥拒 / 形状拒 ----
    {
        std::string w0, w1;
        spake2p_derive_w0_w1("PAIR-9999", "p", "v", w0, w1);
        const std::string x = spake2p_random_scalar();
        const std::string y = spake2p_random_scalar();
        Spake2pParams params;
        params.context = "unidict/sync/pairing/v1";
        params.id_prover = "p";
        params.id_verifier = "v";
        const Spake2pSecrets a =
            spake2p_prover_finish(params, w0, w1, x, spake2p_verifier_share(w0, y));
        const Spake2pSecrets b = spake2p_verifier_finish(
            params, w0, y, spake2p_prover_share(w0, x), spake2p_registration_record(w1));
        assert(a.k_shared == b.k_shared);

        const std::string group_key(32, '\x5a');
        const std::string sealed =
            seal_group_key_envelope(a.k_shared, 3, group_key);
        std::uint32_t ver = 0;
        std::string got;
        assert(open_group_key_envelope(sealed, b.k_shared, ver, got));  // 对端开
        assert(ver == 3);
        assert(got == group_key);

        // 密文篡改 → false 且出参不动
        std::string tampered = sealed;
        tampered[30] ^= 0x40;
        ver = 0;
        got = "sentinel";
        assert(!open_group_key_envelope(tampered, b.k_shared, ver, got));
        assert(ver == 0 && got == "sentinel");
        // 错 k_shared → false（派生钥不同）
        assert(!open_group_key_envelope(sealed, random_bytes(32), ver, got));
        // 形状拒（截断到 AEAD 下限以下）
        assert(!open_group_key_envelope(sealed.substr(0, 20), b.k_shared, ver, got));
        // 非法参数
        assert(throws_invalid_argument(
            [&] { (void)seal_group_key_envelope("short", 1, group_key); }));
        assert(throws_invalid_argument(
            [&] { (void)seal_group_key_envelope(a.k_shared, 1, "short"); }));
        assert(throws_invalid_argument([&] {
            (void)open_group_key_envelope(sealed, "short", ver, got);
        }));
        // 两次密封不同（随机 nonce）
        assert(seal_group_key_envelope(a.k_shared, 3, group_key) != sealed);
    }

    // ---- 白盒：域/点算术代数恒等式（公开 API 够不着的内部分支）----
    {
        using Fe = wb::UnidictCoreStd::Fe;

        auto fe_from_hex = [](const char* h) {
            Fe f{};
            for (int i = 0; i < 8; ++i) {
                std::uint32_t w = 0;
                for (int j = 0; j < 8; ++j) {
                    const char c = h[56 - i * 8 + j];
                    const unsigned n =
                        (c <= '9') ? (unsigned)(c - '0') : (unsigned)(c - 'a' + 10);
                    w = (w << 4) | n;
                }
                f.v[i] = w;
            }
            return f;
        };
        auto fe_to_hex = [](const Fe& f) {
            std::string s;
            s.reserve(64);
            char buf[9];
            for (int i = 7; i >= 0; --i) {
                std::snprintf(buf, sizeof buf, "%08x", f.v[i]);
                s += buf;
            }
            return s;
        };
        const char* pm1 = "ffffffff00000001000000000000000000000000fffffffffffffffffffffffe";
        // (p-1) + 1 = 0：命中 fe_ge 全等路径 + fe_reduce_once 实际减 p 分支
        assert(fe_to_hex(wb::UnidictCoreStd::fe_add(fe_from_hex(pm1), fe_from_hex(
                                   "0000000000000000000000000000000000000000000000000000000000000001"))) ==
               "0000000000000000000000000000000000000000000000000000000000000000");
        // (p-1) + (p-1) = p-2：和 ≥ 2^256 走折叠，落回 [p, 2p) 再条件减
        assert(fe_to_hex(wb::UnidictCoreStd::fe_add(fe_from_hex(pm1), fe_from_hex(pm1))) ==
               "ffffffff00000001000000000000000000000000fffffffffffffffffffffffd");
        // (p-1)² = 1 mod p：费马小定理（fe_pow/fe_inv 主链的对偶检查）
        assert(fe_to_hex(wb::UnidictCoreStd::fe_mul(fe_from_hex(pm1), fe_from_hex(pm1))) ==
               "0000000000000000000000000000000000000000000000000000000000000001");
        assert(fe_to_hex(wb::UnidictCoreStd::fe_inv(fe_from_hex(pm1))) == pm1);  // (-1)^(-1) = -1

        // 点：P + P 与 pt_double 一致（pt_add 等点分支），P + (-P) = 无穷远
        namespace wbc = wb::UnidictCoreStd;
        const wbc::Jac g = wbc::pt_affine(wbc::kGx, wbc::kGy);
        unsigned char lhs[65], rhs[65];
        wbc::pt_to_uncompressed(lhs, wbc::pt_add(g, g));
        wbc::pt_to_uncompressed(rhs, wbc::pt_double(g));
        assert(std::string(reinterpret_cast<const char*>(lhs), 65) ==
               std::string(reinterpret_cast<const char*>(rhs), 65));
        assert(wbc::pt_is_infinity(wbc::pt_add(g, wbc::pt_neg(g))));
        // 标量 0：点乘回到无穷远（pt_mul_scalar 总函数性）
        assert(wbc::pt_is_infinity(wbc::pt_mul_scalar(wbc::fe_zero(), g)));
    }

    std::puts("spake2_std_test: all assertions passed");
    return 0;
}
