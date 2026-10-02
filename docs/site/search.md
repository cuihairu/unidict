# 查词

Unidict 的检索有六种模式，CLI 与桌面 GUI 同一套引擎；GUI 里还多一层**聚合查询**——一次出多本词典的释义。

## CLI：模式怎么选

::: code-group

```bash [精确]
# 默认模式：词形完全匹配
unidict_cli_std -d dict.mdx hello
```

```bash [前缀]
# 给开头就往下补——适合边输边看候选
unidict_cli_std -d dict.mdx -m prefix -p inter
```

```bash [模糊]
# 拼写接近的词都出来——记不清拼写时用
unidict_cli_std -d dict.ifo -m fuzzy -p helo
```

```bash [全文]
# 在释义正文里搜，倒排索引 + TF/IDF 排序
unidict_cli_std -d dict.mdx -m fulltext --pattern "annual meeting"
```

```bash [通配/正则]
unidict_cli_std -d dict.mdx --wildcard "hel*o"
unidict_cli_std -d dict.mdx --regex "^h.*o$"
```

:::

`-d` 可给 StarDict 的 `.ifo`、MDict 的 `.mdx`、DSL、JSON、CSV 等任意支持格式（见[格式矩阵](/dictionaries)）。

## 桌面 GUI

左栏搜索框 + 「自动」模式下拉框：切精确/前缀/模糊/全文等模式；查询区顶部的 结果 / 历史 / 生词本 三个页签在查词、翻记录、存生词之间切换。聚合结果按词典分组展示，每条释义带 朗读 / 收藏 / 复制 操作与前后翻页（词条内跳转历史）。

## 词典顺序与多词典

一次可加载多本词典（`UNIDICT_DICTS` 冒号或分号分隔多路径，或 GUI 里导入多个文件），聚合查询按词典优先级合并；桌面 GUI 里词典的启用状态与顺序自动持久化，下次启动恢复。

## 检索细节速查

- 中文查询走全角→半角、全角标点归一；拉丁词走重音折叠（`café` ↔ `cafe`）
- 全文检索对中文按 CJK 逐字 + 二字组合切分（更细的分词改进在[路线图](/server#里程碑)）
- CLI 定位纯查词与诊断：不写历史、没有生词本入口；学习管理集中在桌面 GUI 与 Android
