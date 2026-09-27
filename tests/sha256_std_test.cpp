// SHA-256 纯 std 测试：FIPS 180-4/NIST 向量、块边界与分块等价、增量
// 收尾幂等、流式文件哈希（正常/打不开/半途 I/O 错）与 hex 形状校验。
// 不联网、不碰模型资产。
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "std/sha256_std.h"

using namespace UnidictCoreStd;

namespace {

namespace fs = std::filesystem;

// FIPS 180-4 附录 B / NIST 的四条经典向量
void test_reference_vectors() {
    assert(sha256_hex(std::string("")) ==
           "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(sha256_hex(std::string("abc")) ==
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // 448 bit = 56 字节：正好撑满一块还要多补一个长度块（padding 的两
    // 条路径都被这条覆盖）
    assert(sha256_hex(std::string("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmn"
                                  "omnopnopq")) ==
           "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // 896 bit = 112 字节：跨两块
    assert(sha256_hex(std::string("abcdefghbcdefghicdefghijdefghijkefghijklfghij"
                                  "klmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmno"
                                  "pqrsmnopqrstnopqrstu")) ==
           "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    // 显式 (void*, len) 重载与 string 重载同源
    const std::string s = "abc";
    assert(sha256_hex(s.data(), s.size()) == sha256_hex(s));
}

// 一百万个 a：分块喂（update 的整块/半块两条路径）
void test_million_a_chunked() {
    Sha256HasherStd h;
    const std::string chunk(1000, 'a');
    for (int i = 0; i < 1000; ++i) {
        h.update(chunk);
    }
    assert(h.hex() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

// 喂法不影响结果：逐字节 vs 整块，跨 padding 分界的长度各来一遍
void test_chunking_equivalence() {
    for (std::size_t n : {1u, 55u, 56u, 57u, 63u, 64u, 65u, 119u, 120u, 200u}) {
        const std::string s(n, 'x');
        Sha256HasherStd whole;
        whole.update(s);
        Sha256HasherStd per_byte;
        for (std::size_t i = 0; i < n; ++i) {
            per_byte.update(s.data() + i, 1);
        }
        assert(whole.hex() == per_byte.hex());
        // 半块 + 整块混合喂（模拟流式：先 7 字节、再 100 字节、再剩下的）
        Sha256HasherStd mixed;
        const std::size_t takes[] = {7, 100, n};
        std::size_t at = 0;
        for (const std::size_t take : takes) {
            if (at >= n) break;
            const std::size_t step = (at + take > n) ? (n - at) : take;
            mixed.update(s.data() + at, step);
            at += step;
        }
        assert(mixed.hex() == whole.hex());
    }
}

// 收尾幂等 + reset 可复用：资产校验是一次性动作，但把对象当"可重入
// 的黑盒"用才不出错
void test_finalize_idempotent_and_reset() {
    Sha256HasherStd h;
    h.update(std::string("abc"));
    const std::string first = h.hex();
    assert(h.hex() == first);
    assert(h.hex() == first);
    // 收尾后再喂数据：静默忽略（padding 之后的数据没有意义，算进去就
    // 会得到一个谁也复现不出来的哈希）
    h.update(std::string("more"));
    assert(h.hex() == first);
    h.reset();
    h.update(std::string("abc"));
    assert(h.hex() == first);
    // 空输入（长度 0 的 update 也走一趟 no-op 早退）
    Sha256HasherStd empty;
    empty.update(nullptr, 0);
    assert(empty.hex() ==
           "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

// 流式文件哈希：正常、大文件跨多块、打不开、半途 I/O 错
void test_file_hashing() {
    const fs::path dir = fs::temp_directory_path() / "sha256_std_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // 与内存结果一致
    const fs::path small = dir / "small.bin";
    {
        std::ofstream out(small, std::ios::binary);
        out << "abc";
    }
    std::string hex, err;
    assert(sha256_file_hex(small.string(), hex, err));
    assert(hex == sha256_hex(std::string("abc")));
    assert(err.empty());

    // 2.5MiB > 单块缓冲（1MiB）：多轮读循环
    const std::string filler(1024 * 1024, 'a');
    const fs::path big = dir / "big.bin";
    {
        std::ofstream out(big, std::ios::binary);
        out << filler << filler;
        out << std::string(512 * 1024, 'a');
    }
    Sha256HasherStd expect;
    expect.update(filler);
    expect.update(filler);
    expect.update(std::string(512 * 1024, 'a'));
    assert(sha256_file_hex(big.string(), hex, err));
    assert(hex == expect.hex());

    // 空文件（0 字节的合法资产，如占位 vocab）
    const fs::path zero = dir / "zero.bin";
    { std::ofstream out(zero, std::ios::binary); }
    assert(sha256_file_hex(zero.string(), hex, err));
    assert(hex == sha256_hex(std::string("")));

    // 打不开 → false + err（与"哈希算完了"必须能分开）
    err.clear();
    assert(!sha256_file_hex((dir / "nope.bin").string(), hex, err));
    assert(!err.empty());
    assert(err.find("cannot open") != std::string::npos);

    // 目录当文件读：Linux 上 open 成功、read 立刻 EISDIR（覆盖 in.bad()
    // 那条"半途 I/O 错不当成算完了"）；Windows 上 open 就失败——两种
    // 平台都该是 false + 非空 err，这里只断言这个语义
    err.clear();
    assert(!sha256_file_hex(dir.string(), hex, err));
    assert(!err.empty());

    fs::remove_all(dir);
}

// 64 位小写 hex 的形状校验：手抄哈希最常错的就是位数与大小写
void test_is_sha256_hex() {
    assert(is_sha256_hex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca4959"
                         "91b7852b855"));
    assert(!is_sha256_hex(""));
    assert(!is_sha256_hex("abc"));
    assert(!is_sha256_hex(std::string(63, 'a')));      // 少一位
    assert(!is_sha256_hex(std::string(65, 'a')));      // 多一位
    assert(!is_sha256_hex("E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA49"
                          "5991B7852B855"));           // 大写：比对必失败
    assert(!is_sha256_hex("g3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca49"
                          "5991b7852b855"));           // 非 hex 字符
}

}  // namespace

int main() {
    test_reference_vectors();
    test_million_a_chunked();
    test_chunking_equivalence();
    test_finalize_idempotent_and_reset();
    test_file_hashing();
    test_is_sha256_hex();
    std::cout << "sha256_std_test: all assertions passed\n";
    return 0;
}
