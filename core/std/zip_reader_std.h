#ifndef UNIDICT_ZIP_READER_STD_H
#define UNIDICT_ZIP_READER_STD_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace UnidictCoreStd {

// 只读 zip 容器解析（EPUB 词典用）。支持 stored(0)/deflate(8) 两种压缩
// 方法——EPUB 归档工具产出的就是这两种；zip64 与加密条目明确拒绝。
// 防御面（畸形/恶意 zip 不崩溃、不失控）：
// - EOCD 从文件尾回扫，注释长度必须自洽
// - central directory 与条目头的 offset/size 全部做越界校验
// - 解压输出以 central directory 声明的 uncompressed_size 封顶（zip 炸弹防线）
// - CRC32 校验解压结果
class ZipReaderStd {
public:
    bool open(const std::string& path);
    bool is_open() const { return loaded_; }

    std::vector<std::string> entry_names() const;
    // 返回该条目解压后的完整内容；条目不存在或解析/解压失败返回 false
    bool read_entry(const std::string& name, std::string& out) const;

private:
    struct Entry {
        uint16_t method = 0;
        uint32_t crc32 = 0;
        uint32_t compressed_size = 0;
        uint32_t uncompressed_size = 0;
        uint64_t local_header_offset = 0;
    };

    bool loaded_ = false;
    std::vector<uint8_t> data_; // 整个归档读进内存（词典 epub 几 MB～几十 MB）
    std::unordered_map<std::string, Entry> entries_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_ZIP_READER_STD_H
