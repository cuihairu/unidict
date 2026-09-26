#include "std/mdict_crypto_std.h"

#include <array>
#include <cstring>

#include "std/ripemd128_std.h"

namespace UnidictCoreStd {
namespace {

std::string digest_to_string(const Ripemd128Digest& d) {
    return std::string(reinterpret_cast<const char*>(d.data()), d.size());
}

// 密钥固定那条路专用的魔数（MDX 的 key block info 派生）。
constexpr uint8_t kKeyInfoMagic[4] = {0x95, 0x36, 0x00, 0x00};  // LE32(0x3695)

}  // namespace

BlockInfo parse_block_info(const std::string& block) {
    BlockInfo info;
    if (block.size() < 4) {
        return info;  // 头部不足：全 0，交给调用方按长度判断
    }
    const uint32_t word = static_cast<uint32_t>(
                             static_cast<uint8_t>(block[0])) |
                         (static_cast<uint32_t>(
                              static_cast<uint8_t>(block[1]))
                          << 8) |
                         (static_cast<uint32_t>(
                              static_cast<uint8_t>(block[2]))
                          << 16) |
                         (static_cast<uint32_t>(
                              static_cast<uint8_t>(block[3]))
                          << 24);
    info.compression = static_cast<uint8_t>(word & 0x0F);
    info.encryption = static_cast<uint8_t>((word >> 4) & 0x0F);
    info.encryption_size = static_cast<uint8_t>((word >> 8) & 0xFF);
    return info;
}

std::string fast_decrypt(const std::string& data, const std::string& key) {
    if (key.empty()) {
        return data;
    }
    std::string out;
    out.resize(data.size());
    uint8_t previous = 0x36;  // 初值来自 MDX 规范
    for (size_t i = 0; i < data.size(); ++i) {
        const uint8_t b = static_cast<uint8_t>(data[i]);
        // 交换高/低半字节
        uint8_t t = static_cast<uint8_t>((b >> 4) | (b << 4));
        // previous 取的是**上一个密文字节**（原字节），不是解出来的明文
        t = static_cast<uint8_t>(t ^ previous ^ static_cast<uint8_t>(i & 0xFF) ^
                                 static_cast<uint8_t>(key[i % key.size()]));
        previous = b;
        out[i] = static_cast<char>(t);
    }
    return out;
}

std::string fast_encrypt(const std::string& data, const std::string& key) {
    if (key.empty()) {
        return data;
    }
    std::string out;
    out.resize(data.size());
    uint8_t previous = 0x36;
    for (size_t i = 0; i < data.size(); ++i) {
        const uint8_t p = static_cast<uint8_t>(data[i]);
        // 解密时：t = swap(b) ^ prev ^ (i&0xFF) ^ key[...]  =>  b = p
        // 所以加密时：由明文 p 反推密文字节 b，使上式成立。
        // swap 是自身的逆（4 位旋转），但它作用在**密文**字节上，所以
        // 这里要先求出"未交换的密文字节"再交换回去。
        const uint8_t raw = static_cast<uint8_t>(
            p ^ previous ^ static_cast<uint8_t>(i & 0xFF) ^
            static_cast<uint8_t>(key[i % key.size()]));
        out[i] = static_cast<char>((raw >> 4) | (raw << 4));
        previous = static_cast<uint8_t>(out[i]);  // 链的是密文
    }
    return out;
}

std::string key_info_key(const std::string& adler32_bytes) {
    std::string seed = adler32_bytes;
    seed.append(reinterpret_cast<const char*>(kKeyInfoMagic), 4);
    return digest_to_string(ripemd128(seed));
}

std::string block_key(const std::string& adler32_bytes) {
    return digest_to_string(ripemd128(adler32_bytes));
}

bool decrypt_block(std::string& block, const std::string& key) {
    if (block.size() < 8) {
        return false;
    }
    const BlockInfo info = parse_block_info(block);
    if (info.encryption == 0) {
        block.erase(0, 8);  // 去掉 info + adler，剩下就是明文数据
        return true;
    }
    if (info.encryption != 1) {
        // 2 = Salsa20（需要用户提供 regcode + userid 派生）。这里如实拒绝，
        // 绝不用别的算法"试一下"——试出来的结果是错数据，界面上看不出来。
        return false;
    }
    if (static_cast<size_t>(info.encryption_size) + 8 > block.size()) {
        return false;  // 加密长度越界：拒绝，不做部分解密
    }
    const std::string k = key.empty() ? block_key(block.substr(4, 4)) : key;
    const size_t n = info.encryption_size;
    // 只有前 n 字节是密文，其余原样
    const std::string tail = block.substr(8 + n);
    std::string head = block.substr(8, n);
    head = fast_decrypt(head, k);
    block = head + tail;
    return true;
}

uint32_t adler32_of(const std::string& data) {
    // 与 zlib 的 adler32 语义一致：A=1, B=0，按 5552 取模。
    constexpr uint32_t kBase = 65521u;
    uint32_t a = 1, b = 0;
    for (const char ch : data) {
        a = (a + static_cast<uint8_t>(ch)) % kBase;
        b = (b + a) % kBase;
    }
    return (b << 16) | a;
}

std::string to_hex_string(const std::string& bytes) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const char c : bytes) {
        const auto b = static_cast<uint8_t>(c);
        out.push_back(kHex[b >> 4]);
        out.push_back(kHex[b & 0x0F]);
    }
    return out;
}

}  // namespace UnidictCoreStd
