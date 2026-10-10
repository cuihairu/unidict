#ifndef NOTES_EXPORT_STD_H
#define NOTES_EXPORT_STD_H

#include "data_store_std.h"

#include <string>

namespace UnidictCoreStd {

// 笔记导出结果（口径同 dictionary_export_std）：ok=写入成功；
// error=失败原因；note_count=写出条数
struct NotesExportResultStd {
    bool ok = false;
    std::string error;
    int note_count = 0;
};

// 词条笔记导出为独立 HTML（roadmap Export notes）：按 get_notes 顺序
// 输出 section；word/text 做 HTML 转义（& < > " '），换行统一转 <br>；
// updated_at（epoch 秒）格式化为 UTC "YYYY-MM-DD HH:MM:SS"，0/负留空。
// 空笔记库也写出合法文档（ok=true, note_count=0）。父目录自动创建 +
// 打开失败兜底，语义同 JSON 词典导出。
NotesExportResultStd export_notes_html(const DataStoreStd& store,
                                       const std::string& out_path);

} // namespace UnidictCoreStd

#endif // NOTES_EXPORT_STD_H
