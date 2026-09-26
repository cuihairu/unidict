#!/usr/bin/env python3
"""生成 core/std/text_norm_case_std.inc —— 大小写折叠表。

为什么要生成：大小写折叠是**破坏性**的，区间边界写错一个码点就是把某个
字母折成另一个字母（用户看到"词典里明明有这个词却查不到"），而这种错不
会编译报错、不会崩。Unicode 的大小写规则又不是一条简单偏移：西里尔是
+0x20 / +0x50、希腊是 +0x20 但有几个特例、越南语是奇偶交替 +1、乔治亚文
的现代映射跨了 0x2000。手写这些区间就是手写 bug。

所以直接拿 Python 的 str.lower()（也就是 Unicode 自己的数据）当基准，把
每个码点的真实小写反解成"区间 + 偏移"或"奇偶交替"两种形态，机器生成，
不存在手抄错位的可能。

校验：scripts/check_text_norm_tables.py 会把生成的表再拿 Python lower()
逐码点对一遍（等于自证），同时核升序与不重叠。

用法：python3 scripts/gen_text_norm_tables.py > core/std/text_norm_case_std.inc
"""

import sys

# BMP + 增补平面里已分配的大小写成对区块。扫到 0x30000 足够覆盖
# 亚美尼亚/切罗尼/etc. 之外的成对音系，其余大平面基本无大小写。
SCAN_MAX = 0x30000

lower_of = {}
for cp in range(SCAN_MAX):
    ch = chr(cp)
    lo = ch.lower()
    if len(lo) == 1 and ord(lo) != cp:
        lower_of[cp] = ord(lo)

# ---- 形态一：奇偶交替（偶数 +1 得小写，奇数已是小写）----
odd_even = {}
for cp, lo in lower_of.items():
    if lo == cp + 1 and (cp & 1) == 0:
        odd_even[cp] = True

# ---- 形态二：成对区间统一偏移 ----
offsets = {}
for cp, lo in lower_of.items():
    if cp in odd_even:
        continue
    offsets[cp] = lo - cp

# 把 offsets 压成极大区间：相邻码点、同一 delta 的连成一段
offset_ranges = []
for cp in sorted(offsets):
    d = offsets[cp]
    if offset_ranges and offset_ranges[-1][1] + 1 == cp and offset_ranges[-1][2] == d:
        offset_ranges[-1][1] = cp
    else:
        offset_ranges.append([cp, cp, d])

# 把 odd_even 压成极大区间
odd_even_ranges = []
for cp in sorted(odd_even):
    if odd_even_ranges and odd_even_ranges[-1][1] + 1 == cp:
        odd_even_ranges[-1][1] = cp
    else:
        odd_even_ranges.append([cp, cp])


def emit_ranges(name, ranges, fmt):
    print('const %s %s[] = {' % (fmt[0], name))
    for i in range(0, len(ranges), 4):
        chunk = ranges[i:i + 4]
        print('    ' + ' '.join(fmt[1](r) + ',' for r in chunk))
    print('};')
    print()


out = []
p = out.append
p('// 由 scripts/gen_text_norm_tables.py 生成，请勿手工编辑。')
p('//')
p('// 基准是 Python 的 str.lower()（即 Unicode Character Database 自己的数据），')
p('// 把每个码点的真实小写反解成两种形态之一：')
p('//   kCaseOffset   —— 区间内统一 +delta（西里尔 +0x20/+0x50、希腊 +0x20、')
p('//                   乔治亚 +0x2000 等）')
p('//   kCaseOddEven  —— 偶数码点 +1、奇数已是小写（越南语等成对交替区）')
p('//')
p('// 手工维护这些区间几乎必然出错：把某个字母折成另一个字母不会编译报错，')
p('// 也不会崩，用户只看到"词典里明明有这个词却查不到"。')
p('// 改折叠规则请改生成脚本，不要改这个文件。')
p('')
p('#include <cstdint>')
p('')
p('namespace UnidictCoreStd::TextNorm {')
p('namespace {')
p('')
p('struct CaseOffset {')
p('    uint32_t lo;')
p('    uint32_t hi;')
p('    // 必须是有符号：少数大写码点排在对应小写**之后**（如 Ÿ U+0178 -> y')
p('    // U+0079），偏移是负的；uint32_t 在 {} 初始化里直接报 narrowing。')
p('    int32_t delta;')
p('};')
p('')
p('struct CaseOddEven {')
p('    uint32_t lo;')
p('    uint32_t hi;')
p('};')
p('')

sys.stdout.write('\n'.join(out) + '\n')

def _delta(d):
    # delta 可以是负的（少数大写码点排在对应小写之后，如 Ÿ U+0178 -> y
    # U+0079）。写成 0x%02X 会得到 "0x-79" 这种非法字面量。
    return '-0x%02X' % (-d) if d < 0 else '0x%02X' % d


emit_ranges('kCaseOffset', offset_ranges,
            ('CaseOffset',
             lambda r: '{0x%04X, 0x%04X, %s}' % (r[0], r[1], _delta(r[2]))))
emit_ranges('kCaseOddEven', odd_even_ranges,
            ('CaseOddEven',
             lambda r: '{0x%04X, 0x%04X}' % (r[0], r[1])))

print('}  // namespace')
print()
print('}  // namespace UnidictCoreStd::TextNorm')
