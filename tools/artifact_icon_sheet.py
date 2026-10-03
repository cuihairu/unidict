#!/usr/bin/env python3
"""从**构建产物**（而非仓库资产）出图标对照图 + 逐帧字节级对账。

为什么不用 build_icons.py --sheets 就完事：那张图画的是仓库里的
assets/icons/*，证的是「资产本身对」。用户要验的是另一件事——**打进产物
里的图标是不是这张**。本脚本的输入全部来自产物：

  * Windows：nightly zip 里 unidict_qml.exe 的 PE 资源段（pe_check.py 解析
    RT_GROUP_ICON/RT_ICON，还原成独立 .ico 再解码，不看仓库资产）；
  * macOS：unidict_qml.app/Contents/Resources/unidict.icns（bundle 内文件）；
  * Linux：安装后的 share/icons/hicolor/*/apps/unidict.png。

产出：docs/icons/product-icon-artifacts.png（产物帧 + 与仓库资产的差值图），
并把 md5/覆盖率等客观数字打到 stdout 供留档。缺哪个产物就少一格面板，
不猜、不用仓库资产顶替。

用法：
  python3 tools/artifact_icon_sheet.py --win-exe /tmp/nb/ex/unidict_qml.exe \
      --mac-icns /tmp/nb/unidict_qml.app/Contents/Resources/unidict.icns \
      --linux-png /tmp/prefix/share/icons/hicolor/256x256/apps/unidict.png
"""

import argparse
import hashlib
import os
import struct
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_icons as bi                                   # noqa: E402
import pe_check                                            # noqa: E402

ROOT = bi.ROOT
OUT_DEFAULT = os.path.join(ROOT, "docs", "icons", "product-icon-artifacts.png")


def md5_file(path):
    return hashlib.md5(open(path, "rb").read()).hexdigest()


def decode_ico_frames(ico_path):
    """ICO 帧解码交给 ImageMagick（DIB 高度 2× / XOR+AND / 行序都由它处理）。"""
    blob = open(ico_path, "rb").read()
    n = struct.unpack_from("<H", blob, 4)[0]
    return [bi.ico_decode(ico_path, i) for i in range(n)]


def diff_panel(img_a, img_b, label):
    """差值图：黑 = 逐像素完全相同；返回 (图, 标签, 最大通道差)。"""
    if img_a.size != img_b.size:
        return img_a.convert("RGBA"), f"{label} 尺寸不同", 255
    d = ImageChops.difference(img_a.convert("RGB"), img_b.convert("RGB"))
    peak = max(d.getextrema()[c][1] for c in range(3))
    return d.convert("RGBA"), f"{label} Δ{peak}", peak


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--win-exe", help="产物 unidict_qml.exe（Windows PE）")
    ap.add_argument("--mac-icns", help="产物 .app/Contents/Resources/unidict.icns")
    ap.add_argument("--linux-png", help="产物安装后的 hicolor apps/unidict.png")
    ap.add_argument("--out", default=OUT_DEFAULT, help=f"输出 PNG（默认 {os.path.relpath(OUT_DEFAULT, ROOT)}）")
    args = ap.parse_args()

    panels, notes = [], []
    ok = True          # 对账结论：有任一平台不一致 → 退出码 2

    # ---- Windows：从 PE 资源段还原 ----
    win_frames, win_ico_md5 = [], ""
    if args.win_exe:
        info = pe_check.parse(args.win_exe)
        sizes = [f["w"] for f in info["icon_group_frames"]]
        codecs = [f["codec"] for f in info["icon_group_frames"]]
        with tempfile.TemporaryDirectory() as td:
            ico_path, n = pe_check.dump_ico(info, os.path.join(td, "from_pe.ico"))
            win_ico_bytes = os.path.getsize(ico_path)
            win_ico_md5 = md5_file(ico_path)
            win_frames = decode_ico_frames(ico_path)
        print(f"产物 exe: {os.path.abspath(args.win_exe)}")
        print(f"  子系统={info['subsystem']} 资源段={info['resource_bytes']} bytes "
              f"帧尺寸集={sizes}")
        print(f"  从 PE 还原 .ico：{n} 帧 {win_ico_bytes} bytes md5={win_ico_md5}")
        print(f"  帧负载编码={codecs}")
        repo_ico = os.path.join(ROOT, "assets", "icons", "unidict.ico")
        same = md5_file(repo_ico) == win_ico_md5
        ok &= same
        notes.append(f"Windows exe 图标组 == assets/icons/unidict.ico: "
                     f"{'逐字节一致' if same else '不一致'}")
        print(f"  仓库资产 assets/icons/unidict.ico md5={md5_file(repo_ico)} → "
              f"{'逐字节一致 ✓' if same else '不一致 ✗'}")
        assert n == len(win_frames) == len(sizes)
        for img, size in zip(win_frames, sizes):
            nz, pct, opaque = bi.coverage(img)
            print(f"  {size:>3}px 非透明={nz:<6} 覆盖率={pct:>5}% 全不透明={opaque:<6} "
                  f"{'软 alpha ✓' if opaque < nz else '硬掩码 ✗'}")

    # ---- macOS：bundle 内 icns ----
    mac_frames = []
    if args.mac_icns:
        mac_frames = bi.icns_decode_frames(args.mac_icns)
        blob = open(args.mac_icns, "rb").read()
        magic, declared = struct.unpack(">4sI", blob[:8])
        repo_icns = os.path.join(ROOT, "assets", "icons", "unidict.icns")
        same = md5_file(repo_icns) == md5_file(args.mac_icns)
        ok &= same
        notes.append(f"macOS bundle icns == assets/icons/unidict.icns: "
                     f"{'逐字节一致' if same else '不一致'}")
        print(f"产物 icns: {args.mac_icns}")
        print(f"  magic={magic.decode()} 声明={declared} 实际={len(blob)} "
              f"{'一致 ✓' if magic == b'icns' and declared == len(blob) else '不一致 ✗'}")
        print(f"  帧数={len(mac_frames)} 尺寸={[f.size[0] for f in mac_frames]}")
        print(f"  仓库资产 md5 比对 → {'逐字节一致 ✓' if same else '不一致 ✗'}")

    # ---- Linux：安装后的 hicolor png ----
    lin_img = None
    if args.linux_png:
        lin_img = Image.open(args.linux_png).convert("RGBA")
        repo_png = os.path.join(ROOT, "assets", "icons", "unidict_256.png")
        same = md5_file(repo_png) == md5_file(args.linux_png)
        ok &= same
        notes.append(f"Linux hicolor png == assets/icons/unidict_256.png: "
                     f"{'逐字节一致' if same else '不一致'}")
        print(f"产物 png: {args.linux_png}  {lin_img.size[0]}x{lin_img.size[1]} "
              f"→ {'逐字节一致 ✓' if same else '不一致 ✗'}")

    # ---- 组面板 ----
    repo_frames = [bi.ico_decode(os.path.join(ROOT, "assets", "icons", "unidict.ico"), i)
                   for i in range(len(win_frames))] if win_frames else []
    if win_frames:
        sizes = [f["w"] for f in pe_check.parse(args.win_exe)["icon_group_frames"]]
        panels.append((
            f"Windows 产物 — {os.path.basename(args.win_exe)} 的 PE 资源段 RT_ICON",
            "从 exe 里还原（非仓库资产）· 16/24 BMP-in-ICO 32bpp · 32 及以上 PNG-in-ICO 软 alpha",
            [(img, f"{s}px") for img, s in zip(win_frames, sizes)]))
        if repo_frames:
            diffs = [diff_panel(a, b, f"{s}px") for a, b, s
                     in zip(win_frames, repo_frames, sizes)]
            ok &= all(peak == 0 for _img, _lab, peak in diffs)
            panels.append((
                "产物帧 − 仓库资产帧（差值图，纯黑 = 逐像素相同）",
                f"资产源 assets/icons/unidict.ico · 整文件 md5 {'一致' if md5_file(os.path.join(ROOT, 'assets', 'icons', 'unidict.ico')) == win_ico_md5 else '不一致'}",
                [(img, lab) for img, lab, _peak in diffs]))
    if mac_frames:
        labels = [t for t, _ in bi.ICNS_FRAMES]
        panels.append((
            f"macOS 产物 — {os.path.basename(args.mac_icns)}（.app/Contents/Resources）",
            "PNG-in-ICNS 软 alpha 六帧 · Finder/启动台按 icns 里的这些帧取图",
            [(img, f"{lab}={img.size[0]}px") for img, lab in zip(mac_frames, labels)]))
    if lin_img:
        panels.append((
            f"Linux 产物 — {os.path.basename(args.linux_png)}（hicolor apps/）",
            "安装后 .desktop 的 Icon=unidict 经 hicolor 索引取到这张 · 下采样到常用格",
            [(lin_img.resize((s, s), Image.LANCZOS), f"{s}px")
             for s in (16, 32, 48, 64, 128, 256)]))

    if not panels:
        print("没有可用产物（至少给 --win-exe / --mac-icns / --linux-png 之一）",
              file=sys.stderr)
        return 2

    # 每行两格：四格横排会宽到 5000+px，缩略后 16px 帧看不清单像素结构
    sheet = bi.sheet_grid(panels, per_row=2)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    sheet.save(args.out, optimize=True)
    print()
    for line in notes:
        print(f"对账: {line}")
    print(f"对照图 {os.path.relpath(args.out, ROOT)}  {sheet.size[0]}x{sheet.size[1]}  "
          f"{os.path.getsize(args.out)} bytes")
    print("对账结论: " + ("产物图标与仓库资产全部逐字节一致 ✓" if ok
                        else "存在不一致 ✗（见上）"))
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
