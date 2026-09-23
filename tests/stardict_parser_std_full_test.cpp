// StarDictParserStd 全表面覆盖：.dz 解压缓存（miss/hit 两走）、
// 64 位 idx 偏移、charset/description 头、read_field 缺终止符容错、
// 无释义字段时的回落路径。

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include "std/stardict_parser_std.h"

static void w32(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = { (unsigned char)((v>>24)&0xFF), (unsigned char)((v>>16)&0xFF),
                           (unsigned char)((v>>8)&0xFF), (unsigned char)(v&0xFF) };
    out.write((const char*)b, 4);
}

// 造一份单词条词典；compress=true 时词条体写进 .dict.dz（无 .dict）
static std::string write_dict(const std::string& tag,
                              const std::string& ifo_extra,
                              const std::string& word,
                              const std::string& raw_entry,
                              bool compress) {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / ("sd_full_" + tag);
    fs::create_directories(dir);
    fs::path base = dir / "sample";

    if (compress) {
        gzFile gz = gzopen((base.string() + ".dict.dz").c_str(), "wb");
        assert(gz != nullptr);
        gzwrite(gz, raw_entry.data(), (unsigned)raw_entry.size());
        gzclose(gz);
    } else {
        std::ofstream d((base.string() + ".dict").c_str(), std::ios::binary);
        d.write(raw_entry.data(), (std::streamsize)raw_entry.size());
    }

    std::ofstream idx((base.string() + ".idx").c_str(), std::ios::binary);
    idx.write(word.c_str(), (std::streamsize)word.size());
    idx.put('\0');
    if (ifo_extra.find("idxoffsetbits=64") != std::string::npos) {
        // 64 位格式：8 字节 BE 偏移 + 4 字节 BE 长度
        unsigned char off8[8] = {0};
        idx.write((const char*)off8, 8);
        w32(idx, (uint32_t)raw_entry.size());
    } else {
        w32(idx, 0);
        w32(idx, (uint32_t)raw_entry.size());
    }
    idx.close();

    std::ofstream ifo((base.string() + ".ifo").c_str(), std::ios::binary);
    ifo << "bookname=Full " << tag << "\n";
    ifo << "wordcount=1\n";
    int off_bytes = (ifo_extra.find("idxoffsetbits=64") != std::string::npos) ? 12 : 8;
    ifo << "idxfilesize=" << (word.size() + 1 + off_bytes) << "\n";
    ifo << ifo_extra;
    ifo.close();

    return base.string() + ".ifo";
}

int main() {
    setenv("UNIDICT_CACHE_DIR", "build-local/sd_full_cache", 1);

    // 1) .dict.dz：解压落缓存后查词（缓存 miss 分支）
    std::string ifo1 = write_dict("dzmiss", "idxoffsetbits=32\n", "alpha",
                                  "Definition of alpha.", true);
    {
        UnidictCoreStd::StarDictParserStd sp;
        assert(sp.load_dictionary(ifo1));
        assert(sp.is_loaded());
        assert(sp.lookup("alpha") == "Definition of alpha.");
        // 再加载一次：缓存文件已存在，走命中分支直接打开
        assert(sp.load_dictionary(ifo1));
        assert(sp.lookup("alpha") == "Definition of alpha.");
    }

    // 2) 64 位 idx 偏移（be64 分支）
    {
        std::string ifo = write_dict("bits64", "idxoffsetbits=64\n", "beta",
                                     "meaning of beta", false);
        UnidictCoreStd::StarDictParserStd sp;
        assert(sp.load_dictionary(ifo));
        assert(sp.word_count() == 1);
        assert(sp.lookup("beta") == "meaning of beta");
    }

    // 3) charset 与 description 头；dictionary_description 返回原文
    {
        std::string raw = std::string("m") + std::string("greeting\0", 9);
        std::string ifo = write_dict("meta", "charset=UTF-8\ndescription=My desc\n",
                                     "gamma", raw, false);
        UnidictCoreStd::StarDictParserStd sp;
        assert(sp.load_dictionary(ifo));
        assert(sp.dictionary_description() == "My desc");
        assert(sp.dictionary_name() == "Full meta");
        assert(sp.lookup("gamma") == "greeting");
    }

    // 4) 规范多字段缺终止符：末段无 \0，容错取到末尾
    {
        std::string raw = std::string("m") + std::string("abc\0def", 7);
        std::string ifo = write_dict("nonull", "idxoffsetbits=32\n", "delta",
                                     raw, false);
        UnidictCoreStd::StarDictParserStd sp;
        assert(sp.load_dictionary(ifo));
        assert(sp.lookup("delta") == "abc");
    }

    // 5) 只有非释义字段（t 音标）：回落取首个字段原文
    {
        std::string raw = std::string("t") + std::string("/ipa\0", 5);
        std::string ifo = write_dict("nontext", "idxoffsetbits=32\n", "epsilon",
                                     raw, false);
        UnidictCoreStd::StarDictParserStd sp;
        assert(sp.load_dictionary(ifo));
        assert(sp.lookup("epsilon") == "/ipa");
    }

    return 0;
}
