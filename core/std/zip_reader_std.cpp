#include <cstdint>
#include "zip_reader_std.h"

#include <algorithm>
#include <cstring>
#include <fstream>

extern "C" {
#include <zlib.h>
}

namespace UnidictCoreStd {

namespace {

// 归档上限：词典 epub 再大也在百 MB 量级，超过按损坏拒绝（防呆不防真）
constexpr size_t kMaxArchiveBytes = 1024ull * 1024 * 1024;
constexpr size_t kMaxEntryBytes = 256ull * 1024 * 1024; // 单条目解压上限（谎报 CDE 防线）
constexpr size_t kMinZipBytes = 22; // 空 EOCD 就占 22 字节

uint16_t read_u16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint16_t>(data[offset]) |
           static_cast<uint16_t>(data[offset + 1]) << 8;
}

uint32_t read_u32(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint32_t>(data[offset]) |
           static_cast<uint32_t>(data[offset + 1]) << 8 |
           static_cast<uint32_t>(data[offset + 2]) << 16 |
           static_cast<uint32_t>(data[offset + 3]) << 24;
}

std::string read_string(const std::vector<uint8_t>& data, size_t offset, size_t length) {
    return std::string(data.begin() + static_cast<std::ptrdiff_t>(offset),
                       data.begin() + static_cast<std::ptrdiff_t>(offset + length));
}

// inflate raw deflate 流，输出以 limit 封顶；超过 limit 或流提前结束都算失败。
// expected_crc 非 0 时校验解压结果的 CRC32。
bool inflate_raw(const uint8_t* input, size_t input_size, size_t limit,
                 uint32_t expected_crc, std::string& out) {
    out.clear();
    out.reserve(limit);

    z_stream stream{};
    if (inflateInit2(&stream, -15) != Z_OK) { // raw deflate，无 zlib/gzip 头
        return false;
    }

    stream.next_in = const_cast<Bytef*>(input);
    stream.avail_in = static_cast<uInt>(input_size);
    int ret = Z_OK;
    std::vector<uint8_t> chunk(65536);
    while (ret != Z_STREAM_END) {
        stream.next_out = chunk.data();
        stream.avail_out = static_cast<uInt>(chunk.size());
        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&stream);
            return false;
        }
        out.append(reinterpret_cast<const char*>(chunk.data()),
                   chunk.size() - stream.avail_out);
        if (out.size() > limit) { // 声明大小之外还有产出 → 谎报/炸弹
            inflateEnd(&stream);
            return false;
        }
        if (ret == Z_OK && stream.avail_in == 0 && stream.avail_out != 0) {
            // 输入耗尽但流未结束：截断的 deflate 流
            inflateEnd(&stream);
            return false;
        }
    }
    inflateEnd(&stream);

    if (out.size() != limit) {
        return false;
    }
    if (expected_crc != 0 &&
        crc32(0, reinterpret_cast<const Bytef*>(out.data()),
              static_cast<uInt>(out.size())) != expected_crc) {
        return false;
    }
    return true;
}

} // namespace

bool ZipReaderStd::open(const std::string& path) {
    loaded_ = false;
    entries_.clear();
    data_.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < static_cast<std::streamoff>(kMinZipBytes) ||
        size > static_cast<std::streamoff>(kMaxArchiveBytes)) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    data_.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data_.data()), size);
    if (!file) {
        return false;
    }

    // EOCD 定位：从尾部回扫签名，注释长度必须正好补到文件尾
    size_t eocd = data_.size();
    const size_t scan_floor = data_.size() > 65557 ? data_.size() - 65557 : 0;
    for (size_t i = data_.size() - kMinZipBytes + 1; i-- > scan_floor;) {
        if (data_[i] == 'P' && data_[i + 1] == 'K' && data_[i + 2] == 5 && data_[i + 3] == 6) {
            const uint16_t comment_length = read_u16(data_, i + 20);
            if (i + 22 + comment_length == data_.size()) {
                eocd = i;
                break;
            }
        }
    }
    if (eocd == data_.size()) {
        return false; // 没找到自洽的 EOCD
    }

    const uint16_t entry_count = read_u16(data_, eocd + 10);
    const uint32_t cd_size = read_u32(data_, eocd + 12);
    const uint32_t cd_offset = read_u32(data_, eocd + 16);
    if (cd_offset == 0xFFFFFFFFu || cd_size == 0xFFFFFFFFu) {
        return false; // zip64 不支持（词典 epub 不会用到）
    }
    if (static_cast<uint64_t>(cd_offset) + cd_size > eocd) {
        return false; // central directory 越界（EOCD 之前必须放得下）
    }

    size_t pos = cd_offset;
    for (uint16_t i = 0; i < entry_count; ++i) {
        if (pos + 46 > eocd + cd_size || read_u32(data_, pos) != 0x02014b50u) {
            return false; // CDE 签名不符
        }
        const uint16_t method = read_u16(data_, pos + 10);
        const uint32_t crc = read_u32(data_, pos + 16);
        const uint32_t compressed_size = read_u32(data_, pos + 20);
        const uint32_t uncompressed_size = read_u32(data_, pos + 24);
        const uint16_t name_length = read_u16(data_, pos + 28);
        const uint16_t extra_length = read_u16(data_, pos + 30);
        const uint16_t comment_length = read_u16(data_, pos + 32);
        const uint32_t local_offset = read_u32(data_, pos + 42);
        if (method != 0 && method != 8) {
            return false; // 加密/其他压缩方法明确拒绝
        }
        if (pos + 46 + name_length + extra_length + comment_length > eocd + cd_size) {
            return false;
        }

        Entry entry;
        entry.method = method;
        entry.crc32 = crc;
        entry.compressed_size = compressed_size;
        entry.uncompressed_size = uncompressed_size;
        entry.local_header_offset = local_offset;
        entries_[read_string(data_, pos + 46, name_length)] = entry;
        pos += 46 + name_length + extra_length + comment_length;
    }

    loaded_ = true;
    return true;
}

std::vector<std::string> ZipReaderStd::entry_names() const {
    std::vector<std::string> names;
    names.reserve(entries_.size());
    for (const auto& pair : entries_) {
        names.push_back(pair.first);
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool ZipReaderStd::read_entry(const std::string& name, std::string& out) const {
    const auto it = entries_.find(name);
    if (it == entries_.end() || !loaded_) {
        return false;
    }
    const Entry& entry = it->second;
    if (entry.uncompressed_size > kMaxEntryBytes) {
        return false;
    }

    // local file header：data 起点要按 LFH 自己的 name/extra 长度算
    //（CDE 与 LFH 的 extra 可以不同）
    const uint64_t offset = entry.local_header_offset;
    if (offset + 30 > data_.size() || read_u32(data_, static_cast<size_t>(offset)) != 0x04034b50u) {
        return false;
    }
    const uint16_t name_length = read_u16(data_, static_cast<size_t>(offset) + 26);
    const uint16_t extra_length = read_u16(data_, static_cast<size_t>(offset) + 28);
    const uint64_t data_offset = offset + 30 + name_length + extra_length;
    if (data_offset + entry.compressed_size > data_.size()) {
        return false; // 条目数据越界
    }

    const uint8_t* raw = data_.data() + data_offset;
    if (entry.method == 0) { // stored
        if (entry.compressed_size != entry.uncompressed_size) {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(raw), entry.compressed_size);
    } else { // deflate
        if (!inflate_raw(raw, entry.compressed_size, entry.uncompressed_size,
                         entry.crc32, out)) {
            return false;
        }
    }

    if (entry.method == 0 && entry.crc32 != 0 &&
        crc32(0, reinterpret_cast<const Bytef*>(out.data()),
              static_cast<uInt>(out.size())) != entry.crc32) {
        return false;
    }
    return true;
}

} // namespace UnidictCoreStd
