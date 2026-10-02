# 内置词典署名与许可

## ccedict-zh-en.json

- **数据来源**：[CC-CEDICT](https://www.mdbg.net/chinese/cedict/)，
  MDBG（https://www.mdbg.net/）维护的社区中文-英文词典。
- **源版本**：2026-10-01 导出的 `cedict_1_0_ts_utf-8_mdbg`
  （125,195 行 / 125,166 个简体词头）。
- **许可**：[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)。
  本文件是 CC-CEDICT 的改编产物（格式转换），以同一许可
  CC BY-SA 4.0 分发；随包分发时保留本署名文件即满足署名要求。
- **转换方式**：`tools/build_ccedict_dict.py`（入库），
  逐行 `繁體 简体 [拼音] /释义/` → `{"word": 简体, "definition": "[拼音] 释义"}`。
- **既定取舍**：内置版仅取**简体词头**（繁体查询不命中）；需要繁体或
  完整版的用户可从上列地址自行导入 CC-CEDICT 源文件。

## 许可文本

Creative Commons Attribution-ShareAlike 4.0 International 的完整许可文本
见 https://creativecommons.org/licenses/by-sa/4.0/legalcode 。
