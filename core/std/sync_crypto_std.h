#pragma once

// 同步链路的密钥管理层（server_plan §7 B3-a）：组密钥生命周期 + 指令
// 加解密封装 + 动态配对码。密码学原语在 crypto_std.h，本层只做语义编排。
//
// 红线口径（§6）：
// - 密钥不出端：加解密都发生在本层内，接口只进明文/出密封串
// - 指令一律 XChaCha20-Poly1305，密封形态 = 版本(4B LE) || nonce(24B) ||
//   ct || tag(16B)；版本字段明文，用于轮换后选钥解在途指令
// - AAD 绑定用途上下文（kSyncCommandAad），指令密文不能被裁剪到别的
//   用途重放
//
// B3-a 范围注记：SPAKE2 换钥是 B3-b（未实现）；import_key() 是换钥完成
// 后的注入面。配对码只是人类转述的口令输入，不是密钥——本层不提供任
// 何「短码直接派生密钥」的路径，防降级误用。

#include <cstdint>
#include <string>
#include <unordered_map>

#include "crypto_std.h"

namespace UnidictCoreStd {

// 指令 AAD 上下文；协议字段演进时递增版本号
constexpr const char* kSyncCommandAad = "unidict/sync/cmd/v1";

constexpr std::size_t kSyncKeyLen = 32;        // 组密钥长度
constexpr std::size_t kSyncVersionLen = 4;     // 版本字段（LE u32）
constexpr std::size_t kSyncSealedOverhead = kSyncVersionLen + kXchaChaNonceLen + kAeadTagLen;

// 组密钥环：当前版本 + 轮换保留的历史版本（解在途指令用）。
// 非线程安全（同步引擎单线程驱动的口径，与 sync_engine_std 一致）。
class SyncKeyRingStd {
public:
    // 新建组：随机组密钥、版本从 1 起。已持钥时先整体擦除再建
    // （语义 = 重建组，旧密文全部作废）。
    void init_new_group();

    // 注入既定版本的组密钥（B3-b PAKE 换钥完成后调用）。版本必须大于
    // 当前版本（历史钥不允许被旧值覆盖）；key 长度必须 kSyncKeyLen。
    // 违反抛 std::invalid_argument。
    void import_key(std::uint32_t version, const std::string& key32);

    bool has_current() const { return current_version_ != 0; }
    std::uint32_t current_version() const { return current_version_; }

    // 轮换：生成新随机钥作为当前版本，旧钥保留解在途指令。返回新版本
    // 号。未持钥抛 std::logic_error。
    std::uint32_t rotate();

    // 组退出/重置：擦除并清空全部版本密钥（含当前）。
    void destroy();

    // 用当前版本钥封指令。未持钥抛 std::logic_error。
    std::string seal_command(const std::string& plaintext) const;

    // 按头版本解指令（当前或保留版本均可）。长度不足/版本未知/tag 校
    // 验失败返回 false 且不动 out_plaintext。
    bool open_command(const std::string& sealed, std::string& out_plaintext) const;

private:
    std::uint32_t current_version_ = 0;
    std::unordered_map<std::uint32_t, std::string> keys_;
};

// ---- 动态配对码 ----

// 8 字符 Crockford Base32（40 bit），来源 5 字节 CSPRNG
constexpr std::size_t kPairingCodeLen = 8;
// 默认时效 10 分钟；签发侧按设备就近程度可传更短
constexpr std::uint64_t kPairingDefaultTtlMs = 600 * 1000;

struct PairingCodeStd {
    std::string code;                  // 规范形态（大写 Crockford 字母表）
    std::uint64_t issued_at_ms = 0;
    std::uint64_t expires_at_ms = 0;
};

// 5 字节熵 → 8 字符规范码（高位在前，每字符 5 bit）。bytes 必须
// kPairingCodeLen*5/8 = 5 字节，否则抛 std::invalid_argument。
std::string pairing_code_encode(const std::string& bytes5);

// 用户转述输入的归一化：剔除空白/连字符、大写、Crockford 别名映射
// （O→0、I/L→1）；出现字母表外字符（含 U）返回空串。
std::string pairing_code_normalize(const std::string& raw);

class PairingCodeManagerStd {
public:
    // 签发新码并作废旧码（同一时刻至多一个活跃码）。时间由调用方注入
    // （core/ 不摸系统时钟，测试保持确定性）。
    PairingCodeStd issue(std::uint64_t now_ms,
                         std::uint64_t ttl_ms = kPairingDefaultTtlMs);

    // 校验并消费：归一化后与活跃码相等、未过期、未用过 → true 且标记
    // 已用（单次有效）。任何不满足返回 false。
    bool consume(const std::string& code, std::uint64_t now_ms);

    bool has_active() const { return !active_.code.empty(); }
    const PairingCodeStd& active() const { return active_; }

    // 作废当前码（组解散/重置口令）。
    void reset() { active_ = PairingCodeStd{}; consumed_ = false; }

private:
    PairingCodeStd active_;
    bool consumed_ = false;
};

}  // namespace UnidictCoreStd
