// mdict UTF-16LE/BE 头解码：ASCII 属性回归 + 非 ASCII 标题/描述全量
//（BMP 双/三字节 + 代理对四字节 + 孤立代理吸收 + BE 通路）——真实中文
// 市场词典 Title 惯例非 ASCII，ASCII-only 截断曾把词典名退回文件名。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include "std/mdict_parser_std.h"

static std::string make_utf16le(const std::u16string& s) {
    std::string out;
    out.push_back((char)0xFF); out.push_back((char)0xFE); // BOM LE
    for (char16_t ch : s) {
        out.push_back((char)(ch & 0xFF));
        out.push_back((char)((ch >> 8) & 0xFF));
    }
    return out;
}

static std::string make_utf16be(const std::u16string& s) {
    std::string out;
    out.push_back((char)0xFE); out.push_back((char)0xFF); // BOM BE
    for (char16_t ch : s) {
        out.push_back((char)((ch >> 8) & 0xFF));
        out.push_back((char)(ch & 0xFF));
    }
    return out;
}

static void write_mdx(const std::filesystem::path& path, const std::string& head) {
    std::ofstream out(path.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(head.data(), (std::streamsize)head.size());
    out << "\nDATA";
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "mdict_utf16_sample";
    fs::create_directories(dir);

    // ① ASCII 属性回归（原形态）
    {
        std::u16string header = u"<Dictionary title=\"DemoUTF16\" description=\"Header\"/>\n";
        fs::path mdx = dir / "demo.mdx";
        write_mdx(mdx, make_utf16le(header));
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.dictionary_name() == std::string("DemoUTF16"));
    }

    // ② 中文标题/描述（BMP 三字节臂）：真实中文市场词典头形态
    {
        std::u16string header =
            u"<Dictionary title=\"牛津高阶\" description=\"双解词典\"/>\n";
        fs::path mdx = dir / "zh.mdx";
        write_mdx(mdx, make_utf16le(header));
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.dictionary_name() == std::string("牛津高阶"));
    }

    // ③ 双字节臂（é）+ 代理对四字节臂（𝄞 U+1D11E）
    {
        std::u16string header =
            u"<Dictionary title=\"Caf\x00e9 \xD834\xDD1E\"/>\n";
        fs::path mdx = dir / "astral.mdx";
        write_mdx(mdx, make_utf16le(header));
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        // "Café 𝄞" 的 UTF-8 字节：c3 a9 + f0 9d 84 9e
        assert(mp.dictionary_name() ==
               std::string("Caf\xC3\xA9 \xF0\x9D\x84\x9E"));
    }

    // ④ 相邻高低代理合法成对 → U+10000 四字节（转义拆字面量：
    // \xDC00B 的 'B' 是十六进制字符会被贪婪吞进转义；每片都必须带
    // u 前缀——narrow 片里 0xDC00 超 char 范围，MSVC C7744 硬错）
    {
        std::u16string header =
            u"<Dictionary title=\"A\xD800" u"\xDC00" u"B\"/>\n";
        fs::path mdx = dir / "pair.mdx";
        write_mdx(mdx, make_utf16le(header));
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.dictionary_name() ==
               std::string("A\xF0\x90\x80\x80" "B"));
    }

    // ⑤ 真孤立高位（串尾无低位可配）→ 吸收
    {
        std::u16string header =
            u"<Dictionary title=\"X\xD800\"/>\n";
        fs::path mdx = dir / "lonehi.mdx";
        write_mdx(mdx, make_utf16le(header));
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.dictionary_name() == std::string("X"));
    }

    // ⑥ BE BOM + 中文（BE 通路非 ASCII）
    {
        std::u16string header = u"<Dictionary title=\"汉英\"/>\n";
        fs::path mdx = dir / "be.mdx";
        write_mdx(mdx, make_utf16be(header));
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.dictionary_name() == std::string("汉英"));
    }

    return 0;
}
