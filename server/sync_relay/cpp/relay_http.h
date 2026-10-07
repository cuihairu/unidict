// unidict-relay 的传输壳：极简 HTTP/1.1 服务 + 契约所需的极简 JSON。
//
// 只服务 PROTOCOL.md v1 的路由面：请求行/头/体解析（Content-Length 定界，
// 应答恒 Connection: close）、线程 per 连接（服务生命周期 = 进程生命周期，
// 连接线程 detach）。契约语义全在 core 的 SyncRelayStateStd，本层只做
// 路由与编解码。不做内建 TLS（PROTOCOL §0：反代/隧道负责）。
#ifndef UNIDICT_RELAY_HTTP_H
#define UNIDICT_RELAY_HTTP_H

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace UnidictRelay {

// ---- 极简 JSON（契约面够用：对象/数组/字符串/整数/浮点/bool/null） ----

struct JsonValue {
    enum class Type { Null, Bool, Int, Double, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    long long integer = 0;
    double number = 0.0;
    std::string str;
    std::vector<JsonValue> items;
    std::vector<std::pair<std::string, JsonValue>> members;  // 保序；小体线性查

    const JsonValue* find(const std::string& key) const;
    bool is_string() const { return type == Type::String; }
    bool is_int() const { return type == Type::Int; }
    bool is_object() const { return type == Type::Object; }
    bool is_array() const { return type == Type::Array; }
};

// 严格 JSON（RFC 8259 子集）：坏文法返回 false。嵌套深度限 64。
bool json_parse(const std::string& text, JsonValue* out);

// 字符串转 JSON 字面量（引号+转义，控制字符 \u00XX，UTF-8 原样通过）
std::string json_quote(const std::string& s);

// ---- HTTP ----

struct HttpRequest {
    std::string method;
    std::string target;  // 原始 path?query（不做百分号解码——gid 白名单
                         // 字符集不含 %，编码形态自然被形态校验拒掉）
    std::map<std::string, std::string> query;  // 同名取首值
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string body;
};

class HttpServerStd {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    HttpServerStd() = default;
    ~HttpServerStd();
    HttpServerStd(const HttpServerStd&) = delete;
    HttpServerStd& operator=(const HttpServerStd&) = delete;

    // 绑定+监听；port=0 由内核挑空闲口（start 后 port() 可取实际值）
    bool start(const std::string& host, int port, std::string* err);
    int port() const { return port_; }

    void set_handler(Handler h) { handler_ = std::move(h); }

    // accept 循环（阻塞不返回；每连接一线程，detach）
    void run();

private:
    // 平台套接字句柄以 uintptr_t 承载（POSIX int / Windows SOCKET）
    void handle_connection(uintptr_t fd);

    uintptr_t listen_fd_ = (uintptr_t)-1;  // kInvalid
    int port_ = 0;
    Handler handler_;
};

}  // namespace UnidictRelay

#endif  // UNIDICT_RELAY_HTTP_H
