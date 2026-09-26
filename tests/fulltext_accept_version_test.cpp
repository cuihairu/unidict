// load_fulltext_index_relaxed 的 accept_version 闸门。
//
// 背景：auto 兼容模式先走 load_fulltext_index（校验签名），失败后退到
// 宽松加载。原来宽松加载"成功且版本 != 1"时，调用方返回的是
// load_fulltext_index_relaxed 的 out_error —— 而那个 out_error 只在**解析
// 失败**时才被写。于是成功路径上它恒为空串，auto 模式把一套签名不匹配
// 的 v2/v3 索引当成"加载成功"返回，索引还留在内存里继续服务全文检索。
// accept_version=1 就是堵这个洞：只放行 legacy v1，其余版本拒绝且不提交。

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

namespace {

fs::path tmp_root() {
    static const fs::path dir = [] {
        const char* t = std::getenv("TMPDIR");
        fs::path d = fs::path(t && *t ? t : "/tmp") / "unidict_ft_acceptver";
        fs::remove_all(d);
        fs::create_directories(d);
        return d;
    }();
    return dir;
}

void write_dict(const fs::path& p, const std::string& name,
                const std::vector<std::pair<std::string, std::string>>& kv) {
    std::string j = "{\"name\":\"" + name + "\",\"entries\":[";
    for (size_t i = 0; i < kv.size(); ++i) {
        j += (i ? "," : "");
        j += "{\"word\":\"" + kv[i].first + "\",\"definition\":\"" +
             kv[i].second + "\"}";
    }
    j += "]}";
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o << j;
}

// 索引文件布局（core/std/fulltext_index_std.cpp load()，均为小端 u32）：
//   magic(5) [v2/v3: siglen(4) + sig] docs(4) [docs*8] terms(4) [terms...]
// 0 docs + 0 terms 的空索引即可被解析，下面两个 helper 就是构造它。
void put_u32(std::ofstream& o, uint32_t v) {
    unsigned char b[4] = {(unsigned char)(v & 0xFF), (unsigned char)((v >> 8) & 0xFF),
                          (unsigned char)((v >> 16) & 0xFF), (unsigned char)((v >> 24) & 0xFF)};
    o.write((const char*)b, 4);
}

// 手工写一个最小可解析的 legacy v1 索引（无签名段）
void write_legacy_v1(const fs::path& p) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o.write("UDFT1", 5);
    put_u32(o, 0);   // docs
    put_u32(o, 0);   // terms
}

// 一个"有签名但签名内容任意"的 v3 索引
void write_v3_with_signature(const fs::path& p, const std::string& sig) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o.write("UDFT3", 5);
    put_u32(o, (uint32_t)sig.size());
    o.write(sig.data(), (std::streamsize)sig.size());
    put_u32(o, 0);   // docs
    put_u32(o, 0);   // terms
}

void test_relaxed_default_accepts_any_version() {
    // accept_version 缺省 = 0（loose 语义）：v3 一律放行，与旧行为一致
    const auto v3 = tmp_root() / "any_v3.udft";
    write_v3_with_signature(v3, "whatever-signature");
    DictionaryManagerStd mgr;
    int ver = 0;
    std::string err;
    assert(mgr.load_fulltext_index_relaxed(v3.string(), &ver, &err));
    assert(ver == 3);
    assert(err.empty());
    // 确实被提交了
    assert(mgr.fulltext_stats().version == 3);
}

void test_accept_version_1_rejects_signed_index() {
    const auto v3 = tmp_root() / "reject_v3.udft";
    write_v3_with_signature(v3, "NV=1;N=1;dictA|4|a|d");
    DictionaryManagerStd mgr;
    int ver = 0;
    std::string err;
    // 必须拒绝
    assert(!mgr.load_fulltext_index_relaxed(v3.string(), &ver, &err, 1));
    assert(err.find("signature mismatch") != std::string::npos);
    // 关键：被拒绝的索引不能留在内存里
    assert(mgr.fulltext_stats().version == 0);
    assert(mgr.fulltext_stats().docs == 0);
}

void test_accept_version_1_accepts_legacy_v1() {
    const auto v1 = tmp_root() / "accept_v1.udft";
    write_legacy_v1(v1);
    DictionaryManagerStd mgr;
    int ver = 0;
    std::string err;
    assert(mgr.load_fulltext_index_relaxed(v1.string(), &ver, &err, 1));
    assert(ver == 1);
    assert(err.empty());
    assert(mgr.fulltext_stats().version == 1);
}

void test_accept_version_1_rejects_unparsable() {
    const auto junk = tmp_root() / "junk.udft";
    {
        std::ofstream o(junk, std::ios::binary | std::ios::trunc);
        o << "not an index";
    }
    DictionaryManagerStd mgr;
    int ver = 0;
    std::string err;
    assert(!mgr.load_fulltext_index_relaxed(junk.string(), &ver, &err, 1));
    assert(!err.empty());
}

void test_accept_version_1_rejection_keeps_previous_index() {
    // 已经被接受过的索引不应被后续失败的宽松加载顶掉
    const auto v1 = tmp_root() / "keep_v1.udft";
    const auto v3 = tmp_root() / "keep_v3.udft";
    write_legacy_v1(v1);
    write_v3_with_signature(v3, "some-signature");
    DictionaryManagerStd mgr;
    int ver = 0;
    std::string err;
    assert(mgr.load_fulltext_index_relaxed(v1.string(), &ver, &err, 1));
    assert(mgr.fulltext_stats().version == 1);
    // 再试一个会被拒的 v3
    assert(!mgr.load_fulltext_index_relaxed(v3.string(), &ver, &err, 1));
    // 原来那份 v1 还在
    assert(mgr.fulltext_stats().version == 1);
}

void test_strict_load_rejects_mismatched_signature() {
    // accept_version 的存在前提：strict 路径确实会因为签名不符而失败
    const auto d1 = tmp_root() / "s1.json";
    write_dict(d1, "dictA", {{"alpha", "A def"}, {"beta", "B def"}});
    const auto d2 = tmp_root() / "s2.json";
    write_dict(d2, "dictB", {{"gamma", "C def"}});

    DictionaryManagerStd writer;
    assert(writer.add_dictionary(d1.string()));
    const auto idx = tmp_root() / "sig.udft";
    assert(writer.save_fulltext_index(idx.string()));

    // 换一套词典
    DictionaryManagerStd reader;
    assert(reader.add_dictionary(d2.string()));
    assert(!reader.load_fulltext_index(idx.string()));
    int ver = 0;
    std::string err;
    // strict 失败后走 accept_version=1 兜底 → 同样失败（v3 带签名）
    assert(!reader.load_fulltext_index_relaxed(idx.string(), &ver, &err, 1));
    assert(err.find("signature mismatch") != std::string::npos);
    // loose 模式（accept_version=0）则会放行——这正是两条语义的差别
    assert(reader.load_fulltext_index_relaxed(idx.string(), &ver, &err, 0));
    assert(ver == 3);
}

}  // namespace

int main() {
    test_relaxed_default_accepts_any_version();
    test_accept_version_1_rejects_signed_index();
    test_accept_version_1_accepts_legacy_v1();
    test_accept_version_1_rejects_unparsable();
    test_accept_version_1_rejection_keeps_previous_index();
    test_strict_load_rejects_mismatched_signature();
    std::printf("OK\n");
    return 0;
}
