// MdictDecryptorStd 补覆盖：detect_encryption_type 的重复模式 /
// 强加密 / 空数据（连带熵、模式、头部检测的空入参早退）分支，
// decrypt 的 SIMPLE_XOR 无密码与空数据（密钥流失空）分支，
// try_auto_decrypt 的密码 SimpleXOR 命中路径。

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "std/mdict_decryptor_std.h"

using UnidictCoreStd::DecryptResult;
using UnidictCoreStd::MdictDecryptorStd;
using UnidictCoreStd::MdictEncryptionType;

int main() {
    // ===== 1) 高熵 + 重复 4 字节模式 → "检测到XOR模式" =====
    {
        // 0x00..0x7F 各一次（熵略高于 7.0，离开低熵块）+ 尾部 XYZWXYZW×2
        // 使 detect_patterns 非空；头 8 字节仅产生 1 个 <1MB 长度字段，
        // 不足 header 判定阈值（>=2）
        std::vector<uint8_t> data;
        for (int v = 0x00; v <= 0x7F; ++v) data.push_back((uint8_t)v);
        for (int r = 0; r < 2; ++r)
            for (uint8_t b : {(uint8_t)'X', (uint8_t)'Y', (uint8_t)'Z', (uint8_t)'W'})
                data.push_back(b);

        MdictDecryptorStd dec;
        auto r = dec.detect_encryption_type(data);
        assert(!r.success);
        assert(r.error.find("XOR模式") != std::string::npos);
        assert(r.detected_type == MdictEncryptionType::SIMPLE_XOR);
    }

    // ===== 2) 全值一次排列（熵=8.0，无重复 4-gram，无 header 特征）→ 强加密 CUSTOM =====
    {
        // 0x10..0xFF + 0x01..0x0F + 0x00：每个 4-gram 唯一（值只出现一次），
        // 窗口首字节均 >=0x01（大端值 >1MB，不计入 header 长度字段）
        std::vector<uint8_t> data;
        for (int v = 0x10; v <= 0xFF; ++v) data.push_back((uint8_t)v);
        for (int v = 0x01; v <= 0x0F; ++v) data.push_back((uint8_t)v);
        data.push_back(0x00);

        MdictDecryptorStd dec;
        auto r = dec.detect_encryption_type(data);
        assert(!r.success);
        assert(r.error.find("强加密") != std::string::npos);
        assert(r.detected_type == MdictEncryptionType::CUSTOM);
    }

    // ===== 2b) 64 个互异字节（熵精确 6.0，不满足 >6.0 也无模式）→ 兜底"尝试SimpleXOR" =====
    {
        std::vector<uint8_t> data;
        for (int v = 0x10; v <= 0x4F; ++v) data.push_back((uint8_t)v);

        MdictDecryptorStd dec;
        auto r = dec.detect_encryption_type(data);
        assert(!r.success);
        assert(r.error.find("尝试SimpleXOR") != std::string::npos);
        assert(r.detected_type == MdictEncryptionType::SIMPLE_XOR);
    }

    // ===== 3) 空数据：analyze 内部熵/模式/头部检测的空入参早退 =====
    {
        MdictDecryptorStd dec;
        auto r = dec.detect_encryption_type({});
        (void)r;   // 走到哪条回落均可，目标是三个检测函数的空数据守卫
    }

    // ===== 4) SIMPLE_XOR 未设密码 → "需要密码" =====
    {
        MdictDecryptorStd dec;
        std::vector<uint8_t> data(32, 0x41);
        auto r = dec.decrypt(data, MdictEncryptionType::SIMPLE_XOR);
        assert(!r.success);
        assert(r.error.find("需要密码") != std::string::npos);
        assert(!dec.has_password());
    }

    // ===== 5) SIMPLE_XOR 空数据：密钥流长度取 min(256, 0)=0 → 空密钥 =====
    {
        MdictDecryptorStd dec;
        assert(dec.set_password("pw123"));
        auto r = dec.decrypt(std::vector<uint8_t>(), MdictEncryptionType::SIMPLE_XOR);
        // 空密钥被拒（"XOR解密需要密钥"）
        assert(!r.success);
        assert(r.error.find("密钥") != std::string::npos);
    }

    // ===== 6) try_auto_decrypt：密码 SimpleXOR 命中（validate 认得 MDict 标记）=====
    {
        const std::string password = "cov-pass";
        // 三个标记（小写匹配，前 512B 内）使 validate 判定有效（阈值 >=3）
        const std::string plain = "BookName: cover || Description: d || Title: t";
        std::vector<uint8_t> plain_bytes(plain.begin(), plain.end());

        // 用全零密文探出密码密钥流：simple_xor 对称，零串解密结果即密钥流
        MdictDecryptorStd probe;
        assert(probe.set_password(password));
        std::vector<uint8_t> zeros(plain_bytes.size(), 0);
        auto ks = probe.decrypt(zeros, MdictEncryptionType::SIMPLE_XOR);
        assert(ks.success && ks.data.size() == plain_bytes.size());

        // 密文 = 明文 XOR 密钥流
        std::vector<uint8_t> cipher(plain_bytes.size());
        for (size_t i = 0; i < plain_bytes.size(); ++i)
            cipher[i] = plain_bytes[i] ^ (uint8_t)ks.data[i];

        MdictDecryptorStd dec;
        dec.set_debug_mode(true);
        assert(dec.set_password(password));
        auto r = dec.try_auto_decrypt(cipher);
        assert(r.success);
        assert(r.data == plain);
    }

    // ===== 7) debug 模式重跑三条 detect 路径 + 空密码 SIMPLE_XOR =====
    // 打 91/99/105 的 debug 输出行与 decrypt_simple_xor 的空密码守卫
    {
        MdictDecryptorStd dec;
        dec.set_debug_mode(true);

        // 模式路径（同 case 1 数据）
        std::vector<uint8_t> d1;
        for (int v = 0x00; v <= 0x7F; ++v) d1.push_back((uint8_t)v);
        for (int r = 0; r < 2; ++r)
            for (uint8_t b : {(uint8_t)'X', (uint8_t)'Y', (uint8_t)'Z', (uint8_t)'W'})
                d1.push_back(b);
        assert(dec.detect_encryption_type(d1).error.find("XOR模式") != std::string::npos);

        // 强加密路径（同 case 2 数据）
        std::vector<uint8_t> d2;
        for (int v = 0x10; v <= 0xFF; ++v) d2.push_back((uint8_t)v);
        for (int v = 0x01; v <= 0x0F; ++v) d2.push_back((uint8_t)v);
        d2.push_back(0x00);
        assert(dec.detect_encryption_type(d2).error.find("强加密") != std::string::npos);

        // 兜底路径（同 case 2b 数据）
        std::vector<uint8_t> d3;
        for (int v = 0x10; v <= 0x4F; ++v) d3.push_back((uint8_t)v);
        assert(dec.detect_encryption_type(d3).error.find("尝试SimpleXOR") != std::string::npos);

        // is_set 但密码串为空 → decrypt_simple_xor 的空密码守卫
        assert(dec.set_password(""));
        auto r = dec.decrypt(d3, MdictEncryptionType::SIMPLE_XOR);
        assert(!r.success);
        assert(r.error.find("需要密码") != std::string::npos);
    }

    return 0;
}
