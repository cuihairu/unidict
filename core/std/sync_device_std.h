// B4 设备面（server_plan §7）：同步组设备清单与成员管理。
// - 设备元信息：device_id/名称/平台/最近同步/备注/本机标注
// - 同等权限：任意设备可自由进/出组（动态密码入组、主动退出）
// - 仅改自己备注：改不了别人的
// - 密钥不出端：本层只管元数据，密钥与指令加密在 sync_crypto_std

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace UnidictCoreStd {

// 设备平台标识
enum class DevicePlatform {
    Unknown = 0,
    Windows,
    macOS,
    Linux,
    Android,
    iOS,
    HarmonyOS,
    Web,
};

// 同步组内单个设备的元信息
struct SyncDeviceInfoStd {
    std::string device_id;         // 设备唯一标识（引擎生成的 128-bit hex）
    std::string name;              // 用户可读名称（默认 = 平台+随机后缀）
    DevicePlatform platform = DevicePlatform::Unknown;
    std::string platform_string;   // 任意平台字符串（备用）
    uint64_t last_seen_ms = 0;     // 最近同步/心跳时间（毫秒纪元）
    std::string remark;            // 用户自定义备注（仅本设备可改）
    bool is_current = false;       // 标注本机
};

// 设备管理器：维护同步组内的设备清单（内存态；持久化由引擎层负责）
class SyncDeviceManagerStd {
public:
    // 最大设备数上限（防滥用）
    static constexpr size_t kMaxDevices = 64;

    SyncDeviceManagerStd() = default;
    explicit SyncDeviceManagerStd(const std::string& current_device_id);

    // 设置/更新本机 device_id（启动时由引擎注入）
    void set_current_device_id(const std::string& device_id);

    // 当前设备信息（只读）
    const SyncDeviceInfoStd* current_device() const;

    // 全量设备列表（按 last_seen_ms 降序，本机置顶）
    std::vector<SyncDeviceInfoStd> list_devices() const;

    // 查找设备（按 device_id）
    const SyncDeviceInfoStd* find_device(const std::string& device_id) const;

    // ---- 成员变更操作（返回 false = 失败，err 说明） ----

    // 入组：新设备经配对码加入（由持有组密钥的既有设备调用 sign_device_entry()
    // 生成带签名的入组凭证，新设备收到后调用此接口）。为简化 B4 范围，
    // 这里仅做元数据登记（签名验证归 B5 传输层/协议层）。
    // 返回新设备 device_id（同入参），失败返回空串。
    std::string join_group(const std::string& device_id,
                           const std::string& name,
                           DevicePlatform platform,
                           const std::string& platform_string,
                           std::string* err);

    // 主动退组：从清单移除（含本机）。返回 true=已移除，false=不在组内。
    bool leave_group(const std::string& device_id);

    // 更新自己的备注（仅允许改当前设备的 remark）
    bool update_own_remark(const std::string& remark, std::string* err);

    // 心跳/同步完成时刷新 last_seen_ms（由引擎调用）
    void touch_device(const std::string& device_id, uint64_t now_ms);

    // 设备改名（仅当前设备可改自己的 name）
    bool update_own_name(const std::string& name, std::string* err);

    // 序列化/反序列化（JSON，供引擎 save_state/load_state 调用）。
    // 往返无损：引号/反斜杠/控制字符经标准 JSON 转义，非法转义宽松
    // 收敛（字面保留/孤立代理 U+FFFD）；devices 空数组合法（零设备态）
    std::string to_json() const;
    bool from_json(const std::string& json, std::string* err);

    // 清空（退出组时调用）
    void clear();

private:
    std::string current_device_id_;
    std::unordered_map<std::string, SyncDeviceInfoStd> devices_;
};

// 辅助：平台枚举 → 字符串（UI 显示用）
std::string device_platform_to_string(DevicePlatform p);
// 辅助：字符串 → 平台枚举（解析用，未知归 Unknown）
DevicePlatform device_platform_from_string(const std::string& s);

}  // namespace UnidictCoreStd