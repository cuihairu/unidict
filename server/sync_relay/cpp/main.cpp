// unidict sync relay——C++ 版自带中转（server_plan §7 B5；PROTOCOL.md v1）。
//
// 单二进制、仅标准库（Qt-free），契约语义复用 core 的 SyncRelayStateStd：
// 与 dev 参考实现（Python）同一份契约符合性套件验收（run_conformance.py
// 经 UNIDICT_RELAY_EXTERNAL_BASE 打到本进程）。路由面在 relay_routes.h
//（与局域网直传嵌入宿主共用同一份，字节等价）。
//
// 默认只听 127.0.0.1（server_plan §3.1 隐私口径：对外暴露是显式动作）；
// 不带 --data 纯内存运行，带 --data 状态原子落盘跨重启保留。
#include <cstdio>
#include <cstring>
#include <string>

#include "relay_http.h"
#include "relay_routes.h"
#include "std/sync_relay_state_std.h"

using namespace UnidictRelay;

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    int port = 8788;
    std::string data_dir;
    for (int i = 1; i < argc; ++i) {
        const auto need_value = [&](const char** out) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s 需要一个值\n", argv[i]);
                std::exit(2);
            }
            *out = argv[++i];
        };
        if (std::strcmp(argv[i], "--host") == 0) {
            const char* v;
            need_value(&v);
            host = v;
        } else if (std::strcmp(argv[i], "--port") == 0) {
            const char* v;
            need_value(&v);
            port = std::atoi(v);
        } else if (std::strcmp(argv[i], "--data") == 0) {
            const char* v;
            need_value(&v);
            data_dir = v;
        } else {
            std::fprintf(stderr,
                         "用法: unidict-relay [--host H] [--port P] [--data DIR]\n");
            return 2;
        }
    }

    RelayRoutes app(data_dir);
    HttpServerStd server;
    server.set_handler([&app](const HttpRequest& req) { return app.handle(req); });
    std::string err;
    if (!server.start(host, port, &err)) {
        std::fprintf(stderr, "unidict-relay: %s\n", err.c_str());
        return 1;
    }
    std::fprintf(stderr,
                 "unidict sync relay (cpp, protocol %d) listening on http://%s:%d\n",
                 UnidictCoreStd::kRelayProtocolVersion, host.c_str(), server.port());
    server.run();  // 不返回
    return 0;
}
