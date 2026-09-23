// StarDict sametypesequence 解码测试：lookup() 应返回主释义文本，
// 剥离类型字节、size 前缀、\0 终止符与音标等非释义字段。

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

namespace {

struct Sample {
    std::filesystem::path ifo_path;
};

// 构造一份最小 StarDict 词典：单词条 "hello"，词条体由调用方给原始字节。
Sample write_sample(const std::string& dir_name,
                    const std::string& sametypesequence,
                    const std::string& entry_bytes) {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / dir_name;
    fs::create_directories(dir);
    fs::path base = dir / "sample";

    std::ofstream dict((base.string() + ".dict").c_str(), std::ios::binary | std::ios::trunc);
    dict.write(entry_bytes.data(), (std::streamsize)entry_bytes.size());
    dict.close();

    std::ofstream idx((base.string() + ".idx").c_str(), std::ios::binary | std::ios::trunc);
    std::string w = "hello";
    idx.write(w.c_str(), (std::streamsize)w.size()); idx.put('\0');
    be32(idx, 0); be32(idx, (uint32_t)entry_bytes.size());
    idx.close();

    std::ofstream ifo((base.string() + ".ifo").c_str(), std::ios::binary | std::ios::trunc);
    ifo << "bookname=Sample\n";
    ifo << "wordcount=1\n";
    ifo << "idxfilesize=" << (w.size() + 1 + 8) << "\n";
    ifo << "idxoffsetbits=32\n";
    if (!sametypesequence.empty()) ifo << "sametypesequence=" << sametypesequence << "\n";
    ifo.close();

    return { base.string() + ".ifo" };
}

bool lookup_equals(const Sample& s, const std::string& expect) {
    UnidictCoreStd::StarDictParserStd sp;
    if (!sp.load_dictionary(s.ifo_path.string())) return false;
    return sp.lookup("hello") == expect;
}

} // namespace

int main() {
    // 1) 无 sametypesequence：词条自带类型字节 m + \0 终止文本
    {
        std::string raw = std::string("m") + std::string("greeting\0", 9);
        assert(lookup_equals(write_sample("sd_sts_plain", "", raw), "greeting"));
    }
    // 2) sametypesequence=m：整个词条体即释义（无终止符）
    {
        assert(lookup_equals(write_sample("sd_sts_m", "m", "greeting"), "greeting"));
    }
    // 3) sametypesequence=tm：音标（\0 终止）在前，取后面的 m 主释义
    {
        std::string raw = std::string("/gri:tIN/\0", 10) + std::string("greeting");
        assert(lookup_equals(write_sample("sd_sts_tm", "tm", raw), "greeting"));
    }
    // 4) sametypesequence=h：HTML 释义原样返回
    {
        assert(lookup_equals(write_sample("sd_sts_h", "h", "<b>hi</b>"), "<b>hi</b>"));
    }
    // 5) 未声明序列、多字段：t 音标 + m 释义，取释义
    {
        std::string raw = std::string("t") + std::string("/ipa\0", 5)
                        + std::string("m") + std::string("meaning\0", 8);
        assert(lookup_equals(write_sample("sd_sts_multi", "", raw), "meaning"));
    }
    // 6) sametypesequence=l：Latin-1 转 UTF-8（café）
    {
        std::string raw = std::string("caf\xe9", 4);
        assert(lookup_equals(write_sample("sd_sts_latin", "l", raw), "caf\xc3\xa9"));
    }
    // 7) 大写 W 类型：32 位 BE size 前缀 + 数据；同词条内的 m 字段仍是主释义
    {
        std::string wav = "abc";
        std::string raw = std::string("W");
        raw.push_back((char)0); raw.push_back((char)0); raw.push_back((char)0); raw.push_back((char)wav.size());
        raw += wav;
        raw += std::string("m") + std::string("sound\0", 6);
        assert(lookup_equals(write_sample("sd_sts_wav", "", raw), "sound"));
    }
    // 8) lookup_raw 返回未解码字节（用例 3 的原文）
    {
        std::string raw = std::string("/gri:tIN/\0", 10) + std::string("greeting");
        Sample s = write_sample("sd_sts_raw", "tm", raw);
        UnidictCoreStd::StarDictParserStd sp;
        assert(sp.load_dictionary(s.ifo_path.string()));
        assert(sp.lookup_raw("hello") == raw);
    }
    return 0;
}
