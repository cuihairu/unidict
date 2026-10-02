# 词典管理与导入格式

## 导入格式矩阵

| 格式 | 扩展名 | 说明 |
|---|---|---|
| StarDict | `.ifo/.idx/.dict(/.dict.dz)` | 中英/双语工具书最常见；`.dz` 压缩与损坏文件防御均支持 |
| MDict | `.mdx`（+`.mdd` 资源） | 新华字典、现代汉语词典这类中文工具书的主流分发格式；多种块布局、SimpleXOR 加密、`.mdd` 内图片/音频资源、GB18030 编码词典均支持 |
| DSL | `.dsl` | AbaDict 系词典 |
| JSON / CSV / TSV / 纯文本 | 见下 | 自制词库，UTF-8 |
| EPUB | `.epub` | 词典类电子书 |

加密 MDict 给密码两种方式：参数 `--mdict-password <pw>` 或环境变量 `UNIDICT_MDICT_PASSWORD`。

自制词库格式：JSON（词→释义对象数组）、CSV/TSV（首列词，后列释义）、纯文本（词与释义按约定分隔）。示例见仓库 `examples/dict.json`。

## 加载词典：环境变量

CLI 与源码构建的桌面 GUI 都认环境变量：

```bash
UNIDICT_DICTS="dict.json" unidict_cli_std hello            # 单本
UNIDICT_DICTS="a.ifo:b.mdx:/path/c.mdx" unidict_cli_std …  # 多本（路径分隔符冒号）
```

常用环境变量一览：

| 变量 | 作用 |
|---|---|
| `UNIDICT_DICTS` | 词典列表（多路径分隔） |
| `UNIDICT_DICT_DIR` | 词典目录（整目录扫描） |
| `UNIDICT_DATA_DIR` / `UNIDICT_CACHE_DIR` | 数据与缓存目录 |
| `UNIDICT_MDICT_PASSWORD` | MDict 默认密码 |

## 桌面 GUI

启动时同目录有 `dict.json` 会自动加载（每日构建包如此）。词典管理入口支持导入文件与扫描目录；词典的启停、排序、启用状态自动持久化，重启恢复。

## Android

词典管理页：系统文件选择器（SAF）导入 `.ifo/.mdx` 等文件到应用私有目录，支持启用/停用、删除、词量查看。首启自带三格式演示词典并自动冒烟（`M2-SMOKE-OK`）。

## 中文工具书现状

装新华字典/现代汉语词典这类中文词典：`.mdx` 格式**现在就能装**。中文特有检索面（拼音检索、部首/笔画、繁简转换）现状与路线见[服务端规划 · 中文格式面](/server#中文词典格式面)。
