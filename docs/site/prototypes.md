# 界面原型

这里是桌面端四个主要页面的原型图。原型与实现共用一套设计令牌：品牌色 `#b11964`（暗主题 `#ff6fa9`）、圆角三档 4/8/12、卡片加描边、命令输入条用凹陷面。做视觉改动时先看这组图，改令牌时同步更新原型源文件重新出图。

原型是设计稿，不是运行截图。当前实际界面的截图见 [README](https://github.com/cuihairu/unidict) 与每日构建产物。

## 主检索页

左侧命令条输入即出补全，建议按「精确 / 前缀 / 词条」三段分组；右侧结果按词典分组出卡，多词典命中时先给命中词典一览 chip。

![主检索页亮色原型](./prototypes/assets/search_light.png)

![主检索页暗色原型](./prototypes/assets/search_dark.png)

## 词库管理页

目录导入、逐本词条数与格式、启用开关、全文索引状态、失败隔离与重试都在一页。禁用不删除文件。

![词库管理页亮色原型](./prototypes/assets/dictionaries_light.png)

![词库管理页暗色原型](./prototypes/assets/dictionaries_dark.png)

## 生词本页

标签筛选、卡片/表格双视图、笔记荧光笔多色（黄/蓝/绿/粉/橙），全部存本机。

![生词本页亮色原型](./prototypes/assets/vocabulary_light.png)

![生词本页暗色原型](./prototypes/assets/vocabulary_dark.png)

## 设置页

发音、外观、取词、同步四组，改动即时生效，无账号无云配置。

![设置页亮色原型](./prototypes/assets/settings_light.png)

![设置页暗色原型](./prototypes/assets/settings_dark.png)

## 源文件与再生成

- 原型源：[`prototypes/src/prototype.html`](https://github.com/cuihairu/unidict/blob/main/docs/site/prototypes/src/prototype.html)（单文件，亮暗两套令牌）
- 出图脚本：[`prototypes/src/render.js`](https://github.com/cuihairu/unidict/blob/main/docs/site/prototypes/src/render.js)（Playwright，四页 × 双主题共八张）
- 令牌出处：`qmlui/theme_tokens.h`
