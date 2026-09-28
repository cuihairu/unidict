// stardict_parser_std 分支缺口补测（真实缺边 53 条，第四大簇）。
//
// 缺边构成：ifo 形态矩阵（目录 ifo/无 '=' 行/charset 行/无 bookname）、
// idx 形态矩阵（空文件/末词条无 \0 终止/空词条跳过/64 位与 32 位
// 截断尾条目）、dict 打开矩阵（垃圾 .dz 致 gzopen 败/.dz 二次加载
// 走缓存命中/用 .dict 路径加载）、权限形态（不可读 ifo 与 .dict，
// POSIX + 非 root 守卫）、缓存目录被环境变量指到文件（143 臂）、
// decode_entry 全类型码矩阵（11 码各臂、h/x 作为非首文本字段、size
// 前缀截断与钳制、无 \0 小写码宽容回退、sametypesequence 的 i 耗尽
// 与 read_field 假臂）、lookup 守卫（未加载/seekg 失败/上限 break）。
// 全部真实输入驱动（手改文件形态与权限是解析器的真实输入面）。

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <zlib.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "std/stardict_parser_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path base_dir() {
    fs::path d = fs::current_path() / "build-local" / "sdbr";
    fs::remove_all(d);  // hermetic：清掉上轮（可能中途夭折）的遗留状态
    fs::create_directories(d);
    return d;
}

static fs::path write_bytes(const fs::path& p, const std::string& body) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << body;
    assert(out.good());
    return p;
}

static void be32s(std::string& s, uint32_t v) {
    s.push_back((char)((v >> 24) & 0xFF));
    s.push_back((char)((v >> 16) & 0xFF));
    s.push_back((char)((v >> 8) & 0xFF));
    s.push_back((char)(v & 0xFF));
}

// 小写类型码字段：类型字节 + 值 + '\0' 终止
static std::string f(char t, const std::string& v) {
    std::string s;
    s.push_back(t);
    s += v;
    s.push_back('\0');
    return s;
}

// 大写类型码字段：类型字节 + BE32 长度 + 值
static std::string F(char t, const std::string& v) {
    std::string s;
    s.push_back(t);
    be32s(s, (uint32_t)v.size());
    s += v;
    return s;
}

// 组装 ifo/.idx/.dict 三件套（dict_as_dz 时写 .dict.dz 不写 .dict）。
// ifo_extra 追加在标准头之后。
static fs::path build_sd(const fs::path& base,
                         const std::vector<std::pair<std::string, std::string>>& entries,
                         const std::string& ifo_extra = "",
                         bool dict_as_dz = false) {
    std::string idx, dict;
    for (const auto& e : entries) {
        idx += e.first;
        idx.push_back('\0');
        be32s(idx, (uint32_t)dict.size());
        be32s(idx, (uint32_t)e.second.size());
        dict += e.second;
    }
    write_bytes(base.string() + ".idx", idx);
    if (dict_as_dz) {
        gzFile gz = gzopen((base.string() + ".dict.dz").c_str(), "wb");
        assert(gz != nullptr);
        gzwrite(gz, dict.data(), (unsigned)dict.size());
        gzclose(gz);
    } else {
        write_bytes(base.string() + ".dict", dict);
    }
    return write_bytes(base.string() + ".ifo",
                       "bookname=SDBR\nwordcount=" + std::to_string(entries.size()) +
                           "\nidxfilesize=" + std::to_string(idx.size()) + "\n" + ifo_extra);
}

static std::string std_ifo(size_t n, size_t idx_size) {
    return "bookname=SDBR\nwordcount=" + std::to_string(n) +
           "\nidxfilesize=" + std::to_string(idx_size) + "\n";
}

// T1 ifo/idx 形态矩阵 + 权限形态
static void test_ifo_idx_shapes() {
    fs::path dir = base_dir();
    // ifo 是目录：exists 真、读入为空 → load_ifo 假臂
    {
        fs::path d = dir / "dirifo.ifo";
        fs::create_directories(d);
        StarDictParserStd p;
        assert(!p.load_dictionary(d.string()));
        fs::remove(d);
    }
    // 无 '=' 行跳过 + 四件套正常加载
    {
        build_sd(dir / "garbage", {{"w", f('m', "ok")}}, "this line has no equals\n");
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "garbage.ifo").string()));
        assert(p.lookup("w") == "ok");
    }
    // ifo 在、idx 缺失：167 真臂
    {
        write_bytes(dir / "noidx.ifo", std_ifo(1, 8));
        StarDictParserStd p;
        assert(!p.load_dictionary((dir / "noidx.ifo").string()));
    }
    // charset 行进入 header（71 真臂）
    {
        build_sd(dir / "cset", {{"w", f('m', "ok")}}, "charset=GB18030\n");
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "cset.ifo").string()));
        assert(p.dictionary_charset() == "GB18030");
    }
    // 空 .idx：读入为空 → load_idx 假臂（79）
    {
        build_sd(dir / "eidx", {});
        StarDictParserStd p;
        assert(!p.load_dictionary((dir / "eidx.ifo").string()));
    }
    // 末词条无 \0 终止：扫描撞到 end（85 假臂）+ break（86 真），
    // 前面完整词条仍入索引
    {
        std::string d1 = f('m', "ok");
        std::string idx;
        idx += "ok";
        idx.push_back('\0');
        be32s(idx, 0);
        be32s(idx, (uint32_t)d1.size());
        idx += "tail";  // 无 \0 无偏移，直接到 EOF
        write_bytes(dir / "unterm.idx", idx);
        write_bytes(dir / "unterm.dict", d1);
        write_bytes(dir / "unterm.ifo", std_ifo(1, idx.size()));
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "unterm.ifo").string()));
        assert(p.word_count() == 1);
        assert(p.lookup("ok") == "ok");
    }
    // 空词条目（\0 起头）：word.empty() 跳过（101 真）。注意 101 的
    // continue 只消费 \0 不读偏移尾，故空词条目必须放 idx 末尾作尾垫，
    // 否则后续偏移字节会被当词条重新扫描而错位
    {
        std::string idx, dict;
        idx += "a";
        idx.push_back('\0');
        be32s(idx, 0);
        be32s(idx, 3);
        dict += "mx";
        dict.push_back('\0');
        idx += "w";
        idx.push_back('\0');
        be32s(idx, 3);
        be32s(idx, 3);
        dict += "my";
        dict.push_back('\0');
        idx.push_back('\0');  // 空词条目 + 8 字节占位偏移（扫描空词跳过）
        idx += std::string(8, '\0');
        write_bytes(dir / "nullw.idx", idx);
        write_bytes(dir / "nullw.dict", dict);
        write_bytes(dir / "nullw.ifo", std_ifo(2, idx.size()));
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "nullw.ifo").string()));
        assert(p.word_count() == 2);
        assert(p.lookup("a") == "x");
        assert(p.lookup("w") == "y");
    }
    // 64 位偏移 + 尾条目 <12 字节：103 真臂 break
    {
        std::string idx;
        idx += "w";
        idx.push_back('\0');
        for (int i = 0; i < 8; ++i) idx.push_back('\0');  // off=0
        be32s(idx, 3);
        idx += "u";
        idx.push_back('\0');
        idx += "123456";  // 仅 6 字节，不足 8+4
        write_bytes(dir / "t64.idx", idx);
        write_bytes(dir / "t64.dict", f('m', "z"));
        write_bytes(dir / "t64.ifo",
                    std_ifo(2, idx.size()) + "idxoffsetbits=64\n");
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "t64.ifo").string()));
        assert(p.word_count() == 1);
        assert(p.lookup("w") == "z");
    }
    // 32 位偏移 + 尾条目 <8 字节：107 真臂 break
    {
        std::string idx;
        idx += "w";
        idx.push_back('\0');
        be32s(idx, 0);
        be32s(idx, 3);
        idx += "u";
        idx.push_back('\0');
        idx += "abc";  // 仅 3 字节，不足 4+4
        write_bytes(dir / "t32.idx", idx);
        write_bytes(dir / "t32.dict", f('m', "z"));
        write_bytes(dir / "t32.ifo", std_ifo(2, idx.size()));
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "t32.ifo").string()));
        assert(p.word_count() == 1);
    }
#if !defined(_WIN32)
    if (geteuid() != 0) {
        // 恢复位：读写齐全（permission 用 remove/replace，replace 只置
        // 给定位，必须把写位一并还原，否则下轮写文件会失败）
        const auto rw = fs::perms::owner_read | fs::perms::owner_write |
                        fs::perms::group_read | fs::perms::group_write;
        // ifo 存在但不可读：ifstream 打不开（17 真 + 56 真）
        fs::path ifo = build_sd(dir / "perm", {{"w", f('m', "x")}});
        fs::permissions(ifo, fs::perms::owner_read | fs::perms::group_read |
                                 fs::perms::others_read,
                        fs::perm_options::remove);
        {
            StarDictParserStd p;
            assert(!p.load_dictionary(ifo.string()));
        }
        fs::permissions(ifo, rw, fs::perm_options::replace);
        // .dict 不可读：open_dict 打不开（176 真）
        fs::path dictp = dir / "perm.dict";
        fs::permissions(dictp, fs::perms::owner_read | fs::perms::group_read |
                                   fs::perms::others_read,
                        fs::perm_options::remove);
        {
            StarDictParserStd p;
            assert(!p.load_dictionary(ifo.string()));
        }
        fs::permissions(dictp, rw, fs::perm_options::replace);
    }
#endif
}

// T2 dict 打开矩阵：垃圾 .dz、缓存命中、.dict 路径加载、无 bookname
static void test_dict_open_shapes() {
    fs::path dir = base_dir();
    // .dict.dz 不可读：gzopen 打不开路径 → null（141 真）→ load 假。
    // 注意 zlib 对非 gzip 内容走 transparent 模式，gzopen 不因内容失败，
    // 只会因路径打不开失败
#if !defined(_WIN32)
    if (geteuid() != 0)
    {
        build_sd(dir / "gdz", {{"w", f('m', "ok")}}, "", /*dict_as_dz=*/true);
        fs::path dz = dir / "gdz.dict.dz";
        fs::permissions(dz, fs::perms::owner_read | fs::perms::group_read |
                                fs::perms::others_read,
                        fs::perm_options::remove);
        StarDictParserStd p;
        assert(!p.load_dictionary((dir / "gdz.ifo").string()));
        fs::permissions(dz,
                        fs::perms::owner_read | fs::perms::owner_write |
                            fs::perms::group_read | fs::perms::group_write,
                        fs::perm_options::replace);
    }
#endif
    // 同一 .dz 两次加载：第二次缓存文件已存在 → 跳过解压（152 臂）
    {
        build_sd(dir / "cdz", {{"w", f('m', "cached")}}, "", true);
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "cdz.ifo").string()));
        assert(p.lookup("w") == "cached");
        assert(p.load_dictionary((dir / "cdz.ifo").string()));
        assert(p.lookup("w") == "cached");
    }
    // 用 .dict 路径加载：ext != ".ifo" 臂（163）
    {
        build_sd(dir / "viadict", {{"w", f('m', "via")}});
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "viadict.dict").string()));
        assert(p.lookup("w") == "via");
    }
    // ifo 无 bookname：dictionary_name 回落 "StarDict"（183）
    {
        std::string d1 = f('m', "no");
        std::string idx;
        idx += "w";
        idx.push_back('\0');
        be32s(idx, 0);
        be32s(idx, (uint32_t)d1.size());
        write_bytes(dir / "noname.idx", idx);
        write_bytes(dir / "noname.dict", d1);
        write_bytes(dir / "noname.ifo",
                    "wordcount=1\nidxfilesize=" + std::to_string(idx.size()) + "\n");
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "noname.ifo").string()));
        assert(p.dictionary_name() == "StarDict");
    }
}

// T3 decode_entry 全类型码矩阵
static void test_decode_matrix() {
    fs::path dir = base_dir();
    std::vector<std::pair<std::string, std::string>> entries = {
        {"c_m", f('m', "gloss")},  {"c_l", f('l', "Latin")},
        {"c_g", f('g', "GBK")},    {"c_x", f('x', "X-body")},
        {"c_h", f('h', "H-body")}, {"c_t", f('t', "T-body")},
        {"c_y", f('y', "Y-body")}, {"c_k", f('k', "K-body")},
        {"c_r", f('r', "R-body")}, {"c_W", F('W', "Wiki-body")},
        {"c_P", F('P', "Pic-body")},
        // 非文本字段打头：h / x 作为首个文本字段被选中
        {"th", f('t', "Phon") + f('h', "text")},
        {"tx", f('t', "Phon") + f('x', "text")},
        // size 前缀声明超出实际：钳制到剩余字节（207）
        {"clamp", std::string("W\0\0\0\x64xy", 7)},
        // size 前缀不足 4 字节：read_field 假 → break → 空字段（204/237/247）
        {"trunc", "Wabc"},
        // 小写码但无 \0 终止且非大写：宽容回退整段为文本（232 假臂）
        {"mabc", "mabc"},
        // 全非文本字段：回落首字段（248）
        {"rres", f('r', "Res")},
    };
    build_sd(dir / "dec", entries);
    StarDictParserStd p;
    assert(p.load_dictionary((dir / "dec.ifo").string()));
    assert(p.lookup("c_m") == "gloss");
    assert(p.lookup("c_l") == "Latin");
    assert(p.lookup("c_g") == "GBK");
    assert(p.lookup("c_x") == "X-body");
    assert(p.lookup("c_h") == "H-body");
    assert(p.lookup("c_t") == "T-body");
    assert(p.lookup("c_y") == "Y-body");
    assert(p.lookup("c_k") == "K-body");
    assert(p.lookup("c_r") == "R-body");
    assert(p.lookup("c_W") == "Wiki-body");
    assert(p.lookup("c_P") == "Pic-body");
    assert(p.lookup("th") == "text");
    assert(p.lookup("tx") == "text");
    assert(p.lookup("clamp") == "xy");
    assert(p.lookup("trunc").empty());
    assert(p.lookup("mabc") == "mabc");
    assert(p.lookup("rres") == "Res");

    // sametypesequence=mg：值无 \0 终止 → npos 容错取到末尾，
    // seq 循环 i 耗尽退出（229 假臂）
    {
        build_sd(dir / "seqmg", {{"w", "abc"}}, "sametypesequence=mg\n");
        StarDictParserStd q;
        assert(q.load_dictionary((dir / "seqmg.ifo").string()));
        assert(q.lookup("w") == "abc");
    }
    // sametypesequence=W + 数据不足 4 字节：read_field 假 →
    // seq 循环 break（230 真）→ 字段空（247 真）
    {
        build_sd(dir / "seqW", {{"w", "ab"}}, "sametypesequence=W\n");
        StarDictParserStd q;
        assert(q.load_dictionary((dir / "seqW.ifo").string()));
        assert(q.lookup("w").empty());
    }
}

// T4 lookup 守卫：未加载、seekg 失败、find_similar 上限 break
static void test_lookup_guards() {
    fs::path dir = base_dir();
    // 未加载即查（309 真）
    {
        StarDictParserStd p;
        assert(p.lookup_raw("x").empty());
        assert(p.find_similar("x", 5).empty());
    }
    // 词条偏移全 1（64 位）：转 streamoff 为 -1 → seekg 失败（314 真）。
    // 注意 32 位下 0xFFFFFFFF 是 +4294967295，seek 越过 EOF 不会失败
    {
        std::string idx;
        idx += "far";
        idx.push_back('\0');
        for (int i = 0; i < 8; ++i) idx.push_back('\xFF');  // be64 = -1
        be32s(idx, 4);
        write_bytes(dir / "far.idx", idx);
        write_bytes(dir / "far.dict", f('m', "x"));
        write_bytes(dir / "far.ifo",
                    std_ifo(1, idx.size()) + "idxoffsetbits=64\n");
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "far.ifo").string()));
        assert(p.lookup("far").empty());
    }
    // find_similar 命中数到上限即 break（324 真）
    {
        build_sd(dir / "sim",
                 {{"ab", f('m', "x")}, {"ac", f('m', "y")}, {"bd", f('m', "z")}});
        StarDictParserStd p;
        assert(p.load_dictionary((dir / "sim.ifo").string()));
        auto v = p.find_similar("a", 1);
        assert(v.size() == 1);
        assert(p.find_similar("zz", 5).empty());
    }
}

// T5 缓存目录被环境变量指向文件：解压缓存 ofstream 打不开（143 真）。
// 环境敏感用例放最后，结束后恢复环境。
static void test_cache_env_failure() {
#if !defined(_WIN32)
    fs::path dir = base_dir();
    const char* old = getenv("UNIDICT_CACHE_DIR");
    fs::path blocker = write_bytes(dir / "cacheblocker", "x");
    setenv("UNIDICT_CACHE_DIR", blocker.string().c_str(), 1);
    {
        build_sd(dir / "envdz", {{"w", f('m', "env")}}, "", true);
        StarDictParserStd p;
        assert(!p.load_dictionary((dir / "envdz.ifo").string()));
    }
    if (old)
        setenv("UNIDICT_CACHE_DIR", old, 1);
    else
        unsetenv("UNIDICT_CACHE_DIR");
#endif
}

int main() {
    test_ifo_idx_shapes();
    test_dict_open_shapes();
    test_decode_matrix();
    test_lookup_guards();
    test_cache_env_failure();
    std::cout << "OK\n";
    return 0;
}
