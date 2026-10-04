# 发音

## 本地 TTS（已上线）

- **桌面**：查词结果与生词本卡片点喇叭图标朗读，支持音色/语速/音调/音量调节、自动发音（查到即读）与语音预设；剪贴板取词+全局热键联动发音
- **Android**：系统 TextToSpeech 引擎朗读，卡片上一键播放；引擎缺失/初始化失败不挡其余功能（状态行 `M4-TTS-READY` / `M4-TTS-FAIL` 供诊断）

本地合成不发网络请求，发音不外发任何内容。

## 在线「全球发音」（已上线）

真实人声发音，支持口音切换。桌面与 Android 双端同一套语义，设置里三个入口：

### 发音源三态

| 模式 | 行为 | 网络失败时 |
| --- | --- | --- |
| **本地语音**（默认） | 走系统 TTS，不发网络请求 | — |
| **在线发音** | 只播在线人声 | 只报状态，不打断本地功能 |
| **自动（回落本地）** | 在线优先 | 自动回落本地 TTS |

默认是「本地语音」——开在线是显式动作，不会悄悄改。

### 口音

美音 / 英音 / 澳音，或「自动」。挑选规则：偏好命中优先，否则按美音 → 英音 → 澳音 → 未标注 → 首条的顺序兜底。口音从音频文件名里的 `-us` / `-uk` / `-au` 标记推断，文件名没标时看音标文本里的 `American` / `British` / `Australian` 标注。

### 接口：dictionaryapi.dev

用的是 [Free Dictionary API](https://dictionaryapi.dev/)（dictionaryapi.dev）——免密钥、无调用配额：

```
GET https://api.dictionaryapi.dev/api/v2/entries/en/<查询词>
```

响应里的 `phonetics[].audio` 是音频直链（多为 `.mp3`），`phonetics[].text` 是音标/口音标注。空 `audio` 自动跳过，同一 URL 自动去重。

换一家服务商不需要改 UI：`core/std/online_pron_std.h` 里的 `PronunciationSourceStd` 是纯虚接口（`name` / `request_url` / `parse_response` 三个函数），实现一个新源并在平台层注册即可。

### 隐私口径

- **默认关闭**：在线发音要在设置里手动选「在线发音」或「自动」才会启用
- **只发查询词**：请求 URL 里唯一外发内容就是你要读的那个词，不带历史、不带生词本、不带任何标识信息
- **开启时明示**：选了在线后，设置页当场显示这行说明，不用翻文档

桌面端走网络栈发请求，Android 端用系统 `HttpURLConnection`，两端走 HTTPS。

::: tip 差旅/离线场景
「自动（回落本地）」是给网络不稳用的：在线能取到就播人声，取不到（超时、无该词片段、服务不可达）自动回本地 TTS，不会出现「点了没声音」。
:::

## 发音评分（实验性）

本地离线发音评分（M3）：onnxruntime 声学模型 + CTC 强制对齐 + GOP 逐音素打分，供跟读练习。构建开关、模型下载与用法见 [docs/pronunciation-plan.md](https://github.com/cuihairu/unidict/blob/main/docs/pronunciation-plan.md)（模型约 635MB，不随包分发）。
