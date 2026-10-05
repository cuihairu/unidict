# BUGS

用户报告与自查缺陷登记。格式：现象 / 根因 / 修复 / 验收。修复完成即勾，
带后续验收项的写明口径。

## BUG-011 查词返回内容不对（用户：查词还是瞎搞的，返回的是啥啊）✅修复（2026-10-05 登记并修复）

**现象（用户原话）**：「查词还是瞎搞的，返回的是啥啊」。BUG-009 修的是
展示聚合与去重，本次用户再报——查词**返回的内容本身**不对，与展示无关。

**复现与对照**（std 面 search_grouped，CC-CEDICT 形汉英 + 学习词典形英中
双词典夹具，`/tmp/lookup_repro` 驱动）：
- 查询「你好」（hello 释义「int. 你好；招呼语」明明含它）→ 修复前
  `(no groups)`，层 2 全文检索对中文结构性失效；修复后命中
  `hello => int. 你好；招呼语`（relevance 2 + fulltext 标注，返回真词头）。
- 查询 `Hello`/`HELLO` 打 MDX/StarDict 词典（词头 `Hello`）→ 修复前
  层 0 词头精确 miss（索引键=原始词形），掉进层 1 拿 12 条等权前缀
  候选，用户看到的是「一堆不知道哪来的词」；修复后层 0 折叠回退直接
  命中释义。

**根因（两个，均实锤于管线剖查）**：
1. **层 0 折叠回退只落了 JSON 一家**：`JsonParserStd::lookup` 有精确
   miss → fold_key 回退（注释还声称「与 stardict/mdict/dsl 及 Qt 面
   同口径」），但 `MdictParserStd::lookup` 是纯 `entries_.find`（键=原始
   词形）、`StarDictParserStd::lookup_raw` 是纯 `index_.find`（键=转
   UTF-8 后的原始词形）。输入首字母大写/全大写/全角变体在 MDX 与
   StarDict 词典上层 0 必漏，静默滑向层 1 前缀噪声。Qt 面两 parser
   （`core/mdict_parser.cpp` / `core/stardict_parser.cpp`）各自已有
   小写键折叠，无此缺口——差距只在 std 面。
2. **全文索引分词是字节级 ASCII 口径，CJK 全死**：
   `FullTextIndexStd::is_word_char` 按单字节 `isalnum` 判——UTF-8 高
   字节在 C locale 下非字母数字，中文释义的每个字被当分隔符，产出
   **零**中文词条；中文查询同样 tokenize 为空 → 层 2 对中文结构性
   失效（substring 后备也永不触发：查询根本没有 token）。且层 2 是
   汉英词典（词头=中文）查询英文词的唯一命中层，死掉即「返回的是啥」。

**修复**：
- `core/std/mdict_parser_std.{h,cpp}` / `core/std/stardict_parser_std.{h,cpp}`：
  lookup 精确 miss 后走折叠索引回退——`fold_key(词头) → 词头原形`
  按 words_ 原序惰建（unordered_map 迭代序不定，first-wins 的 canonical
  必须走有序词表；load 起点置脏重建），与 JsonParserStd::lookup 同口径
  成真。
- `core/std/fulltext_index_std.{h,cpp}`：tokenize 改 UTF-8 码点感知。
  CJK 连续段（统一表意/扩展 A/兼容/假名/谚文/扩展 B，刻意不含 CJK
  标点）按段切词；**文档侧发 unigram+bigram**（单字查询与多字查询都
  命中），**查询侧多字串只发 bigram**（发单字会把「你好」扩成「含你 ∪
  含好」，层 2 退化成单字噪声）；单字发 unigram。非法 UTF-8 字节当
  分隔符；非 CJK 合法高字节并入词（"café" 整词保留、emoji 不再腰斩）；
  全角/CJK 标点显式按分隔符（否则「你好；招呼」产出垃圾 token「；」），
  全角字母数字并入词。`is_word_char` 高字节成词字符，字节级 ngram
  substring 后备链路随之恢复。查询侧与文档侧口径分家由
  `tokenize_query` / `tokenize` 两入口承担。
- `core/std/dictionary_manager_std.cpp`：全文持久化签名（UDFT）掺入
  分词器版本 `TV=`（`FullTextIndexStd::kTokenizerVersion`，本次 2）——
  分词器换了、旧缓存词表不换，是「签名绿灯但查不到」的静默坏数据；
  旧 UDFT 文件签名失配自动重建。
- Qt 面无改动：`DictionaryManager::fullTextSearch` 的 `m_ftIndex` 就是
  std 的 `FullTextIndexStd`，分词修复双面同享；层 0 折叠 Qt parser 本
  就有。

**测试**（真实词例断言，全部进既有测试文件）：
- `tests/stardict_std_test.cpp`：词头 `Bank`，`bank`/`BANK`/全角 `ｂａｎｋ`
  层 0 命中同一释义，词典外查询返回空。
- `tests/mdict_parser_std_branches_test.cpp`（B12）：词头 `Hello`/`Bank`，
  精确/大小写/全角变体与 fold miss 臂。
- `tests/fulltext_index_std_test.cpp`：中文 doc「你好；招呼语」——「你好」
  只命中该 doc（含单字「你」的另一 doc 不被召回）、单字「你」召回两
  doc、「招呼」跨 doc bigram 命中；café/emoji/全角字母并入词臂；全角
  分号与句号分段臂；超长编码/代理区/截断/落单续字节等非法字节当分
  隔符臂；4 字节 CJK（扩展 B）臂；`kTokenizerVersion==2` 钉版。
- `tests/dictionary_manager_std_grouped_test.cpp`（T9/T10）：stardict
  词头 `Hello` 的小写/全角查询端到端层 0 命中原文释义；常用词
  `good`/`Run` 层 0 原文抽查；中文「你好」端到端层 2 命中真词头
  `hello` 且单字噪声不召回；`fulltext_signature()` 含 `TV=2`。

**验收**：build-std 124 / build 149 全绿；coverage lines 100.0%
(8044/8044)、functions 100.0% (831/831)。

**后续项（登记不半修）**：层 2 反向命中的排序语义（gloss 全词相等 >
前缀 > 包含）现在按 tf-idf 分数序，查询词=词头时的「最该排第一」
没有结构保证——用户词典（汉英词头）查英文词的观感与它相关，另行
立项。

## BUG-010 UI 未按原型图实现 + 大量点击无反应（2026-10-04 登记，2026-10-05 修复+双端验收通过）

**现象（用户原话）**：「现在难点是界面 虽然有原型图 但还是没有根据原型图
实现 点击很多都没有反应」。

**修法（用户指定，两件并案，插队优先于在途批次）**：
1. 对照原型图（`docs/design-references/eudic-lookup-page.png`，欧路风格，
   BUG-009 定案基准）逐控件走查实现：逐屏截图与原型对照，布局/层级/
   配色/控件形态逐项对齐，偏差列清单改齐；
2. 点击无反应全面排查：逐个可点元素（按钮/词条卡/Tab/折叠卡/发音/
   设置项）**真点测试**（离屏注入真实鼠标事件走事件路径，非直接调
   函数），断线的接上信号/回调，点不动的不许留，每修一个真点验证一次。

**修复明细**：
- **装饰样式从未拼上**：`MainDesktop.decorateHtml` 的 `<pre>`/外层 div
  样式是坏的 JS 字符串（`+ Theme.window +` 落在字面量内部），主题色/
  字体/边框全没上到释义 HTML——改为真拼接。
- **朗读三类「点了像没点」**：音标行喇叭本地 TTS 不可用时静默无反馈
  → 改词条卡朗读统一入口，点击必有状态行反馈（不可用时明示去
  设置→语音 换在线发音）；英/美分口音路径；例句喇叭改读整句。
- **复制点击 TypeError**：MouseArea 里的裸名 `clip` 被 `QQuickItem.clip`
  布尔属性遮蔽（不是服务对象）→ `win.clipService` 限定，补空态回退
  分支，点击落「已复制释义 · 词典」状态行。
- **tab 懒取无加载提示**：例句/全文首查构建索引秒级阻塞 → 状态行
  收口（加载中… → 共 N 条）。
- **两个真断线**（`05dab90`，qmlui 审计挖出）：① 导航栈 push/pop 原地
  改数组不发 changed 信号，←/→ 的 enabled 绑定永不重评估、永远禁用
  → 改整赋；② `ensureTabData` 用 `tabData[key]=…` 原地变异，例句/词组/
  近反义/全文四个内容 tab 的 model 绑定永不重评估、永远空表 → 改整赋
  新对象。
- **结果区按欧路标准重排**（`759f93e`）：配色收敛、层级拉开、括号注释
  三级灰弱化、实体色 chip（修 rgba 浮点 alpha 解析成纯黑的坑）、gui 端
  HTML 同口径。
- **gui 端基建**：`6b02e99` 补 16 处 setObjectName 供定位 + 47 项真点
  审计 harness。

**验收结果（2026-10-05，两项口径全过）**：
1. **逐屏截图对照**：qmlui 八屏设计基线刷新为现实现（10-04 重排后），
   旧稿|新实现演进记录与客观度量见 `docs/ui/compare/README.md`；gui 端
   12 屏留档 `docs/ui/compare/gui/`（设计基准=gui-ui-structure.md，
   md5 全唯一、亮暗灰度 250/45）。
2. **可点元素全接线清单**：qmlui **57 项**（`ui_click_audit`，5 连跑
   稳定）+ gui **47 项**（`gui_click_audit`，3 连跑稳定），双端均进
   ctest 常驻回归；逐项证据（真点后可观测状态差）如下。

<details><summary>qmlui 57 项真点清单</summary>

1. decorateHtml <pre>/外层 div 主题样式拼接 — 含 background:#、color:# 主题色且无字面 "+ Theme." 残留
2. openWord(hello) 聚合查询落状态行 — statusText=找到 1 个词典结果
3. 音标行英/美喇叭存在（fixture 词头带 英/美 音标） — phonSpeakerBr 与 phonSpeakerUs 均可见于词条卡
4. 英音喇叭点击反馈 — hasLocalTts=false, statusText=本地语音不可用，可在 设置→语音 换用在线发音
5. 美音喇叭点击反馈（与英音分口音路径） — statusText=本地语音不可用，可在 设置→语音 换用在线发音
6. 生词本轻入口点击 — statusText=已加入生词本: hello
7. 复制轻入口点击（剪贴板写入） — statusText=已复制释义 · Unidict Click Audit Fixture
8. 笔记轻入口打开笔记弹层 — notePopup visible
9. 笔记弹层取消按钮点击收起 — notePopup hidden after cancel
10. 内容 tab 元素定位 — contentTab_examples found
11. 例句 tab 点击（懒取+加载状态行收口） — examples=5 条, statusText=共 5 条
12. 例句喇叭点击（读整句+反馈） — statusText=本地语音不可用（未检测到系统 TTS 语音），可在 设置→语音 换用在线发音
13. 例句行词链点击跳转 — currentWord=greeting, 期望=greeting
14. 词组 tab 点击懒取数据 — phrases[0].word=hello everyone
15. 词组词链点击跳转 — currentWord=hello everyone, 期望=hello everyone
16. 近反义 tab 点击懒取数据 — related[0].word=hello everyone
17. 近义词链接点击跳转 — currentWord=hello everyone, 期望=hello everyone
18. 全文检索 tab 点击（懒取+状态行） — fulltext=5 条, statusText=共 5 条
19. 分组头存在 — groupHeader_0 located
20. 分组头点击折叠 — collapsed 含 true
21. 分组头再点展开 — collapsed 全 false
22. 组内词头链点击跳转 — currentWord=hello, 期望=hello
23. 导航 ← 返回上一词 — currentWord=hello
24. 导航 → 前进 — currentWord=world
25. 侧栏「查」按钮点击提交查询 — currentWord=hello
26. 建议列表项点击查询 — currentWord=hello, 期望=hello
27. 侧栏「历史」tab 点击 — currentTabIndex=1
28. 历史项点击查询 — currentWord=greeting, 期望=greeting
29. 侧栏「生词本」tab 点击 — currentTabIndex=2
30. 标签筛选 chip 点击过滤 — vocabTagFilter=greeting
31. 「全部」chip 点击清筛选 — vocabTagFilter 为空
32. 生词卡点击查询 — currentWord=hello, 期望=hello
33. 生词卡「+标签」打开标签对话框 — tagDialog visible
34. 生词卡「笔记」打开笔记对话框 — noteDialog visible
35. 「导出CSV」点击（signal 级：native 文件对话框离屏不出窗） — clicked 计数=1
36. 头部「历史」按钮 — currentTabIndex=1
37. 头部「生词本」按钮 — currentTabIndex=2
38. 头部「设置」按钮开抽屉 — toolsDrawer visible
39. 剪贴板取词开关点击（UI+core 双证） — checked 0→1
40. 剪贴板取词开关还原 — core 状态回原
41. 取词悬浮窗开关点击 — checked 1→0
42. 轮询间隔 Slider 点击改值 — value 500→1900
43. 最短词长 SpinBox「+」步进钮点击 — value 2→3
44. 最长词长 SpinBox「+」步进钮点击 — value 50→51
45. 抽屉「语音」tab 切换 — volumeSlider 可见
46. 「停止」按钮点击 — 执行无异常（停止播放副作用离屏不可观测）
47. 「刷新」语音按钮点击 — 执行无异常（重扫语音副作用离屏不可观测）
48. 口音 ComboBox 选择接线（signal 级） — pronAccent=2
49. 发音源 ComboBox 选择接线（signal 级） — pronSourceMode=1
50. 发音源还原本地态 — pronSourceMode=0
51. 页脚「清空历史」按钮 — statusText=历史已清空, history=0
52. 取词窗弹出 — quickLookupPane visible
53. 取词窗朗读按钮（TTS 缺失给明示） — statusText=本地语音不可用（未检测到系统 TTS 语音），可在 设置→语音 换用在线发音
54. 取词窗生词本按钮 — statusText=已加入生词本: hello
55. 取词窗 ✕ 关闭 — pane hidden
56. 取词窗「在主窗打开」收窗并落主窗词条卡 — pane hidden, currentWord=hello
57. 取词窗「关闭」按钮 — pane hidden
</details>

<details><summary>gui 47 项真点清单</summary>

1. 启动初始态：收藏按钮禁用（未查询） — starButton enabled=0
2. 启动初始态：搜索框就位 — placeholder=输入单词或词组…
3. 启动初始态：词典管理入口存在 — manageButton located
4. 回车查询落状态行 — statusText=Found in Unidict Click Audit Fixture
5. 回车查询出释义 — resultView len=222
6. 查询成功后收藏按钮可用 — starButton enabled=1
7. 补全选中回填并查询 — items=1 picked=hello
8. 内容页签五个 — count=5
9. 五内容页签逐个真点切换 — 0-4 全部切到位
10. 例句页有数据 — len=598
11. 词组页有数据 — len=89
12. 近反义页有数据 — len=15
13. 全文页有数据 — len=583
14. 例句页 #ft 锚点真点跳转 — clicked=1 word=hello
15. 侧栏页签（历史/收藏） — count=2
16. 侧栏「收藏」页签真点切换 — currentIndex=1
17. 侧栏「历史」页签真点切换 — currentIndex=0
18. 历史有条目（前序查询产生） — count=1
19. 历史项双击回查 — word=hello input=hello
20. 历史右键菜单真点「置顶/取消置顶」 — menuPicked=1 pinned 0→1
21. 历史右键菜单真点「删除该条」 — removed=1 count 2→1
22. 收藏按钮真点入库 — vocabList=1 stored=1
23. 收藏项双击回查 — word=hello input=hello
24. 收藏右键真点「设置标签…」→ 输入框代填落库 — modalSeen=1 tagged=1
25. 收藏分组下拉聚合了 en 标签 — items=2
26. 收藏分组过滤真点生效 — picked=1 count=1
27. 收藏右键菜单真点「移除收藏」 — removed=1 count 1→0
28. 工具栏剪贴板取词按钮就位 — located
29. 剪贴板取词开关真点翻转+记忆 — 0→1
30. 剪贴板取词开关还原 — 回 0
31. 主题循环按钮就位 — 主题: 跟随系统
32. 主题真点切换 — 主题: 跟随系统 → 主题: 浅色
33. 主题循环还原 — 回 主题: 跟随系统
34. 释义字体按钮就位 — located
35. 释义字体真点弹字体对话框（取消路径） — modalSeen=1
36. 笔记按钮就位且可用 — enabled=1
37. 笔记真点代填保存落库 — modalSeen=1 note=audit note
38. 发音练习按钮就位 — located
39. 发音练习真点弹面板（离屏即关） — modalSeen=1
40. 词典管理入口就位 — located
41. 词典管理对话框真点打开，八按钮齐全 — modalSeen=1 buttons=8
42. 词典管理「启用/禁用」真点翻转 — toggled=1
43. 分组标签真点设置 → 分组下拉聚合 — groupBox items=2
44. 「添加词典文件…」真点弹文件对话框（取消路径） — modalSeen=1 fileDialog=1
45. 全局热键按钮就位 — located
46. 全局热键平台态正确 — supported=0 enabled=0
47. 关窗收口（离屏无托盘直退） — visible=0
</details>

## BUG-009 查词结果展示全是乱的、很多重复 ✅修复+三词走查通过（2026-10-03 登记并修复）

**现象**：用户报告查词结果展示全是乱的、很多重复。实测 `good`/`example`
等英文词在汉英词典（CC-CEDICT）下返回 12 条释义包含命中的词条平铺混排：
无分组、无排序、无去重，词条按词典内部顺序交错，肉眼即「乱+重复」。

**根因（两层）**：
1. 展示层没有聚合口径——BUG-005 修的全文兜底把「词头 miss → 释义包含」
   的命中条目直接平铺进结果列表，多词典、多词条、同词头大小写变体
   （Dup/DUP）不做分组折叠，UI 逐条渲染即乱；
2. `MainDesktop.qml` 的 `resetHome` 未清新加的查询面属性
   （resultGroups/flatEntries/headPhonetics/matchLevel），场景切换时
   残留上一个词的分组数据（走查 example 页曾显示 hello 的旧分组）。

**定案（用户中途定）**：查询界面按**欧路词典**界面做，参考图
`docs/design-references/eudic-lookup-page.png`（欧路网页版单词页实拍，
已入库）。原 BUG-003 的 `docs/ui/` 原型逐像素对照口径**由本定案覆盖**
（新对照基准=欧路参考图），不回改 docs/ui。

**修复（core 不引 Qt：聚合在 core，样式在 adapter/UI）**：
- `core/unidict_core.{h,cpp}`（Qt 面核心）：新增
  `DictionaryManager::searchGrouped(word, tagFilter)` →
  `QVector<DictionaryGroup{dictionaryId, dictionaryName, entries}>`。
  **三层降级**：词头精确（relevance 0）> 词头前缀（relevance 1）>
  释义包含（relevance 2，matchType=fulltext），有上层命中不掺下层；
  组内同 headword 折叠去重（大小写折叠键）；`searchAll` 重写为
  searchGrouped 的平铺形态（口径同源）。
- `core/std/dictionary_manager_std.cpp`（std 面）：`search_all` 同口径
  补层 1 前缀（`allow_fulltext_fallback` 闸门同样控前缀层）+ 层 2 全文
  去重（dict_name + fold_key 复合键，跨词典不误伤）。
- `qmlui/lookup_adapter.{h,cpp}`：`aggregateLookup` 改分组返回；新增
  `extractPhonetics`（释义开头惯例音标提取，复用 core/std
  extract_phonetic_variants）、`fullTextLookup`、`relatedLookup`。
- `qmlui/MainDesktop.qml` + `components/EntryResultsPane.qml`（重写）：
  欧路面板——词条卡头（大词加粗+匹配层级 chip+英/美音标行带发音喇叭+
  生词本/笔记/复制轻入口）、内容 Tab 栏（词典/例句/词组/近反义词/
  全文检索，选中浅蓝，懒取数据）、词典 tab=每词典一个可折叠分组卡
  （词典名+折叠箭头+浅灰细线，不用硬边框）、例句/全文目标词蓝色高亮；
  `resetHome` 补清四个新属性。
- `gui/main.cpp`（Qt Widgets 端同布局）：主区改五页 QTabWidget
  （词典分组 HTML：大词+音标+分组小灰标题+词条链接+MDict 富渲染），
  例句/词组/近反义/全文四页锚点回查（#w:/#ft:），链接蓝与
  Theme.link 同值 `#1b6ac9`。
- `qmlui/theme_tokens.h` + `theme.h`：新增 `link` token
  （亮 #1b6ac9 / 暗 #82b1ff），过 WCAG AA 4.5:1 对比度门禁。
- 数据现实约束（不造假）：demo/CEDICT 无词频考试标签→用真实匹配层级
  chip（未收录/前缀匹配/释义匹配）；无独立音标字段→从释义开头惯例
  提取，无则显示总朗读喇叭；无独立例句库→全文命中词条作例句展示。

**走查补修（2026-10-04，三词 hello/good/说明 复验发现）**：
1. **QML 同名绑定静默失效（根因 3）**：`EntryResultsPane`/`SidebarPanel`
   实例上 `currentWord: currentWord` 这类绑定右值不是 id 时，QML 名字
   解析（id > 实例属性 > 外层）落到实例自身属性——自引用循环，绑定
   失效取默认值：pane 的 `root.currentWord` 恒空（词头行该隐未隐、
   tabData 不清）、`fallbackHtml`/`lookup`/`flatEntries` 恒空（例句
   tab 发声与空态回落失效），SidebarPanel 的 `suggestMode`/`currentWord`
   （历史高亮）/`lookup` 同样恒空。修复：右值改 `win.` 限定或根作用域
   抓取（`lookupService`/`clipService`），id 型（entriesModel 等）保持
   裸名，handler 裸赋值改 `win.`（同名遮蔽机制与 `lookupService` 先例
   同源）。
2. **拼音行 CJK 闸门（双端）**：查英文词走 fulltext 层时首条中文条目
   的 `[拼音]` 冒充卡头音标（good 页显示「拼 [bu4 zhi1 hao3 dai3]」）。
   修复：拼音行仅查询词本身含汉字时展示（qmlui `showPinyinPhonetic`、
   gui `cjkRe` 匹配 query），英文词回退朗读兜底喇叭。
3. **义项 "; " 逐行分段（双端）**：纯文本释义按分号切义项逐行
   （`EntryResultsPane.formatDefinition` / gui `splitDefinitionSenses`，
   实体收尾分号不切、MDict 富文本走原 HTML 管线）——CC-CEDICT 义项
   糊成一坨是「乱」观感来源之一。

**验收（2026-10-03，双词典 ccedict-zh-en + examples/dict.json）**：
- 离屏走查三词 × 亮暗 × 双尺寸（ui_sandbox，临时三词 probe 后已还原）：
  `hello`=Unidict Sample 单组 1 条精确命中+英/美音标行、无 chip；
  `good`/`example`=「CC-CEDICT 汉英词典 · 12 条」单一可折叠分组+
  「释义匹配」chip、无重复、词条与 core 返回一一对应（adapter 分组
  dump 交叉核对）。
- 回归：`tests/core_lookup_tests.cpp` 新增 `searchGroupedTiersAndDedup`
  （三层降级+分组+去重+searchAll 平铺一致）；`tests/std_lookup_parity_test.cpp`
  新增 `test_search_all_tiers_and_dedup`（std 面同口径）。
- 留待真机：折叠交互/Tab 懒取/发音的实际手感（离屏截图不覆盖交互）。

**复验（2026-10-04，hello/good/说明 × 亮暗 × 双尺寸 24 张）**：
- 音标行三态正确：hello=英/美行（Unidict Sample 无音标数据时朗读兜底）、
  good=「🔊 朗读」（拼音噪音已消）、说明=「拼 [shuo1 ming2]」；
- 词头行（`word !== currentWord` 蓝链）在精确命中时正确隐藏，fulltext
  层非当前词条头正常可跳；义项逐行、分组卡、历史当前词高亮全部生效。
- 截图：`/tmp/eudic_walk_final/`（客观度量核验：good 页中部墨量 5731
  vs 说明页 160，文件与内容一一对应）；门禁 ctest 137/137 + std 115/115。

## BUG-008 Android 启动图标缺失（APK 没有 android:icon）✅修复（2026-10-03 登记并修复）

**现象**：BUG-006 接完 Windows/macOS/Linux 三平台图标后复查分发面，发现
Android 侧还空着——`android/app/src/main/AndroidManifest.xml` 的
`<application>` 没有 `android:icon`，启动器只能取系统默认图标（绿色
机器人或空白底），跟其它平台的品牌图标对不上。

**根因**：Android 端从未做过启动图标资产，也没在 manifest 声明图标资源。
BUG-006 的 `assets/icons/`（ico/icns/png）是桌面格式，`.ico` 能塞进
`mipmap-*` 但不合适（启动器按密度桶取 PNG，API 26+ 还要自适应图标 XML），
所以要单独生成。

**修复**（与桌面资产同源，同一份 `docs/logo.svg`）：
- legacy 图标 `mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher.png` 与
  `ic_launcher_round.png`：48…192px 五档，字形按 alpha 内容框裁切后居中、
  占方图 78%，方版四角不透明、圆版圆形 alpha 遮罩。
- 自适应前景 `mipmap-*/ic_launcher_foreground.png`：108dp 画布
  （48…432px），字形比例 66/108 落在规范安全区内。
- `mipmap-anydpi-v26/ic_launcher{,_round}.xml`：API 26+ 自适应图标，
  背景 `@color/ic_launcher_background`、前景 `@mipmap/ic_launcher_foreground`。
- `values/ic_launcher_colors.xml`：自适应层底色 `#F5EFF3`（品牌色 #b11964
  的 5% 淡底）——字形保持 logo 原色不反白，淡底只为在白底启动器上能看出
  图标边界。
- `AndroidManifest.xml`：`<application>` 补 `android:icon`/`android:roundIcon`。
- `tools/build_icons.py` 增 `write_android_assets()`/`report_android()`，
  与 ico/icns 同一函数入口生成，`--check` 一并自检 Android 尺寸集。

**验收（2026-10-03，本地 release APK 实测）**：
- `aapt2 dump badging`：`label='Unidict' icon='res/BW.xml'`；
  `aapt2 dump xmltree`：`android:icon=@0x7f0d0000`、`android:roundIcon=@0x7f0d0002`
  （资源名经 `aapt2 dump resources` 解析，未按位置猜）。
- 资源表 `mipmap/ic_launcher`、`ic_launcher_round` 六变体
  （anydpi-v26 + 五档密度）、`ic_launcher_foreground` 五变体齐全，
  **R8 + resource shrinking 之后仍在**。
- **15 帧 PNG 与仓库源资产像素级一致，不一致帧 0**（`aapt2` 会重编码 PNG，
  文件字节长度有差但解码像素相同——按字节比会误判）。
- 前景字形 bbox：mdpi `(21,21,87,87)`、xxxhdpi `(84,84,348,348)`，
  与 66/108 安全区恰好重合（字形主色 `#b11964`，未反白/未改色）。
- **观感对照图（入库）** `docs/icons/android-launcher-icons.png`——legacy
  方/圆五档、API 26+ 自适应图标按 72/108 可见区套 circle/squircle/full
  三种启动器遮罩的还原、前景层 66/108 安全区框（红框，可见字形恰好
  贴合），以及 background+foreground 合成结果；同
  `tools/build_icons.py --sheets` 生成。
- `python3 tools/build_icons.py` 重跑后 `assets/icons/` 无 diff（生成可重复）；
  ctest 136/136 绿。留档 `ui_sandbox_out/bug008/android_launcher_icon.log`。
- 留待真机：真机/模拟器启动器上的**实际显示观感**与不同厂商主题遮罩下的
  观感（本轮只证「图标资源在 APK 里且像素/结构正确」）。

## BUG-007 macOS .app 的 CFBundleIdentifier/CFBundleName 为空 ❗修复待产物复验（2026-10-03 登记；首版修复无效，同日订正）

**现象**：nightly 产物 `unidict_qml.app/Contents/Info.plist` 里
`CFBundleIdentifier` 与 `CFBundleName` 是空串（同批 `CFBundleExecutable`/
`CFBundleIconFile`/`CFBundlePackageType` 均正常）——做 BUG-006 产物核验时
顺带发现。

**影响**：图标与可执行文件定位不受影响（BUG-006 口径仍成立）；但 Launch
Services 侧没有应用身份可用于归一（多版本共存、偏好设置归属、「打开方式」
列表、后续签名与公证都要用它）。

**根因（订正版）**：`qmlui/CMakeLists.txt` 的 APPLE 分支没设
`MACOSX_BUNDLE_BUNDLE_NAME`，`CFBundleName` 直接空串。
`CFBundleIdentifier` 的空另有原因，且**首版修复对它完全无效**——c83e7e8
设的 `MACOSX_BUNDLE_IDENTIFIER` 只喂 CPack（DMG 标识），CMake 生成的
bundle 模板 `Modules/MacOSXBundleInfo.plist.in` 第 13-14 行代入的是
**`MACOSX_BUNDLE_GUI_IDENTIFIER`**（CMake 4.2 仍是这个历史变量名）。
证据链：首版修完后 `CFBundleName=Unidict` 生效、`CFBundleIdentifier` 依旧
空串——同一段 `set_target_properties` 里两个属性一个中一个不中，只可能是
变量名对不上模板。

**修复**：
- `MACOSX_BUNDLE_GUI_IDENTIFIER "com.unidict.unidict"`（与
  `cmake/BuildOptions.cmake` 的 `CPACK_BUNDLE_IDENTIFIER` 同值，不新造域名）、
  `MACOSX_BUNDLE_BUNDLE_NAME "Unidict"`；
- 顺带补版本串（同属「必填键不能空」：`CFBundleShortVersionString`/
  `CFBundleVersion` 原先也是空串，Finder 版本位空白、签名公证校验不过）→
  `MACOSX_BUNDLE_SHORT_VERSION_STRING`/`MACOSX_BUNDLE_BUNDLE_VERSION` 取
  `${UNIDICT_VERSION}`；
- **CI 回归闸门**：`daily-build.yml` 新增 `Verify bundle identity + icon keys
  (macOS)`，打包前用 PlistBuddy 逐键打印并断言 CFBundle{Identifier,Name,
  IconFile,ShortVersionString,Version,Executable} 全非空，且
  `Contents/Resources/<CFBundleIconFile>` 存在且 magic 为 `icns`——空值即
  失败，不再让「修完又空」靠人眼发现。

**验收（本地）**：
- 模板口径核对：`grep -A1 CFBundleIdentifier
  /usr/share/cmake-4.2/Modules/MacOSXBundleInfo.plist.in` → 代入变量为
  `MACOSX_BUNDLE_GUI_IDENTIFIER`（留档 `ui_sandbox_out/bug007/plist_template.log`）。
- Linux 本地构建不受影响（APPLE 分支不生效）；ctest 136/136 绿。
- **产物面待下一轮 nightly 复验**（bundle 只能在 macOS runner 生成，本机
  无从构建；CI 闸门会在下一次 macOS 构建里自动判定）。首版就是因为「本地
  绿即收工」被推翻，故此处**不勾**。

## BUG-006 二进制图标没换（SVG 不能直接当 OS 图标）✅修复（2026-10-03 登记并修复）

**现象**：品牌图标只有 SVG（docs/logo.svg），各平台二进制/桌面对象
不带图标——Windows exe 默认通用图标、macOS .app 默认图标、Linux 无
.desktop 集成。

**根因**：从未生成多尺寸光栅资产，也从未接线——无 .ico/.icns/.rc/
.desktop，CMake 的 WIN32/MACOSX_BUNDLE 目标都没挂图标资源。

**修复**（资产 + 三平台接线）：
- 资产：docs/logo.svg（1024 viewBox 单 path）矢量直渲 1024 master +
  Lanczos 下采样 → `assets/icons/`：`unidict.ico`（16/24/32/48/64/128/256
  七帧，16/24 为 BMP-in-ICO 32bpp XOR+AND 掩码以兼容老 shell，32 及以上
  为 PNG-in-ICO 软 alpha——首版 16-128 是 8bpp 调色板 + 1bit 硬掩码，
  任务栏/资源管理器边缘锯齿，度量见下）、`unidict.icns`
  （ic11/12/07/08/09/10 = 32/64/128/256/512/1024 六帧 PNG-in-ICNS）、
  `unidict_256/512.png`（Linux hicolor）。
- 可重复生成：`tools/build_icons.py`（`--check` 只校验不写盘）。SVG 光栅化
  后端按可用性择优 resvg → rsvg-convert → inkscape → ImageMagick；ICO/ICNS
  容器由脚本按 spec 手工拼装并回读自检（尺寸集/PNG 负载/容器声明长度）。
  本机无 resvg/inkscape/rsvg-convert，实跑走 ImageMagick 内建 SVG 渲染器
  （本仓 logo 是单 path、无文字/渐变依赖，换后端不改变像素语义）。
- Windows：`qmlui/unidict.rc` + `gui/unidict.rc`（IDI_ICON1）编进 PE
  资源段（CI MSVC 自动 rc.exe）。
- macOS：`MACOSX_BUNDLE_ICON_FILE` + icns 落 Contents/Resources
  （默认 Info.plist 自动注入 CFBundleIconFile；macdeployqt 保留）。
- Linux：`assets/linux/unidict.desktop` + hicolor 256/512 install 规则
  （顶层 CMakeLists），并补 `install(TARGETS unidict_qml)`（RUNTIME
  bin；INSTALL_RPATH 清空绕 Qt6 link 线与 install 前缀的 RPATH_CHANGE
  校验冲突）。

**验收（2026-10-03）**：
- **Windows exe 资源段（nightly run 37099269267 产物实测，非本地推断）**：
  解析 `unidict-windows-x64.zip` 内 `unidict_qml.exe` 的 PE 资源目录 →
  `RT_ICON` 7 条 + `RT_GROUP_ICON` 1 条，帧尺寸集 **[16,24,32,48,64,128,
  256]**，subsystem=2 (WINDOWS_GUI)。对照 `unidict_cli.exe`
  （subsystem=3）只有 RT_MANIFEST、无图标组——说明图标确实来自本项目
  rc 资源而非 Qt/Qt 工具链。留档 `ui_sandbox_out/bug006/pe_resources.txt`
  （解析脚本 `/tmp/opencode/pe_icon_check.py`，纯 PE 结构解析，无 Windows）。
- **macOS bundle（同一 run 产物）**：`unidict_qml.app/Contents/Resources/
  unidict.icns` 存在，magic=icns、声明长度=实际长度=151332B、六帧
  （ic11/12/07/08/09/10）齐全；`Contents/Info.plist` 含
  `CFBundleIconFile = unidict.icns`（Finder/启动台取图标即此键）。
- **Linux**：`cmake --install --prefix` 干净完成 → bin/unidict_qml +
  share/applications/unidict.desktop + share/icons/hicolor/{256,512}/
  apps/unidict.png 布局正确。
- **帧级客观度量**（`ui_sandbox_out/bug006/icon_verify.log`）：
  同尺寸 ICO 帧与 ICNS 帧**像素缓冲 md5 逐帧一致**（32/64/128/256 四档），
  ICO 256 帧与 `unidict_256.png` 像素一致；三平台资产同源于一次下采样。
  alpha 软硬度量（非透明像素 vs 全不透明像素）：现 **七帧全部软 alpha**
  （如 32px 非透明 713 / 全不透明 157），首版 16-128 帧两者相等（硬 1bit
  掩码）——这是本次资产重做的直接原因。非透明覆盖率随尺寸收敛
  69.6%→38.1%（1024 master 基准 38.09%），无空白帧、无裁切。
- 帧对照图（可直接看图核验，非文字断言）：**入库**
  `docs/icons/desktop-icon-frames.png` —— 三平台全部帧一次排开，
  16-64px 帧按最近邻放大以便看单像素结构，透明区垫棋盘格以区分
  「透明」与「白」；由 `python3 tools/build_icons.py --sheets` 可重复
  生成（同机重跑 md5 不变），随代码走而不随本地输出目录消失。
  另有走查当轮留档 `ui_sandbox_out/bug006/icon_frames_showcase.png`。
- `python3 tools/build_icons.py --check` 自检通过；ctest 136/136 绿。
- 留待真机：Windows 资源管理器/任务栏、macOS Finder 图标的**实际显示
  观感**（本轮只证「图标资源在产物里且结构正确」，像素观感需 Win/mac 屏
  对拍）。

**产物面复验（nightly run 37100723843 / sha 2194344，软 alpha 改版之后）**：
- 三平台产物图标与仓库资产**逐字节一致**（`tools/artifact_icon_sheet.py`
  对账，输入全部取自产物而非 `assets/`）：
  | 产物 | 取自 | 对账 |
  | --- | --- | --- |
  | `unidict-windows-x64.zip` 内 `unidict_qml.exe` 的 PE 资源段 | 7 帧还原成独立 .ico 47290B | md5 `94971b13…` == `assets/icons/unidict.ico` |
  | `unidict-macos-arm64.zip` 内 `.app/Contents/Resources/unidict.icns` | magic=icns 声明=实际=141606B 六帧 32…1024 | md5 == `assets/icons/unidict.icns` |
  | `cmake --install` 后 `share/icons/hicolor/256x256/apps/unidict.png` | 256×256 | md5 == `assets/icons/unidict_256.png` |
- **exe 里确实是软 alpha**（首版 8bpp 硬掩码的问题已不在产物里）：资源段
  48216 bytes（首版 305664），帧负载 16/24 = BMP32bpp（XOR+AND）、32 及
  以上 = PNG；七帧「非透明 px ≫ 全不透明 px」全部成立（16px 188/27 →
  256px 27618/20785），覆盖率随尺寸 73.4% → 42.1% 收敛，无空白帧。
- 工具增量：`tools/pe_check.py --dump-icon-dir DIR` 可把任意 exe 的图标组
  还原成标准 .ico（GRPICONDIRENTRY 14 字节 ↔ ICONDIRENTRY 16 字节，偏移
  按拼接位置回填）；`tools/artifact_icon_sheet.py` 出产物帧对照图
  （含「产物帧 − 资产帧」差值面板，纯黑即逐像素相同）。
- 帧对照图（入库）**`docs/icons/product-icon-artifacts.png`**——三平台
  **产物**帧一次排开 + 差值面板，可直接看图核验。
- BUG-007 联动：同一 run 的 macOS bundle 身份键仍为空（首版修复对
  `CFBundleIdentifier` 无效），见该条根因订正与 CI 闸门。

## BUG-005 顶栏 125170 词条但简单词查不出 ✅修复（2026-10-03 登记并修复）

**现象**：GUI 加载词典显示「2 本词典 · 125170 词条」，查简单词
（good/the 等英文高频词、大写形态 Hello/QT）全部查不出。

**根因（双）**：
1. **JsonParser::lookup 大小写敏感**——四个 parser 里唯一精确匹配的
   （stardict/mdict/epub 都查小写折叠键表）。键表 `m_lowerWords` 早已
   在加载期构建（prefixSearch 在用），查词侧漏了折叠回退：Hello 查不到
   词头 hello，只给 Did-you-mean 提示。
2. **查询链不走释义全文索引**——CC-CEDICT 是汉英词典，词头全是汉字，
   英文只存在于释义文本里；`searchWord/searchAll` 只查词头，
   `fullTextSearch`（倒排）有数据有测试但从未接入查询链 → 计数对
   （getWordCount 求和 125170）、英文查询键在词头索引里不存在。

**修复**（core 双点）：
- `json_parser.cpp` lookup：精确 miss → `m_lowerWords` 折叠键回退，
  与其余 parser 同口径。
- `unidict_core.cpp` searchWord/searchAll：词头全 miss →
  `fullTextSearch` 兜底（限 12 条，倒排懒构建+进程内缓存），命中条目
  `metadata.matchType = "fulltext"` 供 UI/CLI 标注「释义匹配」。

**验收（2026-10-03）**：
- **用户词典抽 10 词真查（`ui_sandbox_out/bug005/lookup10.log`，10/10 命中）**：
  源词典 = 用户加载的随包 CC-CEDICT（`dictionaries/ccedict-zh-en.json`，
  125166 词头）+ demo 4 词 = 顶栏 125170 同集；固定随机种子 20261003 从
  **该词典条目本身**抽样（大写形态与短语取自其释义文本，不是外部词表）：
  词头 4（中英/码分多址/马龙区/一时瑜亮）、大写 3（LING/EMERGENCY/NUMBER）、
  短语 3（yao yao/yao ling/ling the）。判据为客观三条件：无「未找到」+
  有释义行 + 大写/短语回显含查询 token。
- CLI 双词典抽词真查：Hello/QT/UNIDICT（大写）命中、good/the 释义匹配
  命中、你好/hello 词头命中——修复前 good/the/Hello/QT/UNIDICT 五类全 miss。
- GUI offscreen 复现用户原样场景（顶栏 125170）：查 good 出 CC-CEDICT
  释义匹配卡片（截图 `ui_sandbox_out` 临时档）。
- 回归测试 `bug005CountAndLookupConsistency`（core_lookup_tests）：
  计数与可查一致性（词头逐一可查）+ 大小写互通 + 全文兜底 matchType
  + 词头命中不掺兜底。
- 词头命中路径无回归：CLI 2.0s（原水平）；BUG-003 八屏 md5 复测 8/8。
- 已知取舍：全文兜底首查多 ~3s（125k 词倒排在查询线程懒构建，进程内
  缓存；GUI 长驻只付一次，CLI 每进程冷启动）。异步化/索引落盘复用
  留后续优化。

**补：std 面同源缺陷（2026-10-03 复查发现并修复）**——首版修复只落在 Qt 面
（`core/json_parser.cpp` + `core/unidict_core.cpp`），而 **core/std/ 是另一套
平行实现**：`unidict_cli_std` 与 Android JNI 走 `DictionaryManagerStd` +
`JsonParserStd`，那里 JSON 解析器仍是精确匹配、大小写敏感，查询链也不接释义
全文索引。实测修复前（用户同一份词典）：`unidict_cli_std Hello` →
`Word not found`，`the`/`good` → **一行输出都没有**直接 exit 7。Android 面
同理（`lookup_jni.cpp` 的 searchAll/lookupDefinition 同走这条路），只是既有
冒烟只查了中文词头（你好）没照出来。

- `JsonParserStd`：装载期建折叠键表（键用 `TextNorm::fold_key`，与
  `IndexEngineStd::exact_match` 归一同口径），`lookup` 精确 miss 时回退——
  Hello/HELLO/ｈｅｌｌｏ/cafe 都能命中 canonical 词形。
- `DictionaryManagerStd::search_word`：词头全 miss → 释义全文兜底（Qt 面
  同口径）。
- `DictionaryManagerStd::search_all` 增 `allow_fulltext_fallback`（**默认
  false**）：prefix/fuzzy 路径是拿候选词逐个回调本方法要释义，兜底在那里会
  往结果里塞无关条目；只有直查入口（Android JNI searchAll、聚合 lookup、
  CLI）显式开启。
- `cli-std` exact 模式：索引没命中时改为直查释义（可带兜底）、逐条标注
  「释义匹配」，确实查不到且**有词典可查**（`indexed_word_count() > 0`）才
  报 `Word not found`。全部词典打不开（加密缺密码等）时保持静默 + exit 7
  ——既有契约（test_cli_std_mdict_password）不因这条改动让步：把「查不了」
  说成「查不到」是倒退。
- 回归测试 `test_std_lookup_parity`（新）：折叠键（大小写/全半角/重音）、
  词头命中不掺兜底、兜底默认关闭/显式开启、假词不凭空造条目、计数与可查
  一致性。**负控已做**：在修复前的 HEAD 隔离 worktree 里同测试编译失败（三参
  重载不存在）/ 行为断言 `jp.lookup("Hello") == hello` 当场断言失败——确认
  这条测试确实咬得住。
- 验收实测（随包 CC-CEDICT + demo，`ui_sandbox_out/bug005/lookup_std_cli.log`）：
  你好 词头命中；Hello/ｈｅｌｌｏ/QT/UNIDICT 词头命中（大小写/全半角折叠）；
  GOOD/the/computer 释义匹配命中并标注来源词典；zzz_not_here 明确
  `Word not found`；无词典时静默 exit 7。计时：词头命中 2.7s（装载为主），
  兜底首查 5.6s（倒排懒构建 +2.9s，与 Qt 面同一取舍）。
- ctest **137/137** 绿（新增 1 条）。

## BUG-003 界面与原型不一致（第二轮报告：上一轮未达标）✅修复+离屏验收通过（2026-10-02 登记，2026-10-03 修复）

**现象**：用户第二轮反馈「界面还是对不上原型」，明确上一轮（BUG-002：
换发 unidict_qml + md5 同图自证）没有达标，不接受"功能差不多"交差。

**升级做法（用户指定）**：逐屏对照 `docs/ui/` 的 Qt Design 设计稿**重做**，
每屏交「原型图 vs 实现截图」左右对照图，差一处改一处，直到与原型一致；
设计稿未覆盖的细节列清单问用户，不自行发挥。

**排查方向**：
1. 本地 offscreen 截图 vs 原型的逐像素差（此前 md5 一致的结论需在当前
   HEAD 复测——中间隔了 M6 语音 tab 新控件等改动）；
2. **分发面环境差**：Windows 包 windeployqt 是否带 Material 样式插件
   （缺插件会静默回落 Default 样式，观感完全不同）、字体差异；
3. 运行态 vs 静态稿：空历史/空生词本/未加载词典提示等动态态原稿未定义。

**验收**：八屏（home/result/vocab/settings × 亮暗）每屏一张
「原型 | 实现」并排对照图入 `docs/ui/compare/`，客观度量（像素差）
达标；双端真实走查截图回传。

**客观度量与根因定位（2026-10-03，ui_sandbox offscreen 1200×760 ×8 屏
vs `docs/ui/`，逐像素容差 8）**：

- **基线（nightly 3973dfe）**：home/result/vocab × 亮暗共 **6 屏 md5
  逐字节一致**；settings 两屏各差 1094px（bbox x781-1171 / y519-683，
  全部在抽屉下部空白带）。
- **根因①（顶/底栏，9380689）**：elevation 2 / 边距 16 / spacing 12 与
  原型（1 / 12 / 10）不符——状态栏文字 ±4px、顶栏 logo +4 / 计数 +6 /
  右侧 tabs −4/−6/−8 位移（按位移量对齐后残差归零，纯参数差不涉及
  阴影渲染）。
- **根因②（settings 抽屉，fa00d2d）**：语音 tab 新增 4 控件使
  StackLayout 隐式高度增大，无显式高度的 Drawer 从内容自适应
  （≈505px，原型口径：抽屉下方露出主窗格）变为全高，盖住了原型中
  可见的列表分隔线与滚动条。隔离 worktree 仅撤该 hunk 复测 →
  **8/8 屏 md5 一致**（含根因①一并回退）。
- **6aa30a7 度量**：与原型差 2.8%-5.5%/屏（25k-50k px，合计 29.9 万
  px），较修复前（3.7k-4.6k px/屏）扩大一个数量级——字号/边距/间距
  全面增大，方向与 `docs/ui/` 设计稿相反。按本缺陷验收口径（对照
  docs/ui）**未达标，不勾选**。对照图：`ui_sandbox_out/bug003/
  compare_6aa30a7/`（现状）与 `compare_reverted/`（回退实验，8/8 一致）。
- **口径提醒**：若以重渲 `docs/ui/*.png` 来达成「一致」，即回到第一轮
  被否的「md5 同图自证」（用户第二轮明确不接受）；设计稿若要更新，
  应先出新旧设计对照并留档，再重制原型图。

**修复落盘（2026-10-03，2646422）**：
- 根因①回退：两笔 Revert（944ae37 / 7f5657c）恢复顶/底栏原型口径
  （elevation 1、边距 12、间距 10、原型字号）。
- 根因②修复：`toolsDrawer` 显式定高 535（实测底沿落 y519，与原型
  抽屉底沿逐像素对齐——首版 519 实得 503 差 16px，校准后归零）；
  语音 tab 增高内容包进 `ScrollView` 兜底，不再撑高抽屉。
- **验收（离屏面）**：ui_sandbox 1200×760 × 8 屏 vs `docs/ui/`
  逐像素容差 8 **全部归零、md5 8/8 逐字节一致**；ctest 136/136 绿。
  对照图 `docs/ui/compare/` 八张已重制（2026-10-03 口径）。
- 待办：Windows/macOS 产物与真机双端走查回传截图（同 BUG-002 口径，
  需对应环境）。

## BUG-004 开箱无真实词典可查 ✅修复+双端走查通过（2026-10-02 登记，2026-10-03 验收）

**现象**：用户反馈装完开箱一个词都查不到（或只有 4 词演示样本，
够不上"真实可用"）。

**现状（修复前）**：随包 `examples/dict.json` 仅 4 词；Android 首启
种子为三格式演示词典共 12 词。

**要求（用户指定）**：首次安装/启动**内置至少一个真实可用词典**，
许可合规优先——CC-CEDICT/CC 授权词典数据打包内置（不许无授权数据）；
桌面与 Android 双端内置；查词冒烟：装完直接查一个单词出释义。

**修复**（两笔提交：perf 装载提速 + feat 内置词典）：
- 资产：CC-CEDICT（CC BY-SA 4.0，MDBG 导出）经 `tools/build_ccedict_dict.py`
  转项目 JSON 格式，出 `dictionaries/ccedict-zh-en.json`（125,166 条
  简体词头）+ `dictionaries/CC-CEDICT-ATTRIBUTION.md` 署名/许可/取舍说明
  （繁体不命中为既定取舍，源文件地址在署名里，用户可自行导入繁体版）。
- 装载性能（惠及所有大词典用户）：`fold_key` 汉字带 0x3400-0x9FFF 快速
  通道（五张折叠表键全在带外，带边界测试）；JSON 解析改 string_view
  直扫（免对象子串/免逐调用 pattern 构造/词数 reserve）——125k 词
  全程 CLI 约 2.1s。
- 桌面分发：`main.cpp` 兜底（UNIDICT_DICTS 未设）收集 exe 同目录 /
  macOS `Contents/Resources` 全部随包词典；CMake POST_BUILD 落位；
  daily-build Windows zip/安装器、macOS bundle、Linux zip 全部 staging
  词典+署名文件，Windows 安装器验证加「计算机→computer」CEDICT 冒烟。
- Android：`assets.srcDir("../../dictionaries")` + 首启种子列表首位加
  `ccedict-zh-en.json`（APK 自带，与演示样张同机制）。

**验收（2026-10-03 双端真实走查）**：
- 桌面：全新环境（无 UNIDICT_DICTS）起 `unidict_qml`，启动日志
  `已加载随包词典: …/ccedict-zh-en.json:…/dict.json`；顶栏
  `2 本词典 · 125170 词条`；查「你好」出释义
  `[ni3 hao3] hello; hi`（来源标注 CC-CEDICT 汉英词典），状态行
  「找到 1 个词典结果」。截图 `ui_sandbox_out/b004_desktop_*.png`。
- Android：全新安装（清数据首启）→ 引导 → `M2-SMOKE-OK dicts=4`；
  查「你好」出 `聚合结果 1 条` + CC-CEDICT 卡片 `[ni3 hao3] hello; hi`；
  词典页 `已装载 4 部`、CC-CEDICT `125166 词`。截图
  `ui_sandbox_out/b004_android_{result,dicts}.png`。
- CLI 冒烟：`UNIDICT_DICTS=ccedict-zh-en.json unidict_cli_std 你好` →
  `你好: [ni3 hao3] hello; hi`；Windows 安装器 CI 验证含同口径冒烟。
- 许可：词典与署名文件进包（AGENTS「不提交词典资产」按用户 BUG-004
  明令覆盖，CC BY-SA 4.0 署名齐备）。

**留档（非本缺陷）**：克隆模拟器（test30 副本，-no-audio）上
M4-TTS-FAIL code=-1，为该环境 TTS 引擎未初始化，与词典/装载无关；
真机与既有验收面口径不变。

## BUG-001 Windows 双击启动 GUI 弹 terminal 控制台窗口 ✅修复+产物验收通过（2026-10-02）

**现象**：Windows 下双击 `unidict_gui.exe`（每日构建包）启动全程带着一个
黑色控制台窗口，GUI 程序不该有。

**根因**（两处叠加）：
1. `gui/CMakeLists.txt` 用裸 `add_executable(unidict_gui ...)`——缺
   `WIN32` 属性即 console 子系统，Windows 装载器为 console 程序分配
   控制台。`qmlui/CMakeLists.txt` 桌面分支同病。
2. AI 桥 `adapters/qt/ai_service_qt.cpp` 的 `QProcess::start` 拉
   外部命令：GUI 进程拉控制台子进程时每次执行闪一个 terminal 窗口。

**修复**（单提交）：
- 两个桌面目标 `add_executable(... WIN32 ...)`（GUI 子系统；Qt6 对
  `WIN32_EXECUTABLE` 目标自动提供 WinMain 入口，无需手写）。
- AI 桥 Windows 下 `setCreateProcessArgumentsModifier` 加
  `CREATE_NO_WINDOW`，子进程静默拉起，输出仍走管道。

**验收**：CI Windows 构建绿；取每日构建产物验 PE 头
subsystem=2（IMAGE_SUBSYSTEM_WINDOWS_GUI）；双击启动（含触发 AI 桥）
零控制台窗口——后一项待 Windows 真机，先以 PE 头 + 代码口径留档。

**产物验收（2026-10-02 run 36982676557）**：安装器 Verify installer 步骤
通过——静默装 + 文件/开始菜单快捷方式/卸载器齐 + `unidict_qml.exe`
PE subsystem = 2 + CLI 冒烟，全链绿（首次跑出的 0 是验收脚本自身偏移
错误 +68→+92，非产物问题，已修）。真机双击走查仍留待用户顺手确认。

**收口补验（2026-10-02 run 37049418211，nightly 刷新到 3973dfe）**：
- windeployqt 样式插件完整进包：Windows zip 内
  `qml\QtQuick\Controls\Material\`（71 项 QML +
  `qtquickcontrols2materialstyleplugin.dll` +
  `Qt6QuickControls2Material(.StyleImpl).dll`）——BUG-003 排查方向 2
  的「缺 Material 插件静默回落 Default 样式」风险在 Windows 分发面
  排除（macOS 同链 `macdeployqt -qmldir=qmlui`，同口径）。
- 包内主程序对：`unidict_qml.exe`（GUI）+ `unidict_cli.exe` /
  `unidict_cli_std.exe`；安装器 Verify 步骤绿（静默装/开始菜单/
  卸载器/PE subsystem=2/样本+CEDICT 冒烟）。
- 包格式命名按本文件口径：`unidict-windows-x64.zip` /
  `unidict-windows-x64-setup.exe`；nightly Release 13 资产清旧传新，
  BUILD_INFO commit=3973dfe。
- 随包词典（BUG-004 联动）：zip 内 `ccedict-zh-en.json`（11.7MB）+
  `CC-CEDICT-ATTRIBUTION.md`。
- 真机双击零控制台窗口一项仍留用户顺手确认（PE subsystem 已=2）。

## BUG-002 GUI 实现与 Qt Design 原型不一致 ✅修复待产物验收（2026-10-02）

**现象**：用户对照仓库 `docs/ui/` 的原型图后判定「GUI 界面和原型根本
不是一个东西」。

**排查（客观度量）**：`docs/ui/` 四屏原型图（home/result/vocab/settings ×
亮暗）与 QML 应用（`MainDesktop.qml`）离屏截图 **md5 逐字节一致**（四屏
全 MATCH）——QML 实现就是原型本体，零像素差。

**根因：分发面发错了程序**。每日构建 Windows/macOS 包的 GUI 主程序一直是
`unidict_gui`（`gui/` Qt Widgets 演示，另一套完全不同的界面），而与原型
同源的 QML 应用 `unidict_qml` 被构建开关 `-DUNIDICT_BUILD_QT_QMLUI=OFF`
排除在分发外。用户按 VERIFY.md 双击 `unidict_gui` → 看到的与原型自然
「根本是两个东西」。

**修复**（单提交）：
- 分发 GUI 主程序换成 `unidict_qml`：Windows `unidict_qml.exe`（windeployqt
  `--qmldir` 收 Quick/Material/Dialogs 插件）；macOS `unidict_qml.app`
  bundle（新增 MACOSX_BUNDLE，macdeployqt `-qmldir`）；Widgets demo 不再进包
- 双击即有词典：`UNIDICT_DICTS` 未设时 main.cpp 兜底加载 exe 同目录 /
  .app `Contents/Resources` 的随包 dict.json（env 显式设置仍优先）
- VERIFY.md / PLATFORM-NOTES / Release notes / install.sh / install.ps1
  文案同步

**验收**：本地走查过——offscreen 运行日志命中「已加载随包词典」，
env 设置时不触发兜底；对照图 `docs/ui/compare/`（原型 | 实现左右并排，
md5 同图为判据）；CI Windows/macOS 构建绿 + 每日构建 dispatch 后产物
内 `unidict_qml` 实跑。

**产物验收（2026-10-02 run 36982676557）**：Windows 安装器内
`unidict_qml.exe` 安装/启动路径验证通过（Verify installer 绿）；macOS
bundle 同班构建绿。文档站「下载」Tab 直链表与 Release 资产名一致。

**产物验收收口·UI 面（2026-10-03，nightly 3973dfe）**：以同提交离屏
渲染对照 `docs/ui/` 原型——home/result/vocab × 亮暗 **6/8 屏 md5 逐字节
一致**；settings 两屏 1094px 差为开发树 fa00d2d 引入（抽屉高度问题，
见 BUG-003 根因②），**非分发面回归**（包内 Material 样式插件齐备，
见上方收口补验）。Linux 包为 CLI 面（仅 `unidict_cli_std`，无 GUI），
Win/mac 产物 GUI 逐屏走查仍需对应环境/真机。BUG-002 口径维持 ✅。

**设计稿缺失清单（待用户定，不自行发挥）**：
1. 原型设置屏右栏「词典」tab 的具体设计——现有截图停在「取词」tab；
   QML 应用目前无词典导入/管理 UI（只认 UNIDICT_DICTS 环境变量），
   「添加自己的词典」入口没有设计稿可依。
2. 顶栏「历史 生词本 设置」与左栏「结果 历史 生词本」tabs 的交互关系
   （重复入口的跳转行为）原型未给说明，现实现按静态稿复刻两处入口。
3. 原型未覆盖的动态态（查词加载中、错误提示、空词典首启引导）无设计稿。
