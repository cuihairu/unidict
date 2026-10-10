#include "notes_export_std.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace UnidictCoreStd {

// HTML 受限转义：内容五字符 + 单引号；其余字节（含 UTF-8 多字节）直通
static std::string html_escape(const std::string& s) {
    std::string out; out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out.push_back((char)c); break;
        }
    }
    return out;
}

// 转义后换行统一转 <br>（\r\n 与裸 \r 都归一）
static std::string html_text(const std::string& s) {
    std::string esc = html_escape(s);
    std::string out; out.reserve(esc.size() + 8);
    for (size_t i = 0; i < esc.size(); ++i) {
        if (esc[i] == '\r') {
            if (i + 1 < esc.size() && esc[i + 1] == '\n') ++i;
            out += "<br>";
        } else if (esc[i] == '\n') {
            out += "<br>";
        } else {
            out.push_back(esc[i]);
        }
    }
    return out;
}

// epoch 秒 → UTC "YYYY-MM-DD HH:MM:SS"；0/负 = 未知，留空
static std::string format_time(long long epoch) {
    if (epoch <= 0) return "";
    std::time_t t = static_cast<std::time_t>(epoch);
    std::tm tmv{};
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return buf;
}

NotesExportResultStd export_notes_html(const DataStoreStd& store,
                                       const std::string& out_path) {
    NotesExportResultStd r;
    const std::vector<NoteItemStd> notes = store.get_notes();

    std::error_code ec;
    fs::create_directories(fs::path(out_path).parent_path(), ec);
    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        r.error = "cannot open output file: " + out_path;
        return r;
    }
    // 单行语句：gcov 对链式 ostream 块的行归属会拆散，逐语句写
    out << "<!DOCTYPE html>\n";
    out << "<html>\n";
    out << "<head>\n";
    out << "<meta charset=\"utf-8\">\n";
    out << "<title>Unidict notes</title>\n";
    out << "</head>\n";
    out << "<body>\n";
    out << "<h1>Unidict notes</h1>\n";
    for (const NoteItemStd& n : notes) {
        out << "<section>\n";
        out << "<h2>" << html_escape(n.word) << "</h2>\n";
        out << "<p>" << html_text(n.text) << "</p>\n";
        out << "<p class=\"updated\">" << format_time(n.updated_at)
            << "</p>\n";
        out << "</section>\n";
    }
    out << "</body>\n";
    out << "</html>\n";
    out.flush();
    // flush 后失败=磁盘满/IO 错，跨平台无确定性构造手段（打开失败兜底
    // 已由用例覆盖）——留档防御分支，整行排除
    if (!out) { r.error = "write failed: " + out_path; return r; }  // GCOVR_EXCL_LINE
    r.ok = true;
    r.note_count = static_cast<int>(notes.size());
    return r;
}

} // namespace UnidictCoreStd
