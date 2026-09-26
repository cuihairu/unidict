# 发音练习计划（发音纠正 · 跟读 · 全球发音）

> 状态：规划中（2026-09）。本文档是方向定稿与分级拆解，不是实现承诺。
> 关联：roadmap「Voice Features → Pronunciation practice & scoring」。

## 背景与目标

AI 语音这几年的进展让"离线发音纠正"第一次成为个人项目可行的事：
开源 ASR 模型（zipformer/paraformer 系）+ ONNX 推理运行时已经小到能随词典
应用分发，音素级打分（GOP 系算法）不需要云端也能做到可用精度。

Unidict 的发音练习要解决三件事：

1. **发音纠正**——用户读一个词/句子，本地给出音素级评分与差异定位
   （哪个音发偏了），不是只给一个笼统的百分制。
2. **跟读模式**——TTS 示范 → 用户跟读 → 评分 → 再来的完整闭环，
   面向"跟着词典学发音"的日常使用。
3. **全球发音**——同一个词在不同口音（美/英/澳等）下的示范、音标与
   评分参考都跟着口音走，而不是默认全世界都读美音。

铁律不变：**离线优先**。核心评分必须本地可跑，云端 AI 只能是可选增强
（走现有 AI 外部命令桥接模式，不硬集成任何云 SDK）。

## 技术选型

### 评分引擎：本地 ONNX 方案为主

| 方案 | 精度 | 离线 | 体积 | 结论 |
|------|------|------|------|------|
| Azure 发音评测 | 最好 | ✗ | 0 | 违背离线优先，排除为"主"方案 |
| Kaldi + GOP | 好 | ✓ | 大、链路重 | 工程成本过高，不选 |
| **sherpa-onnx + 开源 CTC 模型做对齐 + GOP 打分** | 可用 | ✓ | 模型 50-200MB | **选它** |
| 纯自研（声学模型自己训） | — | ✓ | — | 个人项目不现实，不选 |

定案思路：sherpa-onnx 是 Apache 2.0 的纯推理运行时（k2-fsa 生态，
C++ API 干净，跨 Win/macOS/Linux），它本身不带发音评测，但提供
CTC ASR 模型与 force-alignment 能力；经典 **GOP（Goodness of
Pronunciation）** 算法——先强制对齐拿到每个音素的帧段，再比对
"目标音素 vs 实际声学似然"——在这一层自己实现，纯算法代码可以进
`core/` 单测。也就是：**对齐用现成运行时，打分算法自持**。

云端增强（远期可选）：同一段录音可以丢给外部 AI 命令桥接做"更有人味
的点评"，架构上只是评测接口的另一个实现，不阻塞主线。

### 录音采集：Qt Multimedia

- `QAudioSource`，16kHz / 16bit / 单声道 PCM——语音模型的标准输入，
  采集层就按这个规格录制，不做花活。
- 麦克风权限：Windows 走系统设置（首次访问系统弹窗）、macOS 需要
  Info.plist 加 `NSMicrophoneUsageDescription`（现在还没做 mac 打包，
  先在文档里记着）。
- **CI offscreen 环境没有音频设备**：采集/播放代码全部锁在 GUI 层，
  不进 `core/`，测试只测纯逻辑（见架构衔接）。

### TTS：沿用 Qt TextToSpeech

跟读的"示范音"直接用现有 TTS 管线（qmlui 已接，gui 未接——计划把
发音练习面板先落在 Qt Widgets GUI，届时补上）。系统 TTS 引擎自带的
多 locale voice 就是"全球发音"的第一层示范源。

### 分层架构（对齐 core 无重依赖原则）

```
core/std/            评分算法（纯逻辑）：GOP 打分、音素映射、评分归一化
adapters/pron/       sherpa-onnx 适配器：模型加载、force-alignment
gui/                 发音练习面板：录音、回放、波形、评分展示、跟读循环
```

- `core/` 只见接口（如 `PronunciationScorer` 纯虚）与纯算法，不拉
  ONNX 头文件——和 core 无 Qt 同一个纪律：重依赖走适配器。
- 模型文件**不进 git**（与词典资产同一铁律），运行时放在数据目录，
  首次使用引导下载（来源写进文档，校验哈希）。
- CI 只测 `core/` 纯逻辑与接口 mock；适配器层做编译验证不跑模型。

## 分级里程碑

**M1 录音基建**（已完成 2026-09-24，d82a4a2）
QAudioSource 封装（开始/停止/取 PCM）+ 简单波形绘制 + 录音回放。
这一步独立有价值：为将来"语音查词"（voice search）也打地基。
落地纪律：纯逻辑（pcm_util.h 波形降采样 + WAV 封装/解析）与 Qt 壳
物理分离，只有纯逻辑进测试——Qt 全量/std-only 双形态 ctest 均含
pcm_util 9 组用例；采集用 pull 模式 + 40ms 定时器轮询（QIODevice
readyRead 时序平台间差异大，轮询最笨最稳）；30 秒录音上限熔断。

**M2 跟读循环 MVP**（已完成 2026-09-24，6f7bcb3）
TTS 播报词条 → 点按钮跟读录音 → 自己的录音与 TTS 依次回放（「对比」
按钮），人耳对比自评。UI 与录音层的坑在这一步踩完：
QTextToSpeech 惰性构造（引擎插件缺失零成本）、对比流程 5 秒兜底
定时器（引擎哑火不吊死按钮）、unique_ptr<T> 成员显式析构放 .cpp。

**M3a 评分纯逻辑内核**（已完成 2026-09-24）
ARPAbet 39 音素表、Needleman-Wunsch 全局对齐（gap 0.75，同类替换
0.5/跨类 1）、alignment_similarity（(n_match+0.5·n_sub−0.25·n_ins)/
target_len）、词分聚合（0.7·mean + 0.3·min，最差音素不许藏拙）。
全部落 `core/std/pronunciation_score_std.*`，纯 std 进 CTest——
评分语义先钉死，M3b 的 sherpa-onnx 适配器照接口喂数据。
踩坑：测试断言先手推数值再写，`0.7*mean + 0.3*min ≥ 0.7*mean`
这类恒等式拿"坏音素压分"当断言必翻车（对照全好词才对）。

**M3 本地评分 MVP**（核心一步；先只做英语，主场景是中文用户学英文）
原案 sherpa-onnx → 2026-09-24 实测定案 onnxruntime + wav2vec2-espeak-ctc
（见上「待定问题 2」）；IPA→ARPAbet 映射以 `espeak_arpabet_std` 落地。

**M3b 推理壳与管线打通**（2026-09-25 实装，真模型端到端验证进行中）
- **分层**：`core/std/` 纯逻辑全部就位——`pron_vocab_std`（HF vocab.json
  严苛解析：UTF-8/转义/代理对、重复 id 与稀疏空洞拒收、blank 候选
  `<pad>/[PAD]/<blk>/|` 识别）、`pron_wave_std`（int16→float + 零均值/
  单位方差归一化 + 静音护栏 + 16kHz/单声道/16bit WAV 拒收式校验）、
  `ctc_logits_std.h`（IEEE fp16→fp32 位级转换 + 减 max 的 log_softmax，
  原地安全）；`adapters/pron/onnx_pron_scorer`（onnxruntime C++ 推理
  壳，Pimpl 把 ORT 头挡在 .cpp，加载失败回退语义对齐 M2）。
- **测试**： Constant 假模型 fixture（614 字节，`scripts/
  gen_fake_ctc_fixture.py` 手写 protobuf 生成，不依赖 python onnx 包）
  让加载→推理→张量解析→fp16→log_softmax→CTC 对齐→GOP 全链路进
  CTest（`test_onnx_pron_scorer`）；纯逻辑另有 pron_vocab/pron_wave/
  ctc_logits 三组独立测试。log_softmax 测试当场抓出"exp 后再减
  log_sum"的实现 bug——测试先行的价值实证。
- **CMake**：`UNIDICT_BUILD_PRON=ON` 时才拉 onnxruntime 1.30.0 预编译
  包（URL 可覆盖、支持预放置归档、IMPORTED target + 构建树 rpath），
  OFF 时完全不参与构建。CLI `unidict_cli_std --pron-score` 同开关下
  编进，可无 GUI 端到端评分。
- **教训**：并行分段下载（Range 切片拼接）在慢速高丢包网络下会产生
  "尺寸对但字节坏"的归档——下载完整性必须用 tar -tzf / sha256 校验，
  尤其 635MB 的模型资产（LFS pointer 带 sha256，可验）。

**M3b 真模型端到端验证**（2026-09-25 通过）
模型 sadda-speech/wav2vec2-espeak-ctc（sha256 校验通过）+ 真人发音
（Wiktionary En-us-cat.ogg / En-us-dog.ogg，ffmpeg 转 16k/mono/16bit）：
- **正向**：cat 音频评 "K AE T" → K=0.978 / AE=0.650 / T=0.942，
  词分 0.795；帧级 argmax 干净落在 k(40ms)/æ(120ms)/t(340ms)，
  置信度与 GOP 分数一一对应——管线正确，分数可解释。
- **反向**：cat 音频评 "D AO G" → 词分 0.000，区分度充分。
- **已知现象**（非 bug）：dog 音频评自身音标只得 0.032——帧诊断
  （CLI --pron-dump）显示 d→t 清浊混淆、尾音 g→ŋ 同化、元音证据
  分散到多语符号。这正是计划预判的"变体容忍"调优项：D/T、G/NG
  同类变体对与 AO 的 ɔ/ɑ 变体（cot-caught 合并）纳入 M4 变体容忍
  表（GOP 取 max(log p(主键), log p(变体))）。
- 诊断入口：`unidict_cli_std --pron-score <wav> --pron-dump` 逐帧
  top-1 类与 log p（M4 音素定位的调优基建）。

**M4 音素级定位 + 跟读整合**
句子级对齐打分、差异音素高亮展示、跟读循环接入评分、生词本联动
（发音不稳的词自动打 tag/进复习队列——这是和词典学习闭环的真正差异点）。

**M4 实装记录（2026-09-26 起）**
- **变体容忍表**（`core/std/pron_variants_std`）：GOP 取 max(主键,
  变体)——对齐仍钉在主键类上（定位语义不变），打分若区间内变体
  证据更足则按变体记。首批条目按"自然语音过程/口音变体"收录：
  T/D→ɾ（闪音，latter/ladder 合流）、L/N→l̩/n̩（音节辅音）、ER→ɜ
  （非儿化）、AH→ə/ɐ、AO→ɔ/ɑ（cot-caught 合并，唯一的计划内跨域
  例外）、AA→ɑ/ɒ、EH→e、IH→ᵻ、IY→i、OW→o/oː、UW→u。**明确不收**
  G→ŋ（真实过程但只在词尾，位置盲表会连"goal 读错"一起原谅，等
  位置感知变体再收）与一切清浊/调音部位混淆（那是错误不是变体）。
  真模型回归：cat 0.795 不变（无损），dog 0.032→0.042（d→t 是真
  错误照扣，符合设计——dog 的问题在清浊与元音段过短，不该洗白）。
  测试踩坑：合成数据里主键若全帧无证，Viterbi 对齐位置退化为平局
  任意选——真模型主键总有质量，测试必须给主键留弱证据才有定义
 良好的对齐语义。
- **词典 IPA 文本 → ARPAbet**（`core/std/ipa_to_arpabet_std`）：跟读
  整合的前置——GUI/CLI 直接拿词条音标评分，不必手抄。双模式：
  空白分词全是合法 ARPAbet（容忍重音数字 "AE1"）直接放行；否则按
  UTF-8 走 espeak 音素表最长前缀匹配（`match_espeak_prefix`，含
  əl/ɑːɹ 合写展开、l̩ 音节符），重音/切分/tie bar（U+0361/035C，
  摘掉后 tʃ 天然命中单音素）等标注跳过，未收录码点整串拒收——
  宁可不评也不拿半截序列打分。CLI 新增 `--pron-ipa`（与
  --pron-phones 互斥），真模型验证 "ˈkæt" 与 "K AE T" 得分一致。
  工程教训：含 \x 转义的测试字面量里，\x 后跟 hex 字符（e/d 都是
  ）会被贪婪续读成一个字节；音标字面量一律写原生 UTF-8。
- **GUI 面板接线评分**（gui/pronunciation_panel，M4 收口）：发音练习
  面板新增「评分」按钮，跟读闭环从"人耳对比"升级为"机器逐音素
  对照"。接线要点：
  - 构建门控 `UNIDICT_GUI_PRON`（UNIDICT_BUILD_PRON 打开时才链
    unidict_pron + Qt6::Concurrent）：OFF 时评分按钮整体不出现，
    面板保持 M2 无评分跟读——同 cli-std 的"功能不存在而非链接失败"；
  - 目标音素来自释义文本：词典没有独立发音字段，`pronunciation`
    至今无入口填充——新增 `core/std` 的 `extract_phonetic_text`
    （测试齐全）：去 HTML 标签后扫前 256 字节里的 /…/ 或 […] 字段，
    纯 ASCII 候选只认合法 ARPAbet（"hello" 恰好全由单字母音素组成、
    URL 域名这类"能解析"的普通文本一律不收），非 ASCII 候选须整体
    通过 IPA 严格解析。主词条优先，空则依次看前 5 个释义源；提取
    失败（空/非英语 IPA/HTML 实体编码的音标——只剥标签不解实体的
    已知局限）按钮禁用 + tooltip 说明原因，静默退回 M2；
  - 模型路径约定 `~/.cache/unidict-models/wav2vec2-espeak-ctc/
    {model.onnx,vocab.json}`，`UNIDICT_PRON_MODEL/UNIDICT_PRON_VOCAB`
    环境变量可覆盖；加载失败评分永久禁用（不反复试），状态栏说明
    后保持跟读模式；
  - 推理走 QtConcurrent 后台线程，任务值拷贝 PCM/音素/scorer，
    lambda 不碰 this（评分中面板可随时关闭）；模型加载一次复用；
  - 展示为词分 + 逐音素 GOP + 最弱音素点名（"该练哪"的最小可用
    形态，不做撒糖配色）；录音 <0.5 秒不进评分（连一个音素都切
    不出来，分数没有意义）；
  - 分层纪律不变：面板是平台壳不进任何测试，评分内核的可测逻辑
    全部在 core/std（M3a/M3b/M4 已覆盖）。

**M5 全球发音**
- 示范层：TTS voice 按 locale/口音枚举选择（en-US/en-GB/en-AU…），
  GUI 加口音选择器；
- 展示层：词条同时展示 BrE/AmE 音标（词典数据里通常都有）；
- 评分层：按所选口音切换评测参考音素序列，避免"英音被按美音扣分"。

远期（不承诺）：社区真人发音库（Forvo 式众包）——涉及版权与网络服务，
只在 roadmap 占位。

## 已知风险与坑（经验预判）

- **模型体积与下载**：英语 CTC 模型 50-200MB，必须做下载管理 + 哈希
  校验 + 断点续传，失败要能干净回退到"无评分跟读"（M2 能力）。
- **CI 假绿风险**：音频/模型相关代码一旦混进被测路径，offscreen 环境
  会以各种奇怪方式退化。对策：纯逻辑与平台代码物理分层，测试里不出现
  任何音频设备假设。
- **评分的可解释性**：只给一个总分用户不会买账，GOP 的音素级后处理
  （映射到用户能看懂的"这个音发成了那个音"）比算法本身更花功夫，
  M4 预留足量时间。
- **非母语口音的基线**：GOP 对重口音用户天然偏严，评分要按用户自身
  历史做相对化（跟自己的进步比），而不是绝对分——M4 引入历史曲线时
  一起设计。
- **Qt Multimedia 版本差异**：Qt6 的 QAudioSource/QMediaPlayer API
  与 Qt5 完全不同，且 6.2/6.4/6.5 之间有小改，适配器里统一封装，
  不要在 UI 层直接摸 API。

## 待定问题（实现前想清楚）

1. sherpa-onnx 以子源码 vendored 还是系统依赖/包管理？倾向
   FetchContent vendored（版本锁定，离线构建友好），CMake 选项控制
   `UNIDICT_BUILD_PRON=OFF` 时完全不参与构建。
2. 英语评测模型选型（zipformer CTC 各版本）在 M3 启动时实测再定，
   文档只锁"CTC + 可 force-alignment"这条硬要求。

   **2026-09-24 M3b 选型实测结论**：
   - `sherpa-onnx-zipformer-ctc-en-2023-10-02`（k2 官方 383MB）：下载
     验过 tokens.txt——**BPE subword（`▁THE`/`ING`），不是音素**，
     force alignment 到 ARPAbet 无从谈起，排除。教训：k2 官方模型
     清单里"CTC"不等于"音素级"，必须下下来看 tokens 才算数。
   - sherpa-onnx 公开 API **不暴露帧级 CTC logits**（只有 token 级
     timestamps），就算有音素模型也得改上游源码——GOP 的帧级
     posterior 拿不到就全盘不通。
   - 定选 **`sadda-speech/wav2vec2-espeak-ctc`**（HuggingFace，
     Apache-2.0）：facebook/wav2vec2-lv-60-espeak-cv-ft 的 fp16 ONNX
     转换，**输出就是帧级 CTC logits (1,T,392)、50 帧/秒**，输入
     16kHz 单声道原始波形（模型自带 CNN 前端——kaldi fbank 的
     bit 级兼容坑整个绕开），词表 espeak IPA 音素（392 含多语）。
     作者本人拿它做 phone-level forced aligner，用途完全对口。
   - 代价：**fp16 onnx 635MB**，超出原预估（50-200MB）——作为
     用户可选下载可接受，int8 量化（~320MB）列为后续优化不阻塞
     MVP；espeak IPA → ARPAbet 映射表写死（两边都是英语音素全集，
     一一对应基本成立），映射不到的按非法音素走 M3a 的 cost 1 兜底。
   - 推理走 onnxruntime 预编译库（FetchContent 拉官方 release 包 +
     IMPORTED target），sherpa-onnx 整个不引了——它对我们唯一的价值
     （解码/流式/语言模型）在评分场景全用不上。
   - **映射域已知坑（实测前记录）**：`arpabet_to_espeak("T")` 取主键
     "t"，但地道美音的 butter t 会 flap 成 ɾ——模型帧证据落 ɾ 类时
     T 的 GOP 被错扣（用户读得越地道扣得越狠）。变体容忍（T 的 GOP
     取 max(log p(t), log p(ɾ))）列入 M3b 实测后的调优项；同类变体
     对（ER 的 ɚ/ɜː 等）一并盘点。
3. 发音练习面板先落 Qt Widgets GUI（与现有学习功能同处一窗），
   QML 端是否同步跟进等 Widgets 版验证后再说。
