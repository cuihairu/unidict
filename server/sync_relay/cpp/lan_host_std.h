// 局域网直传宿主（server_plan §7 B5 剩余增量三）：本机充当组内设备兼
// 中转——HTTP 面与 unidict-relay 同一份契约路由（relay_routes.h 字节
// 等价，PROTOCOL.md v1），外加 UDP 发现应答（同网设备「扫描」即见）。
//
// 红线不变：宿主只见密文（payload 对本层不透明），发现面只报元信息
// （服务名/协议版本/HTTP 端口/设备名），不携任何数据与密钥。
// 线程模型：HTTP accept 循环跑在独立线程（HttpServerStd::stop 停机），
// UDP 应答循环一线程（SO_RCVTIMEO 周期醒检 running_ + close 唤醒）。
#ifndef UNIDICT_LAN_HOST_STD_H
#define UNIDICT_LAN_HOST_STD_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "relay_http.h"
#include "relay_routes.h"

namespace UnidictRelay {

class SyncLanHostStd {
public:
    SyncLanHostStd() = default;
    ~SyncLanHostStd();  // stop() 兜底
    SyncLanHostStd(const SyncLanHostStd&) = delete;
    SyncLanHostStd& operator=(const SyncLanHostStd&) = delete;

    // 发现协议常量（PROTOCOL.md §7 v1）
    static constexpr int kDefaultDiscoveryPort = 8789;
    static constexpr const char* kQueryService = "unidict-sync-lan";
    static constexpr const char* kReplyService = "unidict-sync-relay";
    // 设备名上限（字节；超限截断，接收端按 UTF-8 容错）
    static constexpr std::size_t kMaxDeviceNameBytes = 128;

    // 起宿主：HTTP 契约面（bind_host:http_port，port 0 内核挑口）+ UDP
    // 发现应答（udp_port）。data_dir 非空时组状态落盘（宿主重启组内
    // 位点不丢），空 = 纯内存。device_name 进发现 reply（JSON 转义）。
    // 任一步失败返回 false 且不留半开状态。
    bool start(const std::string& bind_host, int http_port, int udp_port,
               const std::string& data_dir, const std::string& device_name,
               std::string* err);

    // 停机：HTTP accept 与 UDP 应答线程收尾 join。幂等。
    void stop();

    int http_port() const { return http_.port(); }
    // 实际生效的发现口（udp_port 传 0 内核挑口后由此取）
    int udp_port() const { return udp_port_; }
    bool running() const { return running_.load(); }

private:
    void udp_loop();

    HttpServerStd http_;
    std::unique_ptr<RelayRoutes> routes_;
    std::string device_name_;
    std::atomic<uintptr_t> udp_fd_{(uintptr_t)-1};
    std::atomic<bool> running_{false};
    std::thread http_thread_;
    std::thread udp_thread_;
    int udp_port_ = 0;
};

}  // namespace UnidictRelay

#endif  // UNIDICT_LAN_HOST_STD_H
