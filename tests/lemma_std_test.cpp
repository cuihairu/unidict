// Lemma::lemma_candidates 单测（P-11 查词能力线批二）：不规则表/后缀规则
// 全臂覆盖 + 短词与非 ASCII 闸门 + 「候选不含输入词自身」契约。
// 断言口径：候选向量整体相等（顺序=置信序，也是契约面）。

#include <cassert>
#include <string>
#include <vector>

#include "lemma_std.h"

using namespace UnidictCoreStd;
using Lemma::lemma_candidates;

static void ok(const std::string& in, const std::vector<std::string>& want) {
    assert(lemma_candidates(in) == want);
}

int main() {
    // --- 不规则表（名词/动词/形容词级）---
    ok("wolves", {"wolf"});
    ok("WOLVES", {"wolf"});        // 大小写不敏感
    ok("went", {"go"});
    ok("better", {"good"});
    ok("children", {"child"});
    ok("heroes", {"hero"});        // -oes 名词单列
    ok("men", {"man"});            // 三字母不规则（整体闸门 ≥3 下可达）
    ok("led", {"lead"});

    // --- -ied → -y（优先于 -ed，否则拆出伪词 studi）---
    ok("studied", {"study"});
    ok("carried", {"carry"});

    // --- -ies → -y ---
    ok("studies", {"study"});
    ok("flies", {"fly"});

    // --- -es（s/x/z/ch/sh 后）---
    ok("boxes", {"box"});
    ok("buses", {"bus"});
    ok("matches", {"match"});
    ok("washes", {"wash"});
    // -es 不成臂：prev 不在集合（goes 走不规则表；shoes 留给 -s 臂）
    ok("shoes", {"shoe"});

    // --- -ing 三候选：裸去 / 双写回退 / e 复原 ---
    ok("doing", {"do", "doe"});      // doe 为无害噪声候选，词典回查裁决
    ok("fishing", {"fish", "fishe"});
    // running 精确序：裸去 runn → 双写回退 run → e 复原 runne
    ok("running", {"runn", "run", "runne"});
    ok("making", {"mak", "make"});
    ok("using", {"us", "use"});

    // --- -ed 三候选 ---
    ok("looked", {"look", "looke"});
    ok("stopped", {"stopp", "stop", "stoppe"});
    ok("loved", {"lov", "love"});
    ok("died", {"di", "die"});     // 4 字母 -ed 闸门（≥4）下可达
    ok("used", {"us", "use"});

    // --- -s（排除 ss/us/is）---
    ok("cats", {"cat"});
    ok("walks", {"walk"});
    ok("glass", {});               // -ss 排除
    ok("bus", {});                 // 长度闸门（<4）
    ok("this", {});                // -is 排除

    // --- 闸门与空手 ---
    ok("", {});
    ok("ab", {});                  // <3
    ok("cat", {});                 // 无规则可套
    ok("\xE4\xBD\xA0\xE5\xA5\xBD", {});  // 非 ASCII（你好）空手
    ok("ab1ed", {"ab1", "ab1e"});  // ASCII 但含数字：规则照套（调用方回查裁决）
    ok("xxing", {"xx", "xxe"});    // 双写 x 不回退（w/x/y 不双写）
    ok("x11ed", {"x11", "x11e"});  // 双写非字母不回退

    return 0;
}
