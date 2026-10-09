#include "dictionary_export_std.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace UnidictCoreStd {

// 受限转义表（与 data_store_std/dictionary_manager_std 状态文件同口径）：
// 只写 \\ \" \n \r \t；UTF-8 直通不产 \u，\{ 不转义（JSON 无括号转义，
// 解析侧对象扫描字符串感知可跳过）。
static std::string json_escape(const std::string& s) {
    std::string out; out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back((char)c); break;
        }
    }
    return out;
}

DictionaryExportResultStd export_dictionary_json(const DictionaryManagerStd& manager,
                                                 const std::string& dict_name,
                                                 const std::string& out_path) {
    DictionaryExportResultStd r;
    const auto loaded = manager.loaded_dictionaries();
    if (std::find(loaded.begin(), loaded.end(), dict_name) == loaded.end()) {
        r.error = "dictionary not found: " + dict_name;
        return r;
    }
    const std::vector<DictEntryStd> entries = manager.dictionary_entries(dict_name);

    std::string description;
    for (const auto& m : manager.dictionaries_meta()) {
        if (m.name == dict_name) { description = m.description; break; }
    }

    std::error_code ec;
    fs::create_directories(fs::path(out_path).parent_path(), ec);
    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        r.error = "cannot open output file: " + out_path;
        return r;
    }
    // 单行语句：gcov 对链式 ostream 块的行归属会拆散（首段块计数
    // 丢失误报未覆盖），逐语句写让行计数干净
    out << "{\n";
    out << "  \"name\": \"" << json_escape(dict_name) << "\",\n";
    out << "  \"description\": \"" << json_escape(description) << "\",\n";
    out << "  \"entries\": [\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        out << "    { \"word\": \"" << json_escape(entries[i].word)
            << "\", \"definition\": \"" << json_escape(entries[i].definition)
            << "\" }" << (i + 1 < entries.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    out.flush();
    // flush 后失败=磁盘满/IO 错，跨平台无确定性构造手段（打开失败兜底
    // 已由用例覆盖）——留档防御分支，整行排除
    if (!out) { r.error = "write failed: " + out_path; return r; }  // GCOVR_EXCL_LINE
    r.ok = true;
    r.entry_count = static_cast<int>(entries.size());
    return r;
}

} // namespace UnidictCoreStd
