#!/usr/bin/env python3
"""从 docs/logo.svg 生成三平台二进制图标资产（BUG-006，可重复执行）。

产物（全部写进 assets/icons/）：
  unidict.ico        16/24 BMP-in-ICO（8bpp 调色板 + AND 掩码，兼容老 shell）
                     32/48/64/128/256 PNG-in-ICO（Vista+ 软 alpha，任务栏/
                     资源管理器不再锯齿）
  unidict.icns       ic11/12/07/08/09/10 = 32/64/128/256/512/1024 PNG-in-ICNS
  unidict_256.png    Linux hicolor 256
  unidict_512.png    Linux hicolor 512
  Android 启动图标   android/app/src/main/res/mipmap-*/ic_launcher{,_round}.png
                     （mdpi 48 … xxxhdpi 192）+ mipmap-*/ic_launcher_foreground.png
                     （108dp 画布 48…432）+ mipmap-anydpi-v26/ic_launcher{,_round}.xml
                     自适应图标（前景字形落在 66/108 安全区内）+ 白底

SVG 光栅化后端按可用性择优：resvg → rsvg-convert → inkscape → ImageMagick。
本仓 logo 是「1024 viewBox 单 path」（无文字、无渐变依赖），四个后端的输出
在像素度量上等价，换后端不改变资产语义。

用法：python3 tools/build_icons.py [--check] [--sheets]
  --check 只校验现有资产（不写文件），走查/CI 用；退出码非 0 = 资产损坏
  --sheets 只重出 docs/icons/ 对照图（给人眼核验，资产本身不动）
"""

import argparse
import io
import os
import struct
import subprocess
import sys
import tempfile
from functools import lru_cache
from shutil import which

from PIL import Image, ImageDraw, ImageFont

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

# Android 启动图标：dpi 桶 → (legacy 边长, 自适应前景画布边长)
ANDROID_RES = os.path.join(ROOT, "android", "app", "src", "main", "res")
ANDROID_DPI = {"mdpi": (48, 108), "hdpi": (72, 162), "xhdpi": (96, 216),
               "xxhdpi": (144, 324), "xxxhdpi": (192, 432)}
ANDROID_BG = (245, 239, 243, 255)   # #F5EFF3：品牌色 5% 淡底。字形保持原样
                                    # （不自行给 logo 换色/反白），淡底只为白底
                                    # 启动器上能看出图标边界
LEGACY_GLYPH_RATIO = 0.78            # 字形占 legacy 方图的比例
ADAPTIVE_GLYPH_RATIO = 66 / 108      # Android 规范：字形落在 66/108 安全区内


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


def glyph(master):
    """按 alpha 通道裁到字形内容框——SVG 画布留白不均，直接缩放会偏心。"""
    box = master.getchannel("A").getbbox()
    assert box, "master 全透明，没有字形"
    return master.crop(box)


def place(g, canvas_size, ratio, background=None, circle=False):
    """把字形按 ratio 居中放进 canvas_size 画布（可带底色/圆形遮罩）。"""
    side = max(1, int(round(canvas_size * ratio)))
    scaled = g.resize((side, side), Image.LANCZOS)
    canvas = Image.new("RGBA", (canvas_size, canvas_size),
                       background if background else (0, 0, 0, 0))
    if circle and background:                    # round 变体：圆形 alpha 遮罩
        mask = Image.new("L", (canvas_size, canvas_size), 0)
        from PIL import ImageDraw
        ImageDraw.Draw(mask).ellipse([0, 0, canvas_size - 1, canvas_size - 1], fill=255)
        canvas.putalpha(mask)
    canvas.alpha_composite(scaled, ((canvas_size - side) // 2, (canvas_size - side) // 2))
    return canvas


def write_android_assets(master):
    """Android 启动图标：legacy 方/圆 + 自适应前景 + anydpi-v26 XML。"""
    from PIL import ImageDraw
    g = glyph(master)
    written = []
    for dpi, (legacy, fg) in ANDROID_DPI.items():
        out_dir = os.path.join(ANDROID_RES, f"mipmap-{dpi}")
        os.makedirs(out_dir, exist_ok=True)
        place(g, legacy, LEGACY_GLYPH_RATIO, ANDROID_BG).save(
            os.path.join(out_dir, "ic_launcher.png"), optimize=True)
        place(g, legacy, LEGACY_GLYPH_RATIO, ANDROID_BG, circle=True).save(
            os.path.join(out_dir, "ic_launcher_round.png"), optimize=True)
        place(g, fg, ADAPTIVE_GLYPH_RATIO).save(
            os.path.join(out_dir, "ic_launcher_foreground.png"), optimize=True)
        written += [f"mipmap-{dpi}/ic_launcher.png ({legacy}px)",
                    f"mipmap-{dpi}/ic_launcher_round.png ({legacy}px)",
                    f"mipmap-{dpi}/ic_launcher_foreground.png ({fg}px)"]
    anydpi = os.path.join(ANDROID_RES, "mipmap-anydpi-v26")
    os.makedirs(anydpi, exist_ok=True)
    xml = ('<?xml version="1.0" encoding="utf-8"?>\n'
           '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
           '    <background android:drawable="@color/ic_launcher_background" />\n'
           '    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n'
           '</adaptive-icon>\n')
    for name in ("ic_launcher.xml", "ic_launcher_round.xml"):
        with open(os.path.join(anydpi, name), "w", encoding="utf-8") as f:
            f.write(xml)
        written.append(f"mipmap-anydpi-v26/{name}")
    colors = os.path.join(ANDROID_RES, "values", "ic_launcher_colors.xml")
    with open(colors, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n'
                '<resources>\n'
                '    <!-- 启动图标自适应层底色 #F5EFF3（品牌色 #b11964 的 5% 淡底）：\n'
                '     字形保持 logo 原色不反白，淡底只为白底启动器上能看出\n'
                '     图标边界 -->\n'
                '    <color name="ic_launcher_background">#F5EFF3</color>\n'
                '</resources>\n')
    written.append("values/ic_launcher_colors.xml")
    return written


def report_android():
    ok = True
    for dpi, (legacy, fg) in ANDROID_DPI.items():
        for name, expect in (("ic_launcher.png", legacy), ("ic_launcher_round.png", legacy),
                             ("ic_launcher_foreground.png", fg)):
            p = os.path.join(ANDROID_RES, f"mipmap-{dpi}", name)
            if not os.path.exists(p):
                print(f"  缺 {os.path.relpath(p, ROOT)}")
                ok = False
                continue
            img = Image.open(p).convert("RGBA")
            nz, pct, _ = coverage(img)
            flag = "" if img.size == (expect, expect) else "  ✗尺寸异常"
            ok &= img.size == (expect, expect)
            print(f"  mipmap-{dpi}/{name:<28} {img.size[0]}x{img.size[1]} "
                  f"非透明={nz} ({pct}%){flag}")
    for name in ("ic_launcher.xml", "ic_launcher_round.xml"):
        p = os.path.join(ANDROID_RES, "mipmap-anydpi-v26", name)
        exists = os.path.exists(p)
        ok &= exists
        print(f"  mipmap-anydpi-v26/{name:<24} {'存在' if exists else '缺失'}")
    p = os.path.join(ANDROID_RES, "values", "ic_launcher_colors.xml")
    exists = os.path.exists(p)
    ok &= exists
    print(f"  values/ic_launcher_colors.xml{'':<12} {'存在' if exists else '缺失'}")
    return ok


# ---- docs/icons/ 对照图：给人眼核验用的派生文档，不是分发资产 ----
SHEET_DIR = os.path.join(ROOT, "docs", "icons")
SHEET_CELL, SHEET_HEAD, SHEET_PAD, SHEET_GAP = 132, 26, 40, 34
SHEET_BG = (252, 252, 253)
ADAPTIVE_VIEW = 72 / 108          # Android 规范：自适应图标只有中间 72/108 可见


@lru_cache(maxsize=None)
def sheet_font(size):
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
              "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"):
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def checkerboard(size, cell=8):
    im = Image.new("RGB", (size, size), (255, 255, 255))
    d = ImageDraw.Draw(im)
    for y in range(0, size, cell):
        for x in range(0, size, cell):
            if (x // cell + y // cell) % 2:
                d.rectangle([x, y, x + cell - 1, y + cell - 1], fill=(228, 228, 232))
    return im


def adaptive_preview(fg, mask="circle"):
    """自适应图标在启动器里的观感：108dp 画布 → 72/108 可见区 → 套启动器遮罩。"""
    side = fg.size[0]
    vis = int(round(side * ADAPTIVE_VIEW))
    canvas = Image.new("RGBA", (side, side), ANDROID_BG)
    canvas.alpha_composite(fg.resize((vis, vis), Image.LANCZOS),
                           ((side - vis) // 2, (side - vis) // 2))
    if mask == "circle":
        m = Image.new("L", (side, side), 0)
        ImageDraw.Draw(m).ellipse([0, 0, side - 1, side - 1], fill=255)
        canvas.putalpha(m)
    elif mask == "squircle":
        big = Image.new("L", (side * 4, side * 4), 0)
        ImageDraw.Draw(big).rounded_rectangle([0, 0, side * 4 - 1, side * 4 - 1],
                                              radius=int(side * 4 * 0.28), fill=255)
        canvas.putalpha(big.resize((side, side), Image.LANCZOS))
    elif mask != "full":
        raise ValueError(f"未知遮罩 {mask}")
    return canvas


def with_safe_zone(fg):
    """前景层叠 66/108 安全区框（四边各内缩 21/108）。"""
    im = fg.copy()
    d = ImageDraw.Draw(im)
    lo = int(round(fg.size[0] * (1 - ADAPTIVE_GLYPH_RATIO) / 2))
    d.rectangle([lo, lo, fg.size[0] - 1 - lo, fg.size[1] - 1 - lo],
                outline=(255, 0, 0, 255), width=max(1, fg.size[0] // 54))
    return im


def sheet_grid(panels, per_row=0):
    """panels: [(标题, 副标题, [(图, 标签), ...]), ...] 排成对照图。

    小尺寸帧按最近邻放大到格宽，便于肉眼看单像素结构；缩略图下的棋盘格
    用来区分「透明」与「白」。
    """
    per_row = per_row or len(panels)
    rows = [panels[i:i + per_row] for i in range(0, len(panels), per_row)]
    ncol = max(len(p[2]) for p in panels)
    pw = SHEET_PAD * 2 + ncol * (SHEET_CELL + SHEET_GAP)
    ph = SHEET_HEAD + SHEET_CELL + 46
    W = SHEET_PAD * 2 + per_row * pw + (per_row - 1) * SHEET_PAD
    H = SHEET_PAD * 2 + len(rows) * ph + (len(rows) - 1) * SHEET_PAD
    sheet = Image.new("RGB", (W, H), SHEET_BG)
    d = ImageDraw.Draw(sheet)
    for idx, (title, subtitle, cells) in enumerate(panels):
        r, c = divmod(idx, per_row)
        x0 = SHEET_PAD + c * (pw + SHEET_PAD)
        y0 = SHEET_PAD + r * (ph + SHEET_PAD)
        d.rectangle([x0, y0, x0 + pw, y0 + ph], fill=(255, 255, 255),
                    outline=(226, 226, 232))
        d.text((x0 + 14, y0 + 8), title, fill=(18, 18, 22), font=sheet_font(17))
        for j, (img, label) in enumerate(cells):
            cx = x0 + SHEET_PAD + j * (SHEET_CELL + SHEET_GAP)
            shown = img.resize((SHEET_CELL, SHEET_CELL), Image.NEAREST)
            bg = checkerboard(SHEET_CELL)
            bg.paste(shown, ((SHEET_CELL - shown.size[0]) // 2,
                             (SHEET_CELL - shown.size[1]) // 2), shown)
            sheet.paste(bg, (cx, y0 + SHEET_HEAD))
            d.text((cx + 2, y0 + SHEET_HEAD + SHEET_CELL + 5), label,
                   fill=(55, 55, 66), font=sheet_font(13))
        d.text((x0 + 14, y0 + SHEET_HEAD + SHEET_CELL + 26), subtitle,
               fill=(110, 110, 124), font=sheet_font(12))
    return sheet


def icns_decode_frames(icns_path):
    blob = open(icns_path, "rb").read()
    frames, off = [], 8
    while off < len(blob):
        _tag, sz = struct.unpack(">4sI", blob[off:off + 8])
        frames.append(Image.open(io.BytesIO(blob[off + 8:off + sz])).convert("RGBA"))
        off += sz
    return frames


def write_sheets():
    """出两张对照图：桌面三平台全部帧 + Android 启动图标。"""
    os.makedirs(SHEET_DIR, exist_ok=True)
    written = []

    desktop = sheet_grid([
        ("Windows — assets/icons/unidict.ico",
         "16/24 BMP-in-ICO（32bpp XOR+AND 掩码，兼容老 shell）· 32 及以上 PNG-in-ICO 软 alpha",
         [(ico_decode(ICO, i), f"{s}px") for i, s in enumerate(ICO_SIZES)]),
        ("macOS — assets/icons/unidict.icns",
         "ic11=32 ic12=64 ic07=128 ic08=256 ic09=512 ic10=1024 PNG-in-ICNS · 落 Contents/Resources",
         [(img, f"{size}px") for img, (_t, size)
          in zip(icns_decode_frames(ICNS), ICNS_FRAMES)]),
        ("Linux — share/icons/hicolor（installed）",
         "Icon=unidict → hicolor/256x256/apps/unidict.png（512 版同装）下采样到常用格",
         [(Image.open(PNG256).convert("RGBA").resize((s, s), Image.LANCZOS), f"{s}px")
          for s in (16, 32, 48, 64, 128, 256)]),
    ])
    p = os.path.join(SHEET_DIR, "desktop-icon-frames.png")
    desktop.save(p, optimize=True)
    written.append((p, desktop.size))

    def res_png(dpi, name):
        return Image.open(os.path.join(ANDROID_RES, f"mipmap-{dpi}", name)).convert("RGBA")

    android = sheet_grid([
        ("legacy 方形 ic_launcher",
         "mdpi 48 → xxxhdpi 192px · 底色 #F5EFF3 · 字形占方图 78% · manifest android:icon",
         [(res_png(dpi, "ic_launcher.png"), f"{dpi} {ANDROID_DPI[dpi][0]}") for dpi in ANDROID_DPI]),
        ("legacy 圆形 ic_launcher_round",
         "圆形 alpha 遮罩（四角 alpha 实测 = 0）· manifest android:roundIcon",
         [(res_png(dpi, "ic_launcher_round.png"), dpi) for dpi in ANDROID_DPI]),
        ("自适应图标（API 26+ 启动器遮罩）",
         "anydpi-v26：前景 108dp 画布按 72/108 可见区 + 背景 #F5EFF3 · xxxhdpi",
         [(adaptive_preview(res_png("xxxhdpi", "ic_launcher_foreground.png"), m), m)
          for m in ("circle", "squircle", "full")]),
        ("前景层 66/108 安全区",
         "ic_launcher_foreground 是透明底图层；字形恰好贴合安全区（红框）",
         [(with_safe_zone(res_png(dpi, "ic_launcher_foreground.png")), dpi)
          for dpi in ("mdpi", "xxxhdpi")]),
        ("自适应图标 = 前景 + 背景",
         "res/mipmap-anydpi-v26/ic_launcher.xml：background=@color/ic_launcher_background"
         " foreground=@mipmap/ic_launcher_foreground",
         [(adaptive_preview(res_png(dpi, "ic_launcher_foreground.png"), "squircle"),
           f"{dpi}·squircle") for dpi in ("mdpi", "xhdpi", "xxxhdpi")]),
    ], per_row=3)
    p = os.path.join(SHEET_DIR, "android-launcher-icons.png")
    android.save(p, optimize=True)
    written.append((p, android.size))

    for path, size in written:
        print(f"对照图 {os.path.relpath(path, ROOT)}  {size[0]}x{size[1]}  "
              f"{os.path.getsize(path)} bytes")
    return written


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
    if os.path.isdir(ANDROID_RES):
        print("-- Android 启动图标 --")
        report_android()
    print("OK: 资产结构与尺寸集自检通过")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true", help="只校验现有资产，不写文件")
    ap.add_argument("--sheets", action="store_true",
                    help="只重出 docs/icons/ 对照图（资产本身不动）")
    args = ap.parse_args()
    if args.check:
        check_only()
        return
    if args.sheets:
        write_sheets()
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
        if os.path.isdir(ANDROID_RES):
            for rel in write_android_assets(master):
                print(f"写入 {os.path.relpath(os.path.join(ANDROID_RES, rel), ROOT)}")
    check_only()


if __name__ == "__main__":
    main()
