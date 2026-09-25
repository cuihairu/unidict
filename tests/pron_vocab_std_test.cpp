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

}  // namespace

int main() {
    test_real_vocab_shape();
    test_blank_aliases();
    test_escapes();
    test_malformed_rejected();
    return 0;
}
