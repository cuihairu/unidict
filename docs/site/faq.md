# 常见问题

## 安装

### Windows 双击被 SmartScreen 拦

每日构建未做代码签名：点「更多信息 → 仍要运行」。安装器版同理。

### macOS 打不开（"已损坏"或 Gatekeeper 拦截）

未签名应用必须先清隔离属性：`xattr -cr unidict_qml.app && open unidict_qml.app`。从浏览器下载的包通常带 quarantine 标记，这一步不能省。

### Linux 为什么只有 CLI 包？

Qt 桌面应用没有依赖打包在别的机器跑不起来（glibc/Qt 版本地狱）——每日构建只出无 Qt 依赖的 CLI（zip/deb/rpm）；桌面 GUI 按[源码构建](/quickstart#源码构建)自建。deb/rpm 的系统包基于 ubuntu-24.04 runner 构建（glibc 2.39+），更老的发行版请用源码构建。

### 一键安装脚本报"不支持的 CPU 架构"

支持 x86_64/arm64(aarch64)。Intel Mac 不在每日构建矩阵里（Qt 6.10 起不再分发 Intel macOS 库）；其他架构请到仓库 issues 提需求并附 `uname -m` 输出。

## 词典

### 加密的 .mdx 怎么给密码

`--mdict-password <pw>` 参数或 `UNIDICT_MDICT_PASSWORD` 环境变量。

### 词典放哪、怎么多本一起用

任意路径，用 `UNIDICT_DICTS` 指向文件（多本用路径分隔符连起来）或 `UNIDICT_DICT_DIR` 指向目录；桌面 GUI 导入后顺序与启停自动记住。见[词典管理](/dictionaries)。

### 中文词典（新华字典这类）能装吗

能——`.mdx` 主流中文工具书格式直接装。拼音检索/部首笔画等中文特有面在[路线图](/server#里程碑)。

## 分发与隐私

### GitHub App 里看不到产物

GitHub **App** 不显示 Actions artifacts；用手机浏览器开 [nightly Release](https://github.com/cuihairu/unidict/releases/tag/nightly)（匿名可达），或本站[下载页](/download)。

### nightly Release 是什么版本

不是版本号——滚动每日构建（固定 tag `nightly`，每次清旧传新）。正式发版见 roadmap。

### 隐私与数据

数据外发默认关闭、显式开启、开启时明示范围：本地 TTS/查词/生词本/历史不发网络请求；[在线发音](/pronunciation)只把查询词发给 dictionaryapi.dev、默认关闭、开启时设置页当场明示，不带历史与生词本；生词本/查词历史不上云。CSV 导出随时可带走你的数据。
