<div align="center">
  <img src="../docs/logo.svg" width="120" alt="Unidict logo"/>
</div>

# Unidict 用户使用指南

Unidict 把你手里的 StarDict / MDict / DSL / JSON / EPUB / CSV 词典装进一个离线查询工具：六种检索模式、多词典聚合分组、本地 TTS 朗读、生词本与笔记。查词核心链路不发网络请求；唯一默认外发是在线发音（默认关，启用时仅发送查询词）。

## 快速开始

### 命令行（unidict_cli_std）

```bash
# 基本查词
unidict_cli_std -d /path/to/dict.mdx hello

# 前缀搜索
unidict_cli_std --mode prefix inter

# 模糊搜索
unidict_cli_std --mode fuzzy compuetr

# 环境变量词典列表（Windows 用 ';' 分隔）
export UNIDICT_DICTS="/path/to/dict1.mdx:/path/to/dict2.ifo"
unidict_cli_std hello
```

### 桌面图形界面（qmlui）

```bash
export UNIDICT_DICTS="/path/to/dict.mdx"
build/qmlui/unidict_qml
```

侧栏输入词条回车即查。结果页按词典折叠分组；内容 Tab 切换 例句 / 词组 / 近义联想 / 全文检索；「历史」「生词本」两个侧栏 tab 管理学习数据。生词本、历史、笔记集中在桌面 GUI，CLI 不提供这些入口，查词也不写入历史。

### 支持的词典格式

| 格式 | 扩展名 | 描述 | 示例 |
|------|----------|------|------|
| MDict | .mdx, .mdd | 加密词典需要密码（见下文加密词典） | `longman.mdx` |
| StarDict | .ifo, .idx, .dict | 开源格式，支持压缩 | `stardict.ifo` |
| DSL | .dsl | ABBYY Lingvo 词典格式 | `lingvo.dsl` |
| EPUB | .epub | 词典型 EPUB | `dict.epub` |
| JSON | .json | 自定义格式 | `mydict.json` |
| CSV/TSV | .csv, .tsv | 自定义格式 | `mydict.csv` |

## 搜索模式

### 精确匹配（exact）

```bash
unidict_cli_std --mode exact computer
```

词头完全一致才命中，走词表直查。确定拼写时用这个最快。

### 前缀搜索（prefix）

```bash
unidict_cli_std --mode prefix inter
```

匹配以输入开头的词头，适合补全。桌面端「自动」模式先走前缀，无结果再退到模糊。

### 模糊搜索（fuzzy）

```bash
unidict_cli_std --mode fuzzy compuetr
```

按编辑距离给出相近词头，容忍拼写错误。

### 通配符（wildcard）

```bash
unidict_cli_std --mode wildcard --pattern "te*t?"
```

`*` 匹配任意字符序列，`?` 匹配单个字符，`[abc]` 匹配字符集合，`[a-z]` 匹配字符范围。

### 正则（regex）

```bash
unidict_cli_std --mode regex --pattern "^inter.*et$"
```

### 全文搜索（fulltext）

```bash
unidict_cli_std --mode fulltext --pattern "machine learning"
```

在释义文本里检索，结果按相关性排序。索引可以落盘，二次查询明显变快（见下文索引与缓存）。

## 词典管理

```bash
# 查看已加载词典（含词条数）
unidict_cli_std --list-dicts-verbose

# 递归扫描目录并加载
unidict_cli_std --scan-dir /path/to/dictionaries

# 按名字移除已加载词典
unidict_cli_std --drop-dict "Longman"

# 查一个词都在哪些词典里
unidict_cli_std --where hello

# 精确命中时展示全部释义
unidict_cli_std --all hello

# 同时加载多个词典
unidict_cli_std -d dict1.mdx -d dict2.ifo -d dict3.json
```

## 加密词典

MDX 被标记为加密时按「尽力而为」解析；SimpleXOR 等变体需要密码。密码两种给法：

```bash
# 环境变量（对整个会话生效）
export UNIDICT_MDICT_PASSWORD="your-password"

# 或命令行参数（仅本次运行）
unidict_cli_std --mdict-password "your-password" secret_word
```

桌面端在侧栏输入密码后 Apply & Reload 即可（检测到加密词典时侧栏会出现提示行）。兼容说明：`UNIDICT_PASSWORD` 也会被当作 MDict 密码，后续可能移除。

## 索引与缓存

```bash
# 词条索引落盘与复用
unidict_cli_std --index-save my_index.db
unidict_cli_std --index-load my_index.db
unidict_cli_std --index-count

# 全文索引落盘、统计、校验
unidict_cli_std --fulltext-index-save ft_index.db
unidict_cli_std --fulltext-index-load ft_index.db
unidict_cli_std --ft-index-stats ft_index.db
unidict_cli_std --ft-index-verify ft_index.db

# 缓存
unidict_cli_std --cache-size
unidict_cli_std --cache-dir
unidict_cli_std --cache-prune-mb 500
unidict_cli_std --cache-prune-days 30
unidict_cli_std --clear-cache
```

大型词典建议先建索引再查询；全文检索配合落盘索引收益最大。

## 桌面界面（qmlui）

- **侧栏**：搜索框 + 模式下拉（自动/前缀/模糊/通配符/正则）+ 实时建议列表；「历史」「生词本」tab。生词卡上直接加标签、写笔记、按标签筛选、导出 CSV；生词本页的笔记搜索框可按笔记内容过滤（移动端在生词本筛选框里直接可搜笔记）；「导出笔记」按钮把全部词条笔记导成独立 HTML（.pdf 后缀则导 PDF）。
- **词条卡**：顶部轻入口为 朗读 / 收藏 / 复制 / 笔记 / 造句（✨ 造句走 AI 外部命令桥 `sentences` 子命令，未配置时输出自标 `[Mock sentences]` 的离线兜底）；释义按词典折叠分组。
- **内容 Tab**：例句 / 词组 / 近义联想 / 全文检索，点开时才取数。
- **工具抽屉**（头部「设置」按钮）：取词（剪贴板开关、轮询间隔、词长范围；划词取词开关——X11 下选中即查，其余平台不提供）、语音（发音源三态、口音、音量）、快捷键。
- **悬浮取词窗**：剪贴板取词或 quick_lookup 热键唤起，贴光标显示，失焦即收；窗内可朗读、加入生词本、回主窗打开。
- **快捷键**：Enter 搜索、Esc 清空搜索框、Ctrl+1/2/3 切 结果/历史/生词本；全局热键 Ctrl+Alt+U 当前仅 Windows 生效。

在线发音默认关闭；开启后仅向 dictionaryapi.dev 发送查询词本身，界面有明示文案。

发音评分（跟读、逐音素反馈）是独立实验功能：默认不构建，评分操作面在 gui（QWidget）版桌面，模型约 635MB 需自行下载，见 [pronunciation-plan](pronunciation-plan.md)。

## 跨平台安装

每日 05:17（北京时间）自动构建滚动 nightly Release（固定 tag `nightly`，匿名下载）：<https://github.com/cuihairu/unidict/releases/tag/nightly>

```bash
# Linux / macOS 一键安装（识别 OS/架构，覆盖安装即升级）
curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
```

```powershell
# Windows
irm https://raw.githubusercontent.com/cuihairu/unidict/main/install.ps1 | iex
```

Linux 另有 cli-std 的 deb/rpm 包。Android 原生壳已交付词典导入、查词与生词本/历史/笔记（TTS 与出包打磨未做）；iOS / HarmonyOS 未开始。

## 故障排除

**词典加载失败**：确认路径与格式受支持，再看详细列表：

```bash
unidict_cli_std -d your_dict.mdx --list-dicts-verbose
```

仍失败时用 `--scan-dir` 验证文件能否被识别。损坏词典会被隔离并在列表里标 `[FAILED]`，修复文件后重试即可恢复。

**搜索不到结果**：先查拼写，再换模式——不确定拼写用 `--mode fuzzy`，只知道开头用 `--mode prefix`，查释义内容用 `--mode fulltext`。

**MDict 打不开**：加密词典需要密码（见上文加密词典）。排查文件结构可用：

```bash
unidict_cli_std --mdx-debug encrypted.mdx
```

**内存占用高**：`--cache-prune-mb` 限制缓存，`--index-load` 复用索引减少常驻结构；同时加载的词典越多占用越大，按需裁剪 `UNIDICT_DICTS`。

## 扩展阅读

- [架构现状](CURRENT_ARCHITECTURE.md) / [功能矩阵](CURRENT_FEATURE_MATRIX.md)：当前真实实现状态
- [开发环境](dev.md) 与根目录 [ARCHITECTURE](../ARCHITECTURE.md)：构建与分层说明
- [贡献指南](../CONTRIBUTING.md)
- [roadmap](roadmap.md)：规划条目（部分条目与现状有漂移，以功能矩阵为准）
- 文档站：<https://cuihairu.github.io/unidict/>

## 获取帮助

```bash
unidict_cli_std --help
```

问题反馈走 [GitHub Issues](https://github.com/cuihairu/unidict/issues)。
