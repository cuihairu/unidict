#include "std/sync_crypto_std.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace UnidictCoreStd {

namespace {

std::string put_version_le32(std::uint32_t v) {
    std::string s(4, '\0');
    s[0] = (char)(v & 0xff);
    s[1] = (char)((v >> 8) & 0xff);
    s[2] = (char)((v >> 16) & 0xff);
    s[3] = (char)((v >> 24) & 0xff);
    return s;
}

std::uint32_t read_version_le32(const std::string& s) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.data());
    return (std::uint32_t)p[0] | ((std::uint32_t)p[1] << 8) |
           ((std::uint32_t)p[2] << 16) | ((std::uint32_t)p[3] << 24);
}

}  // namespace

// ---- SyncKeyRingStd ----

void SyncKeyRingStd::init_new_group() {
    destroy();
    import_key(1, random_bytes(kSyncKeyLen));
}

void SyncKeyRingStd::import_key(std::uint32_t version, const std::string& key32) {
    if (key32.size() != kSyncKeyLen)
        throw std::invalid_argument("sync group key must be 32 bytes");
    if (version == 0 || version <= current_version_)
        throw std::invalid_argument("sync key version must be newer than current");
    keys_[version] = key32;
    current_version_ = version;
}

std::uint32_t SyncKeyRingStd::rotate() {
    if (!has_current()) throw std::logic_error("rotate on empty key ring");
    const std::uint32_t next = current_version_ + 1;
    import_key(next, random_bytes(kSyncKeyLen));
    return next;
}

void SyncKeyRingStd::destroy() {
    for (auto& kv : keys_) secure_wipe(kv.second);
    keys_.clear();
    current_version_ = 0;
}

std::string SyncKeyRingStd::seal_command(const std::string& plaintext) const {
    if (!has_current()) throw std::logic_error("seal on empty key ring");
    const std::string body =
        aead_xchacha20poly1305_seal(keys_.at(current_version_), plaintext, kSyncCommandAad);
    return put_version_le32(current_version_) + body;
}

bool SyncKeyRingStd::open_command(const std::string& sealed,
                                  std::string& out_plaintext) const {
    if (sealed.size() < kSyncSealedOverhead) return false;
    const std::uint32_t version = read_version_le32(sealed);
    const auto it = keys_.find(version);
    if (it == keys_.end()) return false;
    return aead_xchacha20poly1305_open(sealed.substr(kSyncVersionLen), it->second,
                                       kSyncCommandAad, out_plaintext);
}

// ---- 配对码 ----

// Crockford Base32 字母表（0-9 + 去 I/L/O/U 的 22 个大写字母）
constexpr const char* kCrockfordAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

std::string pairing_code_encode(const std::string& bytes5) {
    if (bytes5.size() != 5) throw std::invalid_argument("pairing code needs 5 entropy bytes");
    // 40 bit 高位在前，每 5 bit 查表
    std::uint64_t bits = 0;
    for (unsigned char ch : bytes5) bits = (bits << 8) | ch;
    std::string out(kPairingCodeLen, '0');
    for (std::size_t i = 0; i < kPairingCodeLen; ++i)
        out[i] = kCrockfordAlphabet[(bits >> (5 * (7 - i))) & 0x1f];
    return out;
}

std::string pairing_code_normalize(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (const char ch : raw) {
        if (ch == '-' || ch == ' ' || ch == '\t') continue;  // 展示分组的分隔符
        char up = ch;
        if (ch >= 'a' && ch <= 'z') up = (char)(ch - 'a' + 'A');
        switch (up) {
            case 'O': up = '0'; break;
            case 'I':
            case 'L': up = '1'; break;
            case 'U': return {};  // Crockford 排除字母，按非法处理
            default: break;
        }
        bool legal = (up >= '0' && up <= '9');
        for (const char* p = kCrockfordAlphabet + 10; *p; ++p) {
            if (up == *p) {
                legal = true;
                break;
            }
        }
        if (!legal) return {};
        out += up;
    }
    return out;
}

PairingCodeStd PairingCodeManagerStd::issue(std::uint64_t now_ms, std::uint64_t ttl_ms) {
    // 单活跃码：新码签发即作废旧码（旧的 consume 一律 false）
    active_.code = pairing_code_encode(random_bytes(5));
    active_.issued_at_ms = now_ms;
    active_.expires_at_ms = now_ms + ttl_ms;
    consumed_ = false;
    return active_;
}

bool PairingCodeManagerStd::consume(const std::string& code, std::uint64_t now_ms) {
    if (consumed_ || active_.code.empty()) return false;
    if (now_ms >= active_.expires_at_ms) return false;
    if (pairing_code_normalize(code) != active_.code) return false;
    consumed_ = true;
    return true;
}

// ---- 组密钥环持久化 ----

bool serialize_keyring(const SyncKeyRingStd& ring, std::string* out) {
    if (!out) return false;
    // 版本升序（map 收集 + 排序，序列化逐字节确定）
    std::vector<std::uint32_t> versions;
    versions.reserve(ring.keys_.size());
    for (const auto& kv : ring.keys_) versions.push_back(kv.first);
    std::sort(versions.begin(), versions.end());

    out->clear();
    out->append("UNIDICT-KR1", 11);
    const std::uint32_t count = static_cast<std::uint32_t>(versions.size());
    for (int i = 0; i < 4; ++i)
        out->push_back(static_cast<char>((count >> (8 * i)) & 0xff));
    for (std::uint32_t v : versions) {
        for (int i = 0; i < 4; ++i)
            out->push_back(static_cast<char>((v >> (8 * i)) & 0xff));
        out->append(ring.keys_.at(v));
    }
    return true;
}

bool parse_keyring(const std::string& blob, SyncKeyRingStd* out) {
    if (!out) return false;
    if (blob.size() < 15 || blob.compare(0, 11, "UNIDICT-KR1") != 0) {
        return false;
    }
    auto u32_at = [&blob](std::size_t pos) -> std::uint32_t {
        return static_cast<std::uint8_t>(blob[pos]) |
               (static_cast<std::uint32_t>(
                    static_cast<std::uint8_t>(blob[pos + 1]))
                << 8) |
               (static_cast<std::uint32_t>(
                    static_cast<std::uint8_t>(blob[pos + 2]))
                << 16) |
               (static_cast<std::uint32_t>(
                    static_cast<std::uint8_t>(blob[pos + 3]))
                << 24);
    };
    const std::uint32_t count = u32_at(11);
    // 每条 36B：版本 4 + 密钥 32；size 溢出/不够长即拒
    if (blob.size() != 15 + static_cast<std::size_t>(count) * 36) return false;

    // 先在临时环上按升序重建（import 要求版本严格递增），全部通过才
    // 换入 out——半截坏文件不动调用方状态
    SyncKeyRingStd rebuilt;
    std::uint32_t prev = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t off = 15 + static_cast<std::size_t>(i) * 36;
        const std::uint32_t version = u32_at(off);
        if (version == 0 || version <= prev) return false;
        rebuilt.import_key(version, blob.substr(off + 4, 32));
        prev = version;
    }
    *out = std::move(rebuilt);
    return true;
}

}  // namespace UnidictCoreStd
