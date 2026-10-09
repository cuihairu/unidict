// 真实 MDict v2 .mdx 忠实读取（引擎 2.0 节布局，std-only）。
//
// 布局口径与 MddResourceParser::parse_mdict_sections（.mdd 侧）及 Qt 版
// MdictParser::loadMdxFile（tests/mdict_fixture.h buildMdxBytes 印证）一致：
//   头：u32 BE 文本长 + UTF-16LE XML 属性串 + u32 LE adler32（读而不验，
//       上游写库同口径）
//   key 节：5×u64 BE（num_key_blocks / num_entries / 索引块解压长 /
//           索引块长 / key 块总长）+ u32 BE adler（读而不验）
//   索引块（key block info）条目 ×num_key_blocks：
//       { u64 块内条目数(读而不验); u16 首词单元数; 首词+NUL;
//         u16 末词单元数; 末词+NUL; u64 压缩长; u64 解压长 }
//       （单元数 = UTF-16LE 编码 2 字节/单元，其余编码 1 字节/单元）
//   key 块条目：{ u64 record_offset; 词头(按头 Encoding 编码); NUL 终止符 }
//   record 节：4×u64 BE（num_record_blocks / num_entries / info 长 /
//           块总长）+ n×{ u64 压缩长; u64 解压长 }（不压缩直排）+ 压缩块
//   record offset 指全部 record 块解压后拼接流中的位置，释义长度由相邻
//   词头 offset 差推导（键序与 record 写入序一致）。
// 通用块封装：{ u32 BE 压缩类型(2=zlib/0=存储); u32 BE adler32; 载荷 }，
//   adler 读而不校验（.mdd 侧同口径）。
//
// 编码：词头/释义按头 Encoding 属性转 UTF-8——UTF-16LE 自转，GBK/GB2312/
//   GB18030 走 charset_codec，Latin-1/CP1252 同走 charset_codec，缺省按
//   UTF-8 透传。
//
// 加密（头 Encrypted 属性 & 2）：索引块与 key 块套 mdict_crypto 口径的加
//   密壳 { u32 LE 块信息字(低4压缩/次4加密=1/8位加密字节数);
//   u32 LE adler32(明文数据的 adler); 密文(前 N 字节) }。密钥按
//   mdict_crypto_std 的既有派生：索引块 key_info_key(adler 字节)、key 块
//   block_key(adler 字节)（RIPEMD-128）。解密后先过 adler32 判据（权威，
//   防误解密出垃圾）再按块信息字的压缩方式解压。Encrypted & 1（record 块
//   Salsa20，需用户 regcode+userid 派生）不支持——诚实失败并给诊断，
//   绝不解出错误数据。
//
// 容器：文件若以 PK 魔数开头，按 zip 容器处理——ZipReaderStd 解包取 .mdx
//   条目（优先同名 stem.mdx，其次首个 .mdx 条目）后递归走同一解析。
//
// 已知边界：节头计数字段按定长 u64 读（上列口径三方一致）；真实
//   MdxBuilder 对 num_entries 的 vint 紧凑编码（≥128 时 9 字节形式）变体
//   不在本路径内，待真实样本批次补测。LZO 压缩块（引擎 1.x）不支持。

#ifndef UNIDICT_MDx_V2_READER_STD_H
#define UNIDICT_MDx_V2_READER_STD_H

#include <string>
#include <vector>

namespace UnidictCoreStd {

struct MdxV2Entry {
    std::string key;         // 词头（已转 UTF-8）
    std::string definition;  // 释义原文（已转 UTF-8，@@@LINK= 语义留给上层）
};

struct MdxV2Dict {
    std::string title;
    std::string description;
    std::string encoding;  // 头声明的 Encoding 原串（空 = 未声明）
    std::vector<MdxV2Entry> entries;
};

class MdxV2ReaderStd {
public:
    // 从文件读。自动处理 zip 容器形态（PK 魔数）。失败返回假并给诊断串
    // （诊断非空 = 探测到疑似 v2 但解析失败；诊断空 = 不是 v2 文件）。
    static bool read_file(const std::string& path, MdxV2Dict& out,
                          std::string& diag);

    // 从内存缓冲读裸 .mdx 字节（zip 解包后的递归入口）。
    static bool read_buffer(const std::string& data, MdxV2Dict& out,
                            std::string& diag);
};

}  // namespace UnidictCoreStd

#endif  // UNIDICT_MDx_V2_READER_STD_H
