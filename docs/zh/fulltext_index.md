> 语言: **简体中文** | [English](../fulltext_index.md)

# 全文索引格式（UDFT）

本文档描述 Unidict 磁盘上的全文索引格式、其演进过程，以及迁移/兼容性指引。

术语
- 「FT 索引」：为已加载各词典的释义持久化的全文索引。
- 「签名」：把索引与确切的词典集合及其伴生文件（路径/大小/mtime）绑定的确定性摘要，用于防止误复用。

版本

1) UDFT1（legacy）
- 头部：`UDFT1`（5 字节）
- 无签名
- 朴素倒排记录：每个词项对应一组以固定 32 位整数写入的 `(docId, tf)` 对
- 状态：已废弃；出于向后兼容仍在加载时支持；当前版本不再写出

2) UDFT2（带签名）
- 头部：`UDFT2`
- 签名块：`u32 sig_len`，随后是 `sig_len` 字节的签名字符串
- 主体：与 UDFT1 相同（朴素倒排记录）
- 签名内容：`hex_fnv64|payload`，其中 payload 对每部词典编码
  - `name|word_count|first_word|last_word|`，以及对每个源/伴生路径：`path|size|mtime#...;`
  - 覆盖的伴生文件：StarDict 的 `.ifo`、`.idx`、`.dict`/`.dict.dz`；MDict 的 `.mdx` 及同词干的关联 `.mdd` 文件
- 用途：严格校验 FT 索引与当前词典集匹配。生产环境推荐。

3) UDFT3（压缩 + 带签名）
- 头部：`UDFT3`
- 签名块：与 UDFT2 相同
- 倒排记录：按词项压缩
  - 对含 `n` 条倒排的词项，写入 `u32 n`，再写 `u32 comp_len`，随后 `comp_len` 字节
  - 编码：按 `docId` 排序，对 `docId` 做差分编码，对 `doc_delta` 与 `tf` 做 varint 编码（类 LEB128）
- 收益：索引体积更小；加载端透明支持 UDFT1/2/3

兼容性与模式

- strict（严格）：只接受 UDFT2/3 并强制签名匹配；拒绝 legacy UDFT1 或签名不匹配
- auto（默认）：先尝试 strict；失败后尝试无签名加载 UDFT1（打印一条提示）；对签名不匹配的 UDFT2/3 仍然拒绝
- loose（宽松）：接受任意版本并忽略签名（打印警告）。仅用于临时抽查。

CLI 标志（std CLI）

- 加载/保存
  - `--fulltext-index-save <file>`：写出 UDFT3
  - `--fulltext-index-load <file>`：加载 UDFT1/2/3
  - `--ft-index-compat strict|auto|loose`：兼容模式（默认：`auto`）

- 升级
  - 单文件：`--ft-index-upgrade-in <old> --ft-index-upgrade-out <new>`
  - 批量：`--ft-index-upgrade-dir <dir>`（递归）
    - `--ft-index-upgrade-suffix <suf>`（默认：`.v2`）
    - `--ft-index-out-dir <dir>`：写到独立根目录，镜像输入目录树
    - `--ft-index-filter-ext .index,.idx`：只处理匹配的扩展名
    - `--ft-index-force`：覆盖已有输出
    - `--ft-index-dry-run`：只展示动作/签名 hex 前缀，不写盘

建议

- 生产使用
  - 使用 `strict` 模式，防止过期/不匹配的 FT 索引被复用
  - 大词典定期保存 UDFT3，加快下次启动

- 迁移
  - 旧 FT 索引：用单文件或批量方式升级到 UDFT3
  - 升级前务必先加载同一词典集（含路径），使新签名与实际情况相符

- 故障排查
  - 加载失败时，检查 `--ft-index-compat`，并确认词典文件/伴生文件未被改动（路径/大小/mtime）
  - 快速验证可用 `loose`，但正常工作流中应立即重新生成带签名的索引
