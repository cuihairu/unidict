#!/usr/bin/env python3
"""校验 core/std/text_norm_std.cpp 里的所有查找表。

这些表是手写的，错了不会编译报错、不会崩，只会把某个字母折成另一个字母
或者直接吞掉——用户看到的就是"词典里明明有这个词却查不到"。所以用脚本
逐条核：

  1. 严格升序（lower_bound 二分的前提；有序但有重复/倒序会静默查错）；
  2. 区间两两不重叠（重叠时命中哪条取决于二分落点，行为不可预测）；
  3. kFold 的每条替换必须非空且不丢信息（空串 = 删字符，只允许 kInvisible
     与 kCombining 用）；
  4. kCaseOffset / kCaseOddEven 折出来的小写必须与 Python 的 str.lower()
     一致——这是外部基准，能一次性发现区间边界写错。
"""
import io
import re
import sys

SRC = io.open('core/std/text_norm_std.cpp', encoding='utf-8').read()
# 大小写表与重音折叠表现在都是**生成**文件（text_norm_case_std.inc /
# text_norm_fold_std.inc），其余手写表在 .cpp 里，拼起来一起查。
# 改生成脚本之后请重跑本脚本。
GEN = (io.open('core/std/text_norm_case_std.inc', encoding='utf-8').read()
       + '\n'
       + io.open('core/std/text_norm_fold_std.inc', encoding='utf-8').read())
SRC_ALL = SRC + '\n' + GEN


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    # 逐行去行尾注释，但不能把字符串字面量里的 // 吃掉（本仓库的表里没有，
    # 保守起见按"不在引号内"处理）
    lines = []
    for line in s.split('\n'):
        inq = False
        cut = len(line)
        i = 0
        while i < len(line):
            if line[i] == '"':
                inq = not inq
            elif not inq and line[i:i + 2] == '//':
                cut = i
                break
            i += 1
        lines.append(line[:cut])
    return '\n'.join(lines)


def grab(name):
    """抓出 `const <T> <name>[] = { ... };` 的元素。"""
    body = strip_comments(SRC_ALL)
    m = re.search(r'\b' + name + r'\[\]\s*=\s*\{(.*?)\n\};', body, re.S)
    if not m:
        return None
    return m.group(1)


def parse_ranges(name):
    body = grab(name)
    if body is None:
        return None
    out = []
    for m in re.finditer(
            r'\{\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*,', body):
        out.append((int(m.group(1), 0), int(m.group(2), 0)))
    return out


def parse_repls(name):
    body = grab(name)
    if body is None:
        return None
    out = []
    for m in re.finditer(
            r'\{\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*,'
            r'\s*("(?:[^"\\]|\\.)*")\s*\}', body):
        out.append((int(m.group(1), 0), int(m.group(2), 0), m.group(3)))
    return out


def parse_singles(name):
    body = grab(name)
    if body is None:
        return None
    out = []
    for m in re.finditer(
            r'\{\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*("(?:[^"\\]|\\.)*")\s*\}', body):
        out.append((int(m.group(1), 0), m.group(2)))
    return out


errors = []


def check_ascending(name, entries, key=lambda e: e[0]):
    prev = None
    prev_desc = None
    for e in entries:
        k = key(e)
        if prev is not None:
            if k == prev:
                errors.append('%s: 重复的键 0x%X' % (name, k))
            elif k < prev:
                errors.append('%s: 乱序 0x%X 出现在 0x%X 之后（表必须严格升序，'
                              'lower_bound 才正确）' % (name, k, prev))
        prev = k
        prev_desc = e


def check_no_overlap(name, entries):
    ordered = sorted(entries, key=lambda e: e[0])
    for a, b in zip(ordered, ordered[1:]):
        if b[0] <= a[1]:
            errors.append('%s: 区间 [0x%X,0x%X] 与 [0x%X,0x%X] 重叠'
                          % (name, a[0], a[1], b[0], b[1]))


def check_overlap_within(name, entries):
    """同一张表内部允许"故意嵌套"（如 0x0E31 单点嵌在 0x0E34-0x0E3A 之前），
    只要外层区间在 lo 升序上不吞掉后一条的 lo 即可——也就是 lower_bound
    按 hi 查时总能落到正确的区间。这里只报"前一条的 hi 越过后一条的 lo 且
    前一条 lo 更小"这种会真正吃掉后一条的情况。"""
    ordered = sorted(entries, key=lambda e: e[0])
    for i, a in enumerate(ordered):
        for b in ordered[i + 1:]:
            if b[0] <= a[1] and a[0] < b[0]:
                # a 覆盖了 b 的起点：lower_bound(hi<b.lo) 会在 a 处停下
                if a[1] < b[1]:
                    errors.append('%s: [0x%X,0x%X] 吞掉了后面的 [0x%X,0x%X]'
                                  % (name, a[0], a[1], b[0], b[1]))


# ---- kFold / kInvisible / kCombining / kLigature：区间表 ----
for tname in ('kFold', 'kInvisible', 'kCombining', 'kLigature'):
    rs = parse_ranges(tname)
    if rs is None:
        errors.append('%s: 解析不到' % tname)
        continue
    check_ascending(tname, rs, key=lambda e: e[0])
    check_no_overlap(tname, rs)
    check_overlap_within(tname, rs)

# ---- kPunct：单码点表 ----
puncts = parse_singles('kPunct')
if puncts is None:
    errors.append('kPunct: 解析不到')
else:
    check_ascending('kPunct', puncts)
    cpmap = dict(puncts)
    for cp in cpmap:
        if 0xFF01 <= cp <= 0xFF5E and cp not in (0xFF3C,):
            errors.append('kPunct: 0x%X 落在全角区间，会被全角步提前处理，'
                          '这条永远不生效' % cp)
        if cp == 0x3000:
            errors.append('kPunct: 0x3000 由全角步处理，这条不生效')
        if cp == 0x00A0 or 0x2000 <= cp <= 0x200A or cp in (0x202F, 0x205F,
                                                           0x2007, 0x2008, 0x2009,
                                                           0x200A):
            pass  # Unicode 空白，非全角，确实要靠这张表

# ---- kCaseOffset ----
coff = parse_ranges('kCaseOffset')
if coff is None:
    errors.append('kCaseOffset: 解析不到')
else:
    # 结构是 {lo, hi, delta}
    body = strip_comments(SRC_ALL)
    m = re.search(r'\bkCaseOffset\[\]\s*=\s*\{(.*?)\n\};', body, re.S)
    offs = []
    for mm in re.finditer(
            r'\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+)\s*,'
            r'\s*(0x[0-9A-Fa-f]+)\s*\}', m.group(1)):
        offs.append((int(mm.group(1), 0), int(mm.group(2), 0), int(mm.group(3), 0)))
    check_ascending('kCaseOffset', offs)
    for lo, hi, delta in offs:
        for cp in range(lo, hi + 1):
            try:
                ch = chr(cp)
            except ValueError:
                errors.append('kCaseOffset: 0x%X 不是码点' % cp)
                continue
            ref = ch.lower()
            if len(ref) == 1 and ref != ch:
                got = cp + delta
                if got != ord(ref):
                    errors.append('kCaseOffset: U+%04X +%d = U+%04X，'
                                  '但 Python lower() 给的是 U+%04X'
                                  % (cp, delta, got, ord(ref)))

# ---- kCaseOddEven ----
body = strip_comments(SRC_ALL)
m = re.search(r'\bkCaseOddEven\[\]\s*=\s*\{(.*?)\n\};', body, re.S)
odd = []
if m:
    for mm in re.finditer(r'\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+)\s*\}',
                         m.group(1)):
        odd.append((int(mm.group(1), 0), int(mm.group(2), 0)))
    check_ascending('kCaseOddEven', odd)
    for lo, hi in odd:
        for cp in range(lo, hi + 1):
            ch = chr(cp)
            ref = ch.lower()
            if len(ref) == 1 and ref != ch:
                got = cp + 1 if (cp & 1) == 0 else cp
                if got != ord(ref):
                    errors.append('kCaseOddEven: U+%04X 折成 U+%04X，'
                                  '但 Python lower() 给的是 U+%04X'
                                  % (cp, got, ord(ref)))

# ---- kFold 的替换不能是空串（那是删除字符） ----
for tname in ('kFold', 'kLigature'):
    body = strip_comments(SRC_ALL)
    m = re.search(r'\b' + tname + r'\[\]\s*=\s*\{(.*?)\n\};', body, re.S)
    for mm in re.finditer(
            r'\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+)\s*,'
            r'\s*("(?:[^"\\]|\\.)*")\s*\}', m.group(1)):
        if mm.group(3) == '""':
            errors.append('%s: [0x%s,0x%s] 替换成空串 = 删字符，只允许 '
                          'kInvisible/kCombining 这样用' %
                          (tname, mm.group(1), mm.group(2)))

# kFold 与 kLigature 都不能把码点映到空串之外的重叠区间：
# 两张表都会命中同一个码点时，先查的那张赢，kLigature 的
# Options::fold_ligatures 开关就形同虚设。
def _parse_all(name):
    body = strip_comments(SRC_ALL)
    m = re.search(r'\b' + name + r'\[\]\s*=\s*\{(.*?)\n\};', body, re.S)
    if not m:
        return None
    out = []
    for mm in re.finditer(
            r'\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+)\s*,'
            r'\s*"(?:[^"\\]|\\.)*"\s*\}', m.group(1)):
        out.append((int(mm.group(1), 0), int(mm.group(2), 0)))
    return out


fk = _parse_all('kFold')
kl = _parse_all('kLigature')
if fk and kl:
    for a in fk:
        for b in kl:
            if a[0] <= b[1] and b[0] <= a[1]:
                errors.append('kFold 与 kLigature 在 [0x%X,0x%X] 重叠：'
                              'kFold 先命中会让 Options::fold_ligatures 失效'
                              % (max(a[0], b[0]), min(a[1], b[1])))

if errors:
    print('表格校验失败 %d 处：' % len(errors))
    for e in errors:
        print('  - ' + e)
    sys.exit(1)
print('表格校验通过：升序、无重叠、kCase 与 Python str.lower() 一致、'
      'kFold 无空替换')
