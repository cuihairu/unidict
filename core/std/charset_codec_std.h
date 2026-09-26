// 词典字节流 → UTF-8 的转码层（无 Qt、无 ICU、无 iconv）。
//
// 存在的原因：词典文件的编码由 .ifo / .mdx 头自己声明，而不是由系统区域设置
// 决定。core/std 里其它一切（正则、排序、匹配、JSON）都假定 std::string 是
// UTF-8，只有这里负责把"非 UTF-8 的词典字节"收敛成 UTF-8，让上层永远只见
// 一种编码。
//
// 覆盖范围按"真实词典里出现频率 × 实现成本"选：
//   UTF-8     —— 透传（含非法序列，不做纠错）
//   Latin-1   —— ISO-8859-1，西欧老词典
//   CP1252    —— Windows-1252，与 Latin-1 只差 0x80–0x9F，但差的那段
//                恰恰是弯引号/破折号/欧元，误转会得到控制字符
//   GB18030   —— 两字节区，**通吃 GB2312 / GBK / GB18030**（三者两字节区完全
//                相同，GB18030 向下兼容）。中文学习词典绝大多数是这一类。
// 不支持的编码走 Unknown：字节原样透传，并在 supports() 里如实上报，
// 由 UI 提示"该词典编码暂不支持"，而不是默默给用户一片乱码。

#ifndef UNIDICT_CHARSET_CODEC_STD_H
#define UNIDICT_CHARSET_CODEC_STD_H

#include <string>

namespace UnidictCoreStd::CharsetCodec {

enum class Charset {
    Unknown = 0,  // 无法识别或暂不支持
    Utf8,
    Latin1,
    Cp1252,
    Gb18030,
};

// .ifo 的 charset 字段 / MDX 头的 Encoding 字段 → 枚举。
// 归一化规则：去空白、转小写、'-'/'_'/' ' 统一丢弃。所以
// "GB-18030"、"gb 18030"、"GB_18030"、"GB18030" 都命中同一个值。
// 收录常见别名：GB2312/GBK/GB_2312-80/CSGB2312/EUC-CN/CP936/Windows-936
// 归到 Gb18030；UTF8/utf-8 归到 Utf8；ISO8859-1/ISO_8859-1/latin1/latin-1
// 归到 Latin1；windows-1252/cp1252 归到 Cp1252。
Charset from_name(const std::string& name);

// 规范化名字，用于 UI/日志。Unknown 返回空串。
const char* name(Charset cs);

// 是否能转码。Unknown == false。
bool is_supported(Charset cs);

// bytes → UTF-8。
//  - 已是合法 UTF-8 的内容原样返回（不重新编码，避免无谓的字节变化）；
//  - 单字节编码逐字节映射；
//  - GB18030 按两字节区查表，ASCII（< 0x80）原样透传。
//
//  **不做启发式探测**：GB18030 两字节区与 UTF-8 在字节层面无法无歧义区分
//  （单是 D6 B8 就既是 GBK 的"指"又是合法的 UTF-8 序列 U+05B7），猜测只会
//  静默产出乱码。声明的编码即权威；漏写/错标 charset 的兜底见
//  salvage_as_gb18030。
//  - **非法字节原样透传，不替换、不丢弃**：残缺序列宁可原样交给上层显示成
//    替换字符，也不能被悄悄改写成 '?'——那等于篡改用户词条内容。
std::string to_utf8(Charset cs, const std::string& bytes);

// 字节流是否是**合法**的 UTF-8。用于"声明 UTF-8 但实际是 GBK"的兜底判断：
// 声明不可信时，只有"不是合法 UTF-8"才允许尝试按 GB18030 救；本身合法
// UTF-8 的一律不动，避免把正常内容误伤。
bool is_valid_utf8(const std::string& bytes);

// 是不是"像 GBK"：每个非 ASCII 字节都能作为 lead/trail 配成完整两字节序列。
// 与 is_valid_utf8 一起构成误标编码的兜底判据。
bool looks_like_gb18030(const std::string& bytes);

// 误标编码兜底：声明 cs（通常是 Utf8 或 Unknown）但数据其实不是合法 UTF-8、
// 且能按 GB18030 解出时，返回 true 并把 bytes 就地改写成 UTF-8。
//
// 只在"确定不是合法 UTF-8"时才动手：合法 UTF-8 原样不动。宁可漏救（用户看到
// 乱码 + 一条提示），不可误改（把正常内容啃坏且无人察觉）。
bool salvage_as_gb18030(const std::string& bytes, Charset cs, std::string& out);

}  // namespace UnidictCoreStd::CharsetCodec

#endif  // UNIDICT_CHARSET_CODEC_STD_H
