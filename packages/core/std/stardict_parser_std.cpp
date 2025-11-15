#include "stardict_parser_std.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <zlib.h>
#include "path_utils_std.h"

namespace fs = std::filesystem;

namespace UnidictCoreStd {

static inline std::string read_file_to_string(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss; ss << in.rdbuf();
    return ss.str();
}

StarDictParserStd::StarDictParserStd() = default;
StarDictParserStd::~StarDictParserStd() { if (dict_stream_.is_open()) dict_stream_.close(); }

bool StarDictParserStd::ends_with(const std::string& s, const std::string& suf) {
    if (s.size() < suf.size()) return false;
    return std::equal(suf.rbegin(), suf.rend(), s.rbegin(), s.rend(), [](char a, char b){ return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
}

std::string StarDictParserStd::dirname(const std::string& path) {
    fs::path p(path); return p.parent_path().string();
}

std::string StarDictParserStd::base_without_ext(const std::string& path) {
    fs::path p(path); p.replace_extension(""); return p.string();
}

uint32_t StarDictParserStd::be32(const unsigned char* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint64_t StarDictParserStd::be64(const unsigned char* p) {
    uint64_t v = 0; for (int i = 0; i < 8; ++i) v = (v << 8) | uint64_t(p[i]); return v;
}

std::string StarDictParserStd::lcase(const std::string& s) {
    std::string t; t.reserve(s.size()); for (unsigned char c : s) t.push_back((char)std::tolower(c)); return t;
}

bool StarDictParserStd::load_ifo(const std::string& ifo_path) {
    std::string txt = read_file_to_string(ifo_path);
    if (txt.empty()) return false;
    std::istringstream is(txt);
    std::string line;
    while (std::getline(is, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = lcase(line.substr(0, eq));
        std::string val = line.substr(eq + 1);
        if (key == "bookname") header_.book_name = val;
        else if (key == "wordcount") header_.word_count = std::atoi(val.c_str());
        else if (key == "idxfilesize") header_.index_file_size = std::atoi(val.c_str());
        else if (key == "idxoffsetbits") header_.idx_offset_bits = std::atoi(val.c_str());
        else if (key == "description") header_.description = val;
        else if (key == "version") header_.version = val;
    }
    return true;
}

bool StarDictParserStd::load_idx(const std::string& idx_path) {
    std::string buf = read_file_to_string(idx_path);
    if (buf.empty()) return false;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(buf.data());
    const unsigned char* end = p + buf.size();
    while (p < end) {
        // parse word (null-terminated)
        const unsigned char* s = p;
        while (p < end && *p) ++p;
        if (p >= end) break;
        std::string word(reinterpret_cast<const char*>(s), reinterpret_cast<const char*>(p));
        ++p; // skip null
        if (header_.idx_offset_bits == 64) {
            if (p + 8 + 4 > end) break;
            uint64_t off = be64(p); p += 8; uint32_t sz = be32(p); p += 4;
            index_[word] = {off, sz}; words_.push_back(word);
        } else {
            if (p + 4 + 4 > end) break;
            uint64_t off = be32(p); p += 4; uint32_t sz = be32(p); p += 4;
            index_[word] = {off, sz}; words_.push_back(word);
        }
    }
    return !index_.empty();
}

static inline uint64_t fnv1a64(const void* data, size_t len) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

bool StarDictParserStd::open_dict(const std::string& dict_path) {
    dict_stream_.close();
    // Plain .dict
    if (!ends_with(dict_path, ".dz")) {
        dict_stream_.open(dict_path, std::ios::binary);
        return (bool)dict_stream_;
    }
    // .dict.dz: decompress to cache with hashed file signature to avoid collisions
    std::error_code ec;
    fs::path src(dict_path);
    auto sz = fs::file_size(src, ec);
    auto ts = fs::last_write_time(src, ec).time_since_epoch().count();
    std::string sig = dict_path + "|" + std::to_string((unsigned long long)sz) + "|" + std::to_string((long long)ts);
    uint64_t hv = fnv1a64(sig.data(), sig.size());
    fs::path outdir = fs::path(UnidictCoreStd::PathUtilsStd::cache_dir()) / "stardict";
    UnidictCoreStd::PathUtilsStd::ensure_dir(outdir.string());
    fs::path outpath = outdir / (std::string("dict_") + std::to_string((unsigned long long)hv) + ".dict");
    if (!fs::exists(outpath, ec)) {
        gzFile gz = gzopen(dict_path.c_str(), "rb");
        if (!gz) return false;
        std::ofstream out(outpath.string(), std::ios::binary | std::ios::trunc);
        if (!out) { gzclose(gz); return false; }
        char buf[1<<15];
        int n;
        while ((n = gzread(gz, buf, sizeof(buf))) > 0) {
            out.write(buf, n);
            if (!out) { gzclose(gz); return false; }
        }
        gzclose(gz);
        out.close();
    }
    dict_stream_.open(outpath.string(), std::ios::binary);
    return (bool)dict_stream_;
}

bool StarDictParserStd::load_dictionary(const std::string& any_path) {
    loaded_ = false; index_.clear(); words_.clear(); if (dict_stream_.is_open()) dict_stream_.close(); header_ = {};
    fs::path p(any_path);
    std::string ext = p.extension().string();
    std::string base = base_without_ext(any_path);
    std::string dir = dirname(any_path);
    std::string ifo = (ext == ".ifo") ? any_path : (base + ".ifo");
    if (!fs::exists(ifo)) return false;
    if (!load_ifo(ifo)) return false;
    std::string idx = base_without_ext(ifo) + ".idx";
    if (!fs::exists(idx)) return false;
    if (!load_idx(idx)) return false;
    std::string dict = base_without_ext(ifo) + ".dict";
    if (!fs::exists(dict)) {
        // try .dict.dz
        std::string dz = base_without_ext(ifo) + ".dict.dz";
        if (!fs::exists(dz)) return false;
        if (!open_dict(dz)) return false;
    } else {
        if (!open_dict(dict)) return false;
    }
    loaded_ = true;
    return true;
}

bool StarDictParserStd::is_loaded() const { return loaded_; }
std::string StarDictParserStd::dictionary_name() const { return header_.book_name.empty() ? std::string("StarDict") : header_.book_name; }
std::string StarDictParserStd::dictionary_description() const { return header_.description; }
int StarDictParserStd::word_count() const { return (int)words_.size(); }

std::string StarDictParserStd::lookup(const std::string& word) const {
    if (!loaded_) return {};
    auto it = index_.find(word);
    if (it == index_.end()) return {};
    uint64_t off = it->second.first; uint32_t sz = it->second.second;
    dict_stream_.seekg((std::streamoff)off, std::ios::beg);
    if (!dict_stream_) return {};
    std::string out; out.resize(sz);
    dict_stream_.read(out.data(), sz);
    return out;
}

std::vector<std::string> StarDictParserStd::find_similar(const std::string& word, int max_results) const {
    std::vector<std::string> out; out.reserve(std::min<int>((int)words_.size(), max_results));
    std::string lw = lcase(word);
    for (const auto& w : words_) {
        if ((int)out.size() >= max_results) break;
        if (lcase(w).rfind(lw, 0) == 0) out.push_back(w);
    }
    return out;
}

std::vector<std::string> StarDictParserStd::all_words() const { return words_; }

} // namespace UnidictCoreStd
