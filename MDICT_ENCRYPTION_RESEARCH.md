# MDict 加密支持

> ⚠️ **本文档此前记错了加密方式。** 初版把 MDX 的"加密"写成 DES / Blowfish / AES，
> 那是凭空推测，真实 MDX 里根本不存在这三种。下面是核对公开规范与参考实现后
> 的结论。

## 官方 MdxBuilder 产出的加密 MDX 只有两种

判定依据是 `.mdx` 头里的 `Encrypted` 属性（值是十进制位标志）：

| `Encrypted` | 含义 | 密钥来源 |
|---|---|---|
| `0` / 缺省 / `No` | 未加密 | — |
| `1` | 加密 **record block** | 用户在 MdxBuilder 填的 "Encryption Key"，由注册码+设备码派生 |
| `2` | 加密 **key block info** | **固定密钥，不需要用户输入** |

`Encrypted="2"`（导出开关未勾选时产生）是现实里最常见的"加密 MDX"，而且
**完全不需要用户输密码**——这类文件此前在本项目里是解不开的。

## 块信息字

每个数据块（key block / record block / key block info）的头 4 字节是块信息
字（**小端** 32 位）：

```
info = LE32(block[0:4])
compression_method = info & 0x0F          // 0 原样 / 1 LZO / 2 zlib
encryption_method  = (info >> 4) & 0x0F   // 0 无 / 1 半字节 XOR / 2 Salsa20
encryption_size    = (info >> 8) & 0xFF   // 只加密前 N 字节，其余原样
adler32            = BE32(block[4:8])     // 解密正确性的权威判据
data               = block[8:]
```

## 密钥派生

两条路径都用 **RIPEMD-128**：

- key block info（不需要密码）：
  `key = RIPEMD128( adler32 字节 ‖ LE32(0x3695) )`
- 单个数据块（用户未提供 key 时）：
  `key = RIPEMD128( 该块自带的 4 字节 adler32 )`

`encryption_method == 2`（Salsa20）与注册码路径都需要 Salsa20 + 用户提供
regcode + userid，**本项目尚未实现**，遇到时如实报错而不是猜。

## 密码（cipher）

`encryption_method == 1` 不是简单 XOR，是半字节交换的链式流水：

```
previous = 0x36
for i in range(len(data)):
    t = ((b[i] >> 4) | (b[i] << 4)) & 0xFF      # 交换高/低半字节
    t = t ^ previous ^ (i & 0xFF) ^ key[i % len(key)]
    previous = b[i]                              # 链的是**密文**原字节
    out[i] = t
```

因为链的是密文字节，这是**非对称**的：往返必须 `fast_encrypt` → `fast_decrypt`，
`fast_decrypt` 作用两次不等于原值。

## 为什么自己实现 RIPEMD-128

`core/std` 要求无 Qt、无外部加密库；而且 OpenSSL 3.x 的默认 provider 里已经
**没有** RIPEMD-128（实测本机 `openssl dgst -ripemd128` 报 Unknown option，
Python `hashlib` 也只剩 `ripemd160`）。依赖它等于依赖一个绝大多数发行版都不
提供的算法。

实现照公开规范 `rmd128.txt` 的伪代码写成，由规范测试向量锚定。

## 正确性怎么保证

解密是否成功**由 adler32 判定**——块头里就带着期望值，比任何"看起来像明文"
的启发式都可靠。

⚠️ 不要用启发式判断（旧实现就是这么做的）：`MdictDecryptorStd::detect_encryption_type()`
按熵值猜加密类型，而 `decrypt_simple_xor()` 是一套自造算法，与真实 MDX 无关，
**永远解不开任何一份真实的加密 MDX**。该文件保留仅作兼容，新代码请用
`core/std/mdict_crypto_std.h`。

## 代码位置

| 文件 | 作用 |
|---|---|
| `core/std/ripemd128_std.{h,cpp}` | RIPEMD-128 |
| `core/std/mdict_crypto_std.{h,cpp}` | 块信息字解析、密钥派生、fast_decrypt/fast_encrypt、decrypt_block、adler32 |
| `tests/mdict_crypto_std_test.cpp` | 规范向量 + 独立 Python 实现交叉验证 + 边界用例 |
| `scripts/gen_mdict_crypto_vectors.py` | 算测试里的独立期望值 |

## 还没做的

- Salsa20（`encryption_method == 2`）与 regcode+userid 密钥派生
- LZO 解压（`compression_method == 1`，MDX v1 老格式用）
- MDX v1/v2 的**确定性**块布局解析（当前 `mdict_parser_std.cpp` 仍在用容器嗅探
  + zlib 扫描的启发式路径，真实布局尚未实现）
