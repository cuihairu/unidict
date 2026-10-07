// B3-a 密码学原语的权威向量钉死。自实现的正确性不靠自觉——向量来自
// RFC/draft 原文，对不上这里就红：
// - AEAD RFC 8439 形态：RFC 8439 §2.8.2（key 80..9f，nonce 07 00 00 00 || 40..47）
// - Poly1305 原语：RFC 8439 §2.5.2（34 字节消息，走尾块缓冲分支）
// - XChaCha 形态：draft-irtf-cfrg-xchacha-03 §A.3.1（HChaCha20 子钥派生
//   在 §2.2.1，经本向量间接钉死）
// - HKDF：RFC 5869 Test Case 1 / Test Case 3
// - PBKDF2-HMAC-SHA256：RFC 7914 §11（备份自救口口令拉伸）
// - HMAC：RFC 4231 Test Case 1（短钥）/ Test Case 6（131 字节长钥）
#include <stdexcept>

#include <cassert>
#include <cstdio>
#include <string>

#include "std/crypto_std.h"

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

std::string hex(const std::string& raw) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(raw.size() * 2);
    for (unsigned char b : raw) {
        out += kHex[b >> 4];
        out += kHex[b & 0x0f];
    }
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

// RFC 8439 §2.8.2 / draft §A.3.1 共用的 sunscreen 明文（114 字节）
const char* kSunscreenHex =
    "4c616469657320616e642047656e746c656d656e206f662074686520636c6173"
    "73206f66202739393a204966204920636f756c64206f6666657220796f75206f"
    "6e6c79206f6e652074697020666f7220746865206675747572652c2073756e73"
    "637265656e20776f756c642062652069742e";

}  // namespace

int main() {
    // ---- RFC 8439 §2.8.2：AEAD chacha 形态确定性向量 ----
    {
        const std::string key = unhex(
            "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
        // RFC 8439 §2.8.2 的 96-bit nonce = constant(07 00 00 00) || iv(40..47)
        const std::string nonce12 = unhex("070000004041424344454647");
        const std::string aad = unhex("50515253c0c1c2c3c4c5c6c7");
        const std::string pt = unhex(kSunscreenHex);
        const std::string expect =
            unhex("d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d6"
                  "3dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b36"
                  "92ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc"
                  "3ff4def08e4b7a9de576d26586cec64b6116") +
            unhex("1ae10b594f09e26a7e902ecbd0600691");
        assert(aead_chacha20poly1305_seal(key, nonce12, pt, aad) == expect);

        std::string got;
        assert(aead_chacha20poly1305_open(expect, key, nonce12, aad, got));
        assert(got == pt);
    }

    // ---- RFC 8439 §2.5.2：Poly1305 原语直接向量 ----
    // 34 字节消息非 16 倍数——AEAD 面的 tag 输入恒为整块序列，只有
    // 这条路能钉死 update 缓冲 + finish 尾块分支
    {
        const std::string key = unhex(
            "85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b");
        assert(hex(poly1305_tag(key, "Cryptographic Forum Research Group")) ==
               "a8061dc1305136c6c22b8baf0c0127a9");
        assert(throws_invalid_argument([] { (void)poly1305_tag("short", "x"); }));
    }

    // ---- draft-irtf-cfrg-xchacha-03 §A.3.1：XChaCha 显式 nonce 向量 ----
    // （内部经 HChaCha20 §2.2.1 派生子钥，一并钉死）
    {
        const std::string key = unhex(
            "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
        const std::string nonce24 = unhex("404142434445464748494a4b4c4d4e4f5051525354555657");
        const std::string aad = unhex("50515253c0c1c2c3c4c5c6c7");
        const std::string pt = unhex(kSunscreenHex);
        const std::string expect =
            unhex("bd6d179d3e83d43b9576579493c0e939572a1700252bfaccbed2902c21396cbb"
                  "731c7f1b0b4aa6440bf3a82f4eda7e39ae64c6708c54c216cb96b72e1213b452"
                  "2f8c9ba40db5d945b11b69b982c1bb9e3f3fac2bc369488f76b2383565d3fff9"
                  "21f9664c97637da9768812f615c68b13b52e") +
            unhex("c0875924c1c7987947deafd8780acf49");
        assert(xchacha20poly1305_seal_with_nonce(key, nonce24, pt, aad) == expect);

        std::string got;
        assert(xchacha20poly1305_open_with_nonce(expect, key, nonce24, aad, got));
        assert(got == pt);
    }

    // ---- RFC 5869 TC1 / TC3：extract 出 PRK + expand 出 OKM ----
    {
        const std::string ikm(22, '\x0b');
        // TC1：带 salt/info
        {
            const std::string salt = unhex("000102030405060708090a0b0c");
            const std::string info = unhex("f0f1f2f3f4f5f6f7f8f9");
            const std::string prk = hkdf_sha256_extract(salt, ikm);
            assert(hex(prk) ==
                   "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5");
            assert(hex(hkdf_sha256_expand(prk, info, 42)) ==
                   "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
                   "34007208d5b887185865");
        }
        // TC3：空 salt/info（缺省形态）
        {
            const std::string prk = hkdf_sha256_extract("", ikm);
            assert(hex(prk) ==
                   "19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04");
            assert(hex(hkdf_sha256_expand(prk, "", 42)) ==
                   "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d"
                   "9d201395faa4b61a96c8");
        }
        // expand 超协议上限（255*32）拒绝
        assert(throws_invalid_argument(
            [] { (void)hkdf_sha256_expand(std::string(32, '\0'), "", 255u * 32u + 1); }));
    }

    // ---- RFC 7914 §11：PBKDF2-HMAC-SHA256 向量（scrypt 规范引用的
    //      权威口径；c=80000 一例顺带量级压测）----
    {
        assert(hex(pbkdf2_hmac_sha256("passwd", "salt", 1, 64)) ==
               "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
               "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
        assert(hex(pbkdf2_hmac_sha256("Password", "NaCl", 80000, 64)) ==
               "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56"
               "a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
        // 非整块 dk_len：跨块截断分支（33 字节 = 1 整块 + 1 字节）
        assert(pbkdf2_hmac_sha256("passwd", "salt", 1, 33) ==
               unhex("55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
                     "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783")
                   .substr(0, 33));
        assert(pbkdf2_hmac_sha256("passwd", "salt", 1, 33).size() == 33);
        // iterations=0 按 1 轮处理；dk_len 滥用上限拒绝
        assert(pbkdf2_hmac_sha256("passwd", "salt", 0, 64) ==
               pbkdf2_hmac_sha256("passwd", "salt", 1, 64));
        assert(throws_invalid_argument(
            [] { (void)pbkdf2_hmac_sha256("p", "s", 1, 4096u * 32u + 1); }));
    }

    // ---- RFC 4231 TC1（短钥）/ TC6（131 字节长钥）----
    {
        assert(hex(hmac_sha256(std::string(20, '\x0b'), "Hi There")) ==
               "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
        assert(hex(hmac_sha256(std::string(131, '\xaa'),
                               "Test Using Larger Than Block-Size Key - Hash Key First")) ==
               "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    }

    // ---- 随机 nonce 封装：往返 / 篡改拒 / 错 aad 拒 / 截断拒 ----
    {
        const std::string key = random_bytes(kAeadKeyLen);
        const std::string pt = "unidict sync command payload \x01\x02\x03";
        const std::string aad = "ctx-a";

        std::string sealed = aead_xchacha20poly1305_seal(key, pt, aad);
        assert(sealed.size() == kXchaChaNonceLen + pt.size() + kAeadTagLen);
        std::string got;
        assert(aead_xchacha20poly1305_open(sealed, key, aad, got));
        assert(got == pt);
        // 两次密封 nonce 独立（同文不同密文，nonce 不复用）
        assert(aead_xchacha20poly1305_seal(key, pt, aad) != sealed);

        // 密文位篡改 → false，且出参不动
        sealed[kXchaChaNonceLen + 1] ^= 0x40;
        got = "sentinel";
        assert(!aead_xchacha20poly1305_open(sealed, key, aad, got));
        assert(got == "sentinel");
        sealed[kXchaChaNonceLen + 1] ^= 0x40;
        // tag 位篡改 → false
        sealed.back() ^= 0x01;
        assert(!aead_xchacha20poly1305_open(sealed, key, aad, got));
        // aad 不对 → false（上下文绑定生效）
        assert(!aead_xchacha20poly1305_open(sealed, key, "ctx-b", got));
        // 截断（短于 nonce+tag）→ false
        assert(!aead_xchacha20poly1305_open(sealed.substr(0, 39), key, aad, got));
        // 换钥 → false
        assert(!aead_xchacha20poly1305_open(sealed, random_bytes(kAeadKeyLen), aad, got));

        // 空明文 / 空 aad 边界
        std::string e;
        assert(aead_xchacha20poly1305_open(
                   aead_xchacha20poly1305_seal(key, "", aad), key, aad, e) && e.empty());
        assert(aead_xchacha20poly1305_open(
                   aead_xchacha20poly1305_seal(key, "", ""), key, "", e) && e.empty());
        // 确定性形态的空明文同样成立（tag 只覆盖 aad 与长度字段）
        std::string k = unhex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
        std::string n12 = unhex("000000000000000000000000");
        assert(aead_chacha20poly1305_open(aead_chacha20poly1305_seal(k, n12, "", ""), k, n12,
                                          "", e) && e.empty());

        // 参数校验臂
        assert(throws_invalid_argument([&] { (void)aead_xchacha20poly1305_seal("short", pt, aad); }));
        assert(throws_invalid_argument([&] { (void)aead_xchacha20poly1305_open(sealed, "short", aad, got); }));
        assert(throws_invalid_argument(
            [&] { (void)aead_chacha20poly1305_seal(k, "bad-nonce", pt, aad); }));
        assert(throws_invalid_argument(
            [&] { (void)xchacha20poly1305_seal_with_nonce(k, "bad-nonce", pt, aad); }));
        assert(throws_invalid_argument(
            [&] { (void)xchacha20poly1305_open_with_nonce("x", k, "bad-nonce", "", got); }));
        // 确定性形态截断
        assert(!aead_chacha20poly1305_open("tiny", k, n12, "", got));
    }

    // ---- 随机与擦除 ----
    {
        assert(random_bytes(0).empty());
        const std::string five = random_bytes(5);  // 非 4 倍数：尾分量分支
        assert(five.size() == 5);
        assert(random_bytes(32) != random_bytes(32));

        char secret[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        secure_wipe(secret, sizeof(secret));
        for (unsigned char b : secret) assert(b == 0);

        std::string s = "key material";
        secure_wipe(s);
        assert(s.empty());
        std::string empty;
        secure_wipe(empty);  // 空串臂
    }

    std::puts("crypto_std_test: all assertions passed");
    return 0;
}
