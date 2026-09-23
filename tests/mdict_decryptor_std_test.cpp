#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>

#include "std/mdict_decryptor_std.h"

using namespace UnidictCoreStd;

// 含 >=3 个 MDict 标记的低熵文本（触发 has_header_structure 与 validate 通过）
static const std::string kValidMdictText =
    "MDX BookName Description encoding Format StyleSheet Title";

static std::vector<uint8_t> to_bytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

static std::vector<uint8_t> xor_bytes(const std::vector<uint8_t>& data, uint8_t key) {
    std::vector<uint8_t> out;
    out.reserve(data.size());
    for (uint8_t b : data) out.push_back(b ^ key);
    return out;
}

// 256 个各不相同字节 = 最高熵，且无 4 字节重复模式
static std::vector<uint8_t> high_entropy_no_pattern() {
    std::vector<uint8_t> data(256);
    for (int i = 0; i < 256; ++i) data[i] = static_cast<uint8_t>(i);
    return data;
}

int main() {
    // ===== 密码管理 =====
    MdictDecryptorStd d;
    assert(!d.has_password());
    assert(d.get_last_error().empty());

    assert(d.set_password("pw"));
    assert(d.has_password());

    // 超长密码拒绝并记录错误
    assert(!d.set_password(std::string(1025, 'a')));
    assert(!d.get_last_error().empty());
    // 被拒后旧密码仍在
    assert(d.has_password());

    d.clear_password();
    assert(!d.has_password());
    assert(d.get_last_error().empty());

    // ===== decrypt(NONE)：原样返回 =====
    {
        auto data = to_bytes("plain");
        auto r = d.decrypt(data, MdictEncryptionType::NONE);
        assert(r.success);
        assert(r.data == "plain");
        assert(r.detected_type == MdictEncryptionType::NONE);
    }

    // ===== decrypt(SIMPLE_XOR)：无密码拒绝，有密码后自逆 roundtrip =====
    {
        auto no_pw = d.decrypt(to_bytes("abc"), MdictEncryptionType::SIMPLE_XOR);
        assert(!no_pw.success);
        assert(no_pw.error.find("密码") != std::string::npos);

        MdictDecryptorStd d2;
        assert(d2.set_password("secret"));
        std::string plain = "MDX BookName Description encoding Format";
        auto enc = d2.decrypt(to_bytes(plain), MdictEncryptionType::SIMPLE_XOR);
        assert(enc.success);
        // XOR 密钥流自逆：对密文再做一次同密码解密应还原原文
        auto dec = d2.decrypt(to_bytes(enc.data), MdictEncryptionType::SIMPLE_XOR);
        assert(dec.success);
        assert(dec.data == plain);
    }

    // ===== 强加密类型：明确报不支持 =====
    {
        struct Case { MdictEncryptionType type; const char* name; };
        const Case cases[] = {
            {MdictEncryptionType::DES_ECB, "DES_ECB"},
            {MdictEncryptionType::DES_CBC, "DES_CBC"},
            {MdictEncryptionType::BLOWFISH_ECB, "BLOWFISH_ECB"},
            {MdictEncryptionType::BLOWFISH_CBC, "BLOWFISH_CBC"},
            {MdictEncryptionType::AES_ECB, "AES_ECB"},
            {MdictEncryptionType::AES_CBC, "AES_CBC"},
        };
        for (const auto& c : cases) {
            auto r = d.decrypt(to_bytes("x"), c.type);
            assert(!r.success);
            assert(r.error == std::string("不支持的加密类型: ") + c.name);
            assert(r.detected_type == c.type);
        }
        auto custom = d.decrypt(to_bytes("x"), MdictEncryptionType::CUSTOM);
        assert(!custom.success);
        assert(custom.error == "自定义加密类型不支持");
    }

    // ===== string 重载与 vector 重载等价 =====
    {
        auto rs = d.decrypt(std::string("str"), MdictEncryptionType::NONE);
        assert(rs.success);
        assert(rs.data == "str");
    }

    // ===== try_auto_decrypt：未加密有效数据直接返回 =====
    {
        auto r = d.try_auto_decrypt(to_bytes(kValidMdictText));
        assert(r.success);
        assert(r.data == kValidMdictText);
        assert(r.detected_type == MdictEncryptionType::NONE);
    }

    // ===== try_auto_decrypt：单字节 XOR 自动破解 =====
    {
        auto enc = xor_bytes(to_bytes(kValidMdictText), 0x5A);
        auto r = d.try_auto_decrypt(enc);
        assert(r.success);
        assert(r.data == kValidMdictText);
        assert(r.detected_type == MdictEncryptionType::SIMPLE_XOR);
    }

    // ===== try_auto_decrypt：有密码时 SimpleXOR 路径 =====
    {
        MdictDecryptorStd d3;
        assert(d3.set_password("mypw"));
        std::string plain = "MDX BookName Description encoding Format";
        // 密文对 NONE 直接判定无效（密钥流输出为乱码，无标记），于是走
        // has_password 的 SimpleXOR 路径还原原文
        auto enc = d3.decrypt(to_bytes(plain), MdictEncryptionType::SIMPLE_XOR);
        assert(enc.success);
        auto r = d3.try_auto_decrypt(to_bytes(enc.data));
        assert(r.success);
        assert(r.data == plain);
    }

    // ===== try_auto_decrypt：高熵无模式数据失败 =====
    {
        auto r = d.try_auto_decrypt(high_entropy_no_pattern());
        assert(!r.success);
        assert(r.error == "自动解密失败");
    }

    // ===== detect_encryption_type：四条分支 =====
    {
        // 结构化低熵 → NONE 成功
        auto r1 = d.detect_encryption_type(to_bytes(kValidMdictText));
        assert(r1.success);
        assert(r1.detected_type == MdictEncryptionType::NONE);

        // 低熵无结构无标记 → SIMPLE_XOR 提示
        auto r2 = d.detect_encryption_type(to_bytes("aaaabbbbccccdddd"));
        assert(!r2.success);
        assert(r2.error == "需要SimpleXOR解密");
        assert(r2.detected_type == MdictEncryptionType::SIMPLE_XOR);

        // 高熵但含 4 字节重复模式 → patterns 分支优先于高熵分支
        auto base = high_entropy_no_pattern();
        std::vector<uint8_t> patterned(base.begin(), base.end());
        patterned.insert(patterned.end(), base.begin(), base.begin() + 16);
        auto r3 = d.detect_encryption_type(patterned);
        assert(!r3.success);
        assert(r3.detected_type == MdictEncryptionType::SIMPLE_XOR);
        assert(r3.error == "检测到XOR模式，需要解密");

        // 高熵无模式 → CUSTOM 强加密
        auto r4 = d.detect_encryption_type(high_entropy_no_pattern());
        assert(!r4.success);
        assert(r4.detected_type == MdictEncryptionType::CUSTOM);
        assert(r4.error == "检测到强加密，不支持");
    }

    // ===== validate_decrypted_data =====
    {
        assert(!d.validate_decrypted_data(""));
        assert(!d.validate_decrypted_data("hello world no markers"));
        assert(d.validate_decrypted_data("MDX BookName Description"));
        assert(d.validate_decrypted_data("mdx bookname description")); // 大小写不敏感
    }

    // ===== get_supported_types =====
    {
        auto types = d.get_supported_types();
        assert(types.size() == 9);
        assert(types[0] == "NONE - 无加密");
        assert(types[8] == "CUSTOM - 自定义加密");
    }

    // ===== debug 模式：跑一遍主路径触达日志分支 =====
    {
        MdictDecryptorStd dd;
        dd.set_debug_mode(true);
        assert(dd.set_password("dbg"));
        dd.detect_encryption_type(to_bytes(kValidMdictText));
        dd.detect_encryption_type(to_bytes("aaaabbbbccccdddd"));
        dd.try_auto_decrypt(to_bytes(kValidMdictText));
        dd.try_auto_decrypt(xor_bytes(to_bytes(kValidMdictText), 0x5A));
        dd.try_auto_decrypt(high_entropy_no_pattern());
        auto enc = dd.decrypt(to_bytes(kValidMdictText), MdictEncryptionType::SIMPLE_XOR);
        assert(enc.success);
        assert(dd.validate_decrypted_data(kValidMdictText));
    }

    std::cout << "OK\n";
    return 0;
}
