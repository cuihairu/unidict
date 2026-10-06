#pragma once

// SHA-256（FIPS 180-4）——发音模型资产的下载校验（M10）。
//
// 为什么要自己写而不是链 OpenSSL：core/ 的纪律是纯 C++17 + STL，Qt 与
// 第三方重依赖一律走适配器（docs/pronunciation-plan.md 分层架构）。而
// "下载完必须校验哈希"是发音计划「已知风险」一节明写的要求——635MB 的
// fp16 模型从 HuggingFace 拉下来，字节坏了的话 onnxruntime 只会在加载
// 几分钟后报一句看不懂的错，用户完全无从判断是网络问题还是模型坏了。
//
// 实现按 FIPS 180-4 的参考实现逐字写，只吃字节流：增量 update + 收尾
// 出小写 hex，600MB 资产分块喂也不会把内存吃穿。
#include <cstddef>
#include <cstdint>
#include <string>

namespace UnidictCoreStd {

// 增量哈希器：喂完一串字节再取结果。非线程安全（一次喂完再 hex()）。
// 生命周期内 hex() 可重复调用，返回同一结果。
class Sha256HasherStd {
public:
    Sha256HasherStd();

    void update(const void* data, std::size_t len);
    void update(const std::string& data);

    // 收尾（补 padding + 长度）并出 64 位小写 hex
    std::string hex();

    // 收尾并出 32 字节原始摘要——HMAC 这类二进制消费者直接吃字节，
    // 免掉 hex 字符串的解析往返。
    std::string digest_raw();

    // 回到初始状态（复用同一个对象算另一个文件）
    void reset();

private:
    void compress(const unsigned char* block);

    std::uint32_t state_[8];
    unsigned char buf_[64];
    std::size_t buf_len_ = 0;
    // 已吞字节数（padding 时写进末块的低 64 位，长度字段按比特）
    std::uint64_t byte_len_ = 0;
    bool done_ = false;
    std::string digest_;
};

// 一次性哈希
std::string sha256_hex(const std::string& data);
std::string sha256_hex(const void* data, std::size_t len);

// 流式哈希文件：分块读，内存占用与文件大小无关（600MB 的模型也只占
// 一块缓冲）。失败（打不开/读出错）返回 false 并写 err —— 与"哈希算完
// 了"是两回事，调用方必须能分开。
bool sha256_file_hex(const std::string& path, std::string& out_hex,
                     std::string& err);

// 64 位小写 hex 串的形状校验：手抄哈希（清单里那串 64 个字符）最容易
// 出错的就是大小写与位数——比对前先过这一道，比对失败时报"哈希格式
// 不对"而不是"文件坏了"，两回事对用户完全不同。
bool is_sha256_hex(const std::string& text);

}  // namespace UnidictCoreStd
