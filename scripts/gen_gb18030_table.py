#!/usr/bin/env python3
"""生成 core/std/gb18030_table_std.inc —— GB2312/GBK/GB18030 两字节区 → Unicode 表。

为什么要自己生成表：core/std 必须无 Qt、无 ICU、无 iconv（Windows 上根本没有
iconv）。而 GBK 词典在中文学习用户里极常见，.ifo 写 charset=GBK 时**整个词典
不可用**（.idx 的词条是 GBK 字节，查询是 UTF-8，index_ 永远查不中）。

表为什么只要 uint16：两字节区共 126×190 = 23940 个槽，映射到的码点全部落在
U+00A4–U+FFE5（BMP 内），所以 uint32 表的 95KB 可以砍一半到 47KB。

范围与取舍：
  * 只覆盖**两字节区**。四字节区（lead 0x81-0xFE, b2 0x30-0x39, b3 0x81-0xFE,
    b4 0x30-0x39，线性映射到 189000 个码点）需要再加一张 160KB 的表，而现实
    中的 StarDict GBK 词典只用两字节区。四字节序列按无效字节原样透传。
  * 真实数据里 GBK 词典的 .ifo 经常错写成 GB2312 或 GB18030，三者两字节区完全
    相同（GB18030 向下兼容），所以一张表通吃。
  * trail 0x7F 在标准里是未定义槽（0x40-0x7E + 0x80-0xFE，跳过 0x7F）。

用法：python3 scripts/gen_gb18030_table.py > core/std/gb18030_table_std.inc
"""

LEAD_MIN, LEAD_MAX = 0x81, 0xFE
TRAILS = list(range(0x40, 0x7F)) + list(range(0x80, 0xFF))
PER_LEAD = len(TRAILS)

out = []
w = out.append

w('// 由 scripts/gen_gb18030_table.py 生成，请勿手工编辑。')
w('//')
w('// GB2312/GBK/GB18030 两字节区 → Unicode。索引 = (lead - 0x81) * %d +' % PER_LEAD)
w('// (trail 在 TRAILS 里的序号)，TRAILS = 0x40..0x7E, 0x80..0xFE（跳过 0x7F）。')
w('// 值 0 表示无映射（表内实际 23940 槽全有映射，0 只作为防御性默认值）。')
w('//')
w('// 全部码点落在 U+00A4–U+FFE5，故用 uint16 而非 uint32（47KB vs 95KB）。')
w('')
w('#include <cstdint>')
w('')
w('namespace UnidictCoreStd::CharsetCodec {')
w('namespace {')
w('')
w('constexpr int kTrailsPerLead = %d;' % PER_LEAD)
w('')
w('// 0x40..0x7E, 0x80..0xFE —— 顺序即索引顺序，改动会让整张表错位')
w('constexpr uint8_t kTrailBytes[kTrailsPerLead] = {')
for i in range(0, PER_LEAD, 16):
    w('    ' + ', '.join('0x%02X' % t for t in TRAILS[i:i + 16]) + ',')
w('};')
w('')
w('// 0 表示无映射的槽（防御性；生成时该区全有映射）')
w('constexpr uint16_t kGb18030ToUni[(0x%02X - 0x%02X + 1) * kTrailsPerLead] = {'
  % (LEAD_MAX, LEAD_MIN))
for lead in range(LEAD_MIN, LEAD_MAX + 1):
    row = []
    for trail in TRAILS:
        try:
            ch = bytes([lead, trail]).decode('gb18030')
        except Exception:
            row.append(0)
            continue
        o = ord(ch)
        assert 0 < o <= 0xFFFF, (lead, trail, o)
        row.append(o)
    label = '// 0x%02X' % lead
    for i in range(0, PER_LEAD, 10):
        chunk = row[i:i + 10]
        suffix = label if i == 0 else ''
        w('    %s,  // %s' % (', '.join('0x%04X' % v for v in chunk), suffix))
w('};')
w('')
w('}  // namespace')
w('')
w('}  // namespace UnidictCoreStd::CharsetCodec')
w('')

print('\n'.join(out))
