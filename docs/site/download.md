---
sidebar: false
---

# 下载

全部产物由**每日构建**自动产出，从滚动 nightly Release 分发（固定 tag `nightly`，每次清旧传新，assets 恒为最新一版）。

- **Release 页**：<https://github.com/cuihairu/unidict/releases/tag/nightly>（手机浏览器/GitHub App 均可达，匿名可下）
- **同域镜像**：<https://cuihairu.github.io/unidict/packages.html>（构建时间与全量包镜像）

## 直链表

| 产物 | 内容 |
|---|---|
| [unidict-daily.zip](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-daily.zip) | 全平台合装 + BUILD_INFO + VERIFY.md |
| [unidict-windows-x64-setup.exe](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-windows-x64-setup.exe) | Windows 安装器（Program Files + 开始菜单 + 卸载入口） |
| [unidict-windows-x64.zip](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-windows-x64.zip) | Windows 绿色包（GUI + CLI） |
| [unidict-macos-arm64.zip](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-macos-arm64.zip) | macOS Apple Silicon（GUI.app + CLI，先 `xattr -cr`） |
| [unidict-linux-x64.deb](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-linux-x64.deb) / [.rpm](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-linux-x64.rpm) | Linux x64 系统包（CLI） |
| [unidict-linux-arm64.deb](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-linux-arm64.deb) / [.rpm](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-linux-arm64.rpm) | Linux arm64 系统包（CLI） |
| [unidict-android.zip](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-android.zip) | Android release apk（R8） |

## 一键安装

::: code-group

```bash [Linux / macOS]
curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
```

```powershell [Windows]
irm https://raw.githubusercontent.com/cuihairu/unidict/main/install.ps1 | iex
```

:::

脚本自动识别操作系统与 CPU 架构，从 nightly Release 下载对应平台包安装；重跑一次即升级。Linux 上优先走发行版系统包（deb/rpm），卸载交给包管理器（`sudo apt remove unidict` / `sudo dnf remove unidict`）；Windows 安装器装到 `Program Files\Unidict`，开始菜单快捷方式齐备。

## 各平台安装

### Windows 10/11 x64

两种装法：

- **安装器**（推荐）：下载 `unidict-windows-x64-setup.exe` 双击安装——装到 `Program Files\Unidict`，开始菜单/桌面快捷方式、控制面板卸载入口齐全；装完开始菜单点 Unidict 启动，全程无控制台窗口。
- **绿色包**：解压 `unidict-windows-x64.zip` 直接双击 `unidict_qml.exe`，随包 dict.json 自动加载。

SmartScreen 拦截时点「更多信息 → 仍要运行」（每日构建未做代码签名）。

### macOS（Apple Silicon）

解压 `unidict-macos-arm64.zip`，清隔离属性后打开：

```bash
xattr -cr unidict_qml.app && open unidict_qml.app
```

bundle 内已带示例词典自动加载。未签名，Gatekeeper 会拦直接打开的 app，务必先 `xattr`。

### Linux

- **系统包**（推荐，deb 系 / rpm 系自动识别）：

  ```bash
  sudo apt install ./unidict-linux-x64.deb    # Debian/Ubuntu
  sudo dnf install ./unidict-linux-x64.rpm    # Fedora/RHEL
  ```

  装到 `/usr/bin/unidict_cli_std` + `/usr/share/unidict/dict.json`，卸载 `sudo apt remove unidict`。arm64 同理（`unidict-linux-arm64.*`）。

- **绿色包**：解压 `unidict-linux-<架构>.zip` 直接跑 `unidict_cli_std`（无 Qt 依赖）。

Linux 只分发 CLI 是既有决策：Qt 桌面应用没有依赖打包在别的机器跑不起来（glibc/Qt 版本地狱），桌面用户按[源码构建](/quickstart#源码构建)自建。

### Android

下载 `unidict-android.zip` 解出 apk 安装（未知来源按机型提示放行）。首启自动跑冒烟，状态行出 `M2-SMOKE-OK` 即正常；词典管理页用系统文件选择器（SAF）导入 `.ifo`/`.mdx` 等词典文件，查词页/生词本卡片可一键朗读（系统 TTS 引擎）。

## 装完先查一个词

::: code-group

```bash [Linux / macOS]
UNIDICT_DICTS=dict.json unidict_cli_std hello
```

```powershell [Windows]
set UNIDICT_DICTS=dict.json && unidict_cli_std.exe hello
```

:::

输出 `hello: A greeting or expression of goodwill.` 即 OK。CLI 的词典加载与检索用法见[查词](/search)，加载自己的词典见[词典管理](/dictionaries)。
