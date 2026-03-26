#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <zlib.h>

#include "std/mdict_parser_std.h"

static void be16w(std::vector<unsigned char>& v, uint16_t x) {
    v.push_back((x >> 8) & 0xFF);
    v.push_back(x & 0xFF);
}

static void be32w(std::vector<unsigned char>& v, uint32_t x) {
    v.push_back((x >> 24) & 0xFF);
    v.push_back((x >> 16) & 0xFF);
    v.push_back((x >> 8) & 0xFF);
    v.push_back(x & 0xFF);
}

static std::vector<unsigned char> zlib_compress(const std::vector<unsigned char>& in) {
    uLongf out_len = compressBound((uLongf)in.size());
    std::vector<unsigned char> out(out_len);
    int rc = compress2(out.data(), &out_len, in.data(), (uLongf)in.size(), Z_BEST_COMPRESSION);
    assert(rc == Z_OK);
    out.resize(out_len);
    return out;
}

static std::vector<unsigned char> simple_xor_encrypt(const std::vector<unsigned char>& in, const std::string& password) {
    std::vector<unsigned char> out;
    out.reserve(in.size());
    std::mt19937 rng(std::hash<std::string>{}(password));
    std::vector<unsigned char> key;
    key.reserve(std::min<size_t>(256, in.size()));
    for (size_t i = 0; i < std::min<size_t>(256, in.size()); ++i) {
        key.push_back(static_cast<unsigned char>(rng() % 256));
    }
    assert(!key.empty());
    for (size_t i = 0; i < in.size(); ++i) {
        out.push_back(in[i] ^ key[i % key.size()]);
    }
    return out;
}

static void set_env(const char* key, const char* value) {
#if defined(_WIN32)
    _putenv_s(key, value ? value : "");
#else
    if (value) {
        ::setenv(key, value, 1);
    } else {
        ::unsetenv(key);
    }
#endif
}

static void clear_password_env() {
    set_env("UNIDICT_MDICT_PASSWORD", nullptr);
    set_env("UNIDICT_PASSWORD", nullptr);
}

static std::filesystem::path write_encrypted_keyb_dict(const std::filesystem::path& dir, const std::string& password) {
    std::filesystem::create_directories(dir);
    const auto mdx = dir / "enc_keyb.mdx";

    std::vector<unsigned char> keyb;
    keyb.insert(keyb.end(), {'K', 'E', 'Y', 'B'});
    be32w(keyb, 2);
    const std::string w1 = "alpha";
    const std::string w2 = "beta";
    be16w(keyb, (uint16_t)w1.size());
    keyb.insert(keyb.end(), w1.begin(), w1.end());
    be32w(keyb, 0);
    be32w(keyb, 1);
    be16w(keyb, (uint16_t)w2.size());
    keyb.insert(keyb.end(), w2.begin(), w2.end());
    be32w(keyb, 1);
    be32w(keyb, 1);

    std::vector<unsigned char> recb;
    recb.insert(recb.end(), {'R', 'E', 'C', 'B'});
    const std::vector<unsigned char> payload = {'A', 'B'};
    const auto comp = zlib_compress(payload);
    recb.insert(recb.end(), comp.begin(), comp.end());

    std::vector<unsigned char> body = keyb;
    body.insert(body.end(), recb.begin(), recb.end());
    const auto encrypted = simple_xor_encrypt(body, password);

    const std::string header = "<Dictionary title=\"EncryptedKEYB\" description=\"env\" encrypted=\"1\"/>\n";
    std::ofstream out(mdx, std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write(reinterpret_cast<const char*>(encrypted.data()), (std::streamsize)encrypted.size());
    out.close();
    return mdx;
}

int main() {
    namespace fs = std::filesystem;

    const fs::path dir = fs::current_path() / "build-local" / "mdict_password_env";
    const fs::path mdx = write_encrypted_keyb_dict(dir, "secret");

    clear_password_env();
    {
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.is_loaded());
        assert(mp.all_words().empty());
        assert(mp.lookup("alpha").empty());
    }

    clear_password_env();
    set_env("UNIDICT_PASSWORD", "secret");
    {
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.is_loaded());
        auto all = mp.all_words();
        bool has_alpha = false;
        bool has_beta = false;
        for (const auto& w : all) {
            if (w == "alpha") has_alpha = true;
            if (w == "beta") has_beta = true;
        }
        assert(has_alpha && has_beta);
        assert(mp.lookup("alpha") == std::string("A"));
        assert(mp.lookup("beta") == std::string("B"));
    }

    set_env("UNIDICT_MDICT_PASSWORD", "wrong");
    {
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.is_loaded());
        assert(mp.all_words().empty());
        assert(mp.lookup("alpha").empty());
    }

    set_env("UNIDICT_MDICT_PASSWORD", "secret");
    {
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.is_loaded());
        assert(mp.lookup("alpha") == std::string("A"));
        assert(mp.lookup("beta") == std::string("B"));
    }

    clear_password_env();
    return 0;
}
