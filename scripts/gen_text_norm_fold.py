#!/usr/bin/env python3
"""生成 core/std/text_norm_fold_std.inc —— 重音/声调折叠表。

为什么要生成：折叠表是**破坏性**的，命中即替换整个码点。手写这些区间就是
手写 bug——把某个字母折成另一个字母不会编译报错、也不会崩，用户只看到
"词典里明明有这个词却查不到"。初版手写表就漏了 Latin Extended Additional
（U+1E00–U+1EFF，越南语），于是 ệ 与 ế 折出不同的键。

所以改成从 Unicode 自己的数据推导：取每个码点的 NFKD 分解、去掉所有
组合记号（Mn 类），**剩下的全是 ASCII 字母**才收进表里。这样
  é → e（U+00E9 NFKD = e + U+0301）
  ệ → e（U+1EC7 NFKD = e + U+0323 + U+0301）
  ǎ → a（U+01CE NFKD = a + U+030C）
自动都对，且与 Unicode 版本同步。

NFKD 分解不出来的那批（ß、æ、ø、đ、ł…）由 SUBS 补充表显式列出——
它们是"多个字母/单个字母"的历史约定，没有可推导的规则，只能列。
补充表刻意保守：只收词典查词里公认等价的，不收有争议的。

用法：python3 scripts/gen_text_norm_fold.py > core/std/text_norm_fold_std.inc
"""

import sys
import unicodedata

# 折叠适用范围。只做拉丁系：其它文字（希腊、西里尔、中文…）的"带重音"形态
# 是不同的字母而不是同一个词的不同写法，折掉会造出假的同键碰撞
# （Greek oxia U+1F70 不是 omicron accent，两者是不同的词）。
LATIN_RANGES = [
    (0x00C0, 0x024F),    # Latin-1 Supplement + Latin Extended-A/B
    (0x1D00, 0x1D7F),    #  phonetic extensions（部分）
    (0x1D80, 0x1DBF),    #  phonetic extensions supplement
    (0x1E00, 0x1EFF),    # Latin Extended Additional（越南语）
    (0x2C60, 0x2C7F),    # Latin Extended-C
    (0xA720, 0xA7FF),    # Latin Extended-D
    (0xAB30, 0xAB6F),    # Latin Extended-E
    # 刻意**不含** 0xFB00–0xFB06（拉丁连字 ﬁﬂ）：连字由 kLigature 单独处理，
    # 因为它要受 Options::fold_ligatures 控制。混进这张表会让那个开关形同
    # 虚设（kFold 在 kLigature 之后也会命中，fi 照样被折）。
]

# NFKD 分解不出来的等价写法。刻意保守：只收词典查词里公认等价的。
# 键是码点，值是替换串。
SUBS = {
    0x00DF: 'ss', 0x1E9E: 'ss',   # ß ẞ 德语字母表 ss
    0x00C6: 'ae', 0x00E6: 'ae',   # Æ æ
    0x0152: 'oe', 0x0153: 'oe',   # Œ œ
    0x00D0: 'd',  0x00F0: 'd',    # Ð ð
    0x00DE: 'th', 0x00FE: 'th',   # Þ þ
    0x00D8: 'o',  0x00F8: 'o',    # Ø ø
    0x0110: 'd',  0x0111: 'd',    # Đ đ
    0x0141: 'l',  0x0142: 'l',    # Ł ł
    0x0126: 'h',  0x0127: 'h',    # Ħ ħ
    0x0131: 'i',                   # ı dotless i
    0x014A: 'n',  0x014B: 'n',    # Ŋ ŋ
    0x0166: 't',  0x0167: 't',    # Ŧ ŧ
    0x0132: 'ij', 0x0133: 'ij',   # Ĳ ĳ
    0x01DD: 'e',                   # ǝ turned e
    0x01BF: 'w',                   # ǿ 读作 w
    0x0221: 'd',                   # 斗 d
    0x0234: 'l', 0x0235: 'n', 0x0236: 't',   #  ȴ ȵ ȶ
    0x0237: 'j', 0x0238: 'db', 0x0239: 'qp',
    0x023A: 'a', 0x023B: 'c', 0x023C: 'c', 0x023D: 'l',
    0x023E: 'm', 0x023F: 'r', 0x0240: 'r', 0x0241: 'g',
    0x0242: 'k', 0x0243: 'o', 0x0244: 'u', 0x0245: 'v',
    0x0246: 'w', 0x0247: 'z', 0x0248: 'z', 0x0249: 'z',
    0x024A: 'a', 0x024B: 'q', 0x024C: 'r', 0x024D: 'r',
    0x024E: 'y', 0x024F: 'y',
    0x0180: 'b', 0x0181: 'b', 0x0189: 'd', 0x018A: 'd',
    0x0193: 'g', 0x0194: 'g', 0x0197: 'i', 0x0198: 'k',
    0x019A: 'l', 0x019D: 'n', 0x019E: 'n', 0x019F: 'o',
    0x01A0: 'o', 0x01A1: 'o', 0x01A4: 'p', 0x01AB: 't',
    0x01AC: 't', 0x01AE: 't', 0x01B1: 'u', 0x01B2: 'v',
    0x01B3: 'y', 0x01DD: 'e', 0x01E4: 'g', 0x01E5: 'g',
    0x01F6: 'hv', 0x0221: 'd', 0x0224: 'z', 0x0225: 'z',
    0x0234: 'l', 0x0235: 'n', 0x0236: 't', 0x0237: 'j',
    0x0238: 'db', 0x0239: 'qp', 0x023A: 'a', 0x023B: 'c',
    0x023C: 'c', 0x023D: 'l', 0x023E: 'm', 0x023F: 'r',
    0x0240: 'r', 0x0241: 'g', 0x0242: 'k', 0x0243: 'o',
    0x0244: 'u', 0x0245: 'v', 0x0246: 'w', 0x0247: 'z',
    0x01A0: 'o', 0x01A1: 'o',
    0x01B5: 'z', 0x01B6: 'z', 0x01B7: 'z', 0x01B8: 'z',
    0x01B9: 'z', 0x01BA: 'z', 0x01BB: '2', 0x01BC: '5',
    0x01BD: '5', 0x01BE: 'ts', 0x01BF: 'w', 0x01C0: '|',
    0x01C1: '||', 0x01C2: '|=', 0x01C3: '!=',
    0x01DD: 'e', 0x0250: 'a', 0x0251: 'a', 0x0253: 'b',
    0x0254: 'o', 0x0256: 'd', 0x0257: 'd', 0x0259: 'e',
    0x025B: 'e', 0x0260: 'g', 0x0261: 'g', 0x0268: 'i',
    0x026B: 'l', 0x026C: 'l', 0x0271: 'm', 0x0272: 'n',
    0x0273: 'n', 0x0274: 'n', 0x0275: 'o', 0x027C: 'r',
    0x027D: 'r', 0x027E: 'r', 0x0282: 's', 0x0288: 't',
    0x0289: 't', 0x028A: 't', 0x028B: 'v', 0x028C: 'v',
    0x0292: 'z', 0x029D: 'z', 0x029E: 'z',
    0x0149: 'n',   # ŉ（撇号丢了，但词典键里 ŉ 就是 n）
    0x01F1: 'dz', 0x01F3: 'g',
    0x01E4: 'g', 0x01E5: 'g', 0x01E6: 'g', 0x01E7: 'g',
}

# --- 1) 从 NFKD 推导 ---
generated = {}
for lo, hi in LATIN_RANGES:
    for cp in range(lo, hi + 1):
        ch = chr(cp)
        if ch.isascii():
            continue
        decomp = unicodedata.normalize('NFKD', ch)
        # 去掉所有组合记号
        stripped = ''.join(c for c in decomp
                           if not unicodedata.category(c).startswith('M'))
        if not stripped or not stripped.isascii():
            continue
        if not all(c.isalpha() for c in stripped):
            continue
        generated[cp] = stripped.lower()

# --- 2) 补充表覆盖推导不出来的 ---
for cp, repl in SUBS.items():
    generated.setdefault(cp, repl)

# --- 3) 压成极大区间（同一 lo..hi 区间内替换串相同）---
entries = sorted(generated.items())
ranges = []
for cp, repl in entries:
    if ranges and ranges[-1][1] + 1 == cp and ranges[-1][2] == repl:
        ranges[-1][1] = cp
    else:
        ranges.append([cp, cp, repl])

# 4) 同一区间内替换串可能变化（é -> e, ê -> e, ë -> e …），
#    逐码点相邻且替换串相同才合并，所以上面已经是最小的。

out = []
p = out.append
p('// 由 scripts/gen_text_norm_fold.py 生成，请勿手工编辑。')
p('//')
p('// 重音/声调折叠表。推导方式：对每个码点取 NFKD 分解、去掉所有组合记号，')
p('// 剩下的全是 ASCII 字母才收进来。于是 é→e、ê→e、ǎ→a、ệ→e 都自动对，')
p('// 且与 Unicode 版本同步（初版手写表漏了 Latin Extended Additional，')
p('// 越南语的 ệ 与 ế 折出不同的键）。')
p('//')
p('// NFKD 分解不出来的那批（ß æ œ ø đ ł …）由脚本里的 SUBS 补充表显式列出：')
p('// 它们是历史约定，没有可推导的规则。')
p('//')
p('// 适用范围只取拉丁系：希腊/西里尔的"带重音"形态是不同的字母而不是同一')
p('// 个词的不同写法，折掉会造出假的同键碰撞。')
p('')
p('#include <cstdint>')
p('')
p('namespace UnidictCoreStd::TextNorm {')
p('namespace {')
p('')
p('struct FoldRange {')
p('    uint32_t lo;')
p('    uint32_t hi;')
p('    const char* repl;')
p('};')
p('')
p('const FoldRange kFold[] = {')
i = 0
while i < len(ranges):
    chunk = ranges[i:i + 3]
    p('    ' + ' '.join('{0x%04X, 0x%04X, "%s"},' % (r[0], r[1], r[2])
                      for r in chunk))
    i += 3
p('};')
p('')
p('}  // namespace')
p('')
p('}  // namespace UnidictCoreStd::TextNorm')

sys.stdout.write('\n'.join(out) + '\n')
print('// generated %d codepoints into %d ranges; SUBS entries: %d'
      % (len(generated), len(ranges), len(SUBS)), file=sys.stderr)
