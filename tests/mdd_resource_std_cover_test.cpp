// MddResource 补覆盖：SimpleKV 条目截断、extract/cache 写目标为已存在
// 目录（ofstream 写模式打开目录必败）、Manager 缓存失败返回空路径。

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/mdd_resource_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static void be16w(std::vector<unsigned char>& v, uint16_t x) {
    v.push_back((unsigned char)(x >> 8));
    v.push_back((unsigned char)(x & 0xFF));
}

static void be32w(std::vector<unsigned char>& v, uint32_t x) {
    v.push_back((unsigned char)(x >> 24));
    v.push_back((unsigned char)((x >> 16) & 0xFF));
    v.push_back((unsigned char)((x >> 8) & 0xFF));
    v.push_back((unsigned char)(x & 0xFF));
}

static std::vector<unsigned char> make_simplekv(
    const std::vector<std::pair<std::string, std::string>>& kv) {
    std::vector<unsigned char> v;
    std::string magic = "SIMPLEKV";
    v.insert(v.end(), magic.begin(), magic.end());
    be32w(v, (uint32_t)kv.size());
    for (const auto& p : kv) {
        be16w(v, (uint16_t)p.first.size());
        v.insert(v.end(), p.first.begin(), p.first.end());
        be32w(v, (uint32_t)p.second.size());
        v.insert(v.end(), p.second.begin(), p.second.end());
    }
    return v;
}

static void write_file(const std::string& p, const std::vector<unsigned char>& d) {
    std::ofstream o(p, std::ios::binary);
    o.write(reinterpret_cast<const char*>(d.data()), (std::streamsize)d.size());
}

int main() {
    fs::path base = fs::current_path() / "build-local" / "mdd_cover";
    fs::remove_all(base);
    fs::create_directories(base);

    // ===== 1) SimpleKV 条目在 value 长度字段处截断 → load false =====
    {
        std::vector<unsigned char> v;
        std::string magic = "SIMPLEKV";
        v.insert(v.end(), magic.begin(), magic.end());
        be32w(v, 1);   // count=1
        be16w(v, 2);   // klen
        v.push_back('k');
        v.push_back('1');
        // 文件在此结束：读 vlen（4 字节）失败
        write_file((base / "trunc.mdd").string(), v);

        MddResourceParser p;
        assert(!p.load((base / "trunc.mdd").string()));
    }

    // ===== 2) 正常库 + extract 槽位为已存在目录 → 写失败 =====
    {
        auto body = make_simplekv({{"a.png", "hello"}});
        write_file((base / "good.mdd").string(), body);

        MddResourceParser p;
        assert(p.load((base / "good.mdd").string()));
        assert(p.get_resource_as_string("a.png") == "hello");

        fs::path edir = base / "edir";
        fs::create_directories(edir / "a.png");   // 槽位已被目录占用
        assert(!p.extract_to_cache("a.png", edir.string()));
    }

    // ===== 3) cache_resource 槽位为已存在目录 → false =====
    {
        fs::path cdir = base / "cdirC";
        fs::create_directories(cdir / "slot");
        MddResourceCache c(cdir.string());
        std::vector<unsigned char> d{'a', 'b'};
        assert(!c.cache_resource(d, "slot", "image/png"));
    }

    // ===== 4) Manager：缓存槽位为目录 → get_resource_path 返回空 =====
    {
        MddResourceManager mgr;
        fs::path mdir = base / "mdir";
        fs::create_directories(mdir / "d1_a.png");
        mgr.set_cache_directory(mdir.string());
        assert(mgr.load_mdd((base / "good.mdd").string(), "d1"));
        // 数据本身取得到，但 cache_resource 写失败 → 返回空路径
        assert(!mgr.get_resource_data("a.png", "d1").empty());
        assert(mgr.get_resource_path("a.png", "d1").empty());
    }

    return 0;
}
