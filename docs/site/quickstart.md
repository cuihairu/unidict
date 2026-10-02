# 快速上手

三步：**装**（[下载](/download)页一键脚本或安装包，两分钟）→ **冒烟验证** → 查自己的词。开发/定制走[源码构建](#源码构建)。

## 安装

各平台一键安装命令、安装包直链与分平台步骤集中在[下载](/download)页。最短路径（Linux/macOS）：

```bash
curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
```

桌面 QML 应用双击即用，随包的示例词典自动加载；CLI 装完按下面冒烟一句验证。

## 冒烟验证

::: code-group

```bash [Linux / macOS]
UNIDICT_DICTS=dict.json unidict_cli_std hello
# 预期输出:
# hello: A greeting or expression of goodwill.
```

```powershell [Windows]
set UNIDICT_DICTS=dict.json && unidict_cli_std.exe hello
```

:::

安装位置与各平台差异见[下载](/download)。

## 源码构建

要求 CMake 3.16+、C++17 编译器、zlib。核心开发推荐 std-only 构建（无 Qt 依赖，编译快）：

```bash
cmake -B build-std -S . \
  -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF
cmake --build build-std -j
ctest --test-dir build-std -R _std --output-on-failure
```

Qt 全量构建（桌面 QML 应用、Widgets 演示、Qt 测试）需要 Qt 6.10+：

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 下一步

- 用自己的词典：[词典管理与导入格式](/dictionaries)
- 换着花样查：[查词用法](/search)
- 查过的词留下来：[生词本](/vocabulary)
