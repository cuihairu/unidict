# Unidict 用户使用指南

欢迎使用Unidict - 下一代的通用词典查找工具！

## 🚀 快速开始

### 基本使用方法

#### 方法一：命令行界面（CLI）
```bash
# 基本查词
unidict_cli -d /path/to/dict.mdx hello

# 前缀搜索
unidict_cli --mode prefix -d /path/to/dict.mdx inter

# 模糊搜索
unidict_cli --mode fuzzy -d /path/to/dict.mdx hello

# 环境变量设置
export UNIDICT_DICTS="/path/to/dict1.mdx:/path/to/dict2.ifo"
unidict_cli word
```

#### 方法二：图形界面（QML）
```bash
# 设置词典环境变量
export UNIDICT_DICTS="/path/to/dict.mdx"

# 启动图形界面
./build/qmlui/unidict_qml
```

### 支持的词典格式

| 格式 | 扩展名 | 描述 | 示例 |
|------|----------|------|------|
| **MDict** | .mdx, .mdd | 最流行的词典格式，支持加密 | `longman.mdx` |
| **StarDict** | .ifo, .idx, .dict | 开源格式，支持压缩 | `stardict.ifo` |
| **DSL** | .dsl | 专用词典格式，功能强大 | `lingvo.dsl` |
| **JSON** | .json | 简单的自定义格式 | `mydict.json` |

## 🔍 搜索模式详解

### 1. 精确匹配（exact）
```bash
# 查找完全匹配的单词
unidict_cli --mode exact computer

# 环境变量形式
UNIDICT_DICTS="dict.mdx" unidict_cli --mode exact computer
```

**特点**：
- 最高效的搜索方式
- 适用于确切的单词拼写
- 时间复杂度：O(log n)

### 2. 前缀搜索（prefix）
```bash
# 查找以"inter"开头的所有单词
unidict_cli --mode prefix inter

# 显示前10个结果
unidict_cli --mode prefix inter --max-results 10
```

**特点**：
- 自动补全功能
- 适用于输入不完整的单词
- 时间复杂度：O(log n)

### 3. 模糊搜索（fuzzy）
```bash
# 使用编辑距离算法
unidict_cli --mode fuzzy compuetr

# 实时显示建议
unidict_cli --mode fuzzy --suggest live
```

**特点**：
- 容忍拼写错误
- 智能纠错建议
- 时间复杂度：O(log n)

### 4. 通配符搜索（wildcard）
```bash
# 使用Shell风格的通配符
unidict_cli --mode wildcard "inter*"

# 正则表达式支持
unidict_cli --mode regex "inter.*et"
```

**支持的通配符**：
- `*` - 匹配任意字符序列
- `?` - 匹配单个字符
- `[abc]` - 匹配字符集合
- `[a-z]` - 字符范围

### 5. 全文搜索（fulltext）
```bash
# 在词典定义中搜索关键词
unidict_cli --mode fulltext "machine learning"

# 使用自定义模式
unidict_cli --mode fulltext --pattern "learn*"
```

**高级功能**：
- 支持TF/IDF相关性排序
- 自动索引持久化
- 时间复杂度：O(m log n)，其中m是平均词条长度

## 🎛 高级功能

### 词典管理

#### 查看已加载的词典
```bash
# 简单列表
unidict_cli --list-dicts

# 详细信息（包含词条数量）
unidict_cli --list-dicts-verbose
```

#### 批量加载词典
```bash
# 同时加载多个词典
unidict_cli -d dict1.mdx -d dict2.ifo -d dict3.json

# 使用环境变量加载多个词典
export UNIDICT_DICTS="dict1.mdx:dict2.ifo:dict3.json"
```

#### 扫描目录中的词典
```bash
# 递归扫描目录
unidict_cli --scan-dir /path/to/dictionaries

# 列出扫描到的词典
unidict_cli --scan-dir /path/to/dictionaries --list-dicts
```

### 生词本功能

#### 保存查词记录
```bash
# 将查询结果保存到生词本
unidict_cli --mode exact computer --save

# 查看生词本
unidict_cli --show-vocab

# 导出生词本为CSV
unidict_cli --export-vocab vocabulary.csv
```

#### 查看查询历史
```bash
# 查看最近20次查询
unidict_cli --history

# 查看最近50次查询
unidict_cli --history 50
```

### 索引优化

#### 索引保存和加载
```bash
# 保存索引以提高下次启动速度
unidict_cli --index-save my_index.db

# 使用保存的索引
unidict_cli --index-load my_index.db

# 查看索引中的单词数量
unidict_cli --index-count

# 导出索引中的单词
unidict_cli --dump-words 100
```

#### 全文索引优化
```bash
# 保存全文索引
unidict_cli --mode fulltext --fulltext-index-save ft_index.db

# 使用全文索引
unidict_cli --mode fulltext --fulltext-index-load ft_index.db

# 查看全文索引统计
unidict_cli --ft-index-stats ft_index.db
```

### 加密词典支持

#### SimpleXOR加密
```bash
# SimpleXOR解密（自动检测）
export UNIDICT_DICTS="encrypted.mdx"
unidict_cli --mode exact secret_word

# 如果自动检测失败，可以手动指定
# （目前支持的算法在开发中）
```

#### 密码管理
```bash
# 注意：密码管理功能正在开发中
# 建议使用临时环境变量或配置文件
export UNIDICT_PASSWORD="your_password"
```

## 🛠️ 配置和优化

### 缓存管理

#### 查看缓存信息
```bash
# 查看缓存大小
unidict_cli --cache-size

# 查看缓存目录
unidict_cli --cache-dir

# 清理缓存
unidict_cli --clear-cache

# 限制缓存大小（MB）
unidict_cli --cache-prune-mb 500

# 清理过期缓存（天数）
unidict_cli --cache-prune-days 30
```

### 性能优化建议

#### 启动优化
1. **使用索引文件**：
   ```bash
   # 创建索引
   unidict_cli --index-save startup_index.db

   # 使用索引启动
   unidict_cli --index-load startup_index.db
   ```

2. **预加载常用词典**：
   ```bash
   # 将常用词典放在环境变量前面
   export UNIDICT_DICTS="frequent.mdx:rare.ifo:custom.json"
   ```

#### 搜索优化
1. **使用合适搜索模式**：
   - 确切单词 → `--mode exact`
   - 不完整单词 → `--mode prefix`
   - 不确定拼写 → `--mode fuzzy`

2. **限制结果数量**：
   ```bash
   # 显示前20个结果，提高响应速度
   unidict_cli --mode prefix inter --max-results 20
   ```

3. **全文搜索优化**：
   ```bash
   # 保存全文索引
   unidict_cli --mode fulltext --fulltext-index-save ft.db

   # 使用索引进行全文搜索
   unidict_cli --mode fulltext --fulltext-index-load ft.db
   ```

## 🎨 用户界面

### QML界面操作

#### 基本操作
1. **输入框**：直接输入要查询的单词
2. **搜索按钮**：执行搜索或按Enter键
3. **建议列表**：实时显示匹配的单词
4. **结果显示**：显示查询的详细定义
5. **生词本按钮**：将当前查询加入生词本
6. **模式选择**：切换不同的搜索模式

#### 快捷键
| 操作 | 快捷键 | 描述 |
|------|----------|------|
| 搜索 | Enter | 执行搜索 |
| 清除 | Esc | 清空输入框 |
| 下一个建议 | ↓ | 选择下一个建议 |
| 上一个建议 | ↑ | 选择上一个建议 |
| 切换模式 | Tab | 在不同搜索模式间切换 |

#### 右键菜单
- **复制定义**：复制选中的文本
- **添加到生词本**：快速保存生词
- **查看词典信息**：显示当前词典详情
- **搜索历史**：查看最近的搜索记录

## 📱 跨平台使用

### Windows

#### 安装
```bash
# 使用安装包
./Unidict-1.0.0-Windows.exe /S

# 或手动解压
unzip Unidict-1.0.0-Windows.zip -d C:\Unidict
```

#### 文件关联
- `.mdx` - MDict词典文件
- `.ifo` - StarDict词典文件
- `.dsl` - DSL词典文件
- `.json` - 自定义词典文件

#### 命令提示符
```cmd
# 添加到系统PATH（自动完成安装）
set PATH=%PATH%;C:\Program Files\Unidict\bin

# 快速启动
unidict_cli computer
```

### macOS

#### 安装
```bash
# 使用DMG安装包
open Unidict-1.0.0-macOS.dmg

# 拖拽安装
# 将Unidict.app拖拽到Applications文件夹
```

#### 服务集成
```bash
# 系统词典集成
osascript -e 'tell application "Unidict" to lookup "computer"'

# 命令行工具
unidict_cli computer
```

### Linux

#### 安装
```bash
# 使用DEB包（Ubuntu/Debian）
sudo dpkg -i unidict_1.0.0_amd64.deb

# 使用RPM包（Fedora/CentOS）
sudo rpm -i unidict-1.0.0-1.x86_64.rpm

# 使用AppImage（通用）
chmod +x Unidict-1.0.0-x86_64.AppImage
./Unidict-1.0.0-x86_64.AppImage
```

#### 系统集成
```bash
# 创建系统别名
echo 'alias dict="unidict_cli"' >> ~/.bashrc
source ~/.bashrc

# 桌面文件（可选）
cp /usr/share/applications/unidict.desktop ~/Desktop/
```

## 🔧 故障排除

### 常见问题

#### 词典加载失败
**问题**：无法加载词典文件
**解决方案**：
1. 检查文件路径是否正确
2. 确认文件格式受支持
3. 检查文件权限
4. 使用`--scan-dir`验证文件识别

```bash
# 验证词典文件
unidict_cli -d your_dict.mdx --list-dicts-verbose
```

#### 搜索结果为空
**问题**：搜索不到任何结果
**解决方案**：
1. 检查单词拼写
2. 尝试模糊搜索模式
3. 使用前缀搜索
4. 确认词典包含该单词

```bash
# 尝试不同搜索模式
unidict_cli --mode exact word
unidict_cli --mode fuzzy word
unidict_cli --mode prefix word
```

#### 内存使用过高
**问题**：程序占用内存过多
**解决方案**：
1. 使用索引文件限制内存使用
2. 启用缓存清理
3. 限制同时打开的词典数量
4. 使用`--cache-prune-mb`限制缓存大小

```bash
# 限制缓存大小
unidict_cli --cache-prune-mb 200
```

#### 加密词典问题
**问题**：无法打开加密词典
**解决方案**：
1. 确认拥有解密权限
2. 检查密码是否正确
3. 验证词典格式支持
4. 联系技术支持

```bash
# 查看解密错误信息
unidict_cli --debug  # 启用详细错误信息
```

### 性能优化

#### 大型词典优化
```bash
# 1. 创建索引文件
unidict_cli -d large_dict.mdx --index-save large_index.db

# 2. 使用索引启动
unidict_cli --index-load large_index.db

# 3. 限制结果数量提高响应速度
unidict_cli --mode prefix word --max-results 10
```

#### 多词典并行搜索
```bash
# 1. 设置多个词典
export UNIDICT_DICTS="dict1.mdx:dict2.mdx:dict3.mdx"

# 2. 使用全文索引进行快速搜索
unidict_cli --mode fulltext --fulltext-index-save combined_index.db
```

## 🔌 技巧和诀窍

### 搜索技巧

1. **使用前缀搜索**：输入部分单词，利用前缀搜索的高效性
2. **组合搜索模式**：先用精确匹配，无结果时自动使用模糊搜索
3. **利用通配符**：使用`*`和`?`进行模式匹配
4. **全文搜索**：查找特定概念或短语，而不仅仅是单词

### 词典选择建议

1. **专业词典优先**：将专业词典放在环境变量前面
2. **语言搭配**：同时加载不同语言的词典进行比较
3. **主题分类**：按主题（计算机、医学、法律等）分类词典

### 学习效率优化

1. **定期生词复习**：使用`--show-vocab`查看生词本，定期复习
2. **导出生词本**：使用`--export-vocab`导出为Anki等格式
3. **查询历史分析**：使用`--history`查看查询模式，发现学习重点

## 📚 扩展阅读

### 开发者文档
- [API文档](../core/) - 详细的API参考
- [插件开发](PLUGIN_DEVELOPMENT.md) - 创建自定义词典格式
- [贡献指南](../CONTRIBUTING.md) - 参与项目开发

### 社区资源
- [GitHub仓库](https://github.com/unidict/unidict) - 源码和问题反馈
- [发布页面](https://github.com/unidict/unidict/releases) - 最新版本下载
- [Wiki文档](https://github.com/unidict/unidict/wiki) - 详细教程和FAQ

## 🆘 获取帮助

### 命令行帮助
```bash
# 查看完整的命令行帮助
unidict_cli --help

# std-only版本帮助
./build/cli-std/unidict_cli_std --help
```

### 在应用内获取帮助
- 点击界面中的"？"按钮
- 查看工具菜单中的帮助选项
- 访问在线文档和社区论坛

---

**开始使用Unidict，探索知识的无限可能！**

*本指南涵盖Unidict的主要功能。更多详细信息和更新，请访问项目文档。*