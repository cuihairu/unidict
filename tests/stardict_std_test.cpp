#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "std/stardict_parser_std.h"

static void be32(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = { (unsigned char)((v>>24)&0xFF), (unsigned char)((v>>16)&0xFF), (unsigned char)((v>>8)&0xFF), (unsigned char)(v&0xFF) };
    out.write((const char*)b, 4);
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "stardict_sample";
    fs::create_directories(dir);
    fs::path base = dir / "sample";

    // Write .dict
    std::string def1 = "Definition of hello.";
    std::string def2 = "Definition of world.";
    std::string def3 = "n. financial institution";  // 词头 Bank：层 0 折叠回退用例
    std::ofstream dict((base.string() + ".dict").c_str(), std::ios::binary | std::ios::trunc);
    uint32_t off1 = 0; dict.write(def1.data(), (std::streamsize)def1.size());
    uint32_t off2 = (uint32_t)def1.size(); dict.write(def2.data(), (std::streamsize)def2.size());
    uint32_t off3 = off2 + (uint32_t)def2.size(); dict.write(def3.data(), (std::streamsize)def3.size());
    dict.close();

    // Write .idx (word + \0 + offset + size)
    std::ofstream idx((base.string() + ".idx").c_str(), std::ios::binary | std::ios::trunc);
    std::string w1 = "hello"; idx.write(w1.c_str(), (std::streamsize)w1.size()); idx.put('\0'); be32(idx, off1); be32(idx, (uint32_t)def1.size());
    std::string w2 = "world"; idx.write(w2.c_str(), (std::streamsize)w2.size()); idx.put('\0'); be32(idx, off2); be32(idx, (uint32_t)def2.size());
    std::string w3 = "Bank"; idx.write(w3.c_str(), (std::streamsize)w3.size()); idx.put('\0'); be32(idx, off3); be32(idx, (uint32_t)def3.size());
    idx.close();

    // Write .ifo (minimal keys used by parser)
    std::ofstream ifo((base.string() + ".ifo").c_str(), std::ios::binary | std::ios::trunc);
    ifo << "bookname=Sample\n";
    ifo << "wordcount=3\n";
    ifo << "idxfilesize=" << (w1.size() + 1 + 8 + w2.size() + 1 + 8 + w3.size() + 1 + 8) << "\n";
    ifo << "idxoffsetbits=32\n";
    ifo.close();

    UnidictCoreStd::StarDictParserStd sp;
    bool ok = sp.load_dictionary((base.string() + ".ifo"));
    assert(ok);
    assert(sp.is_loaded());
    auto all = sp.all_words();
    assert(all.size() == 3);
    auto d1 = sp.lookup("hello");
    auto d2 = sp.lookup("world");
    assert(d1 == def1 && d2 == def2);
    auto sim = sp.find_similar("he", 10);
    bool has_hello = false; for (auto& s : sim) if (s == "hello") has_hello = true; assert(has_hello);

    // 层 0 折叠回退（index_ 以原始词形为键，与 JsonParserStd::lookup 同口径）：
    // 精确命中不动折叠索引；大小写变体经 fold_key 回退拿 canonical 释义；
    // 全角变体经 fold_key 全角→半角归一同样命中；词典外查询 fold miss 返回空。
    assert(sp.lookup("Bank") == def3);          // 精确臂
    assert(sp.lookup("bank") == def3);          // fold 回退（首次 miss 触发惰建）
    assert(sp.lookup("BANK") == def3);          // 已建索引（dirty=false 臂）再命中
    // 全角查询 ｂａｎｋ → fold_key 全角→半角归一命中。字面量用字节转义
    //（UTF-8: EF BD 82/81/8E/8B），避免中日字面量在 MSVC 源码页下的歧义
    assert(sp.lookup("\xEF\xBD\x82\xEF\xBD\x81\xEF\xBD\x8E\xEF\xBD\x8B") == def3);
    assert(sp.lookup("zzz").empty());           // fold miss 臂
    return 0;
}

