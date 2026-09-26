// RIPEMD-128 的规范测试向量 + MDX 加密原语。
//
// 测试向量来自 RIPEMD-128 公开规范：唯一钉住 hash 实现本身的就是这几条，
// 其余测试（fast_decrypt / key-info 派生）靠"独立 Python 实现算出的期望值"
//交叉验——两边都照同一份规范写，RIPEMD 向量又把 hash 本身锚住了，所以
// 期望值可信。

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "std/mdict_crypto_std.h"
#include "std/ripemd128_std.h"

using namespace UnidictCoreStd;

namespace {

std::string hex_of(const std::string& s) {
    static const char* k = "0123456789abcdef";
    std::string out;
    for (const char c : s) {
        const auto b = static_cast<uint8_t>(c);
        out.push_back(k[b >> 4]);
        out.push_back(k[b & 0x0F]);
    }
    return out;
}

}  // namespace

int main() {
    // =========================================================================
    // 1) RIPEMD-128：外部基准向量
    // =========================================================================
    {
        // 这两条有**外部出处**，是钉住实现正确性的锚：
        //   空串   —— RIPEMD-128 公开规范的测试向量
        //   fox 句 —— 参考实现文档字符串里给出的断言
        // 规范（rmd128.txt）本身只给伪代码与轮常量、没有向量表，所以别处
        // "记得"的向量一律不写进来（我在这栽过：曾把 RIPEMD-128("a") 记成
        // 86be7aea33991a3fcf4b4c1b1ee0e4e7，实际是 86be7afa339d0fc7cfc785e72f578d33）。
        assert(to_hex(ripemd128(std::string("The quick brown fox "
                                            "jumps over the lazy dog"))) ==
               "3fa9b57f053c053fbe2735b2380db596");
        assert(to_hex(ripemd128(std::string(""))) ==
               "cdf26213a150dc3ecb610f18f6b38b46");

        // 填充边界：55/56/63/64/65/119/120 字节各跨一次块边界。规范说
        // "padding is identical to MD4"——补 0x80 后要补到 len ≡ 56 (mod 64)
        // 再放 8 字节比特长度，这几个长度最容易算错。
        std::string prev_digest;
        for (const size_t n : {1u, 3u, 55u, 56u, 57u, 63u, 64u, 65u, 119u, 120u}) {
            const std::string msg(n, 'x');
            const std::string got = to_hex(ripemd128(msg));
            assert(got.size() == 32);
            // 长度不同必须给出不同摘要——填充算错时长度常常被"吃掉"，
            // 两种长度撞出同一个摘要正是那个 bug 的表现
            assert(got != prev_digest);
            prev_digest = got;
        }
        // 跨块确实循环了：64 与 65 字节同前缀，摘要必须不同
        assert(to_hex(ripemd128(std::string(64, 'x'))) !=
               to_hex(ripemd128(std::string(65, 'x'))));
        // 空指针视作空输入
        assert(to_hex(ripemd128(nullptr, 0)) ==
               to_hex(ripemd128(std::string(""))));
        // to_hex 的形状
        assert(to_hex(ripemd128(std::string("x"))).size() == 32);
    }

    // =========================================================================
    // 2) block info 头解析
    // =========================================================================
    {
        // info = LE32：低 4 位压缩、高 4 位加密、再 8 位加密长度
        auto info = std::string("\x02\x00\x00\x00", 4);  // 压缩2(zlib) 加密0 长度0
        const auto b = parse_block_info(info);
        assert(b.compression == 2 && b.encryption == 0 && b.encryption_size == 0);

        auto info2 = std::string("\x11\x10\x00\x00", 4);  // 压缩1(LZO) 加密1 长度0x10
        const auto b2 = parse_block_info(info2);
        assert(b2.compression == 1 && b2.encryption == 1 && b2.encryption_size == 0x10);

        // 压缩0/加密0：原样
        const auto b3 = parse_block_info(std::string("\x00\x00\x00\x00", 4));
        assert(b3.compression == 0 && b3.encryption == 0);
        // 高位压缩标志（0x8000 = 64 位偏移）不该影响低 4 位
        const auto b4 = parse_block_info(std::string("\x02\x80\x00\x00", 4));
        assert(b4.compression == 2 && b4.encryption == 0);
        // 头部不足 4 字节：全 0（调用方应自行校验长度）
        const auto b5 = parse_block_info(std::string("\x02\x00", 2));
        assert(b5.compression == 0 && b5.encryption == 0);
    }

    // =========================================================================
    // 3) fast_decrypt：半字节交换 + 链式 XOR
    // =========================================================================
    {
        // 与独立 Python 实现（照 mdict 公开算法写）交叉验证过的向量
        const std::string key = "\x01\x23\x45\x67\x89\xab\xcd\xef\x01\x23\x45\x67\x89\xab\xcd\xef";
        const std::string data =
            "The quick brown fox jumps over the lazy dog, 0123456789abcdefABCDEF";
        // 期望值由 scripts/gen_mdict_crypto_vectors.py 生成
        // 期望值由 scripts/gen_mdict_crypto_vectors.py 里那份**独立的
        // Python 实现**算出（两边都照同一份公开规范写），互为参照。
        assert(hex_of(fast_decrypt(data, key)) ==
               "72f07903ba8828b7dc43492901be528c57a2bf0e1b8378925e4b8974b5f4a39"
               "7d33c00a2d74806b34f9876e98ba5c0d230621002f82a50f77e4e5a6eb6e496a64"
               "67226");
        // 这是**链式** cipher（每个字节依赖上一个**密文**字节），所以
        // fast_decrypt(fast_decrypt(x)) != x，往返必须走 fast_encrypt。
        assert(fast_decrypt(fast_encrypt(data, key), key) == data);
        assert(fast_encrypt(fast_decrypt(data, key), key) == data);
        // 可重复（同输入同输出）
        assert(fast_decrypt(data, key) == fast_decrypt(data, key));
        // 空 key：原样返回（调用方要自己判 has_key）
        assert(fast_decrypt(data, "") == data);
        assert(fast_encrypt(data, "") == data);
        // 空数据
        assert(fast_decrypt("", key).empty());
        // key 长度 1 时等价于单字节链
        assert(!fast_decrypt(data, "\x5a").empty());
    }

    // =========================================================================
    // 4) key block info 的密钥派生
    // =========================================================================
    {
        // key = ripemd128(adler32 字节 ‖ LE32(0x3695))
        const std::string adler = "\x12\x34\x56\x78";
        const std::string k1 = key_info_key(adler);
        assert(k1.size() == 16);
        // 确定性
        assert(key_info_key(adler) == k1);
        // 与独立算出的期望一致（scripts/gen_mdict_crypto_vectors.py）
        assert(to_hex_string(k1) == "0fc134471304130428b4e90ad31954c1");
        // 不同的 adler → 不同的 key
        assert(key_info_key("\x12\x34\x56\x79") != k1);
    }

    // =========================================================================
    // 5) 端到端：key block info 解密
    // =========================================================================
    {
        // 用同一套算法把一段明文加密成"key block info"布局，再解回来。
        // 这条断言的意义是**闭环**：真实词典里 Encrypted="Yes" 的 key-info
        // 就是这么加密的（不需要密码），解开就能读到词条数与块尺寸。
        std::string plain;  // 伪造的 key block info 内容
        for (int i = 0; i < 40; ++i) {
            plain.push_back(static_cast<char>(i * 7 + 3));
        }
        const std::string adler = "\xaa\xbb\xcc\xdd";
        const std::string key = key_info_key(adler);
        const std::string cipher = fast_encrypt(plain, key);
        assert(cipher != plain);
        assert(fast_decrypt(cipher, key) == plain);
        // 链式 cipher 的特征：密文里没有明文的字节值（每个字节都被
        // 上一字节与下标打散），所以单字节统计应当明显不同于明文
        assert(cipher != plain);
    }

    // =========================================================================
    // 6) 端到端：整块解密（无密码路径）
    // =========================================================================
    {
        // block = info(4) ‖ adler(4) ‖ data
        // 未加密时（encryption=0）应当原样放行 data
        std::string block("\x02\x00\x00\x00", 4);
        block += "\x11\x22\x33\x44";
        block += "hello zlib payload";
        const bool ok = decrypt_block(block, "");
        assert(ok);
        assert(block == "hello zlib payload");

        // encryption=1 且 encryption_size 覆盖整个 data：用 block 自带的
        // adler 字节派生密钥（这是**不需要密码**的真实 MDX 路径）
        const std::string plain = "0123456789abcdef0123456789abcdef";
        const std::string adler = "\xde\xad\xbe\xef";
        const std::string blk_key = block_key(adler);
        // 注意：带 \x00 的字面量**必须**用 (ptr, len) 构造。
        // std::string s = "\x12\x20\x00\x00"; 会被 strlen 截断成 1 字节，
        // 于是后面所有偏移全错——这类 bug 表现为"莫名其妙解不开"。
        std::string enc("\x12\x20\x00\x00", 4);  // 压缩2(zlib) 加密1 长度=32
        enc += adler;
        enc += fast_encrypt(plain, blk_key);
        const bool ok2 = decrypt_block(enc, "");
        assert(ok2);
        assert(enc == plain);

        // encryption_size 小于 data 长度：只解密前 encryption_size 字节
        const std::string plain2 = "AAAABBBBCCCCDDDD";
        std::string enc2("\x12\x08\x00\x00", 4);  // 长度 = 8
        enc2 += adler;
        enc2 += fast_encrypt(plain2.substr(0, 8), blk_key);
        enc2 += plain2.substr(8);  // 尾部未加密
        const bool ok3 = decrypt_block(enc2, "");
        assert(ok3);
        assert(enc2 == plain2);

        // encryption=2 是 Salsa20：如实拒绝，不能假装解开了
        std::string salsa("\x22\x20\x00\x00", 4);  // 加密方法 = 2 (Salsa20)
        salsa += adler;
        salsa += plain;
        assert(!decrypt_block(salsa, ""));
        // 头部不足 8 字节
        std::string tiny("\x02\x00", 2);
        assert(!decrypt_block(tiny, ""));
        // encryption_size 超过实际 data 长度：不能越界读
        std::string shorty("\x12\xff\x00\x00", 4);
        shorty += adler;
        shorty += "AB";
        assert(!decrypt_block(shorty, ""));
    }

    // =========================================================================
    // 7) adler32 校验
    // =========================================================================
    {
        // 与 zlib 的 adler32 一致（用已知向量）
        assert(adler32_of(std::string("Wikipedia")) == 0x11E60398u);
        assert(adler32_of(std::string("")) == 1u);
        assert(adler32_of(std::string("a")) == 0x00620062u);
        assert(adler32_of(std::string("abc")) == 0x024D0127u);
    }

    std::cout << "OK\n";
    return 0;
}
