#include "lan_host_std.h"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket kInvalidSocket = -1;
#endif

namespace UnidictRelay {

namespace {

constexpr std::size_t kMaxUdpPacket = 1024;  // 发现包很小，防滥用即可

void close_udp(Socket fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    ::close(fd);
#endif
}

}  // namespace

SyncLanHostStd::~SyncLanHostStd() { stop(); }

bool SyncLanHostStd::start(const std::string& bind_host, int http_port,
                           int udp_port, const std::string& data_dir,
                           const std::string& device_name, std::string* err) {
    if (running_.load()) {
        if (err) *err = "lan host already running";
        return false;
    }
    routes_ = std::make_unique<RelayRoutes>(data_dir);
    http_.set_handler([this](const HttpRequest& req) {
        return routes_->handle(req);
    });
    if (!http_.start(bind_host, http_port, err)) {
        routes_.reset();
        return false;
    }
    device_name_ = device_name.substr(0, kMaxDeviceNameBytes);

    // UDP 发现口：bind 失败整体回滚（不留半开宿主）
    const Socket fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == kInvalidSocket) {
        if (err) *err = "udp socket() failed";
        http_.stop();
        routes_.reset();
        return false;
    }
#ifdef _WIN32
    // 同机多宿主共存（SO_REUSEADDR on UDP：Windows 语义即端口复用）
    int reuse = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse,
                 sizeof reuse);
#else
    int reuse = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 发现面听所有口（同网可见）
    addr.sin_port = htons((unsigned short)udp_port);
    if (::bind(fd, (sockaddr*)&addr, sizeof addr) != 0) {
        if (err) *err = "udp bind() failed (discovery port in use?)";
        close_udp(fd);
        http_.stop();
        routes_.reset();
        return false;
    }
    {
        sockaddr_in got{};
        socklen_t len = sizeof got;
        if (::getsockname(fd, (sockaddr*)&got, &len) == 0) {
            udp_port_ = ntohs(got.sin_port);
        }
    }
    // 应答线程周期醒检 running_（收包超时 500ms），停机不依赖 close 唤醒
    struct timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 500 * 1000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    udp_fd_.store((uintptr_t)fd);
    running_.store(true);
    udp_thread_ = std::thread(&SyncLanHostStd::udp_loop, this);
    http_thread_ = std::thread([this] { http_.run(); });  // accept 循环
    return true;
}

void SyncLanHostStd::stop() {
    if (!running_.exchange(false)) return;
    http_.stop();
    const uintptr_t fd = udp_fd_.exchange((uintptr_t)-1);
    if (fd != (uintptr_t)-1) close_udp((Socket)fd);
    if (http_thread_.joinable()) http_thread_.join();
    if (udp_thread_.joinable()) udp_thread_.join();
    routes_.reset();
}

void SyncLanHostStd::udp_loop() {
    const Socket fd = (Socket)udp_fd_.load();
    if (fd == kInvalidSocket) return;
    char buf[kMaxUdpPacket];
    while (running_.load(std::memory_order_acquire)) {
        sockaddr_in src{};
#ifdef _WIN32
        int src_len = (int)sizeof src;
#else
        socklen_t src_len = sizeof src;
#endif
        const int n = ::recvfrom(fd, buf, sizeof buf - 1, 0,
                                 (sockaddr*)&src, &src_len);
        if (n <= 0) continue;  // 超时醒检 running_ / 停机已关 fd
        if (!running_.load(std::memory_order_acquire)) break;
        buf[n] = '\0';

        // 只回匹配的发现 query（PROTOCOL §7）；其余包一律忽略
        JsonValue q;
        if (!json_parse(std::string(buf, (std::size_t)n), &q) ||
            !q.is_object()) {
            continue;
        }
        const JsonValue* svc = q.find("service");
        const JsonValue* proto = q.find("protocol");
        if (!svc || !svc->is_string() || svc->str != kQueryService ||
            !proto || !proto->is_int() || proto->integer != 1) {
            continue;
        }

        // reply 单播回源：只报元信息（服务/协议/HTTP 端口/设备名）
        const std::string reply =
            std::string("{\"service\":\"") + kReplyService +
            "\",\"protocol\":" +
            std::to_string(UnidictCoreStd::kRelayProtocolVersion) +
            ",\"device\":" + json_quote(device_name_) +
            ",\"port\":" + std::to_string(http_.port()) + "}";
        ::sendto(fd, reply.data(), (int)reply.size(), 0, (sockaddr*)&src,
                 src_len);
    }
}

}  // namespace UnidictRelay
