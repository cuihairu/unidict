// text-norm profile（P1 设置页三档 profile 的 core 面）单测。
// 三档 = options_for_profile 的全旗标矩阵 + 代表性输入行为：
//   Tolerant     = 现行 fold_key（八开关全开，默认档）
//   Conservative = 区分重音/组合记号（é/é 与 e 不互撞，重音语言词形保真）
//   Exact        = 八开关全关，键即原样字节
// 越界枚举值收敛 Tolerant（settings 侧脏值防御的兜底口径）。
#include <cassert>
#include <iostream>
#include "std/text_norm_std.h"

using UnidictCoreStd::TextNorm::Options;
using UnidictCoreStd::TextNorm::Profile;
using UnidictCoreStd::TextNorm::fold_key;
using UnidictCoreStd::TextNorm::options_for_profile;

static bool all_flags(const Options& o, bool v) {
    return o.fold_case == v && o.fold_diacritics == v && o.fold_punctuation == v
        && o.strip_invisible == v && o.strip_combining == v
        && o.fold_ligatures == v && o.trim == v;
}

int main() {
    // 全旗标矩阵：宽容全开、精确全关、谨慎五开两关
    assert(all_flags(options_for_profile(Profile::Tolerant), true));
    assert(all_flags(options_for_profile(Profile::Exact), false));
    {
        const Options c = options_for_profile(Profile::Conservative);
        assert(c.fold_case && c.fold_punctuation && c.strip_invisible
               && c.fold_ligatures && c.trim);
        assert(!c.fold_diacritics && !c.strip_combining);
    }
    // 越界档位收敛 Tolerant
    assert(all_flags(options_for_profile(static_cast<Profile>(7)), true));

    // 重音字母：é 三档三种键形
    const std::string acute = "École";
    assert(fold_key(acute, options_for_profile(Profile::Tolerant)) == "ecole");
    assert(fold_key(acute, options_for_profile(Profile::Conservative)) == "école");
    assert(fold_key(acute, options_for_profile(Profile::Exact)) == acute);

    // 组合记号：e + U+0301，谨慎/精确保留、宽容剥掉
    const std::string combining = "e\xCC\x81";
    assert(fold_key(combining, options_for_profile(Profile::Tolerant)) == "e");
    assert(fold_key(combining, options_for_profile(Profile::Conservative)) == combining);
    assert(fold_key(combining, options_for_profile(Profile::Exact)) == combining);

    // 标点：弯引号宽容/谨慎都归一，精确原样
    const std::string curly = "don\xE2\x80\x99t";
    assert(fold_key(curly, options_for_profile(Profile::Tolerant)) == "don't");
    assert(fold_key(curly, options_for_profile(Profile::Conservative)) == "don't");
    assert(fold_key(curly, options_for_profile(Profile::Exact)) == curly);

    // 非拉丁大小写：西里尔宽容/谨慎都折，精确原样
    const std::string cyrillic = "Москва";
    assert(fold_key(cyrillic, options_for_profile(Profile::Tolerant)) == "москва");
    assert(fold_key(cyrillic, options_for_profile(Profile::Conservative)) == "москва");
    assert(fold_key(cyrillic, options_for_profile(Profile::Exact)) == cyrillic);

    std::cout << "OK\n";
    return 0;
}
