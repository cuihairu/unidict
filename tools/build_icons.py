#!/usr/bin/env python3
"""从 docs/logo.svg 生成三平台二进制图标资产（BUG-006，可重复执行）。

产物（全部写进 assets/icons/）：
  unidict.ico        16/24 BMP-in-ICO（8bpp 调色板 + AND 掩码，兼容老 shell）
                     32/48/64/128/256 PNG-in-ICO（Vista+ 软 alpha，任务栏/
                     资源管理器不再锯齿）
  unidict.icns       ic11/12/07/08/09/10 = 32/64/128/256/512/1024 PNG-in-ICNS
  unidict_256.png    Linux hicolor 256
  unidict_512.png    Linux hicolor 512

SVG 光栅化后端按可用性择优：resvg → rsvg-convert → inkscape → ImageMagick。
本仓 logo 是「1024 viewBox 单 path」（无文字、无渐变依赖），四个后端的输出
在像素度量上等价，换后端不改变资产语义。

用法：python3 tools/build_icons.py [--check]
  --check 只校验现有资产（不写文件），走查/CI 用；退出码非 0 = 资产损坏
"""

import argparse
import io
import os
import struct
import subprocess
import sys
import tempfile
from shutil import which

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SVG = os.path.join(ROOT, "docs", "logo.svg")
OUT_DIR = os.path.join(ROOT, "assets", "icons")
ICO = os.path.join(OUT_DIR, "unidict.ico")
ICNS = os.path.join(OUT_DIR, "unidict.icns")
PNG256 = os.path.join(OUT_DIR, "unidict_256.png")
PNG512 = os.path.join(OUT_DIR, "unidict_512.png")

ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
ICO_BMP_SIZES = {16, 24}          # 小尺寸留 BMP，老 shell 也读得动
ICNS_FRAMES = [("ic11", 32), ("ic12", 64), ("ic07", 128),
               ("ic08", 256), ("ic09", 512), ("ic10", 1024)]
MASTER = 1024
PNG_SIG = b"\x89PNG\r\n\x1a\n"


def raster_cmd(out):
    """按可用性选 SVG 光栅化后端，返回完整 argv。"""
    if which("resvg"):
        return ["resvg", "--width", str(MASTER), SVG, out]
    if which("rsvg-convert"):
        return ["rsvg-convert", "-w", str(MASTER), SVG, out]
    if which("inkscape"):
        return ["inkscape", "--export-type=png", "--export-width", str(MASTER),
                f"--export-filename={out}", SVG]
    if which("convert"):
        # ImageMagick：-density 控栅格化尺寸，-background none 保透明
        return ["convert", "-background", "none", "-density", "384", SVG,
                "-resize", f"{MASTER}x{MASTER}", out]
    sys.exit("没有可用的 SVG 光栅化后端（resvg / rsvg-convert / inkscape / ImageMagick）")


def rasterize_master(tmp):
    out = os.path.join(tmp, "master.png")
    cmd = raster_cmd(out)
    subprocess.run(cmd, check=True)
    img = Image.open(out).convert("RGBA")
    assert img.size == (MASTER, MASTER), f"master 尺寸异常 {img.size}"
    return img, cmd[0]


def downsample(master, size):
    return master.resize((size, size), Image.LANCZOS)


def png_bytes(img):
    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def bmp_ico_payload(img, tmp, tag):
    """让 ImageMagick 写单帧 ICO 再抽负载：DIB 头 + 调色板 + XOR + AND 掩码。"""
    p_in, p_ico = os.path.join(tmp, f"{tag}.png"), os.path.join(tmp, f"{tag}.ico")
    img.save(p_in)
    subprocess.run(["convert", p_in, p_ico], check=True)
    d = open(p_ico, "rb").read()
    n = struct.unpack_from("<H", d, 4)[0]
    assert n == 1, f"{tag}.ico 帧数异常 {n}"
    w, h, _cc, _r, _pl, _bpp, size, off = struct.unpack_from("<BBBBHHII", d, 6)
    assert (w or 256) == img.size[0] and (h or 256) == img.size[1], (w, h, img.size)
    return d[off:off + size]


def build_ico(master, tmp):
    entries, blobs, offset = b"", b"", 6 + 16 * len(ICO_SIZES)
    for s in ICO_SIZES:
        img = downsample(master, s)
        payload = (bmp_ico_payload(img, tmp, f"bmp{s}") if s in ICO_BMP_SIZES
                   else png_bytes(img))
        entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32,
                               len(payload), offset)
        blobs += payload
        offset += len(payload)
    return struct.pack("<HHH", 0, 1, len(ICO_SIZES)) + entries + blobs


def build_icns(master):
    body = b""
    for tag, size in ICNS_FRAMES:
        png = png_bytes(downsample(master, size))
        body += struct.pack(">4sI", tag.encode(), len(png) + 8) + png
    return struct.pack(">4sI", b"icns", len(body) + 8) + body


def coverage(img):
    hist = img.getchannel("A").histogram()
    total = img.size[0] * img.size[1]
    return sum(hist[1:]), round(100.0 * sum(hist[1:]) / total, 2), hist[255]


def ico_decode(ico_path, index):
    """ICO 语义解码交给 ImageMagick（DIB 高度 2× / XOR+AND / 行序都由它处理）。"""
    png = subprocess.run(["convert", f"{ico_path}[{index}]", "png:-"],
                         check=True, stdout=subprocess.PIPE).stdout
    return Image.open(io.BytesIO(png)).convert("RGBA")


def report_ico(ico_path):
    blob = open(ico_path, "rb").read()
    n = struct.unpack_from("<H", blob, 4)[0]
    print(f"{os.path.relpath(ico_path, ROOT)}: ICONDIR 声明 {n} 帧, {len(blob)} bytes")
    sizes = []
    for i in range(n):
        w, h, _c, _r, _p, bpp, size, off = struct.unpack_from("<BBBBHHII", blob, 6 + 16 * i)
        payload = blob[off:off + size]
        kind = "PNG" if payload[:8] == PNG_SIG else "BMP"
        img = ico_decode(ico_path, i)
        assert img.size == (w or 256, h or 256), (i, img.size, w, h)
        nz, pct, opaque = coverage(img)
        sizes.append(w or 256)
        print(f"  {w or 256:>3}x{h or 256:<3} {kind:3} bpp={bpp:<2} 字节={size:<7} "
              f"非透明={nz} ({pct}%) 全不透明={opaque} 解码={img.size[0]}x{img.size[1]}")
    assert sorted(sizes) == ICO_SIZES, f"ICO 尺寸集异常 {sizes}"
    return sizes


def report_icns(icns_path):
    blob = open(icns_path, "rb").read()
    magic, size = struct.unpack(">4sI", blob[:8])
    ok = magic == b"icns" and size == len(blob)
    print(f"{os.path.relpath(icns_path, ROOT)}: magic={magic.decode()} 声明={size} "
          f"实际={len(blob)} {'一致' if ok else '✗不一致'}")
    assert ok, "ICNS 容器头与实际长度不符"
    off, tags = 8, []
    while off < size:
        tag, sz = struct.unpack(">4sI", blob[off:off + 8])
        payload = blob[off + 8:off + sz]
        assert payload[:8] == PNG_SIG, f"{tag.decode()} 不是 PNG 负载"
        img = Image.open(io.BytesIO(payload)).convert("RGBA")
        nz, pct, opaque = coverage(img)
        tags.append(tag.decode())
        print(f"  {tag.decode()} {img.size[0]}x{img.size[1]:<4} PNG 负载={sz - 8:<7} "
              f"非透明={nz} ({pct}%) 全不透明={opaque}")
        off += sz
    assert tags == [t for t, _ in ICNS_FRAMES], f"ICNS 帧集异常 {tags}"
    return tags


def report_png(path, expect):
    img = Image.open(path).convert("RGBA")
    nz, pct, _ = coverage(img)
    print(f"{os.path.relpath(path, ROOT)}: {img.size[0]}x{img.size[1]} "
          f"非透明={nz} ({pct}%)")
    assert img.size == (expect, expect), f"{path} 尺寸异常 {img.size}"
    return img


def check_only():
    report_ico(ICO)
    report_icns(ICNS)
    report_png(PNG256, 256)
    report_png(PNG512, 512)
    print("OK: 资产结构与尺寸集自检通过")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true", help="只校验现有资产，不写文件")
    args = ap.parse_args()
    if args.check:
        check_only()
        return
    os.makedirs(OUT_DIR, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        master, backend = rasterize_master(tmp)
        print(f"光栅化后端: {backend}  master={master.size} "
              f"不透明覆盖率={coverage(master)[1]}%")
        open(ICO, "wb").write(build_ico(master, tmp))
        open(ICNS, "wb").write(build_icns(master))
        downsample(master, 256).save(PNG256, optimize=True)
        downsample(master, 512).save(PNG512, optimize=True)
        for p in (ICO, ICNS, PNG256, PNG512):
            print(f"写入 {os.path.relpath(p, ROOT)}  {os.path.getsize(p)} bytes")
    check_only()


if __name__ == "__main__":
    main()
