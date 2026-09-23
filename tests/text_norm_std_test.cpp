// 字符归一化测试：查词键折叠（全半角/重音/拼音四声/大小写）。

#include <cassert>
#include <string>
#include "std/text_norm_std.h"

using UnidictCoreStd::TextNorm::fold_key;

int main() {
    // 大小写与 trim（原有行为不回退）
    assert(fold_key("  Hello World  ") == "hello world");
    assert(fold_key("ABC") == "abc");

    // 重音折叠：Latin-1
    assert(fold_key("Caf\xc3\xa9") == "cafe");
    assert(fold_key("\xc3\x81NGEL") == "angel"); // ÁNGEL
    assert(fold_key("na\xc3\xafve") == "naive"); // naïve
    assert(fold_key("stra\xc3\x9f" "e") == "strasse"); // straße -> strasse

    // 拼音四声（对中文学习者词典关键）
    assert(fold_key("n\xc7\x90 h\xc3\xa0o") == "ni hao");   // nǐ hǎo
    assert(fold_key("L\xc7\x9a") == "lu");                  // Lǔ
    assert(fold_key("\xc4\x81\xc3\xa1\xc7\x8e\xc3\xa0") == "aaaa"); // āáǎà

    // 全角 -> 半角
    assert(fold_key("\xef\xbd\x88\xef\xbd\x85\xef\xbd\x8c\xef\xbd\x8c\xef\xbd\x8f") == "hello"); // ｈｅｌｌｏ
    assert(fold_key("\xef\xbc\x8d\xef\xbc\x91\xef\xbc\x92\xef\xbc\x93") == "-123");             // －１２３

    // 组合：全角 + 重音 + 大小写
    assert(fold_key("\xef\xbc\xad\xef\xbd\x85\xef\xbd\x8e\xc3\xbc") == "menu"); // Ｍｅｎü

    // 非拉丁文字原样保留（行为不回退）
    assert(fold_key("\xe4\xb8\xad\xe6\x96\x87") == "\xe4\xb8\xad\xe6\x96\x87"); // 中文
    assert(fold_key("") == "");

    return 0;
}
