// u8_path（core/std/fs_path_std.h）审核批 2026-11-12：Windows 上窄字节
// ifstream/ofstream 按 ANSI 代码页解码路径，非 ASCII 词典/索引路径直接
// 打不开。修法 = 统一经 u8_path 转 fs::path（Windows 按 UTF-8→宽，POSIX
// 逐字节保留）。Linux CI 无法复现 ACP 解码的坏行为，本测试钉的是转换
// 契约：字节保真不转码，以及经真实文件系统（中日韩文件名）建/开/删
// 全链路可用——路由若被改回 locale 相关转换，非 ASCII 名在这段就断。
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "std/fs_path_std.h"

using UnidictCoreStd::u8_path;
namespace fs = std::filesystem;

int main() {
    // 字节保真：UTF-8 名逐字节进、逐字节出（不经任何本地编码转写）
    const std::string name = "日本語辞書-词典-café.json";
    const fs::path p = u8_path(name);
    assert(p.u8string() == name);

    // 真实文件系统链路：建目录 + 写 + 读 + 删，全走 u8_path
    const fs::path dir = fs::temp_directory_path() /
                         u8_path("unidict-fs-path-测试");
    fs::create_directories(dir);
    const fs::path file = dir / u8_path(name);
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        assert(out.good());
        out << "hello";
    }
    {
        std::ifstream in(file, std::ios::binary);
        assert(in.good());
        std::string body((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
        assert(body == "hello");
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
    assert(!ec);

    // POSIX 退化形态：ASCII 名行为不变
    assert(u8_path("plain.dict").u8string() == "plain.dict");

    std::cout << "OK\n";
    return 0;
}
