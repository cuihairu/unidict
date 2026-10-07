// B7 自救口（server_plan §7「导出加密备份(防设备全丢)」）：把同步态
// 全量（生词本/笔记/标签/历史/偏好/安装清单）用口令加密成可落盘字节，
// 换机/丢机时凭口令恢复。
//
// 加密口径：PBKDF2-HMAC-SHA256 拉伸口令（低熵口令不入钥）→
// XChaCha20-Poly1305 AEAD；AAD 绑定定长头部（magic/salt/iters），
// 头部任何篡改在解密前即被 AEAD 拒收。备份文件不经过任何中转，
// 口令派生钥不出端——与同步红线同源（只多一道口令因素，防单机
// 全丢后备份文件本身泄露）。
//
// 文件形态（定长头 + 密文，二进制）：
//   magic   12 字节 "UNIDICT-BK1\n"
//   salt    16 字节（随机）
//   iters   4 字节 LE（PBKDF2 轮数；0 非法）
//   nonce   24 字节（随机）
//   sealed  密文 || tag16（AEAD；明文 = serialize_state 确定性 JSON）

#ifndef UNIDICT_SYNC_BACKUP_STD_H
#define UNIDICT_SYNC_BACKUP_STD_H

#include <cstdint>
#include <string>

#include "sync_engine_std.h"

namespace UnidictCoreStd {

constexpr std::size_t kBackupSaltLen = 16;
// 口令拉伸轮数缺省：桌面毫秒级、移动端亚秒级；显式参数仅供测试加速
constexpr uint32_t kBackupDefaultIterations = 100000;

// 导出：state 全量密封为完整文件字节（落盘由调用方负责）。passphrase
// 为空拒绝（无口令的"加密"备份等于明文落盘——自救口不做这种假安全）。
// 随机源失败按 random_bytes 异常向上抛。
std::string sync_backup_export(const SyncVocabStateStd& state,
                               const std::string& passphrase,
                               uint32_t iterations = kBackupDefaultIterations);

// 导入：口令错/文件篡改/截断/形态不认返回 false 并写 err（成功不写
// err，仓库口径）；out_state 仅在成功时替换。
bool sync_backup_import(const std::string& blob,
                        const std::string& passphrase,
                        SyncVocabStateStd& out_state, std::string* err);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_SYNC_BACKUP_STD_H
