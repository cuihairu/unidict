// charset_codec_std 分支缺口补测（真实缺边 41 条/34 行，throw 边过滤后的
// 下一个巡检簇——既有 stardict_charset 测试只驱动快乐路径：合法高位 trail
// 配对、Cp1252 低位段、salvage 各拒绝臂；低位 trail 槽、lead/trail 越界、
// from_name 全别名表、switch Unknown 臂、UTF-8 过长/超范围序列等真实
// 条件臂全部冷）。
//
// 逐边定性（gcov -b，throw 过滤后）：
//  - 可驱动真实缺边：normalize 大写/小写/数字/标点丢弃臂（经 from_name
//    变体名，含 0x7B–0x7F 段与高位字节的 signed-char 全假臂）、from_name
//    四组全别名真臂 + 全链穿透假臂（big5）、name/to_utf8 的 switch
//    Unknown 臂、Latin1 ASCII+高位臂、Cp1252 0x80–0x9F 段与 0xA0+ 落
//    Latin-1 臂、gb18030_lookup 的 lead<0x81/lead>0xFE（两字节形态进
//    lookup；单字节形态被 i+1 卫语句短路，另钉）、trail=0x7F 未定义槽、
//    trail<0x40（idx<0）、trail 0x40–0x7E 低位段命中、to_utf8 GB18030 的
//    ASCII 直通/查表命中/非法透传/尾随落单 lead、is_valid_utf8 的
//    3 字节过长（E0 80 80）/4 字节过长（F0 80 80 80）/超 Unicode 范围
//    （F5 80 80 80）拒收臂。
//  - 源码侧 1 处死别名已清理：from_name Latin-1 链的 "iso_88591_1" 与
//    归一化后的 n（只含 [a-z0-9]）比较永真臂不可达——删除；用户真实
//    拼写 ISO_8859-1 归一化后命中首别名 iso88591，行为无损。
//  - 止步（不加 EXCL，沿死边口径）：append_utf8 的 4 字节臂入边
//    （cp>0xFFFF——全部调用方码点上限 U+FFE5/0x0178，见源码 GCOVR_EXCL
//    注释，为编码器完整性保留）、gb18030_lookup 的 cp==0 防御臂与
//    idx>=kTrailsPerLead 臂（b2 值域内 idx 上限 189 恒 < 190；b2=0xFF
//    在上一分支先落 idx=-1，结构性不可达的防御界）、name/to_utf8
//    switch 的越界保护边（enum class 五值构造不越界，GCC 跳表界检查
//    死臂）、single_byte_to_utf8:86 的一臂（cp1252 真假 × 0x80 下沿内/
//    外四个逻辑结局均已驱动后 0% 边不再变化——GCC 对相邻区间条件
//    b>=0x80 && b<=0x9F 的内部 CFG 合并边，测试不可达）。

#include <cassert>
#include <string>

#include "std/charset_codec_std.h"

using namespace UnidictCoreStd::CharsetCodec;

int main() {
    // ===== from_name：全别名真臂 + 归一化变体 + 全链穿透 =====
    {
        // UTF-8 组三个别名 + 归一化变体（大写/分隔符/空白）
        assert(from_name("utf8") == Charset::Utf8);
        assert(from_name("UTF-8") == Charset::Utf8);
        assert(from_name("utf8bom") == Charset::Utf8);
        assert(from_name("unicode11utf8") == Charset::Utf8);
        assert(from_name("  Utf_8. ") == Charset::Utf8);
        assert(from_name("Unicode-11-UTF8") == Charset::Utf8);

        // GB 组十四个别名（GB2312/GBK/GB18030 两字节区一张表通吃）
        assert(from_name("gb2312") == Charset::Gb18030);
        assert(from_name("gbk") == Charset::Gb18030);
        assert(from_name("gb18030") == Charset::Gb18030);
        assert(from_name("GB-18030") == Charset::Gb18030);
        assert(from_name("gb 18030") == Charset::Gb18030);
        assert(from_name("GB_18030") == Charset::Gb18030);
        assert(from_name("gb231280") == Charset::Gb18030);
        assert(from_name("csgb2312") == Charset::Gb18030);
        assert(from_name("eucn") == Charset::Gb18030);   // EUC-CN
        assert(from_name("cp936") == Charset::Gb18030);
        assert(from_name("windows936") == Charset::Gb18030);
        assert(from_name("ms936") == Charset::Gb18030);
        assert(from_name("gb2312gbk") == Charset::Gb18030);
        assert(from_name("chinese") == Charset::Gb18030);
        assert(from_name("csgbk") == Charset::Gb18030);
        assert(from_name("gb2312gb18030") == Charset::Gb18030);
        assert(from_name("xgbk") == Charset::Gb18030);

        // Latin-1 组八个别名
        assert(from_name("iso88591") == Charset::Latin1);
        assert(from_name("latin1") == Charset::Latin1);
        assert(from_name("Latin.1") == Charset::Latin1);
        assert(from_name("l1") == Charset::Latin1);
        assert(from_name("iso8859") == Charset::Latin1);
        assert(from_name("88591") == Charset::Latin1);
        assert(from_name("cp819") == Charset::Latin1);
        assert(from_name("latin") == Charset::Latin1);
        // 用户真实拼写 ISO_8859-1：归一化丢 '_'/'-' 后命中首别名
        assert(from_name("ISO_8859-1") == Charset::Latin1);

        // CP1252 组四个别名
        assert(from_name("windows1252") == Charset::Cp1252);
        assert(from_name("cp1252") == Charset::Cp1252);
        assert(from_name("CP 1252") == Charset::Cp1252);
        assert(from_name("1252") == Charset::Cp1252);
        assert(from_name("xcp1252") == Charset::Cp1252);

        // 穿透所有四条别名链的假臂：Big5 刻意不收（如实报 Unknown，
        // 好过默默乱码），连同纯标点（归一化后为空）与数字名
        assert(from_name("big5") == Charset::Unknown);
        assert(from_name("EUC-JP") == Charset::Unknown);
        assert(from_name("shift_jis") == Charset::Unknown);
        assert(from_name("") == Charset::Unknown);
        assert(from_name(" - _ . ") == Charset::Unknown);
        assert(from_name("###") == Charset::Unknown);
        // 0x7B–0x7F 段（c>='a' 真而 c<='z' 假的唯一区间）与高位字节
        // （signed char 为负，全部比较假）：都落丢弃臂
        assert(from_name("a{b|c}d~e") == Charset::Unknown);
        assert(from_name(std::string("ut\x7F" "f\xC3")) ==
               Charset::Unknown);
    }

    // ===== name/is_supported：switch 全臂（含 Unknown 空串）=====
    {
        assert(name(Charset::Utf8) == std::string("UTF-8"));
        assert(name(Charset::Latin1) == std::string("ISO-8859-1"));
        assert(name(Charset::Cp1252) == std::string("Windows-1252"));
        assert(name(Charset::Gb18030) == std::string("GB18030"));
        assert(name(Charset::Unknown) == std::string(""));  // Unknown 臂

        assert(is_supported(Charset::Utf8));
        assert(is_supported(Charset::Latin1));
        assert(is_supported(Charset::Cp1252));
        assert(is_supported(Charset::Gb18030));
        assert(!is_supported(Charset::Unknown));
    }

    // ===== to_utf8：Latin-1 逐字节臂 =====
    {
        // ASCII 臂（也走统一编码路径）
        assert(to_utf8(Charset::Latin1, "abc") == "abc");
        // 高位字节即码点：0xE9 → U+00E9（2 字节 UTF-8 C3 A9）
        assert(to_utf8(Charset::Latin1, std::string("\xE9", 1)) ==
               std::string("\xC3\xA9", 2));
        assert(to_utf8(Charset::Latin1, std::string("\xE9\x28", 2)) ==
               std::string("\xC3\xA9(", 3));
    }

    // ===== to_utf8：CP1252 高位段 0x80–0x9F 与 0xA0+ 落 Latin-1 臂 =====
    {
        // 0x80 → U+20AC（€，3 字节）；0x9F → U+0178（2 字节）——
        // 高位表全段两端
        assert(to_utf8(Charset::Cp1252, std::string("\x80", 1)) ==
               std::string("\xE2\x82\xAC", 3));
        assert(to_utf8(Charset::Cp1252, std::string("\x9F", 1)) ==
               std::string("\xC5\xB8", 2));
        // 0xA0+ 不在高位表：按 Latin-1 高位字节即码点
        assert(to_utf8(Charset::Cp1252, std::string("\xE9", 1)) ==
               std::string("\xC3\xA9", 2));
        assert(to_utf8(Charset::Cp1252, std::string("\xFF", 1)) ==
               std::string("\xC3\xBF", 2));
        // ASCII 臂（含 0x7F，紧贴 0x80 下沿）
        assert(to_utf8(Charset::Cp1252, "hi") == "hi");
        assert(to_utf8(Charset::Cp1252, std::string("\x7F", 1)) ==
               std::string("\x7F", 1));
    }

    // ===== to_utf8：GB18030 查表/越界/透传全臂 =====
    {
        // ASCII 直通臂
        assert(to_utf8(Charset::Gb18030, "ab") == "ab");
        // 查表命中：低位 trail 段（0x40–0x7E）——(0x81,0x40)→U+4E02、
        // (0x81,0x44)→U+4E0F（表首行实值，确定性）
        assert(to_utf8(Charset::Gb18030, std::string("\x81\x40", 2)) ==
               std::string("\xE4\xB8\x82", 3));
        assert(to_utf8(Charset::Gb18030, std::string("\x81\x44", 2)) ==
               std::string("\xE4\xB8\x8F", 3));
        // 高位 trail 段（0x80–0xFE）命中臂沿用既有测试，此处混排验证
        // 前后文推进（2 字节消费后继续 ASCII）
        assert(to_utf8(Charset::Gb18030,
                       std::string("\x81\x40z", 3)) ==
               std::string("\xE4\xB8\x82z", 4));

        // lead 越界两臂（两字节形态进 lookup）：0x80（<0x81）与
        // 0xFF（>0xFE）→ 首字节透传、次字节走 ASCII
        assert(to_utf8(Charset::Gb18030, std::string("\x80\x41", 2)) ==
               std::string("\x80\x41", 2));
        assert(to_utf8(Charset::Gb18030, std::string("\xFF\x41", 2)) ==
               std::string("\xFF\x41", 2));
        // 单字节形态不进 lookup（i+1 卫语句），直接透传
        assert(to_utf8(Charset::Gb18030, std::string("\x80", 1)) ==
               std::string("\x80", 1));
        assert(to_utf8(Charset::Gb18030, std::string("\xFF", 1)) ==
               std::string("\xFF", 1));
        // trail=0x7F 未定义槽 → 首字节透传后 0x7F 走 ASCII 直通（两字节都不丢）
        assert(to_utf8(Charset::Gb18030, std::string("\x81\x7F", 2)) ==
               std::string("\x81\x7F", 2));
        // trail<0x40（idx<0 臂）→ 首字节透传后 0x30 走 ASCII 直通
        assert(to_utf8(Charset::Gb18030, std::string("\x81\x30", 2)) ==
               std::string("\x81\x30", 2));
        // trail=0xFF（idx 越界臂）→ 首字节透传后 0xFF 落单透传
        assert(to_utf8(Charset::Gb18030, std::string("\x81\xFF", 2)) ==
               std::string("\x81\xFF", 2));
        // 尾随落单 lead（i+1 越界臂）→ 透传
        assert(to_utf8(Charset::Gb18030, std::string("a\x81", 2)) ==
               std::string("a\x81", 2));
    }

    // ===== to_utf8：switch Unknown 臂（原样透传，绝不猜）=====
    {
        assert(to_utf8(Charset::Unknown, std::string("\xFF\xFE", 2)) ==
               std::string("\xFF\xFE", 2));
        assert(to_utf8(Charset::Unknown, "") == "");
    }

    // ===== is_valid_utf8：合法形态 + 过长/超范围拒收臂 =====
    {
        // 合法：1/2/3/4 字节各一（U+0041/U+00E9/U+6307/U+1F600）
        assert(is_valid_utf8("A"));
        assert(is_valid_utf8("\xC3\xA9"));
        assert(is_valid_utf8("\xE6\x8C\x87"));
        assert(is_valid_utf8("\xF0\x9F\x98\x80"));
        assert(is_valid_utf8(""));
        // 过长 2 字节（C0 80）——既有口径已拒，钉住
        assert(!is_valid_utf8(std::string("\xC0\x80", 2)));
        // 过长 3 字节（E0 80 80，cp=0 < 0x800）
        assert(!is_valid_utf8(std::string("\xE0\x80\x80", 3)));
        // 过长 4 字节（F0 80 80 80，cp=0 < 0x10000）
        assert(!is_valid_utf8(std::string("\xF0\x80\x80\x80", 4)));
        // 超 Unicode 范围（F5 80 80 80，cp=0x140000 > 0x10FFFF）
        assert(!is_valid_utf8(std::string("\xF5\x80\x80\x80", 4)));
        // 代理区（ED A0 80，U+D800）
        assert(!is_valid_utf8(std::string("\xED\xA0\x80", 3)));
        // 截断 / 坏续字节 / 非法 lead
        assert(!is_valid_utf8(std::string("\xC3", 1)));
        assert(!is_valid_utf8(std::string("\xC3\x41", 2)));
        assert(!is_valid_utf8(std::string("\xFF", 1)));
        assert(!is_valid_utf8(std::string("\x80", 1)));
    }

    // ===== looks_like_gb18030 / salvage：拒绝臂 + 成功臂 =====
    {
        assert(looks_like_gb18030(std::string("\x81\x40", 2)));
        assert(looks_like_gb18030("ascii"));  // 纯 ASCII 空谓词成立
        // 混排全配对（\x 后跟 hex 字母会被贪婪吞成越界转义，字面量须拆）
        assert(looks_like_gb18030(std::string("a\x81" "\x44" "b", 4)));
        assert(!looks_like_gb18030(std::string("\x81", 1)));    // 落单 lead
        assert(!looks_like_gb18030(std::string("\x81\x7F", 2))); // 未定义槽

        std::string out;
        // 成功臂：声明 Utf8 但数据不是合法 UTF-8 且 GB 表可解
        // （\x81\x40 → U+4E02；注意 D6 B8 这种"既是指又是 U+05B7"的
        // 两面字节走不了成功臂——它本身就是合法 UTF-8）
        out.clear();
        assert(salvage_as_gb18030(std::string("x\x81\x40", 3),
                                  Charset::Utf8, out));
        assert(out == std::string("x\xE4\xB8\x82", 4));
        // Unknown 声明同样可救
        out.clear();
        assert(salvage_as_gb18030(std::string("\x81\x40", 2),
                                  Charset::Unknown, out));
        assert(out == std::string("\xE4\xB8\x82", 3));
        // 拒绝：已支持的声明（非 Utf8）/ 空字节 / 本身合法 UTF-8
        //（D6 B8 是 U+05B7，绝不碰）/ 不像 GBK
        assert(!salvage_as_gb18030(std::string("\x81\x40", 2),
                                   Charset::Gb18030, out));
        assert(!salvage_as_gb18030("", Charset::Unknown, out));
        assert(!salvage_as_gb18030("plain", Charset::Unknown, out));
        assert(!salvage_as_gb18030(std::string("\xD6\xB8", 2),
                                   Charset::Unknown, out));
        assert(!salvage_as_gb18030(std::string("\x81\x7F", 2),
                                   Charset::Unknown, out));
    }

    return 0;
}
