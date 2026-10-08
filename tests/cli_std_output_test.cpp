// cli-std 六模式查询的结构化输出契约（P-11 查词能力线批四）。
//
// 词表四模式（prefix/fuzzy/wildcard/regex）原来是 grep 形裸词表——命中
// 规模与词典归属都不可读；批四升为「# 计数头 + 词头<TAB>词典归属」行：
// # 头一行 grep -v 可滤，cut -f1 仍取回裸词表，grep 形消费不破。
// fulltext 补计数头 + [词典] 归属；exact miss 接 suggest_corrections
// 建议块（与 qmlui lookupDefinition 同源的模糊优先 + 前缀补位口径）。
//
// 既有契约不破：词典全挂时 exact 静默 + exit 7（test_cli_std_mdict_
// password 另钉），有词典可查时才报 Word not found。

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

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

// 断言带现场：rc/整段输出/用例标签一起打出——裸 assert 只给行号，
// CI 上没法直接重放子进程命令
static void expect(bool cond, int rc, const std::string& out, const char* what) {
    if (!cond) {
        std::cerr << "[cli-output] FAIL " << what << " (rc=" << rc << ")\n"
                  << "---- stdout ----\n" << out << "----------------\n";
    }
    assert(cond);
}

static fs::path write_dict(const fs::path& dir, const std::string& name,
                           const std::string& body) {
    fs::create_directories(dir);
    const fs::path p = dir / name;
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << body;
    return p;
}

int main(int argc, char** argv) {
    assert(argc >= 2);
    const std::string cli = argv[1];

    const fs::path dir = fs::current_path() / "build-local" / "cli_output";
    const fs::path a = write_dict(dir, "out_a.json", R"({
  "name": "OutA",
  "description": "output shape fixture a",
  "entries": [
    {"word": "hello",  "definition": "A greeting or expression of goodwill."},
    {"word": "helper", "definition": "a tool that helps"},
    {"word": "helped", "definition": "past of help"},
    {"word": "world",  "definition": "the earth"},
    {"word": "qt",     "definition": "a c++ gui framework"}
  ]
})");
    const fs::path b = write_dict(dir, "out_b.json", R"({
  "name": "OutB",
  "description": "output shape fixture b",
  "entries": [
    {"word": "hello", "definition": "hi there"},
    {"word": "world", "definition": "the world"}
  ]
})");

    const std::string cli_q = shell_quote(cli);
    const std::string a_q = shell_quote(a.string());
    const std::string b_q = shell_quote(b.string());
    // 环境词典路径与密码一律剥掉：本用例只认 -d 显式夹具
    const std::string clean_env =
        "env -u UNIDICT_DICTS -u UNIDICT_MDICT_PASSWORD -u UNIDICT_PASSWORD ";

    int rc = 0;

    // 1) exact 命中：正文形态不动（`词头: 释义`，首个词典释义）
    std::string out = run_capture(clean_env + cli_q + " -d " + a_q + " -d " + b_q + " hello", rc);
    expect(rc == 0, rc, out, "1 exact-hit rc");
    expect(out.find("hello: A greeting") != std::string::npos, rc, out, "1 exact-hit body");

    // 2) exact miss（有词典可查）：未找到行 + Did-you-mean 建议块。
    //    helo 距 hello 编辑距离 1，模糊层必出；建议行两格缩进
    out = run_capture(clean_env + cli_q + " -d " + a_q + " -d " + b_q + " helo", rc);
    expect(rc == 7, rc, out, "2 exact-miss rc");
    expect(out.find("Word not found: helo") != std::string::npos, rc, out, "2 not-found line");
    expect(out.find("Did you mean:") != std::string::npos, rc, out, "2 did-you-mean header");
    expect(out.find("  hello") != std::string::npos, rc, out, "2 suggestion body");

    // 3) exact miss（词典全挂）：全静默 + exit 7，连 Word not found 都不打
    out = run_capture(clean_env + cli_q + " -d " + shell_quote((dir / "no_such.json").string()) + " zzzqqq", rc);
    expect(rc == 7, rc, out, "3 dead-dict rc");
    expect(out.empty(), rc, out, "3 dead-dict silence");

    // 4) prefix：计数头 + 每词词典归属（hel → hello/helper/helped 三词）。
    //    trie children 是 unordered_map，成员断言不钉序
    out = run_capture(clean_env + cli_q + " -d " + a_q + " -d " + b_q + " -m prefix hel", rc);
    expect(rc == 0, rc, out, "4 prefix rc");
    expect(out.find("# prefix \"hel\" -> 3 matches") != std::string::npos, rc, out, "4 prefix header");
    expect(out.find("hello\tOutA, OutB") != std::string::npos, rc, out, "4 hello attribution");
    expect(out.find("helper\tOutA") != std::string::npos, rc, out, "4 helper attribution");
    expect(out.find("helped\tOutA") != std::string::npos, rc, out, "4 helped attribution");

    // 5) fuzzy 全 miss：计数头照样打（0 matches），exit 7
    out = run_capture(clean_env + cli_q + " -d " + a_q + " -m fuzzy zzzqqq", rc);
    expect(rc == 7, rc, out, "5 fuzzy-miss rc");
    expect(out.find("# fuzzy \"zzzqqq\" -> 0 matches") != std::string::npos, rc, out, "5 fuzzy-miss header");

    // 6) wildcard：真查询走 -p（计数头显示 pattern，不是位置参数）；
    //    pattern 过 shell 会通配展开，必须带引号下发
    out = run_capture(clean_env + cli_q + " -d " + a_q + " -d " + b_q +
                      " -m wildcard -p " + shell_quote("w*rld") + " dummy", rc);
    expect(rc == 0, rc, out, "6 wildcard rc");
    expect(out.find("# wildcard \"w*rld\" -> 1 matches") != std::string::npos, rc, out, "6 wildcard header");
    expect(out.find("world\tOutA, OutB") != std::string::npos, rc, out, "6 world attribution");

    // 7) regex：位置参数即查询。只能带一个位置参数——CLI 对多个位置参数
    //    后者覆盖前者（word 单变量），再垫个 dummy 会把查询顶掉。
    //    查询串避用 cmd 特殊字符（^ 等）：Windows popen 走 cmd.exe，^ 是
    //    转义符，POSIX 单引号引用保不住（CI 实证 '^q' 被吃成 q）；q. 双
    //    平台无特化且同样只命中 qt
    out = run_capture(clean_env + cli_q + " -d " + a_q + " -m regex " + shell_quote("q."), rc);
    expect(rc == 0, rc, out, "7 regex rc");
    expect(out.find("# regex \"q.\" -> 1 matches") != std::string::npos, rc, out, "7 regex header");
    expect(out.find("qt\tOutA") != std::string::npos, rc, out, "7 qt attribution");

    // 8) fulltext：计数头 + [词典] 归属正文（greeting 只在 OutA hello 释义里）
    out = run_capture(clean_env + cli_q + " -d " + a_q + " -d " + b_q +
                      " -m fulltext greeting", rc);
    expect(rc == 0, rc, out, "8 fulltext rc");
    expect(out.find("# fulltext \"greeting\" -> 1 matches") != std::string::npos, rc, out, "8 fulltext header");
    expect(out.find("hello [OutA]: A greeting") != std::string::npos, rc, out, "8 fulltext body");

    return 0;
}
