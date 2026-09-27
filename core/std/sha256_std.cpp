#include "std/sha256_std.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace UnidictCoreStd {
namespace {

// FIPS 180-4 §4.2.2 的 64 个轮常量：前 8 个是质数的小数部分开方，其余
// 是前面若干个数的立方根小数部分
const std::uint32_t kK[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

const std::uint32_t kInitState[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
                                     0xa54ff53au, 0x510e527fu, 0x9b05688cu,
                                     0x1f83d9abu, 0x5be0cd19u};

inline std::uint32_t rotr(std::uint32_t x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}

}  // namespace

Sha256HasherStd::Sha256HasherStd() { reset(); }

void Sha256HasherStd::reset() {
    std::memcpy(state_, kInitState, sizeof(state_));
    std::memset(buf_, 0, sizeof(buf_));
    buf_len_ = 0;
    byte_len_ = 0;
    done_ = false;
    digest_.clear();
}

void Sha256HasherStd::compress(const unsigned char* block) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        // 大端读入：SHA 的消息调度是字节序显式的，不能靠主机端
        w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
               (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<std::uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 =
            rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 =
            rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + s1 + ch + kK[i] + w[i];
        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256HasherStd::update(const void* data, std::size_t len) {
    if (done_ || len == 0) {
        return;  // 收尾后再喂 = 编程错误，静默忽略而不是把 padding 算花
    }
    const unsigned char* p = static_cast<const unsigned char*>(data);
    byte_len_ += len;
    // 先把攒着的半块填满
    if (buf_len_ > 0) {
        const std::size_t need = 64 - buf_len_;
        const std::size_t take = len < need ? len : need;
        std::memcpy(buf_ + buf_len_, p, take);
        buf_len_ += take;
        p += take;
        len -= take;
        if (buf_len_ == 64) {
            compress(buf_);
            buf_len_ = 0;
        }
    }
    // 整块直接压（std::size_t 与 64 同宽，切块不越界）
    while (len >= 64) {
        compress(p);
        p += 64;
        len -= 64;
    }
    if (len > 0) {
        std::memcpy(buf_, p, len);
        buf_len_ = len;
    }
}

void Sha256HasherStd::update(const std::string& data) {
    update(data.data(), data.size());
}

std::string Sha256HasherStd::hex() {
    if (!done_) {
        // padding：0x80 + 若干 0x00 补到 56 字节，再写 8 字节大端长度
        const std::uint64_t bits = byte_len_ * 8;
        buf_[buf_len_++] = 0x80;
        if (buf_len_ > 56) {
            std::memset(buf_ + buf_len_, 0, 64 - buf_len_);
            compress(buf_);
            buf_len_ = 0;
        }
        std::memset(buf_ + buf_len_, 0, 56 - buf_len_);
        for (int i = 0; i < 8; ++i) {
            buf_[56 + i] = static_cast<unsigned char>((bits >> (56 - 8 * i)) & 0xff);
        }
        compress(buf_);
        buf_len_ = 0;
        done_ = true;
        static const char* kHex = "0123456789abcdef";
        digest_.reserve(64);
        for (std::uint32_t word : state_) {
            for (int i = 3; i >= 0; --i) {
                const unsigned byte = (word >> (8 * i)) & 0xff;
                digest_ += kHex[byte >> 4];
                digest_ += kHex[byte & 0x0f];
            }
        }
    }
    return digest_;
}

std::string sha256_hex(const void* data, std::size_t len) {
    Sha256HasherStd h;
    h.update(data, len);
    return h.hex();
}

std::string sha256_hex(const std::string& data) {
    return sha256_hex(data.data(), data.size());
}

bool sha256_file_hex(const std::string& path, std::string& out_hex,
                     std::string& err) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        err = "cannot open: " + path;
        return false;
    }
    // 1MiB 一块：600MB 的模型读 600 次，内存只占这一块
    std::vector<char> buf(1u << 20);
    Sha256HasherStd h;
    while (in.read(buf.data(), static_cast<std::streamsize>(buf.size())) ||
           in.gcount() > 0) {
        h.update(buf.data(), static_cast<std::size_t>(in.gcount()));
    }
    if (in.bad()) err = "read failed: " + path;  // 半途 I/O 错误不当成"算完了"
    if (!err.empty()) return false;
    out_hex = h.hex();
    return true;
}

bool is_sha256_hex(const std::string& text) {
    if (text.size() != 64) {
        return false;
    }
    for (const char c : text) {
        const bool digit = c >= '0' && c <= '9';
        const bool lower = c >= 'a' && c <= 'f';
        if (!digit && !lower) {
            return false;
        }
    }
    return true;
}

}  // namespace UnidictCoreStd
