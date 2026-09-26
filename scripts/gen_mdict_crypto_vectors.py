#!/usr/bin/env python3
"""算出 mdict_crypto_std_test.cpp 里那些"独立期望值"。

为什么要生成而不是手敲：这些期望值的唯一作用是**独立校验 C++ 移植**。
如果人肉从 C++ 里抄一份填进测试，测试就只是在验证"我抄对了自己"。
这里用 Python 照同一份公开规范（mdict readmdict.py 的 _fast_decrypt 与
ripemd128.py）另写一份实现，算出的值与 C++ 互为独立参照。

规范向量本身不在这里生成——RIPEMD-128 的测试向量来自公开规范，是外部
基准，写死在测试里。
"""
import io
import struct

# ---- RIPEMD-128（照公开规范另写一份，与 C++ 实现相互独立） ----
_r = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
      7,4,13,1,10,6,15,3,12,0,9,5,2,14,11,8,
      3,10,14,4,9,15,8,1,2,7,0,6,13,11,5,12,
      1,9,11,10,0,8,12,4,13,3,7,15,14,5,6,2]
_rp = [5,14,7,0,9,2,11,4,13,6,15,8,1,10,3,12,
       6,11,3,7,0,13,5,10,14,15,8,12,4,9,1,2,
       15,5,1,3,7,14,6,9,11,8,12,2,10,0,4,13,
       8,6,4,1,3,11,15,0,5,12,2,13,9,7,10,14]
_s = [11,14,15,12,5,8,7,9,11,13,14,15,6,7,9,8,
      7,6,8,13,11,9,7,15,7,12,15,9,11,7,13,12,
      11,13,6,7,14,9,13,15,14,8,13,6,5,12,7,5,
      11,12,14,15,14,15,9,8,9,14,5,6,8,6,5,12]
_sp = [8,9,9,11,13,15,15,5,7,7,8,11,14,14,12,6,
       9,13,15,7,12,8,9,11,7,7,12,7,6,15,13,11,
       9,7,15,11,8,6,6,14,12,13,5,14,13,13,7,5,
       15,5,8,11,14,14,6,14,6,9,12,9,12,5,15,8]


def _f(j, x, y, z):
    if j < 16:
        return x ^ y ^ z
    if j < 32:
        return (x & y) | (z & (~x & 0xffffffff))
    if j < 48:
        return (x | (~y & 0xffffffff)) ^ z
    return (x & z) | (y & (~z & 0xffffffff))


def _k(j):
    return [0x00000000, 0x5a827999, 0x6ed9eba1, 0x8f1bbcdc][j // 16]


def _kp(j):
    return [0x50a28be6, 0x5c4dd124, 0x6d703ef3, 0x00000000][j // 16]


def _rol(x, s):
    return ((x << s) | (x >> (32 - s))) & 0xffffffff


def ripemd128(msg: bytes) -> bytes:
    origlen = len(msg)
    padlength = 64 - ((origlen - 56) % 64)
    m = msg + b"\x80" + b"\x00" * (padlength - 1) + struct.pack("<Q", origlen * 8)
    h0, h1, h2, h3 = 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476
    for off in range(0, len(m), 64):
        x = list(struct.unpack("<16L", m[off:off + 64]))
        a, b, c, d = h0, h1, h2, h3
        ap, bp, cp, dp = h0, h1, h2, h3
        for j in range(64):
            t = _rol((a + _f(j, b, c, d) + x[_r[j]] + _k(j)) & 0xffffffff, _s[j])
            a, d, c, b = d, c, b, t
            tp = _rol((ap + _f(63 - j, bp, cp, dp) + x[_rp[j]] + _kp(j)) & 0xffffffff,
                      _sp[j])
            ap, dp, cp, bp = dp, cp, bp, tp
        t = (h1 + c + dp) & 0xffffffff
        h1 = (h2 + d + ap) & 0xffffffff
        h2 = (h3 + a + bp) & 0xffffffff
        h3 = (h0 + b + cp) & 0xffffffff
        h0 = t
    return struct.pack("<4L", h0, h1, h2, h3)


# ---- MDX 的 fast_decrypt ----
def fast_decrypt(data: bytes, key: bytes) -> bytes:
    b = bytearray(data)
    k = bytearray(key)
    previous = 0x36
    for i in range(len(b)):
        t = ((b[i] >> 4) | (b[i] << 4)) & 0xFF
        t = t ^ previous ^ (i & 0xFF) ^ k[i % len(k)]
        previous = b[i]
        b[i] = t
    return bytes(b)


# ---- 校验 RIPEMD-128 实现本身 ----
# 只锚定**有外部出处**的两条：空串的摘要是 RIPEMD-128 的公开规范测试向量；
# fox 句来自参考实现文档字符串里给出的断言。规范（rmd128.txt）只给了伪代码
# 与轮常量、没有向量表，所以别处"记得"的向量一律不写进测试——记错了就是
# 一条假失败（我在这上面栽过：曾把 RIPEMD-128("a") 记成
# 86be7aea33991a3fcf4b4c1b1ee0e4e7，实际是 86be7afa339d0fc7cfc785e72f578d33）。
assert ripemd128(b"").hex() == "cdf26213a150dc3ecb610f18f6b38b46", ripemd128(b"").hex()
assert ripemd128(b"The quick brown fox jumps over the lazy dog").hex() == \
    "3fa9b57f053c053fbe2735b2380db596"

KEY = bytes.fromhex("0123456789abcdef0123456789abcdef")
DATA = b"The quick brown fox jumps over the lazy dog, 0123456789abcdefABCDEF"
fast_hex = fast_decrypt(DATA, KEY).hex()

adler = b"\x12\x34\x56\x78"
k1_hex = ripemd128(adler + struct.pack("<L", 0x3695)).hex()

print('fast_decrypt(DATA, KEY) = %s' % fast_hex)
print('key_info_key(12345678)   = %s' % k1_hex)
print()
print('// 把下面两行填回 tests/mdict_crypto_std_test.cpp')
print('fast vector : "%s"' % fast_hex)
print('key vector  : "%s"' % k1_hex)
