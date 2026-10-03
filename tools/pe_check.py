#!/usr/bin/env python3
"""读 Windows PE 的关键属性：子系统 + 资源段清单（图标/版本/清单）。

用途：每日构建产物（Windows zip / 安装器）走查时，在没有 Windows 机器的
环境里客观回答两个问题——「这个 exe 是不是 GUI 子系统」（BUG-001）、
「品牌图标有没有真进 PE 资源段、帧尺寸集是什么」（BUG-006）。只解析 PE
结构，不执行代码。

用法：
  python3 tools/pe_check.py build/Release/unidict_qml.exe [更多 exe...]
  python3 tools/pe_check.py --json <exe>     # 机器可读输出

退出码：0 = 解析成功；2 = 文件不是合法 PE。
"""

import argparse
import json
import os
import struct
import sys

RESOURCE_TYPES = {
    1: "RT_CURSOR", 2: "RT_BITMAP", 3: "RT_ICON", 4: "RT_MENU", 5: "RT_DIALOG",
    6: "RT_STRING", 7: "RT_FONTDIR", 8: "RT_FONT", 9: "RT_ACCELERATOR",
    10: "RT_RCDATA", 11: "RT_MESSAGETABLE", 12: "RT_GROUP_CURSOR",
    14: "RT_GROUP_ICON", 16: "RT_VERSION", 24: "RT_MANIFEST",
}
PNG_SIG = b"\x89PNG\r\n\x1a\n"
SUBSYSTEMS = {1: "NATIVE", 2: "WINDOWS_GUI", 3: "WINDOWS_CUI", 7: "POSIX_CUI",
              9: "WINDOWS_CE_GUI", 10: "EFI_APPLICATION"}
MACHINES = {0x014C: "i386", 0x8664: "x86_64", 0xAA64: "arm64"}


class PeError(Exception):
    pass


def _u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def _u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def parse(path):
    b = open(path, "rb").read()
    if b[:2] != b"MZ":
        raise PeError("MZ 头缺失，不是 PE 文件")
    pe = _u32(b, 0x3C)
    if b[pe:pe + 4] != b"PE\0\0":
        raise PeError("PE 签名缺失")
    machine = _u16(b, pe + 4)
    nsec = _u16(b, pe + 6)
    optsz = _u16(b, pe + 20)
    opt = pe + 24
    magic = _u16(b, opt)
    if magic not in (0x10B, 0x20B):
        raise PeError(f"可选头 magic 异常 0x{magic:x}")
    subsystem = _u16(b, opt + 68)
    dd = opt + (112 if magic == 0x20B else 96)
    res_rva, res_size = struct.unpack_from("<II", b, dd + 2 * 8)

    secs = []
    so = opt + optsz
    for i in range(nsec):
        o = so + 40 * i
        _name, vsize, vaddr, _rsize, raddr = struct.unpack_from("<8sIIII", b, o)
        secs.append((vaddr, max(vsize, _rsize), raddr))

    def rva2off(rva):
        for vaddr, span, raddr in secs:
            if vaddr <= rva < vaddr + span:
                return raddr + (rva - vaddr)
        return None

    def entries(off):
        named, ids = _u16(b, off + 12), _u16(b, off + 14)
        return [( _u32(b, off + 16 + 8 * i), _u32(b, off + 20 + 8 * i))
                for i in range(named + ids)]

    types, icon_frames, icon_codec = [], [], {}
    if res_rva:
        base = rva2off(res_rva)
        if base is None:
            raise PeError("资源表 RVA 落在节表之外")
        for type_id, sub in entries(base):
            tname = RESOURCE_TYPES.get(type_id & 0x7FFFFFFF, f"type#{type_id}")
            count = len(entries(base + (sub & 0x7FFFFFFF)))
            types.append({"id": type_id, "name": tname, "count": count})
            for _name_id, lang in entries(base + (sub & 0x7FFFFFFF)):
                for _lid, data_entry in entries(base + (lang & 0x7FFFFFFF)):
                    de = base + data_entry
                    data_rva, size = struct.unpack_from("<II", b, de)
                    off = rva2off(data_rva)
                    payload = b[off:off + size]
                    if (type_id & 0x7FFFFFFF) == 3:      # RT_ICON
                        icon_codec[_name_id] = (
                            "PNG" if payload[:8] == PNG_SIG else
                            f"BMP{struct.unpack_from('<H', payload, 14)[0]}bpp"
                            if payload[:4] == b"\x28\0\0\0" else "未知")
                    elif (type_id & 0x7FFFFFFF) == 14:   # RT_GROUP_ICON
                        reserved, kind, cnt = struct.unpack_from("<HHH", payload, 0)
                        for i in range(cnt):
                            w, h, ncol, _r, planes, bpp, bsz, rid = struct.unpack_from(
                                "<BBBBHHIH", payload, 6 + 14 * i)
                            icon_frames.append({"w": w or 256, "h": h or 256,
                                                "bpp": bpp, "bytes": bsz, "res_id": rid,
                                                "codec": "?"})
    for f in icon_frames:            # 帧按资源 ID 关联负载编码（两者顺序无关）
        f["codec"] = icon_codec.get(f["res_id"], "?")
    return {
        "file": path,
        "bytes": len(b),
        "machine": MACHINES.get(machine, f"0x{machine:04x}"),
        "pe32_plus": magic == 0x20B,
        "sections": nsec,
        "subsystem": SUBSYSTEMS.get(subsystem, str(subsystem)),
        "subsystem_id": subsystem,
        "resource_rva": f"0x{res_rva:x}",
        "resource_bytes": res_size,
        "resource_types": types,
        "icon_group_frames": sorted(icon_frames, key=lambda f: f["w"]),
        "has_icon": bool(icon_frames),
        "is_gui": subsystem == 2,
    }


def report(info):
    print(f"文件: {info['file']}  {info['bytes']} bytes")
    print(f"  架构={info['machine']} {'PE32+' if info['pe32_plus'] else 'PE32'} "
          f"节数={info['sections']} 子系统={info['subsystem']}({info['subsystem_id']})"
          f"{'  ← GUI 子系统' if info['is_gui'] else ''}")
    print(f"  资源表 RVA={info['resource_rva']} 大小={info['resource_bytes']}")
    for t in info["resource_types"]:
        print(f"    类型 {t['id']:>2} {t['name']:<16} 条目={t['count']}")
    if info["icon_group_frames"]:
        frames = info["icon_group_frames"]
        sizes = [f["w"] for f in frames]
        print(f"  RT_GROUP_ICON 帧数={len(frames)} 尺寸集={sizes}")
        for f in frames:
            print(f"    {f['w']:>3}x{f['h']:<3} bpp={f['bpp']:<2} 字节={f['bytes']:<7} "
                  f"负载={f['codec']} 资源ID={f['res_id']}")
    else:
        print("  无 RT_GROUP_ICON（该 exe 不带图标组）")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+", help="PE 文件（.exe/.dll）")
    ap.add_argument("--json", action="store_true", help="输出 JSON")
    args = ap.parse_args()
    infos, failed = [], False
    for p in args.files:
        if not os.path.exists(p):
            print(f"文件不存在: {p}", file=sys.stderr)
            failed = True
            continue
        try:
            info = parse(p)
        except PeError as e:
            print(f"{p}: 解析失败（{e}）", file=sys.stderr)
            failed = True
            continue
        infos.append(info)
        if not args.json:
            report(info)
            print()
    if args.json:
        print(json.dumps(infos, ensure_ascii=False, indent=2))
    return 2 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
