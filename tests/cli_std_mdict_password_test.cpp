#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <zlib.h>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

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

static std::filesystem::path write_encrypted_keyb_dict(const std::filesystem::path& dir, const std::string& password) {
    std::filesystem::create_directories(dir);
    const auto mdx = dir / "cli_enc_keyb.mdx";

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

    const std::string header = "<Dictionary title=\"EncryptedCLI\" description=\"cli\" encrypted=\"1\"/>\n";
    std::ofstream out(mdx, std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write(reinterpret_cast<const char*>(encrypted.data()), (std::streamsize)encrypted.size());
    out.close();
    return mdx;
}

static std::filesystem::path write_json_dict(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    const auto json = dir / "fallback.json";
    const std::string body =
        "{\n"
        "  \"name\": \"FallbackJSON\",\n"
        "  \"entries\": [\n"
        "    {\"word\": \"alpha\", \"definition\": \"json-alpha\"},\n"
        "    {\"word\": \"beta\", \"definition\": \"json-beta\"},\n"
        "    {\"word\": \"gamma\", \"definition\": \"json-gamma\"}\n"
        "  ]\n"
        "}\n";
    std::ofstream out(json, std::ios::binary | std::ios::trunc);
    out.write(body.data(), (std::streamsize)body.size());
    out.close();
    return json;
}

static std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

static std::string run_capture(const std::string& cmd, int& rc_out) {
    std::string output;
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    assert(pipe != nullptr);
    char buf[512];
    while (std::fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }
#if defined(_WIN32)
    rc_out = _pclose(pipe);
#else
    const int status = pclose(pipe);
    if (WIFEXITED(status)) rc_out = WEXITSTATUS(status);
    else rc_out = status;
#endif
    return output;
}

int main(int argc, char** argv) {
    assert(argc >= 2);
    const std::string cli = argv[1];

    namespace fs = std::filesystem;
    const fs::path dir = fs::current_path() / "build-local" / "cli_mdict_password";
    const fs::path mdx = write_encrypted_keyb_dict(dir, "secret");
    const fs::path json = write_json_dict(dir);

    const std::string cli_q = shell_quote(cli);
    const std::string mdx_q = shell_quote(mdx.string());
    const std::string json_q = shell_quote(json.string());

    int rc = 0;
    std::string out = run_capture("env -u UNIDICT_MDICT_PASSWORD -u UNIDICT_PASSWORD " + cli_q + " -d " + mdx_q + " alpha", rc);
    assert(rc == 7);
    assert(out.empty());

    out = run_capture("env -u UNIDICT_PASSWORD UNIDICT_MDICT_PASSWORD=wrong " + cli_q + " -d " + mdx_q + " alpha", rc);
    assert(rc == 7);
    assert(out.empty());

    out = run_capture("env -u UNIDICT_PASSWORD " + cli_q + " -d " + mdx_q + " --mdict-password secret alpha", rc);
    assert(rc == 0);
    assert(out.find("alpha: A") != std::string::npos);

    out = run_capture("env -u UNIDICT_PASSWORD UNIDICT_MDICT_PASSWORD=wrong " + cli_q + " -d " + mdx_q + " --mdict-password secret beta", rc);
    assert(rc == 0);
    assert(out.find("beta: B") != std::string::npos);

    out = run_capture("env -u UNIDICT_MDICT_PASSWORD UNIDICT_PASSWORD=secret " + cli_q + " -d " + mdx_q + " beta", rc);
    assert(rc == 0);
    assert(out.find("beta: B") != std::string::npos);

    out = run_capture("env -u UNIDICT_PASSWORD UNIDICT_MDICT_PASSWORD=wrong " + cli_q + " -d " + json_q + " -d " + mdx_q + " --mdict-password secret --all alpha", rc);
    assert(rc == 0);
    assert(out.find("FallbackJSON: json-alpha") != std::string::npos);
    assert(out.find("EncryptedCLI: A") != std::string::npos);

    return 0;
}
