// UTF-8 std::string 路径 → std::filesystem::path（审核批 2026-11-12）。
// Windows 上 ifstream/ofstream 的 std::string 重载按 ANSI 代码页解码，
// 非 ASCII 词典/索引路径直接打不开；fs::u8path 把字节按 UTF-8 转宽后走
// 宽接口。POSIX 下逐字节保留（无转换），行为不变。对齐 mdd 侧
// open_mdd_shared 的 CP_UTF8 口径。

#ifndef UNIDICT_FS_PATH_STD_H
#define UNIDICT_FS_PATH_STD_H

#include <filesystem>
#include <string>

namespace UnidictCoreStd {

inline std::filesystem::path u8_path(const std::string& p) {
    return std::filesystem::u8path(p);
}

} // namespace UnidictCoreStd

#endif // UNIDICT_FS_PATH_STD_H
