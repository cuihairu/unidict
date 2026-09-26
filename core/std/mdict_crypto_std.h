// MDX 的"加密"原语：RIPEMD-128 派生密钥 + 半字节交换的链式 XOR。
//
// **先纠正一个此前的错误认知**：MDX 不用 DES / Blowfish / AES。
// core/std/mdict_decryptor_std.h 里那套 MdictEncryptionType{DES_ECB,
// BLOWFISBC_*, AES_*} 枚举在真实 MDX 里从来不会出现——它对应的是
// "自定义加密"这一类第三方变体，而**官方的 MdxBuilder 产出的加密 MDX
// 只有两种**：
//
//   1) 密钥固定的 key block info 加密（header 里 Encrypted="Yes"，
//      且导出开关未勾选那一支）。密钥不需要密码：
//          key = RIPEMD128( adler32 字节 ‖ LE32(0x3695) )
//      这是现实里最常见的一类"加密 MDX"，也恰恰是完全不需要用户输密码的。
//
//   2) 用户给"Encryption Key"加密的 record block（header 里
//      Encrypted="Yes" + 给了 key）。密钥由注册码/设备码经
//      Salsa20 派生，属于要用户提供 regcode+userid 的那一类。
//
// 每个数据块（key block / record block）的头 4 字节是块信息字（LE32）：
//      低 4 位   压缩方式（0 原样 / 1 LZO / 2 zlib）
//      次 4 位   加密方式（0 无 / 1 本模块的 cipher / 2 Salsa20）
//      再 8 位   加密字节数（只加密前 N 字节，其余原样）
// 接着 4 字节是该块的 adler32。**解密是否正确由 adler32 判定**——这是
// 权威判据，比任何"看起来像明文"的启发式都可靠。
//
// 块密钥（无用户密钥时）：key = RIPEMD128(该块的 4 字节 adler)。

#ifndef UNIDICT_MDICT_CRYPTO_STD_H
#define UNIDICT_MDICT_CRYPTO_STD_H

#include <cstdint>
#include <string>

namespace UnidictCoreStd {

// 块信息字解析结果。
struct BlockInfo {
    uint8_t compression = 0;      // 0 原样 / 1 LZO / 2 zlib
    uint8_t encryption = 0;       // 0 无 / 1 本模块 cipher / 2 Salsa20
    uint8_t encryption_size = 0;  // 只加密前 N 字节
};

// 解析块的前 4 字节。输入不足 4 字节时返回全 0。
BlockInfo parse_block_info(const std::string& block);

// MDX 的半字节交换链式 XOR：
//     t = (b[i] >> 4 | b[i] << 4) & 0xFF          // 交换高/低半字节
//     t ^= previous ^ (i & 0xFF) ^ key[i % key.size()]
//     previous = b[i]                               // 注意是**密文**原字节
// key 为空时原样返回。
std::string fast_decrypt(const std::string& data, const std::string& key);

// 上面那个 cipher 的加密方向（与 fast_decrypt 互逆）。
// 测试用它造夹具：真实的加密 MDX 就是这么产生的。
std::string fast_encrypt(const std::string& data, const std::string& key);

// 密钥固定的 key block info 密钥：RIPEMD128( adler ‖ LE32(0x3695) )，16 字节。
// adler 必须是该 key block info 块自带的 4 字节 adler32。
std::string key_info_key(const std::string& adler32_bytes);

// 块密钥（未提供用户密钥时）：RIPEMD128( 该块的 4 字节 adler32 )，16 字节。
std::string block_key(const std::string& adler32_bytes);

// 就地解密一个数据块（block 会被改写）。
//   key 非空 → 用它作块密钥（用户给了 Encryption Key 的情形）
//   key 为空 → 用 block 自带的 adler 派生（密钥固定的情形）
// 返回是否成功。失败的情形：头部不足 8 字节、加密方式 2（Salsa20，本模块
// 未实现）、加密长度越界。**失败时 block 保持原样**，绝不半改半留。
bool decrypt_block(std::string& block, const std::string& key);

// zlib adler32（与 zlib 的实现一致，用作解密正确性的权威判据）。
uint32_t adler32_of(const std::string& data);

// 十六进制小写（测试与日志用）。
std::string to_hex_string(const std::string& bytes);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_MDICT_CRYPTO_STD_H
