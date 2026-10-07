#include "relay_http.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// windows.h 的 min/max 宏会打断 std::min/std::max 调用（MSVC C2589，
// CI windows 两腿实锤），仓库惯例同 core/std/mdd_resource_std.cpp
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
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket kInvalidSocket = -1;
#endif

namespace UnidictRelay {

namespace {

// PROTOCOL §3：请求体上限 64 MiB；头部上限给个防御值
constexpr std::size_t kMaxBodyBytes = 64ull * 1024 * 1024;
constexpr std::size_t kMaxHeaderBytes = 32 * 1024;
constexpr int kMaxJsonDepth = 64;

void close_socket(Socket fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    ::close(fd);
#endif
}

bool send_all(Socket fd, const char* data, std::size_t len) {
    std::size_t sent = 0;
    while (sent < len) {
        const int n = ::send(fd, data + sent, (int)std::min(len - sent,
                                                    (std::size_t)0x7fffffff), 0);
        if (n <= 0) return false;
        sent += (std::size_t)n;
    }
    return true;
}

// 读到定界符（\r\n\r\n 出现）为止，头部总量超限视为坏请求
bool read_headers(Socket fd, std::string* out) {
    out->clear();
    char buf[4096];
    while (out->find("\r\n\r\n") == std::string::npos) {
        if (out->size() > kMaxHeaderBytes) return false;
        const int n = ::recv(fd, buf, (int)sizeof(buf), 0);
        if (n <= 0) return false;
        out->append(buf, (std::size_t)n);
    }
    return true;
}

bool read_exact(Socket fd, std::size_t len, std::string* out) {
    out->clear();
    out->reserve(len);
    char buf[65536];
    while (out->size() < len) {
        const std::size_t want = std::min(len - out->size(), sizeof(buf));
        const int n = ::recv(fd, buf, (int)want, 0);
        if (n <= 0) return false;
        out->append(buf, (std::size_t)n);
    }
    return true;
}

// ---- JSON 解析（严格 RFC 8259 子集，递归下降） ----

struct JsonCursor {
    const std::string& text;
    std::size_t pos = 0;
    int depth = 0;

    void skip_ws() {
        while (pos < text.size() &&
               (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' ||
                text[pos] == '\r')) {
            ++pos;
        }
    }
    bool eat(char c) {
        skip_ws();
        if (pos < text.size() && text[pos] == c) {
            ++pos;
            return true;
        }
        return false;
    }
    char peek() {
        skip_ws();
        return pos < text.size() ? text[pos] : '\0';
    }
};

bool parse_hex4(const std::string& t, std::size_t at, unsigned* out) {
    if (at + 4 > t.size()) return false;
    unsigned v = 0;
    for (int i = 0; i < 4; ++i) {
        const char c = t[at + (std::size_t)i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return false;
    }
    *out = v;
    return true;
}

void append_utf8(std::string& s, unsigned cp) {
    if (cp < 0x80) {
        s += (char)cp;
    } else if (cp < 0x800) {
        s += (char)(0xC0 | (cp >> 6));
        s += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        s += (char)(0xE0 | (cp >> 12));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    } else {
        s += (char)(0xF0 | (cp >> 18));
        s += (char)(0x80 | ((cp >> 12) & 0x3F));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    }
}

bool parse_string(JsonCursor& c, std::string* out) {
    if (!c.eat('"')) return false;
    std::string s;
    while (true) {
        if (c.pos >= c.text.size()) return false;
        const char ch = c.text[c.pos++];
        if (ch == '"') {
            *out = std::move(s);
            return true;
        }
        if ((unsigned char)ch < 0x20) return false;  // 裸控制字符
        if (ch != '\\') {
            s += ch;
            continue;
        }
        if (c.pos >= c.text.size()) return false;
        const char esc = c.text[c.pos++];
        switch (esc) {
            case '"': s += '"'; break;
            case '\\': s += '\\'; break;
            case '/': s += '/'; break;
            case 'b': s += '\b'; break;
            case 'f': s += '\f'; break;
            case 'n': s += '\n'; break;
            case 'r': s += '\r'; break;
            case 't': s += '\t'; break;
            case 'u': {
                unsigned cp = 0;
                if (!parse_hex4(c.text, c.pos, &cp)) return false;
                c.pos += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF) {  // 代理对
                    if (c.pos + 1 < c.text.size() && c.text[c.pos] == '\\' &&
                        c.text[c.pos + 1] == 'u') {
                        unsigned lo = 0;
                        if (!parse_hex4(c.text, c.pos + 2, &lo)) return false;
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            c.pos += 6;
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                    }
                }
                append_utf8(s, cp);
                break;
            }
            default:
                return false;
        }
    }
}

bool parse_number(JsonCursor& c, JsonValue* out) {
    const std::size_t start = c.pos;
    if (c.pos < c.text.size() && c.text[c.pos] == '-') ++c.pos;
    if (c.pos >= c.text.size() || c.text[c.pos] < '0' || c.text[c.pos] > '9')
        return false;
    while (c.pos < c.text.size() && c.text[c.pos] >= '0' && c.text[c.pos] <= '9')
        ++c.pos;
    bool is_double = false;
    if (c.pos < c.text.size() && c.text[c.pos] == '.') {
        is_double = true;
        ++c.pos;
        if (c.pos >= c.text.size() || c.text[c.pos] < '0' || c.text[c.pos] > '9')
            return false;
        while (c.pos < c.text.size() && c.text[c.pos] >= '0' &&
               c.text[c.pos] <= '9') {
            ++c.pos;
        }
    }
    if (c.pos < c.text.size() && (c.text[c.pos] == 'e' || c.text[c.pos] == 'E')) {
        is_double = true;
        ++c.pos;
        if (c.pos < c.text.size() && (c.text[c.pos] == '+' || c.text[c.pos] == '-'))
            ++c.pos;
        if (c.pos >= c.text.size() || c.text[c.pos] < '0' || c.text[c.pos] > '9')
            return false;
        while (c.pos < c.text.size() && c.text[c.pos] >= '0' &&
               c.text[c.pos] <= '9') {
            ++c.pos;
        }
    }
    const std::string num = c.text.substr(start, c.pos - start);
    if (!is_double) {
        errno = 0;
        char* end = nullptr;
        const long long v = std::strtoll(num.c_str(), &end, 10);
        if (errno != ERANGE) {
            out->type = JsonValue::Type::Int;
            out->integer = v;
            return true;
        }
    }
    out->type = JsonValue::Type::Double;
    out->number = std::strtod(num.c_str(), nullptr);
    return true;
}

bool parse_value(JsonCursor& c, JsonValue* out) {
    if (c.depth > kMaxJsonDepth) return false;
    const char ch = c.peek();
    switch (ch) {
        case '{': {
            ++c.pos;
            ++c.depth;
            out->type = JsonValue::Type::Object;
            if (c.eat('}')) {
                --c.depth;
                return true;
            }
            while (true) {
                std::string key;
                if (!parse_string(c, &key)) return false;
                if (!c.eat(':')) return false;
                JsonValue v;
                if (!parse_value(c, &v)) return false;
                out->members.emplace_back(std::move(key), std::move(v));
                if (c.eat('}')) {
                    --c.depth;
                    return true;
                }
                if (!c.eat(',')) return false;
            }
        }
        case '[': {
            ++c.pos;
            ++c.depth;
            out->type = JsonValue::Type::Array;
            if (c.eat(']')) {
                --c.depth;
                return true;
            }
            while (true) {
                JsonValue v;
                if (!parse_value(c, &v)) return false;
                out->items.push_back(std::move(v));
                if (c.eat(']')) {
                    --c.depth;
                    return true;
                }
                if (!c.eat(',')) return false;
            }
        }
        case '"':
            out->type = JsonValue::Type::String;
            return parse_string(c, &out->str);
        case 't':
            if (c.text.compare(c.pos, 4, "true") == 0) {
                c.pos += 4;
                out->type = JsonValue::Type::Bool;
                out->boolean = true;
                return true;
            }
            return false;
        case 'f':
            if (c.text.compare(c.pos, 5, "false") == 0) {
                c.pos += 5;
                out->type = JsonValue::Type::Bool;
                out->boolean = false;
                return true;
            }
            return false;
        case 'n':
            if (c.text.compare(c.pos, 4, "null") == 0) {
                c.pos += 4;
                out->type = JsonValue::Type::Null;
                return true;
            }
            return false;
        default:
            if (ch == '-' || (ch >= '0' && ch <= '9')) return parse_number(c, out);
            return false;
    }
}

const char* status_text(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

}  // namespace

const JsonValue* JsonValue::find(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& [k, v] : members) {
        if (k == key) return &v;
    }
    return nullptr;
}

bool json_parse(const std::string& text, JsonValue* out) {
    JsonCursor c{text};
    if (!parse_value(c, out)) return false;
    c.skip_ws();
    return c.pos == text.size();  // 尾部不许有残渣
}

std::string json_quote(const std::string& s) {
    std::string out = "\"";
    for (char ch : s) {
        const unsigned char c = (unsigned char)ch;
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    out += "\"";
    return out;
}

HttpServerStd::~HttpServerStd() {
    if (listen_fd_ != (uintptr_t)kInvalidSocket) close_socket((Socket)listen_fd_);
}

bool HttpServerStd::start(const std::string& host, int port, std::string* err) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        if (err) *err = "WSAStartup failed";
        return false;
    }
#endif
    Socket fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == kInvalidSocket) {
        if (err) *err = "socket() failed";
        return false;
    }
#ifndef _WIN32
    int reuse = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close_socket(fd);
        if (err) *err = "invalid host: " + host;
        return false;
    }
    if (::bind(fd, (sockaddr*)&addr, sizeof addr) != 0) {
        close_socket(fd);
        if (err) *err = "bind() failed (port in use?)";
        return false;
    }
    if (::listen(fd, 64) != 0) {
        close_socket(fd);
        if (err) *err = "listen() failed";
        return false;
    }
    socklen_t len = sizeof addr;
    if (::getsockname(fd, (sockaddr*)&addr, &len) != 0) {
        close_socket(fd);
        if (err) *err = "getsockname() failed";
        return false;
    }
    listen_fd_ = (uintptr_t)fd;
    port_ = ntohs(addr.sin_port);
    return true;
}

void HttpServerStd::run() {
    while (true) {
        const Socket conn = ::accept((Socket)listen_fd_, nullptr, nullptr);
        if (conn == kInvalidSocket) continue;
        std::thread(&HttpServerStd::handle_connection, this, (uintptr_t)conn)
            .detach();
    }
}

void HttpServerStd::handle_connection(uintptr_t handle) {
    const Socket fd = (Socket)handle;
    std::string head;
    HttpRequest req;
    HttpResponse resp;

    if (!read_headers(fd, &head)) {
        close_socket(fd);
        return;
    }
    // 头部与体切分：read_headers 的最后一次 recv 往往连体带进了 body
    // 起始字节——必须先摘出来留作 body 前缀，否则会在 recv 里等永远等
    // 不到的字节（客户端已把整请求发完）
    const std::size_t hdr_end = head.find("\r\n\r\n");
    if (hdr_end == std::string::npos) {
        close_socket(fd);
        return;
    }
    std::string extra = head.substr(hdr_end + 4);
    head.resize(hdr_end + 2);  // 留到最后一行头的 \r\n，便于行解析收口
    // 请求行：METHOD SP target SP HTTP/x.x
    {
        const std::size_t sp1 = head.find(' ');
        const std::size_t sp2 = head.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos) {
            close_socket(fd);
            return;
        }
        req.method = head.substr(0, sp1);
        req.target = head.substr(sp1 + 1, sp2 - sp1 - 1);
    }
    // 头部：只取 Content-Length（契约面唯一用到的）
    std::size_t content_length = 0;
    {
        std::size_t pos = head.find("\r\n") + 2;
        while (pos + 1 < head.size() && head.compare(pos, 2, "\r\n") != 0) {
            const std::size_t eol = head.find("\r\n", pos);
            const std::string line = head.substr(pos, eol - pos);
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                for (char& ch : key) ch = (char)std::tolower((unsigned char)ch);
                std::size_t vpos = colon + 1;
                while (vpos < line.size() && line[vpos] == ' ') ++vpos;
                if (key == "content-length") content_length = (std::size_t)std::strtoull(line.c_str() + vpos, nullptr, 10);
            }
            pos = eol + 2;
        }
    }
    // query：a=1&b=2，同名取首值（值不做百分号解码）
    {
        const std::size_t q = req.target.find('?');
        if (q != std::string::npos) {
            std::string qs = req.target.substr(q + 1);
            req.target = req.target.substr(0, q);
            std::size_t pos = 0;
            while (pos < qs.size()) {
                const std::size_t amp = qs.find('&', pos);
                const std::string pair =
                    qs.substr(pos, amp == std::string::npos ? std::string::npos
                                                            : amp - pos);
                const std::size_t eq = pair.find('=');
                if (eq != std::string::npos) {
                    req.query.emplace(pair.substr(0, eq), pair.substr(eq + 1));
                }
                if (amp == std::string::npos) break;
                pos = amp + 1;
            }
        }
    }
    if (content_length > kMaxBodyBytes) {
        resp.status = 413;
        resp.body = "{\"error\":\"payload_too_large\"}";
    } else {
        // 体 = 已混进头缓冲的残余 + 从套接字补齐的剩余
        req.body = std::move(extra);
        if (req.body.size() > content_length) req.body.resize(content_length);
        if (req.body.size() < content_length) {
            std::string rest;
            if (!read_exact(fd, content_length - req.body.size(), &rest)) {
                close_socket(fd);
                return;
            }
            req.body += rest;
        }
        resp = handler_ ? handler_(req)
                        : HttpResponse{404, "{\"error\":\"not_found\"}"};
    }

    std::string out = "HTTP/1.1 " + std::to_string(resp.status) + " " +
                      status_text(resp.status) + "\r\n"
                      "Content-Type: application/json; charset=utf-8\r\n"
                      "Content-Length: " + std::to_string(resp.body.size()) +
                      "\r\n"
                      "Connection: close\r\n\r\n" + resp.body;
    (void)send_all(fd, out.data(), out.size());
    close_socket(fd);
}

}  // namespace UnidictRelay
