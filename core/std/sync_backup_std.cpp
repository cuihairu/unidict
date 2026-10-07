#include "sync_backup_std.h"

#include <cstring>
#include <stdexcept>

#include "crypto_std.h"

namespace UnidictCoreStd {

namespace {

constexpr std::size_t kMagicLen = 12;
const char kMagic[kMagicLen] = {'U', 'N', 'I', 'D', 'I', 'C', 'T',
                                '-', 'B', 'K', '1', '\n'};
constexpr std::size_t kItersLen = 4;
constexpr std::size_t kHeaderLen =
    kMagicLen + kBackupSaltLen + kItersLen + kXchaChaNonceLen;

std::string le32(uint32_t v) {
    return std::string({static_cast<char>(v & 0xff),
                        static_cast<char>((v >> 8) & 0xff),
                        static_cast<char>((v >> 16) & 0xff),
                        static_cast<char>((v >> 24) & 0xff)});
}

uint32_t le32_read(const std::string& s, std::size_t off) {
    const unsigned char* p =
        reinterpret_cast<const unsigned char*>(s.data()) + off;
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

std::string derive_key(const std::string& passphrase, const std::string& salt,
                       uint32_t iterations) {
    return pbkdf2_hmac_sha256(passphrase, salt, iterations, kAeadKeyLen);
}

}  // namespace

std::string sync_backup_export(const SyncVocabStateStd& state,
                               const std::string& passphrase,
                               uint32_t iterations) {
    if (passphrase.empty()) throw std::invalid_argument("passphrase required");
    const std::string salt = random_bytes(kBackupSaltLen);
    const std::string nonce = random_bytes(kXchaChaNonceLen);
    std::string header;
    header.reserve(kHeaderLen);
    header.append(kMagic, kMagicLen);
    header += salt;
    header += le32(iterations);
    header += nonce;
    // AAD = 整个头部：改 salt/iters/nonce 任一字节都换钥换 nonce，AEAD
    // 校验必挂——头部与密文绑定，不给人拼装空间
    const std::string sealed =
        aead_xchacha20poly1305_seal(derive_key(passphrase, salt, iterations),
                                    serialize_state(state), header);
    return header + sealed;
}

bool sync_backup_import(const std::string& blob, const std::string& passphrase,
                        SyncVocabStateStd& out_state, std::string* err) {
    if (passphrase.empty()) {
        if (err) *err = "passphrase required";
        return false;
    }
    if (blob.size() < kHeaderLen + kAeadTagLen) {
        if (err) *err = "backup file too short";
        return false;
    }
    if (std::memcmp(blob.data(), kMagic, kMagicLen) != 0) {
        if (err) *err = "not a unidict backup file";
        return false;
    }
    const std::string salt = blob.substr(kMagicLen, kBackupSaltLen);
    const uint32_t iterations = le32_read(blob, kMagicLen + kBackupSaltLen);
    if (iterations == 0) {
        if (err) *err = "bad iterations field";
        return false;
    }
    const std::string header = blob.substr(0, kHeaderLen);
    const std::string sealed = blob.substr(kHeaderLen);
    std::string plain;
    // 口令错与密文篡改同归 AEAD 校验失败——不区分（少一个探测面）
    if (!aead_xchacha20poly1305_open(
            sealed, derive_key(passphrase, salt, iterations), header, plain)) {
        if (err) *err = "wrong passphrase or corrupted backup";
        return false;
    }
    out_state = parse_state(plain);  // 畸形 JSON 按可读前缀容错
    return true;
}

}  // namespace UnidictCoreStd
