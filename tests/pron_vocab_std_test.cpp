// 词表解析纯 std 测试。
// 真词表（wav2vec2-espeak-ctc vocab.json，392 类扁平映射）与各种畸形
// 输入都要钉死：评分管道每个分数按类索引取符号，解析错一位全盘皆错，
// 所以策略是严苛拒收而不是宽容修复。
#include <cassert>
#include <string>

#include "std/pron_vocab_std.h"

using UnidictCoreStd::parse_vocab_json;
using UnidictCoreStd::PronVocab;

namespace {

void test_real_vocab_shape() {
    // 真词表前几项（2026-09-25 抓取）：<pad>=0 blank、<s>=1、n=4、
    // 多字节 UTF-8 符号（ɛ=14、ɾ=15、aɪ=37）
    const auto v = parse_vocab_json(
        "{\"<s>\": 1, \"<pad>\": 0, \"</s>\": 2, \"<unk>\": 3, \"n\": 4, "
        "\"s\": 5, \"t\": 6, \"\xC9\x99\": 7, \"l\": 8, \"a\": 9, "
        "\"i\": 10, \"k\": 11, \"d\": 12, \"m\": 13, \"\xC9\x9B\": 14, "
        "\"\xC9\xBE\": 15, \"a\xC9\xAA\": 37}");
    assert(v.has_value());
    assert(v->blank_index == 0);
    assert(v->labels[0] == "<pad>");
    assert(v->labels[1] == "<s>");
    assert(v->labels[4] == "n");
    // 多字节符号整键取回，不是逐字节
    assert(v->labels[14] == "\xC9\x9B");
    assert(v->labels[15] == "\xC9\xBE");
    assert(v->labels[37] == "a\xC9\xAA");
    // 稀疏空洞（5..6 之间没有洞这里，但 16..36 有）留空串
    assert(v->labels[20].empty());
    assert(v->valid());
}

void test_blank_aliases() {
    // 各生态的 blank 命名都得认
    assert(parse_vocab_json("{\"[PAD]\":3,\"a\":1}").value().blank_index == 3);
    assert(parse_vocab_json("{\"<blk>\":0,\"a\":1}").value().blank_index == 0);
    assert(parse_vocab_json("{\"|\":2,\"a\":1}").value().blank_index == 2);
}

void test_escapes() {
    // \u 转义 → UTF-8（ə = U+0259），与裸 UTF-8 键等价
    const auto v = parse_vocab_json("{\"\\u0259\":5,\"<pad>\":0}");
    assert(v.has_value());
    assert(v->labels[5] == "\xC9\x99");
    // 代理对：😀 = U+1F600
    const auto emoji = parse_vocab_json(
        "{\"\\uD83D\\uDE00\":2,\"<pad>\":0}");
    assert(emoji.has_value());
    assert(emoji->labels[2] == "\xF0\x9F\x98\x80");
}

void test_malformed_rejected() {
    // 结构损坏
    assert(!parse_vocab_json("").has_value());
    assert(!parse_vocab_json("{").has_value());
    assert(!parse_vocab_json("{}").has_value());               // 空词表
    assert(!parse_vocab_json("[1,2]").has_value());            // 不是对象
    assert(!parse_vocab_json("{\"a\":1}garbage").has_value());  // 尾部垃圾
    assert(!parse_vocab_json("{\"a\":1,}").has_value());        // 尾逗号
    assert(!parse_vocab_json("{\"a\" 1}").has_value());         // 缺冒号
    assert(!parse_vocab_json("{\"a\":-1,\"<pad>\":0}").has_value());  // 负 id
    assert(!parse_vocab_json("{\"a\":1.5,\"<pad>\":0}").has_value());  // 小数
    assert(!parse_vocab_json("{\"a\":01,\"<pad>\":0}").has_value());   // 前导零
    assert(!parse_vocab_json("{\"a\":true}").has_value());
    // 重复 id：后写顶掉先写=静默错位，必须拒收
    assert(!parse_vocab_json("{\"a\":4,\"b\":4,\"<pad>\":0}").has_value());
    // 空 symbol 键
    assert(!parse_vocab_json("{\"\":4,\"<pad>\":0}").has_value());
    // 裸控制字符 / 未闭合转义 / 落单代理
    assert(!parse_vocab_json("{\"a\nb\":1,\"<pad>\":0}").has_value());
    assert(!parse_vocab_json("{\"a\\\":1,\"<pad>\":0}").has_value());
    assert(!parse_vocab_json("{\"\\uD800\":1,\"<pad>\":0}").has_value());
    // 无任何 blank 候选符号
    assert(!parse_vocab_json("{\"a\":0,\"b\":1}").has_value());
    // 两个 blank 候选同时在场
    assert(!parse_vocab_json("{\"<pad>\":0,\"|\":5}").has_value());
}

// 词表解析的其余拒收/边界路径。解析器是"只前进不回头、一步不符即失败"
// 的严苛风格，每条 return false 都是一道护栏；这些用例逐条钉住它们。
void test_string_parsing_edges() {
    // \u 的 4 位十六进制里混入非 hex 字符
    assert(!parse_vocab_json("{\"\\uZZZZ\":1,\"<pad>\":0}").has_value());
    // \u 后面不足 4 位就闭合
    assert(!parse_vocab_json("{\"\\u00\":1,\"<pad>\":0}").has_value());
    // 代理对的低代理不是十六进制
    assert(!parse_vocab_json("{\"\\uD83D\\uZZZZ\":1,\"<pad>\":0}").has_value());
    // 代理对的低代理位数不足
    assert(!parse_vocab_json("{\"\\uD83D\\u00\":1,\"<pad>\":0}").has_value());
    // 高代理后面跟的不是 \u
    assert(!parse_vocab_json("{\"\\uD800X\":1,\"<pad>\":0}").has_value());
    // 高代理在字符串末尾（c.pos+1 >= size）
    assert(!parse_vocab_json("{\"\\uD800\":1,\"<pad>\":0}").has_value());
    // 落单低代理
    assert(!parse_vocab_json("{\"\\uDC00\":1,\"<pad>\":0}").has_value());
    // 未收录的转义字符（\v / \a 之类）
    assert(!parse_vocab_json("{\"\\v\":1,\"<pad>\":0}").has_value());
    // 字符串未闭合：EOF 前没等到收尾引号
    assert(!parse_vocab_json("{\"abc").has_value());
    // 键缺收尾引号：解析器把 "abc:1," 当成键，随后要 ':' 却拿到 '<'
    assert(!parse_vocab_json("{\"abc:1,\"<pad>\":0}").has_value());

    // 字符串以反斜杠结尾（吃掉反斜杠后立刻 EOF）
    assert(!parse_vocab_json("{\"a\\").has_value());
    // 反斜杠转义 \\ → 单个反斜杠
    const auto bs = parse_vocab_json("{\"a\\\\b\":1,\"<pad>\":0}");
    assert(bs.has_value());
    assert(bs->labels[1] == "a\\b");
    // 高代理后跟的不是低代理（lo 落在 DC00-DFFF 之外）
    assert(!parse_vocab_json("{\"\\uD800\\u0041\":1,\"<pad>\":0}").has_value());
    // 正斜杠转义 \/ → 单个 '/'
    const auto slash = parse_vocab_json("{\"a\\/b\":1,\"<pad>\":0}");
    assert(slash.has_value());
    assert(slash->labels[1] == "a/b");
    // 单字符转义全套（除已测的 \" \\ \/ 之外）
    const auto v = parse_vocab_json(
        "{\"a\\bb\\fc\\nd\\re\\tf\":1,\"<pad>\":0}");
    assert(v.has_value());
    assert(v->labels[1] == "a\bb\fc\nd\re\tf");
}

void test_append_utf8_all_widths() {
    // \u0000-\u007F → 1 字节
    const auto a = parse_vocab_json("{\"\\u0041\":1,\"<pad>\":0}");
    assert(a.has_value() && a->labels[1] == "A");
    // \u0080-\u07FF → 2 字节
    const auto b = parse_vocab_json("{\"\\u00E9\":1,\"<pad>\":0}");
    assert(b.has_value() && b->labels[1] == "\xC3\xA9");  // é
    // \u0800-\uFFFF → 3 字节
    const auto c = parse_vocab_json("{\"\\u4E2D\":1,\"<pad>\":0}");
    assert(c.has_value() && c->labels[1] == "\xE4\xB8\xAD");  // 中
    // > 0xFFFF（代理对拼接后）→ 4 字节
    const auto d = parse_vocab_json("{\"\\uD83D\\uDE00\":1,\"<pad>\":0}");
    assert(d.has_value() && d->labels[1] == "\xF0\x9F\x98\x80");  // 😀
    // hex_val 的三个分支：小写 a-f / 大写 A-F / 数字
    const auto e = parse_vocab_json("{\"\\u00aB\":1,\"<pad>\":0}");
    assert(e.has_value() && e->labels[1] == "\xC2\xAB");  // «
    const auto f = parse_vocab_json("{\"\\u00AB\":1,\"<pad>\":0}");
    assert(f.has_value() && f->labels[1] == "\xC2\xAB");
}

void test_id_parsing_edges() {
    // id 规模护栏：> 1000000 拒收（392 类的模型不该有百万 id）
    assert(!parse_vocab_json("{\"a\":1000001,\"<pad>\":0}").has_value());
    // 正好等于上限不触发护栏，但会 resize 出百万+1 的 labels，
    // 又找不到 id 1000000 的符号 → 空洞允许，仍能加载
    const auto v = parse_vocab_json("{\"a\":1000000,\"<pad>\":0}");
    assert(v.has_value());
    assert(v->labels.size() == 1000001);
    assert(v->labels[1000000] == "a");
    // 指数记法（"1e3" 的 e 不是 , 或 }）
    assert(!parse_vocab_json("{\"a\":1e3,\"<pad>\":0}").has_value());
    // id 后面直接跟别的键（缺逗号）
    assert(!parse_vocab_json("{\"a\":1 \"b\":2,\"<pad>\":0}").has_value());
    // id 位置是空（缺值）
    assert(!parse_vocab_json("{\"a\":,\"<pad>\":0}").has_value());
    // id 位置是字符串
    assert(!parse_vocab_json("{\"a\":\"1\",\"<pad>\":0}").has_value());
}

void test_structure_edges() {
    // 只有空白
    assert(!parse_vocab_json("   ").has_value());
    // 顶层不是 '{'（数组/字符串/数字）
    assert(!parse_vocab_json("  [ ]").has_value());
    assert(!parse_vocab_json("\"a\"").has_value());
    // 空白穿插在结构里要能正确跳过
    const auto v = parse_vocab_json("  {  \"<pad>\"  :  0  ,  \"a\"  :  1  }  ");
    assert(v.has_value());
    assert(v->blank_index == 0);
    assert(v->labels[1] == "a");
    // 空对象（跳过空白后直接 '}'）
    assert(!parse_vocab_json("   {   }   ").has_value());
    // 逗号之后没有下一项
    assert(!parse_vocab_json("{\"<pad>\":0,}").has_value());
    // 键不是字符串（数字键）
    assert(!parse_vocab_json("{1:0,\"<pad>\":1}").has_value());
    // 项与项之间用分号
    assert(!parse_vocab_json("{\"<pad>\":0;\"a\":1}").has_value());
}

void test_blank_at_high_index() {
    // blank 不在 0：labels 要 resize 到 blank_index+1
    const auto v = parse_vocab_json("{\"a\":0,\"b\":1,\"<pad>\":9}");
    assert(v.has_value());
    assert(v->blank_index == 9);
    assert(v->labels.size() == 10);
    assert(v->labels[9] == "<pad>");
    // 中间空洞留空串
    assert(v->labels[5].empty());
    assert(v->valid());
}

void test_sparse_ids_grow_labels() {
    // id 乱序出现：先给大 id 再给小 id，labels 只增不减
    const auto v = parse_vocab_json("{\"<pad>\":5,\"a\":0,\"b\":20}");
    assert(v.has_value());
    assert(v->blank_index == 5);
    assert(v->labels.size() == 21);
    assert(v->labels[0] == "a");
    assert(v->labels[5] == "<pad>");
    assert(v->labels[20] == "b");
}

}  // namespace

int main() {
    test_real_vocab_shape();
    test_blank_aliases();
    test_escapes();
    test_string_parsing_edges();
    test_append_utf8_all_widths();
    test_id_parsing_edges();
    test_structure_edges();
    test_blank_at_high_index();
    test_sparse_ids_grow_labels();
    test_malformed_rejected();
    test_string_parsing_edges();
    test_append_utf8_all_widths();
    test_id_parsing_edges();
    test_structure_edges();
    test_blank_at_high_index();
    test_sparse_ids_grow_labels();
    return 0;
}
