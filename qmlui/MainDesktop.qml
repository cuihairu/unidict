import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import "components"

ApplicationWindow {
    id: win
    visible: true
    width: 1200
    height: 760
    minimumWidth: 900
    minimumHeight: 560
    title: currentWord && currentWord.length > 0 ? ("Unidict · " + currentWord) : "Unidict"

    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.primary: Theme.card
    Material.accent: Theme.accent
    Material.background: Theme.window
    Material.foreground: Theme.text

    // 关窗到托盘（gui parity，候选#2 F11）：托盘在且「关闭时隐藏到托盘」
    // 开 → 拦截 close 隐藏常驻（热键/剪贴板取词继续可用），退出走托盘
    // 菜单；托盘不可用或开关关 → 默认行为关窗即退出
    onClosing: (close) => {
        if (tray.shouldHideToTray()) {
            close.accepted = false
            tray.notifyHiddenToTray()
            visible = false
        }
    }

    property string currentWord: ""
    property string fallbackHtml: ""
    // fallback 单条原文：字号变化重排时避免 re-lookup（lookupDefinition
    // 命中会再入史），存原文按新字号重建 fallbackHtml
    property string fallbackRawDef: ""
    // 释义字号（巡检续批③ F7）：gui 有 QFontDialog、qmlui 原本硬编码
    // 14px；ui/def_font_size 落盘，外观 tab 档位 radio 即时翻面
    property int defFontSize: settings.getInt("ui/def_font_size", 14)
    // 查询键归一化档位（P1 text-norm）：0 宽容/1 谨慎/2 精确。初始值与
    // main.cpp 装载前落档一致（textnorm/profile 同源）；档位 radio 的
    // checked 绑此属性翻面（lookup.normProfile() 是 Q_INVOKABLE，绑定
    // 不会随换档重求值——须走属性）
    property int normProfileValue: lookup.normProfile()
    property string statusText: ""
    // M3-B 生词本编辑：标签筛选状态 + 全量标签集合（供筛选 chips）
    property string vocabTagFilter: ""
    // 笔记内检索（Search within notes）：非空时生词本列表切到笔记命中集
    property string vocabNoteFilter: ""
    property var vocabTags: []
    property int selectedEntryIndex: 0
    property bool showAllDictionaries: true
    property string currentPronunciation: ""
    // 欧路面板（docs/design-references/eudic-lookup-page.png）：分组聚合
    // 结果 + 卡头音标 + 匹配层级。resultGroups=[{dictionary,dictionaryId,
    // entries:[…]}]（core searchGrouped），flatEntries 为其摊平形态
    property var resultGroups: []
    property var flatEntries: []
    property var headPhonetics: ({})
    property int matchLevel: -1   // 0 精确 / 1 词形还原 / 2 前缀 / 3 释义包含 / 4 模糊
    property bool lastLookupNotFound: false
    // 未收录时的建议词（词条面「未找到」态的可点建议，与侧栏建议同序）
    property var lastSuggestions: []
    property var navBackStack: []
    property var navForwardStack: []
    property var voiceList: []
    property var voicePresetList: []
    property string pronOnlineStatusText: ""
    property bool clipboardEnabled: false
    property bool selectionEnabled: false
    property bool selectionMonitoring: false
    property bool clipboardMonitoring: false
    property int clipboardPollMs: 500
    property int clipboardMinLen: 2
    property int clipboardMaxLen: 50
    // P-5 取词窗形态开关：开=剪贴板取词弹悬浮窗（不打扰前台应用）；
    // 关=回退旧行为（主窗直接展示）。持久化在 quicklookup/enabled
    property bool quickLookupEnabled: true
    property int suggestMode: 0 // 0:auto 1:prefix 2:fuzzy 3:wildcard 4:regex
    property real ttsVolume: 0.8
    property real ttsRate: 1.0
    property real ttsPitch: 0.0
    property string searchQuery: ""
    // P-6 词典库导入面状态（Connections 里 lookup 导入信号驱动）
    property int importProgressTotal: 0
    property int importProgressDone: 0
    property int importProgressFailed: 0
    property string importSummaryText: ""
    // 词典列表/隔离项响应 stamp（函数调用本身不建依赖，先读 stamp 触发
    // 重算——hasEncryptedDictionary 同款模式）
    property var dictMetas: {
        var _stamp = lookup.dictionariesStamp
        return lookup.dictionariesMeta()
    }
    property var dictFailures: {
        var _stamp = lookup.dictionariesStamp
        return lookup.failedDictionaries()
    }
    // 全文索引状态（UI 批二 词典页索引行）：stamp 同款响应式读取
    property var fulltextInfo: {
        var _stamp = lookup.dictionariesStamp
        return lookup.fulltextStats()
    }
    // 根作用域抓取 context 属性：SidebarPanel 子树内同名属性会遮蔽
    property var lookupService: lookup
    // 同理抓 context 的 clipboard（子组件有 property var clip，裸名会被
    // 实例属性遮蔽成自引用——绑定失效取默认 null）
    property var clipService: clip
    // TD-121 拆桥后的 platform/voice 同理：QuickLookupPane 有 property var
    // platform/voice，实例化处裸名会自引用，必须经根作用域中转
    property var platformService: platform
    property var voiceService: voice
    property var vocabService: vocab
    // LAY-001 设置页拆分同理：Settings*Page 有同名字段（platform/
    // settings/startup/syncManager/ai/lookup），实例化处裸名会自引用，
    // 一律经根作用域别名中转
    property var settingsService: settings
    property var startupService: startup
    property var syncManagerService: syncManager
    property var aiService: ai
    // 拼音行只在查询词本身是汉字时展示：查英文词走 fulltext 层时首条
    // 中文条目的 [拼音] 不是当前词的读音，不能冒充卡头音标
    // （CJK 基本区 \u4e00-\u9fff）
    readonly property bool showPinyinPhonetic: (headPhonetics.pinyin || "").length > 0
        && /[\u4e00-\u9fff]/.test(currentWord)

    function hasEncryptedDictionary() {
        var _stamp = lookup.dictionariesStamp
        var metas = lookup.dictionariesMeta()
        for (var i = 0; i < metas.length; ++i) {
            var m = metas[i]
            var desc = m.description ? m.description : ""
            if (desc.indexOf("[encrypted]") !== -1) return true
        }
        return false
    }

    // 外观三态（F4）：拨 Theme.mode 即时生效 + 记忆（gui 面同键 ui/theme）
    function setThemeMode(m) {
        Theme.mode = m
        settings.setInt("ui/theme", m)
    }

    // 界面语言（中英双语，界面重设计批）：拨 Theme.zh 即时翻面（QML 全部
    // 文案绑定 Theme.zh，属性变更自动重求值）+ 记忆 ui/lang
    function setLang(zh) {
        Theme.zh = zh
        settings.setBool("ui/lang", zh)
    }

    function _clampSelectedEntry() {
        if (entriesModel.count <= 0) {
            selectedEntryIndex = 0
            currentPronunciation = ""
            return
        }
        if (selectedEntryIndex < 0) selectedEntryIndex = 0
        if (selectedEntryIndex >= entriesModel.count) selectedEntryIndex = entriesModel.count - 1
        currentPronunciation = entriesModel.get(selectedEntryIndex).pronunciation || ""
    }

    function reloadVoices() {
        voiceList = voice.availableVoices()
        voicePresetList = voice.getVoicePresets()
    }


    function _escapeHtml(s) {
        if (!s) return ""
        return ("" + s)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/\"/g, "&quot;")
            .replace(/'/g, "&#39;")
    }

    function _toHtml(defRaw) {
        if (!defRaw || defRaw.length === 0) return ""
        if (defRaw.indexOf("<") !== -1 && defRaw.indexOf(">") !== -1) {
            return decorateHtml(lookup.sanitizeHtml(defRaw))
        }
        return decorateHtml(_escapeHtml(defRaw).replace(/\n\n+/g, "</p><p>").replace(/\n/g, "<br/>"), true)
    }

    function decorateHtml(html, isAlreadyBody) {
        var body = html || ""
        if (!isAlreadyBody) {
            body = "<div>" + body + "</div>"
        } else {
            body = "<p>" + body + "</p>"
        }
        body = body.replace(/<a\s/gi, "<a style='color:" + Theme.accent + ";text-decoration:none;' ")
        body = body.replace(/<pre/gi, "<pre style='white-space:pre-wrap;font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace;background:" + Theme.window + ";border:1px solid " + Theme.divider + ";border-radius:" + Theme.radiusM + "px;padding:10px;'")
        return "<div style='font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica,Arial; font-size:" + defFontSize + "px; line-height:1.7; color:" + Theme.text + ";'>" +
               body +
               "</div>"
    }

    // 释义字号翻面（F7）：落盘 + 当前查询面按新字号重建（entriesModel 行
    // 与 flatEntries 同序，行内存的是装饰后 html，原始 def 在 flatEntries
    // [.definition]；fallback 单条走 fallbackRawDef 原文重建）
    function applyDefFontSize(n) {
        if (n === defFontSize) return
        defFontSize = n
        settings.setInt("ui/def_font_size", n)
        for (var i = 0; i < entriesModel.count && i < flatEntries.length; i++)
            entriesModel.setProperty(i, "definitionHtml",
                                     decorateHtml(flatEntries[i].definition || ""))
        if (fallbackRawDef.length > 0)
            fallbackHtml = _toHtml(fallbackRawDef)
    }

    // 查询键归一化换档（P1 text-norm）：core 侧即时生效（索引重灌 +
    // 解析器折叠缓存重建），钳位后的实际档位回写属性翻面并落盘
    // textnorm/profile（启动时 main.cpp 装载前按同键落档）
    function applyNormProfile(n) {
        normProfileValue = lookup.setNormProfile(n)
        settings.setInt("textnorm/profile", normProfileValue)
    }

    // 词典移除（F8）：逻辑放 root 而非委托内——removeDictionary 同步发
    // stamp → dictMetas 重绑 → Repeater 即刻销毁委托，委托作用域上续写
    // statusText 会静默丢（S31 实录）；root 函数作用域不受委托销毁影响
    function removeDictAndReport(name) {
        var id = name || ""
        var ok = lookup.removeDictionary(id)
        statusText = ok
            ? (Theme.zh ? ("已移除词典: " + id) : ("Dictionary removed: " + id))
            : (Theme.zh ? ("移除失败: " + id) : ("Remove failed: " + id))
    }

    function _handleLink(link) {
        if (!link || link.length === 0) return
        if (link.indexOf("unidict://lookup") !== 0) return
        var word = ""
        var q = link.indexOf("?word=")
        if (q >= 0) {
            word = decodeURIComponent(link.substring(q + 6))
        } else {
            var slash = link.lastIndexOf("/")
            if (slash >= 0) word = decodeURIComponent(link.substring(slash + 1))
        }
        if (word && word.length > 0) openWord(word)
    }

    function reloadHistory() {
        historyModel.clear()
        // UI 批一 历史命中计数：结构化条目（word+count），同词重查累计
        var entries = vocab.searchHistoryEntries(200)
        for (var i = 0; i < entries.length; i++)
            historyModel.append({ "word": entries[i].word,
                                  "count": entries[i].count,
                                  "pinned": entries[i].pinned === true })
    }

    function reloadVocabulary() {
        vocabModel.clear()
        // 全量 meta 一次拿：标签集合（筛选 chips）取全集，卡片按当前
        // 筛选取子集（core 按标签筛选保持存储序）
        var all = vocab.vocabularyMeta()
        var tags = []
        for (var j = 0; j < all.length; j++) {
            var ts = all[j].tags || []
            for (var k = 0; k < ts.length; k++) {
                if (tags.indexOf(ts[k]) < 0) tags.push(ts[k])
            }
        }
        vocabTags = tags
        var items
        if (vocabNoteFilter.length > 0) {
            // 笔记命中集：searchNotes 只给 {word,text}，回填 meta 保持卡片
            // 形状（定义/标签照常展示；命中词不在 meta 的兜底也进来）
            var hits = vocab.searchNotes(vocabNoteFilter)
            items = []
            for (var m = 0; m < hits.length; m++) {
                var found = null
                for (var q = 0; q < all.length; q++) {
                    if ((all[q].word || "") === hits[m].word) { found = all[q]; break }
                }
                items.push(found || { "word": hits[m].word, "definition": "",
                                     "tags": [] })
            }
        } else {
            items = vocabTagFilter.length > 0
                ? vocab.vocabularyByTag(vocabTagFilter) : all
        }
        for (var i = 0; i < items.length; i++) {
            var word = items[i].word || ""
            var def = items[i].definition || ""
            var snippet = lookup.extractTextFromHtml(def).replace(/\s+/g, " ").trim()
            vocabModel.append({
                "word": word,
                "snippet": snippet.length > 90 ? (snippet.substring(0, 90) + "…") : snippet,
                "tags": (items[i].tags || []).join(";"),
                "note": vocab.getVocabNote(word),
                "noteColor": vocab.getVocabNoteColor(word)
            })
        }
    }

    // UI 批一 搜索建议分组：精确（=查询词，大小写不敏感）→ 前缀（以查询
    // 词开头）→ 词条（其余，模糊/通配等），组内保持检索器原序
    function suggestSection(item, q) {
        var lo = item.toLowerCase()
        var ql = q.toLowerCase()
        if (lo === ql) return Theme.zh ? "精确" : "Exact"
        if (lo.indexOf(ql) === 0) return Theme.zh ? "前缀" : "Prefix"
        return Theme.zh ? "词条" : "Entries"
    }

    // 联想段 → 段头文案：core suggest_sections 的 kind（英文枚举）→ 界面
    // 语言。新增容错段：乱序（同字母异序）/ 包含（子串）/ 通配（模式）
    function suggestKindLabel(kind) {
        var map = Theme.zh
            ? { exact: "精确", prefix: "前缀", anagram: "乱序",
                substring: "包含", wildcard: "通配", fuzzy: "词条" }
            : { exact: "Exact", prefix: "Prefix", anagram: "Anagram",
                substring: "Contains", wildcard: "Wildcard", fuzzy: "Entries" }
        return map[kind] !== undefined ? map[kind] : map.fuzzy
    }

    function reloadSuggestions(prefix) {
        resultsModel.clear()
        if (!prefix || prefix.trim().length === 0) return
        var q = prefix.trim()
        var items = []
        try {
            if (suggestMode === 0) {
                // 自动档：一次调用取齐分段候选（精确→前缀→乱序→通配/包含
                // →词条）。core 侧已跨段去重、扫描段带耗时上限，大词典下
                // 不卡 UI 线程；前缀/乱序走导入时建的索引 O(前缀长+结果数)
                var secs = lookup.suggestAll(q, 50) || []
                for (var si = 0; si < secs.length; si++) {
                    var label = suggestKindLabel(secs[si].kind)
                    var ws = secs[si].words || []
                    for (var wi = 0; wi < ws.length; wi++)
                        resultsModel.append({ "word": ws[wi], "section": label })
                }
                return
            }
            if (suggestMode === 1) {
                items = lookup.suggestPrefix(q, 50)
            } else if (suggestMode === 2) {
                items = lookup.suggestFuzzy(q, 50)
            } else if (suggestMode === 3) {
                items = lookup.searchWildcard(q, 50)
            } else if (suggestMode === 4) {
                items = lookup.searchRegex(q, 50)
            } else {
                items = lookup.suggestPrefix(q, 50)
                if (!items || items.length === 0) items = lookup.suggestFuzzy(q, 50)
            }
        } catch (e) {
            items = []
        }
        // ListView section 只认连续段：按 精确→前缀→词条 稳定重排
        // （section 键随界面语言出 zh/en，段头文案即模型 section 值）
        var sec = Theme.zh
            ? { "精确": [], "前缀": [], "词条": [] }
            : { "Exact": [], "Prefix": [], "Entries": [] }
        for (var i = 0; i < items.length; i++)
            sec[suggestSection(items[i], q)].push(items[i])
        var order = Object.keys(sec)
        for (var s = 0; s < order.length; s++)
            for (var j = 0; j < sec[order[s]].length; j++)
                resultsModel.append({ "word": sec[order[s]][j],
                                      "section": order[s] })
    }

    // 复位到查词主页空态：供离屏截图沙盒逐场景重放（openWord/切 tab 的
    // 状态会跨尺寸与主题残留）。不动词典/生词本/历史数据，只清查询面。
    function resetHome() {
        currentWord = ""
        searchQuery = ""
        fallbackHtml = ""
        fallbackRawDef = ""
        lastLookupNotFound = false
        selectedEntryIndex = 0
        navBackStack = []
        navForwardStack = []
        entriesModel.clear()
        resultsModel.clear()
        resultGroups = []
        flatEntries = []
        headPhonetics = ({})
        matchLevel = -1
        lastSuggestions = []
        // 错误/空态文案不暴露环境变量（界面重设计批）：给用户下一步动作
        statusText = lookup.loadedDictionaries().length > 0
            ? (Theme.zh ? "就绪" : "Ready")
            : (Theme.zh ? "未加载词典：请在「设置 → 词典」导入词典目录"
                        : "No dictionaries loaded — import a folder in Settings → Dictionaries")
    }

    function openWord(word, recordNav) {
        if (!word || word.trim().length === 0) return
        if (recordNav === undefined) recordNav = true
        var w = word.trim()

        if (recordNav && currentWord && currentWord.length > 0 && currentWord !== w) {
            // BUG-010：push/pop 原地改数组不发 changed 信号，enabled 绑定
            // （navBackStack.length > 0）永不重评估 → ←/→ 永远禁用，点了
            // 没反应。必须整赋新数组（copy-on-write）触发绑定
            var backStack = navBackStack.slice()
            backStack.push(currentWord)
            if (backStack.length > 100) backStack.shift()
            navBackStack = backStack
            navForwardStack = []
        }

        searchQuery = w
        currentWord = w
        fallbackHtml = ""
        fallbackRawDef = ""
        lastLookupNotFound = false

        entriesModel.clear()
        resultGroups = []
        flatEntries = []
        headPhonetics = ({})
        matchLevel = -1

        var groups = lookup.aggregateLookup(w, {
            "maxTotalResults": 20,
            "sanitizeHtml": true,
            "rewriteCrossRefs": true
        })

        var firstDefText = ""
        if (groups && groups.length > 0) {
            var flat = 0
            for (var g = 0; g < groups.length; g++) {
                var gg = groups[g]
                for (var i = 0; i < gg.entries.length; i++) {
                    var e = gg.entries[i]
                    var dict = e.dictionary || gg.dictionary || "unknown"
                    var defHtml = e.definition || ""
                    var defText = lookup.extractTextFromHtml(defHtml)
                    entriesModel.append({
                        "word": e.word || w,
                        "dictionary": dict,
                        "pronunciation": e.pronunciation || "",
                        "definitionHtml": decorateHtml(defHtml),
                        "definitionText": defText
                    })
                    if (flatEntries.length === 0) matchLevel = e.relevance
                    flatEntries.push(e)
                    if (firstDefText === "") firstDefText = defText
                    flat++
                }
            }
            // 分组卡用带 entries 的原始组结构；组内条目与 flat 视图同序
            resultGroups = groups
            if (firstDefText.length > 0) headPhonetics = lookup.extractPhonetics(firstDefText)
            selectedEntryIndex = 0
            _clampSelectedEntry()
            lastSuggestions = []
            statusText = Theme.zh
                ? ("找到 " + entriesModel.count + " 个词典结果")
                : ("Found " + entriesModel.count + " dictionary results")
        } else {
            var def = lookup.lookupDefinition(w)
            fallbackRawDef = def || ""
            fallbackHtml = _toHtml(def)
            lastLookupNotFound = def && def.startsWith("Word not found")
            if (lastLookupNotFound) {
                // 错误文案中文化（界面重设计批）：不给原始英文错误，给
                // 「未找到，请试试其他词」+ 建议词（前缀层优先，空则模糊层）
                var sugg = lookup.suggestPrefix(w, 5)
                if (!sugg || sugg.length === 0) sugg = lookup.suggestFuzzy(w, 5)
                lastSuggestions = sugg || []
                statusText = (Theme.zh ? "未找到，请试试其他词"
                                       : "Not found — try another word")
                    + (lastSuggestions.length > 0
                        ? (Theme.zh ? "：" : ": ")
                          + lastSuggestions.join(Theme.zh ? "、" : ", ")
                        : "")
                reloadSuggestions(w)
            } else {
                lastSuggestions = []
                statusText = Theme.zh ? ("已查询: " + w) : ("Searched: " + w)
            }
        }
        reloadHistory()
    }

    function goBack() {
        if (navBackStack.length <= 0) return
        // 同 openWord：整赋触发 changed，enabled 绑定重评估
        var backStack = navBackStack.slice()
        var prev = backStack.pop()
        navBackStack = backStack
        if (currentWord && currentWord.length > 0) navForwardStack = navForwardStack.concat([currentWord])
        openWord(prev, false)
    }

    function goForward() {
        if (navForwardStack.length <= 0) return
        var fwdStack = navForwardStack.slice()
        var next = fwdStack.pop()
        navForwardStack = fwdStack
        if (currentWord && currentWord.length > 0) navBackStack = navBackStack.concat([currentWord])
        openWord(next, false)
    }

    // BUG-010：词条卡朗读统一入口——点击必有状态行反馈（此前本地 TTS
    // 不可用时静默，用户视角就是「点了没反应」）；英/美喇叭经
    // speakWordWithAccent 区分口音（在线发音源），本地 TTS 缺失时明说
    // 去哪开。accent 用发音源口音枚举（0 自动/1 美/2 英/3 澳），-1 表
    // 无口音语义（朗读/拼音）
    function speakHeadword(label, accent) {
        if (voice.pronSourceMode() === 0 && !voice.hasLocalTts()) {
            statusText = Theme.zh
                ? "本地语音不可用，可在 设置→语音 换用在线发音"
                : "Local voice unavailable — switch to online in Settings → Voice"
            return
        }
        statusText = Theme.zh
            ? ("正在朗读（" + label + "）: " + currentWord)
            : ("Speaking (" + label + "): " + currentWord)
        voice.speakWordWithAccent(currentWord, accent)
    }

    // P-5 取词窗统一入口：word 来自剪贴板取词或热键读剪贴板
    function showQuickLookupFor(word) {
        quickLookupPane.showFor(word)
    }

    // quick_lookup 热键文本源升级（F13a）：优先读焦点窗选区（Windows UIA
    // TextPattern；其余平台恒空串），空/全标点回落剪贴板路径（原语义逐字节
    // 不变）。选区>200 字符给专属提示、不回落剪贴板——长选区是用户明确的
    // 取词意图，悄悄换成剪贴板旧内容反而惊吓
    // F13b（macOS，AX API）：开关关 → 直接走剪贴板（尊重用户不读选区的
    // 选择）；开关开但未授权 → 触发一次系统授权提示（方案 d-a，仅一次，
    // 之后由设置页内联引导接管），本次读不到、回落剪贴板
    property bool macAccessibilityPrompted: false
    function quickLookupFromFocusedSelection() {
        if (platform.isMacSelectionReaderSupported()
                && !settings.getBool("quicklookup/macReadSelection", true)) {
            quickLookupFromClipboard()
            return
        }
        if (platform.isMacSelectionReaderSupported()
                && !platform.isMacAccessibilityTrusted()
                && !macAccessibilityPrompted) {
            macAccessibilityPrompted = true
            platform.requestMacAccessibilityPrompt()
        }
        var t = (platform.readFocusedSelection() || "").replace(/\s+/g, " ").trim()
        if (!t) {
            quickLookupFromClipboard()
            return
        }
        t = t.replace(/^[\s"'“”‘’(\[{]+/, "").replace(/[\s"'“”‘’)\]},.;:!?]+$/, "")
        if (!t) {
            quickLookupFromClipboard()
            return
        }
        if (t.length > 200) {
            statusText = Theme.zh ? "选区内容过长（>200 字符），不是取词场景"
                                  : "Selection too long (>200 chars) — not a lookup"
            return
        }
        quickLookupPane.showFor(t)
    }

    // quick_lookup 热键面：读剪贴板，掐首尾标点后交给取词窗。剪贴板
    // 常是整句（复制即查），短语条目在词头库真实存在，不强行截成单词；
    // 超过 200 字符的段落不属于「取词」意图，直接放弃并提示
    function quickLookupFromClipboard() {
        var t = (clip.text() || "").replace(/\s+/g, " ").trim()
        if (!t) {
            statusText = Theme.zh ? "剪贴板为空，没有可取的词"
                                  : "Clipboard is empty"
            return
        }
        t = t.replace(/^[\s"'“”‘’(\[{]+/, "").replace(/[\s"'“”‘’)\]},.;:!?]+$/, "")
        if (!t) {
            statusText = Theme.zh ? "剪贴板里没有可取的词"
                                  : "Nothing look-up-able in the clipboard"
            return
        }
        if (t.length > 200) {
            statusText = Theme.zh ? "剪贴板内容过长（>200 字符），不是取词场景"
                                  : "Clipboard too long (>200 chars) — not a lookup"
            return
        }
        quickLookupPane.showFor(t)
    }

    Component.onCompleted: {
        Theme.zh = settings.getBool("ui/lang", true)
        reloadHistory()
        reloadVocabulary()
        ttsVolume = voice.getVolume()
        ttsRate = voice.getRate()
        ttsPitch = voice.getPitch()
        clipboardEnabled = platform.isClipboardAutoLookupEnabled()
        clipboardMonitoring = platform.isClipboardMonitoring()
        if (clipboardEnabled && !clipboardMonitoring) {
            platform.startClipboardMonitoring()
        }
        // 划词取词（X11 PRIMARY selection）：持久化开关自启；非 xcb
        // 平台 isSupported=false，开关不可见、start no-op
        selectionEnabled = settings.getBool("selection/enabled", false)
        selectionMonitoring = platform.isSelectionMonitoring()
        if (selectionEnabled && platform.isSelectionMonitoringSupported()
                && !selectionMonitoring) {
            platform.startSelectionMonitoring()
        }
        // P-5 取词窗：开关持久化 + 系统级热键自注册（仅支持平台生效；
        // Wayland 等不支持平台注册失败，由设置页提示）
        quickLookupEnabled = settings.getBool("quicklookup/enabled", true)
        if (platform.isGlobalHotkeysSupported()) {
            var hk = settings.getString("quicklookup/hotkey", "Alt+Q")
            if (hk.length > 0) platform.registerGlobalHotkey("quick_lookup", hk)
        }
        searchQuery = ""
        statusText = lookup.loadedDictionaries().length > 0
            ? (Theme.zh ? "就绪" : "Ready")
            : (Theme.zh ? "未加载词典：请在「设置 → 词典」导入词典目录"
                        : "No dictionaries loaded — import a folder in Settings → Dictionaries")
    }

    header: ToolBar {
        Material.elevation: 0
        background: Rectangle {
            color: Theme.panel
            // hairline 分隔替代 Material 投影：扁平层次，靠表面色阶区分
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 8
            spacing: 12

            // 品牌行（原型 topbar 口径）：玫瑰渐变 logomark + 15px 字标
            RowLayout {
                spacing: 9
                Rectangle {
                    implicitWidth: 22
                    implicitHeight: 22
                    radius: Theme.radiusS
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#d81b6a" }
                        GradientStop { position: 1.0; color: "#b11964" }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "U"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: "#ffffff"
                    }
                }
                Label {
                    text: "Unidict"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.2
                    color: Theme.text
                }
            }

            // 词典状态 chip：胶囊底 + accent 点，加载态一眼可辨
            Rectangle {
                radius: height / 2
                implicitHeight: 24
                implicitWidth: statusLabel.implicitWidth + 26
                color: "transparent"
                border.width: 1
                border.color: Theme.divider
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 6; height: 6; radius: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: lookup.loadedDictionaries().length > 0 ? Theme.success : Theme.warning
                    }
                    Label {
                        id: statusLabel
                        anchors.verticalCenter: parent.verticalCenter
                        text: {
                            var _stamp = lookup.dictionariesStamp
                            var n = lookup.loadedDictionaries().length
                            if (n === 0)
                                return Theme.zh ? "未加载词典" : "No dictionaries"
                            return Theme.zh
                                ? (n + " 本词典 · " + lookup.indexedWordCount() + " 词条")
                                : (n + " dictionaries · " + lookup.indexedWordCount() + " entries")
                        }
                        font.pixelSize: 12
                        color: Theme.textSecondary
                    }
                }
            }

            Item { Layout.fillWidth: true }

            component HeaderGhostButton : ToolButton {
                property string glyph: ""
                leftPadding: 10
                rightPadding: 10
                spacing: 6
                background: Rectangle {
                    radius: Theme.radiusM
                    color: parent.hovered ? Theme.hoverOverlay : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                contentItem: RowLayout {
                    spacing: 6
                    Icon {
                        visible: glyph.length > 0
                        name: glyph
                        size: 15
                        color: hovered ? Theme.text : Theme.textSecondary
                    }
                    Text {
                        text: parent.parent.text
                        font.pixelSize: 13
                        color: parent.parent.hovered ? Theme.text : Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
            HeaderGhostButton {
                objectName: "headerHistoryButton"
                glyph: "clock"
                text: Theme.zh ? "历史" : "History"
                onClicked: leftPane.currentTabIndex = 1
            }
            HeaderGhostButton {
                objectName: "headerVocabButton"
                glyph: "bookOpen"
                text: Theme.zh ? "生词本" : "Wordbook"
                onClicked: leftPane.currentTabIndex = 2
            }
            HeaderGhostButton {
                objectName: "headerSettingsButton"
                glyph: "settings"
                text: Theme.zh ? "设置" : "Settings"
                onClicked: toolsDrawer.open()
            }
        }
    }

    SplitView {
        anchors.fill: parent
        // 8px 栅格：贴边布局（原型 app 行无外边距），层次靠 panel/card
        // 表面色阶 + hairline，不再靠悬浮留白
        anchors.margins: 0

        SidebarPanel {
            id: leftPane
            objectName: "leftPane"
            // win./id 限定：SidebarPanel 作用域内同名实例属性会遮蔽外层
            // 绑定（自引用循环→绑定失效取默认值）。resultsModel 等是 id，
            // id 解析优先于实例属性，裸名即可
            suggestMode: win.suggestMode
            currentWord: win.currentWord
            resultsModel: resultsModel
            historyModel: historyModel
            vocabModel: vocabModel
            lookup: win.lookupService
            vocabTags: win.vocabTags
            vocabTagFilter: win.vocabTagFilter
            vocabNoteFilter: win.vocabNoteFilter
            // win. 限定：这里处于 SidebarPanel 作用域，同名属性会遮蔽根函数
            hasEncryptedDictionary: win.hasEncryptedDictionary()
            mdictPasswordPlaceholder: lookupService.hasMdictPassword()
                ? "MDict password is set (enter to replace)"
                : "Enter MDict password"
            queryText: searchQuery
            onSuggestModeSelected: function(index) {
                win.suggestMode = index
                reloadSuggestions(searchQuery)
            }
            onQueryTextEdited: function(text) {
                searchQuery = text
                suggestTimer.restart()
            }
            onQuerySubmitted: function(text) {
                searchQuery = text
                openWord(text)
            }
            onQueryCleared: {
                searchQuery = ""
                resultsModel.clear()
            }
            onApplyPasswordRequested: function(password) {
                if (!lookup.setMdictPassword(password)) return
                var ok = lookup.reloadDictionariesFromEnv()
                statusText = ok
                    ? (Theme.zh ? "已重新加载词典" : "Dictionaries reloaded")
                    : (Theme.zh ? "重新加载失败（请检查词典目录）"
                                : "Reload failed — check your dictionary folder")
            }
            onClearPasswordRequested: {
                lookup.clearMdictPassword()
                statusText = "已清除 MDict 密码"
            }
            onHistoryTabRequested: reloadHistory()
            // 历史置顶（F3）：core 置顶块落库 → 重载（行序随置顶块变化）
            onHistoryPinToggled: function(word, pinned) {
                vocab.setSearchHistoryPinned(word, pinned)
                reloadHistory()
            }
            // 历史逐条删除（F5）：core 删该条 → 重载
            onHistoryRemoveRequested: function(word) {
                vocab.removeSearchHistory(word)
                reloadHistory()
            }
            onVocabularyTabRequested: reloadVocabulary()
            // —— M3-B 生词本编辑：变更 → core 落库 → 重载模型 + 状态行 ——
            // 两处 filter 写 win.：SidebarPanel 声明了同名属性，裸名赋值
            // 会落实例本地（遮蔽根属性），reloadVocabulary 读不到
            onVocabTagFilterRequested: function(tag) {
                win.vocabTagFilter = tag
                reloadVocabulary()
            }
            onVocabNoteFilterRequested: function(text) {
                win.vocabNoteFilter = text
                reloadVocabulary()
            }
            onVocabAddTagRequested: function(word, tag) {
                if (vocab.addVocabTag(word, tag)) {
                    reloadVocabulary()
                    statusText = "已加标签「" + tag + "」→ " + word
                } else {
                    statusText = "加标签失败（空标签或词条不存在）"
                }
            }
            onVocabRemoveTagRequested: function(word, tag) {
                if (vocab.removeVocabTag(word, tag)) {
                    reloadVocabulary()
                    statusText = "已移除标签「" + tag + "」→ " + word
                } else {
                    statusText = "移除标签失败（词条或标签未命中）"
                }
            }
            onVocabNoteSaveRequested: function(word, note, color) {
                vocab.setVocabNoteColored(word, note, color)
                reloadVocabulary()
                statusText = note.length > 0 ? "已保存笔记 → " + word
                                             : "已清除笔记 → " + word
            }
            onVocabExportRequested: vocabExportDialog.open()
            onVocabImportRequested: vocabImportDialog.open()
            onNotesExportRequested: notesExportDialog.open()
            onResultWordRequested: function(word) {
                searchQuery = word
                openWord(word)
            }
        }

        Pane {
            id: rightPane
            SplitView.fillWidth: true
            Material.elevation: 0
            // 8px 栅格：24px 内边距，内容坐在 window画布上（分组卡自承
            // 结构），不再给整块面板套悬浮表面
            padding: 24
            background: Rectangle { color: Theme.window }

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    // 词条卡头（欧路口径，docs/design-references/
                    // eudic-lookup-page.png）：大号词头 + 匹配层级灰标签 +
                    // 音标行（英/美喇叭）+ 生词本/笔记/复制轻入口
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            spacing: 8

                            Label {
                                visible: currentWord && currentWord.length > 0
                                text: currentWord
                                // 词条排版层级（原型 entry-head）：34px 大词头
                                font.pixelSize: 34
                                font.weight: Font.DemiBold
                                font.letterSpacing: -0.8
                                color: Theme.text
                                elide: Text.ElideRight
                            }

                            // 匹配层级标签：非精确层或未收录才有（词典没有
                            // 词频/考试数据，不造标签；真实层级标注见
                            // resultGroups 的 relevance）。欧路考试标签 chip
                            // 口径：细描边圆角胶囊、浅灰字
                            Rectangle {
                                visible: matchLevel > 0 || lastLookupNotFound
                                radius: height / 2
                                color: "transparent"
                                border.width: 1
                                border.color: Theme.divider
                                implicitWidth: levelChip.implicitWidth + 14
                                implicitHeight: levelChip.implicitHeight + 5

                                Label {
                                    id: levelChip
                                    anchors.centerIn: parent
                                    objectName: "levelChip"
                                    text: lastLookupNotFound
                                        ? (Theme.zh ? "未收录" : "Not found")
                                        : (matchLevel === 1
                                            ? (Theme.zh ? "原形匹配" : "Lemma")
                                            : matchLevel === 2
                                            ? (Theme.zh ? "前缀匹配" : "Prefix")
                                            : matchLevel === 4
                                            ? (Theme.zh ? "模糊匹配" : "Fuzzy")
                                            : (Theme.zh ? "释义匹配" : "Definition"))
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        // 音标行（原型 entry-head IPA 口径）：描边喇叭图标 +
                        // 语种标签浅灰、音标值正文色；点击必有状态反馈（BUG-010）
                        RowLayout {
                            spacing: 16
                            visible: entriesModel.count > 0 || fallbackHtml.length > 0

                            // 英音
                            RowLayout {
                                visible: (headPhonetics.british || "").length > 0
                                spacing: 4

                                Item {
                                    objectName: "phonSpeakerBr"
                                    implicitWidth: 16
                                    implicitHeight: 16
                                    Icon {
                                        anchors.centerIn: parent
                                        name: "volume"
                                        size: 16
                                        color: Theme.textTertiary
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: win.speakHeadword(Theme.zh ? "英音" : "UK", 2)
                                    }
                                }

                                Label {
                                    text: Theme.zh ? "英" : "UK"
                                    color: Theme.textTertiary
                                    font.pixelSize: 12
                                }

                                Label {
                                    text: headPhonetics.british || ""
                                    color: Theme.textSecondary
                                    font.pixelSize: 14
                                }
                            }

                            // 美音
                            RowLayout {
                                visible: (headPhonetics.american || "").length > 0
                                spacing: 4

                                Item {
                                    objectName: "phonSpeakerUs"
                                    implicitWidth: 16
                                    implicitHeight: 16
                                    Icon {
                                        anchors.centerIn: parent
                                        name: "volume"
                                        size: 16
                                        color: Theme.textTertiary
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: win.speakHeadword(Theme.zh ? "美音" : "US", 1)
                                    }
                                }

                                Label {
                                    text: Theme.zh ? "美" : "US"
                                    color: Theme.textTertiary
                                    font.pixelSize: 12
                                }

                                Label {
                                    text: headPhonetics.american || ""
                                    color: Theme.textSecondary
                                    font.pixelSize: 14
                                }
                            }

                            // 拼音（仅中文查询开它时显示）
                            RowLayout {
                                visible: win.showPinyinPhonetic
                                spacing: 4

                                Item {
                                    objectName: "phonSpeakerPy"
                                    implicitWidth: 16
                                    implicitHeight: 16
                                    Icon {
                                        anchors.centerIn: parent
                                        name: "volume"
                                        size: 16
                                        color: Theme.textTertiary
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: win.speakHeadword(Theme.zh ? "拼音" : "Pinyin", -1)
                                    }
                                }

                                Label {
                                    text: Theme.zh ? "拼" : "Py"
                                    color: Theme.textTertiary
                                    font.pixelSize: 12
                                }

                                Label {
                                    text: "[" + (headPhonetics.pinyin || "") + "]"
                                    color: Theme.textSecondary
                                    font.pixelSize: 14
                                }
                            }

                            // 无音标数据时给一个总喇叭（TTS 朗读词头）
                            Item {
                                visible: entriesModel.count > 0
                                         && (headPhonetics.british || "").length === 0
                                         && (headPhonetics.american || "").length === 0
                                         && !win.showPinyinPhonetic
                                objectName: "phonSpeakerFallback"
                                implicitWidth: fallbackInner.implicitWidth
                                implicitHeight: fallbackInner.implicitHeight

                                RowLayout {
                                    id: fallbackInner
                                    spacing: 4

                                    Icon {
                                        name: "volume"
                                        size: 16
                                        color: Theme.textTertiary
                                    }
                                    Label {
                                        text: Theme.zh ? "朗读" : "Play"
                                        color: Theme.textTertiary
                                        font.pixelSize: 13
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: win.speakHeadword(Theme.zh ? "朗读" : "Play", -1)
                                }
                            }

                            Item { Layout.fillWidth: true }

                            // 欧路口径：音标行与轻操作之间一道细竖线分隔
                            Rectangle {
                                visible: currentWord.length > 0
                                width: 1
                                height: 14
                                color: Theme.divider
                            }

                            Item {
                                visible: currentWord.length > 0 && entriesModel.count > 0
                                objectName: "vocabAction"
                                implicitWidth: vocabActionRow.implicitWidth
                                implicitHeight: vocabActionRow.implicitHeight

                                RowLayout {
                                    id: vocabActionRow
                                    spacing: 4
                                    Icon {
                                        name: "bookOpen"
                                        size: 14
                                        color: Theme.textSecondary
                                    }
                                    Label {
                                        text: Theme.zh ? "生词本" : "Wordbook"
                                        color: Theme.textSecondary
                                        font.pixelSize: 13
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        vocab.addToVocabulary(currentWord, entriesModel.get(0).definitionHtml)
                                        reloadVocabulary()
                                        statusText = Theme.zh
                                            ? ("已加入生词本: " + currentWord)
                                            : ("Added to wordbook: " + currentWord)
                                    }
                                }
                            }

                            Item {
                                visible: currentWord.length > 0
                                objectName: "noteAction"
                                implicitWidth: noteActionRow.implicitWidth
                                implicitHeight: noteActionRow.implicitHeight

                                RowLayout {
                                    id: noteActionRow
                                    spacing: 4
                                    Icon {
                                        name: "pencil"
                                        size: 14
                                        color: Theme.textSecondary
                                    }
                                    Label {
                                        text: Theme.zh ? "笔记" : "Note"
                                        color: Theme.textSecondary
                                        font.pixelSize: 13
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        notePopup.noteWord = currentWord
                                        noteArea.text = vocab.getVocabNote(currentWord)
                                        notePopup.noteColor = vocab.getVocabNoteColor(currentWord)
                                        notePopup.open()
                                    }
                                }
                            }

                            Item {
                                visible: entriesModel.count > 0 || fallbackHtml.length > 0
                                objectName: "copyAction"
                                implicitWidth: copyActionRow.implicitWidth
                                implicitHeight: copyActionRow.implicitHeight

                                RowLayout {
                                    id: copyActionRow
                                    spacing: 4
                                    Icon {
                                        name: "copy"
                                        size: 14
                                        color: Theme.textSecondary
                                    }
                                    Label {
                                        text: Theme.zh ? "复制" : "Copy"
                                        color: Theme.textSecondary
                                        font.pixelSize: 13
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        // win. 限定：本 MouseArea 处于 Item 作用域，
                                        // 裸名 clip 命中 QQuickItem.clip 布尔属性
                                        // （复制点击一直是 TypeError——BUG-010 主诉之一）
                                        if (entriesModel.count > 0) {
                                            win.clipService.setText(entriesModel.get(0).definitionText)
                                            statusText = Theme.zh
                                                ? ("已复制释义 · " + entriesModel.get(0).dictionary)
                                                : ("Definition copied · " + entriesModel.get(0).dictionary)
                                        } else {
                                            win.clipService.setText(lookup.extractTextFromHtml(fallbackHtml))
                                            statusText = Theme.zh ? "已复制" : "Copied"
                                        }
                                    }
                                }
                            }

                            // AI 语境造句（roadmap AI sentence generation）：
                            // 外部命令桥 UNIDICT_AI_CMD 的 sentences 子命令，
                            // 未配置走启发式 mock（输出自标 [Mock sentences]）
                            Item {
                                visible: currentWord.length > 0
                                objectName: "aiSentenceAction"
                                implicitWidth: aiActionRow.implicitWidth
                                implicitHeight: aiActionRow.implicitHeight

                                RowLayout {
                                    id: aiActionRow
                                    spacing: 4
                                    Icon {
                                        name: "sparkles"
                                        size: 14
                                        color: Theme.textSecondary
                                    }
                                    Label {
                                        text: Theme.zh ? "造句" : "Sentences"
                                        color: Theme.textSecondary
                                        font.pixelSize: 13
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var out = ai.generateSentences(currentWord)
                                        aiSentenceText.text = out.length > 0
                                            ? out
                                            : (Theme.zh
                                                ? "（AI 例句生成失败：外部命令无输出）"
                                                : "(AI sentence generation failed: no output)")
                                        aiSentencePopup.aiSentenceWord = currentWord
                                        aiSentencePopup.open()
                                        statusText = Theme.zh
                                            ? ("已生成 AI 例句 · " + currentWord)
                                            : ("AI sentences ready · " + currentWord)
                                    }
                                }
                            }
                        }
                    }

                    ToolButton {
                        objectName: "navBackButton"
                        enabled: navBackStack.length > 0
                        onClicked: goBack()
                        ToolTip.visible: hovered
                        ToolTip.text: Theme.zh ? "返回" : "Back"
                        ToolTip.delay: 200
                        contentItem: Icon {
                            name: "arrowLeft"
                            size: 16
                            color: parent.enabled ? Theme.textSecondary : Theme.textDisabled
                        }
                    }

                    ToolButton {
                        objectName: "navForwardButton"
                        enabled: navForwardStack.length > 0
                        onClicked: goForward()
                        ToolTip.visible: hovered
                        ToolTip.text: Theme.zh ? "前进" : "Forward"
                        ToolTip.delay: 200
                        contentItem: Icon {
                            name: "arrowRight"
                            size: 16
                            color: parent.enabled ? Theme.textSecondary : Theme.textDisabled
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // D3：结果面板去外层硬边框盒——分组卡自承结构（与面板
                    // 侧 background:transparent 同一口径），内容直接坐在
                    // rightPane 表面上
                    background: Rectangle { color: "transparent"; border.width: 0 }

                    EntryResultsPane {
                        anchors.fill: parent
                        // win./id 限定同名遮蔽（词头行高亮、例句 tab 的
                        // lookup 调用、空态回落都依赖这些绑定真实生效）
                        groups: resultGroups
                        flatEntries: win.flatEntries
                        entriesModel: entriesModel   // id，裸名即 id
                        currentWord: win.currentWord
                        fallbackHtml: win.fallbackHtml
                        defFontSize: win.defFontSize
                        lookup: win.lookupService
                        clip: win.clipService
                        voice: win.voiceService
                        // 空态/未收录态（界面重设计批）：单一居中空态 +
                        // 「未找到」建议态，原始英文错误不再直出
                        notFound: win.lastLookupNotFound
                        notFoundWord: win.currentWord
                        suggestions: win.lastSuggestions
                        onStatusReported: function(value) { statusText = value }
                        onLinkActivated: function(link) { _handleLink(link) }
                        onWordRequested: function(word) { openWord(word) }
                        onEmptyLookupRequested: function(word) { openWord(word) }
                        onOpenDictsRequested: {
                            toolsDrawer.open()
                            toolsTabs.currentIndex = 2
                        }
                    }
                }
            }
        }
    }

    footer: ToolBar {
        Material.elevation: 0
        // 状态条（原型 statusbar 口径）：panel 面 + 顶部 hairline，12px 三级字
        background: Rectangle {
            color: Theme.panel
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.divider }
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 8
            Label {
                objectName: "statusLabel"
                text: statusText
                color: Theme.textTertiary
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            ToolButton {
                objectName: "clearHistoryButton"
                text: Theme.zh ? "清空历史" : "Clear history"
                font.pixelSize: 12
                onClicked: {
                    vocab.clearHistory()
                    reloadHistory()
                    statusText = Theme.zh ? "历史已清空" : "History cleared"
                }
            }
        }
    }

    Drawer {
        id: toolsDrawer
        objectName: "toolsDrawer"
        edge: Qt.RightEdge
        width: Math.min(420, win.width * 0.42)
        // 原型口径：抽屉只占右栏上半（下方露出主窗格卡片），不铺满窗高；
        // M6 语音控件增多后内容超高，由 ScrollView 兜底，不再撑高抽屉
        height: 535
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Pane {
            anchors.fill: parent
            padding: 16
            // 设置抽屉：card 面 + hairline 描边（浮层面），8px 栅格内距
            background: Rectangle {
                color: Theme.card
                border.width: 1
                border.color: Theme.divider
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                Label {
                    text: Theme.zh ? "工具与设置" : "Tools & Settings"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: Theme.text
                }

                TabBar {
                    id: toolsTabs
                    objectName: "toolsTabs"
                    Layout.fillWidth: true
                    currentIndex: 0
                    // 七 tab 等分抽屉宽：英文文案更长，EN 收字号/内距防裁切
                    TabButton { objectName: "toolsTab0"; text: Theme.zh ? "取词" : "Lookup"
                        font.pixelSize: Theme.zh ? 13 : 11
                        leftPadding: Theme.zh ? 12 : 4; rightPadding: Theme.zh ? 12 : 4 }
                    TabButton { objectName: "toolsTab1"; text: Theme.zh ? "语音" : "Voice"
                        font.pixelSize: Theme.zh ? 13 : 11
                        leftPadding: Theme.zh ? 12 : 4; rightPadding: Theme.zh ? 12 : 4 }
                    TabButton { objectName: "toolsTab2"; text: Theme.zh ? "词典" : "Dict"
                        font.pixelSize: Theme.zh ? 13 : 11
                        leftPadding: Theme.zh ? 12 : 4; rightPadding: Theme.zh ? 12 : 4 }
                    // 长标签改短形（同义简称），objectName 不动（审计/沙盒
                    // 按名定位不受影响）
                    TabButton { objectName: "toolsTab3"; text: Theme.zh ? "热键" : "Keys"
                        font.pixelSize: Theme.zh ? 13 : 11
                        leftPadding: Theme.zh ? 12 : 4; rightPadding: Theme.zh ? 12 : 4 }
                    TabButton { objectName: "toolsTab4"; text: Theme.zh ? "同步" : "Sync"
                        font.pixelSize: Theme.zh ? 13 : 11
                        leftPadding: Theme.zh ? 12 : 4; rightPadding: Theme.zh ? 12 : 4 }
                    TabButton { objectName: "toolsTab5"; text: Theme.zh ? "外观" : "Theme"
                        font.pixelSize: Theme.zh ? 13 : 11
                        leftPadding: Theme.zh ? 12 : 4; rightPadding: Theme.zh ? 12 : 4 }
                    TabButton { objectName: "toolsTab6"; text: "AI"
                        font.pixelSize: Theme.zh ? 13 : 11
                        leftPadding: Theme.zh ? 12 : 4; rightPadding: Theme.zh ? 12 : 4 }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: toolsTabs.currentIndex

                    // 取词
                    SettingsLookupPage {
                        objectName: "settingslookupPage"
                        win: win
                        platform: platformService
                        settings: settingsService
                        startup: startupService
                    }
                    // 语音（M6 控件已多于窗高，走 ScrollView 收纳）
                    SettingsVoicePage {
                        objectName: "settingsvoicePage"
                        win: win
                        voice: voiceService
                    }
                    // 词典（P-6 词典库正式面：目录导入 + 装载列表 + 隔离重试）
                    SettingsDictPage {
                        objectName: "settingsdictPage"
                        win: win
                        lookup: lookupService
                    }
                    // 快捷键
                    SettingsKeysPage {
                        objectName: "settingskeysPage"
                        win: win
                        platform: platformService
                        settings: settingsService
                    }
                    // 同步备份（B7：同步默认关闭、显式开启明示范围；
                    // 口令加密备份自救口。传输绑定归 B5 剩余，先落开关与设置）
                    // 内容高于抽屉固定高，同语音 tab 走 ScrollView 收纳
                    SettingsSyncPage {
                        objectName: "settingssyncPage"
                        win: win
                        syncManager: syncManagerService
                    }
                    // 外观（F4 亮暗三态）：0=跟随系统（colorScheme 实时
                    // 跟随），1/2=手动锁定不再跟随；记忆到 ui/theme（gui
                    // 面同键同语义）。P1 text-norm 键归一化区并入后内容
                    // 超过抽屉固定高（535），走 ScrollView 收纳（语音页
                    // 同款）——不收纳时下半段在抽屉界外：渲染被裁，点击
                    // 落在 popup 外触发 CloseOnPressOutside（S38 实录）
                    SettingsThemePage {
                        objectName: "settingsthemePage"
                        win: win
                    }
                    // AI（L99 LLM 集成面）：provider auth 属用户凭据/计费，
                    // 不入仓不接真服务——本页暴露可做的面：结果缓存开关 +
                    // 提示词模板预览（P-9 外部命令桥/启发式兜底仍是活路径）。
                    // 抽屉高度 535 不足时走 ScrollView 收纳（外观页同款）。
                    SettingsAiPage {
                        objectName: "settingsaiPage"
                        win: win
                        ai: aiService
                        settings: settingsService
                    }
                }
            }
        }
    }

    Timer {
        id: suggestTimer
        interval: 120
        repeat: false
        onTriggered: reloadSuggestions(searchQuery)
    }

    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: {
            clipboardMonitoring = platform.isClipboardMonitoring()
            selectionMonitoring = platform.isSelectionMonitoring()
        }
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: leftPane.focusSearchField()
    }

    Shortcut {
        sequence: "Ctrl+1"
        onActivated: leftPane.currentTabIndex = 0
    }
    Shortcut {
        sequence: "Ctrl+2"
        onActivated: leftPane.currentTabIndex = 1
    }
    Shortcut {
        sequence: "Ctrl+3"
        onActivated: leftPane.currentTabIndex = 2
    }

    ListModel { id: resultsModel }
    ListModel { id: historyModel }
    ListModel { id: vocabModel }
    ListModel { id: entriesModel }

    Connections {
        target: entriesModel
        function onCountChanged() { win._clampSelectedEntry() }
    }


    // 平台采集簇信号（剪贴板/划词/热键）走 PlatformAdapter（TD-121 拆桥）
    Connections {
        target: platform
        function onClipboardWordDetected(word) {
            if (!clipboardEnabled) return
            // P-5：取词窗形态弹悬浮窗（前台应用不被打扰）；关闭时回退
            // 旧行为（主窗直接展示）
            if (quickLookupEnabled) quickLookupPane.showFor(word)
            else openWord(word)
        }
        // 划词命中：与剪贴板同款分流（悬浮窗开 → 取词窗，关 → 主窗）
        function onSelectionWordDetected(word) {
            if (!selectionEnabled) return
            if (quickLookupEnabled) quickLookupPane.showFor(word)
            else openWord(word)
        }
        // quick_lookup 热键 → 优先焦点窗选区、空则剪贴板；show_window → 主窗前置
        function onQuickLookupRequested() {
            win.quickLookupFromFocusedSelection()
        }
        function onShowMainWindowRequested() {
            win.show()
            win.raise()
            win.requestActivate()
        }
    }

    // 语音面信号（在线发音状态行）走 VoiceAdapter（TD-121 拆桥第二刀）
    Connections {
        target: voice
        function onPronOnlineStatus(message) {
            pronOnlineStatusText = message
            // BUG-010：发音链路状态镜像到主页脚状态行——设置抽屉没开时，
            // 发音点击照样有可观测反馈（含「本地语音不可用」的明示）
            statusText = message
        }
    }

    Connections {
        target: lookup
        // P-6 词典库导入管道（startDirectoryImport）
        function onImportScanFinished(found) {
            win.importProgressTotal = found
            win.importProgressDone = 0
            win.importProgressFailed = 0
            win.importSummaryText = found > 0
                ? (Theme.zh
                    ? ("发现 " + found + " 个候选词典，开始导入…")
                    : ("Found " + found + " candidate dictionaries, importing…"))
                : (Theme.zh
                    ? "未发现新词典（已装载与隔离项不会重复导入）"
                    : "No new dictionaries (loaded and quarantined ones are skipped)")
        }
        function onImportProgress(imported, failed, total, currentFile) {
            win.importProgressDone = imported
            win.importProgressFailed = failed
            win.importSummaryText = Theme.zh
                ? ("正在导入 " + (imported + failed) + "/" + total + "：" + currentFile)
                : ("Importing " + (imported + failed) + "/" + total + ": " + currentFile)
        }
        function onImportFinished(imported, failed) {
            win.importSummaryText = Theme.zh
                ? ("导入结束：成功 " + imported + "，失败 " + failed
                   + (failed > 0 ? "（失败项见下方隔离列表，可重试）" : ""))
                : ("Import finished: " + imported + " ok, " + failed + " failed"
                   + (failed > 0 ? " (see the quarantined list below, retry available)" : ""))
        }
    }

    // P-5 悬浮取词窗（无边框置顶小窗，默认隐藏）
    QuickLookupPane {
        id: quickLookupPane
        lookup: win.lookupService
        platform: win.platformService
        voice: win.voiceService
        vocab: win.vocabService
    }

    Connections {
        target: quickLookupPane
        // 取词窗「在主窗打开」：收窗 → 主窗词条卡 → 前置主窗
        function onOpenInMainRequested(word) {
            quickLookupPane.close()
            openWord(word)
            win.show()
            win.raise()
            win.requestActivate()
        }
        function onStatusReported(message) {
            statusText = message
        }
    }

    // M3-B 生词本 CSV 导出目标选择（core export_vocabulary_csv 口径：
    // UTF-8 BOM + word,definition,tags,note 四列）
    FileDialog {
        id: vocabExportDialog
        title: "导出生词本 CSV"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV 文件 (*.csv)", "所有文件 (*)"]
        defaultSuffix: "csv"
        onAccepted: {
            var path = decodeURIComponent(
                selectedFile.toString().replace(/^file:\/\//, ""))
            statusText = vocab.exportVocabCsv(path)
                ? "已导出 CSV → " + path
                : "导出失败（路径不可写）→ " + path
        }
    }

    // 生词本 CSV 导入（导入导出批）：导出同方言回吃——已存在整行跳过
    // 不覆盖，结果行带 imported/skipped 计数
    FileDialog {
        id: vocabImportDialog
        title: "导入生词本 CSV"
        fileMode: FileDialog.OpenFile
        nameFilters: ["CSV 文件 (*.csv)", "所有文件 (*)"]
        onAccepted: {
            var path = decodeURIComponent(
                selectedFile.toString().replace(/^file:\/\//, ""))
            var r = vocab.importVocabCsv(path)
            if (r.imported < 0) {
                statusText = "导入失败（文件不可读）→ " + path
            } else {
                statusText = "已导入 " + r.imported + " 条（跳过 "
                             + r.skipped + " 条）→ " + path
                reloadVocabulary()
            }
        }
    }

    // 笔记导出（roadmap Export notes）：HTML=std 导出器；.pdf 后缀走 Qt 排版
    FileDialog {
        id: notesExportDialog
        title: "导出笔记 HTML / PDF"
        fileMode: FileDialog.SaveFile
        nameFilters: ["HTML 文件 (*.html)", "PDF 文件 (*.pdf)", "所有文件 (*)"]
        defaultSuffix: "html"
        onAccepted: {
            var path = decodeURIComponent(
                selectedFile.toString().replace(/^file:\/\//, ""))
            var isPdf = path.toLowerCase().endsWith(".pdf")
            var ok = isPdf ? vocab.exportNotesPdf(path)
                           : vocab.exportNotesHtml(path)
            statusText = ok ? "已导出笔记 → " + path
                            : "导出失败（路径不可写）→ " + path
        }
    }

    // 词条卡头的笔记轻入口（core setVocabNote 口径：空串保存即删除）。
    // 荧光笔多色（UI 批二）：色板行与生词本 noteDialog 同款（Theme.
    // noteHighlight 唯一映射），保存走 setVocabNoteColored
    Popup {
        id: notePopup
        objectName: "notePopup"
        property string noteWord: ""
        property int noteColor: 0
        anchors.centerIn: parent
        width: 440
        height: 290
        padding: 16
        modal: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                text: "笔记 · " + notePopup.noteWord
                font.weight: Font.DemiBold
                color: Theme.text
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextArea {
                    id: noteArea
                    wrapMode: TextArea.Wrap
                    placeholderText: "为该词条记点什么（清空保存即删除）"
                    color: Theme.text
                }
            }

            // 荧光色板：0=无色 + 四色，点选即换（保存时随文本落库）
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: 5
                    delegate: Rectangle {
                        objectName: "noteColorSwatch_" + index
                        readonly property int colorIdx: index
                        implicitWidth: 24
                        implicitHeight: 24
                        radius: height / 2
                        color: index === 0 ? "transparent" : Theme.noteHighlight(index)
                        border.width: notePopup.noteColor === index ? 2 : 1
                        border.color: notePopup.noteColor === index
                                      ? Theme.accent : Theme.divider

                        // 无色档斜杠指示（透明底不能空到看不出可点）
                        Rectangle {
                            visible: index === 0
                            anchors.centerIn: parent
                            width: 14
                            height: 1
                            color: Theme.divider
                            rotation: 45
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: notePopup.noteColor = index
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            RowLayout {
                spacing: 8
                Layout.alignment: Qt.AlignRight

                Button {
                    objectName: "noteCancelButton"
                    flat: true
                    text: "取消"
                    onClicked: notePopup.close()
                }
                Button {
                    objectName: "noteSaveButton"
                    text: "保存"
                    onClicked: {
                        vocab.setVocabNoteColored(notePopup.noteWord,
                                                  noteArea.text,
                                                  notePopup.noteColor)
                        statusText = noteArea.text.length > 0
                            ? "已保存笔记: " + notePopup.noteWord
                            : "已删除笔记: " + notePopup.noteWord
                        notePopup.close()
                    }
                }
            }
        }
    }

    // AI 语境造句弹层：只读展示外部命令桥/启发式输出
    Popup {
        id: aiSentencePopup
        objectName: "aiSentencePopup"
        property string aiSentenceWord: ""
        anchors.centerIn: parent
        width: 460
        height: 320
        padding: 16
        modal: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                text: "AI 例句 · " + aiSentencePopup.aiSentenceWord
                font.weight: Font.DemiBold
                color: Theme.text
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextArea {
                    id: aiSentenceText
                    objectName: "aiSentenceText"
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    color: Theme.text
                }
            }

            RowLayout {
                spacing: 8
                Layout.alignment: Qt.AlignRight

                Button {
                    objectName: "aiSentenceCloseButton"
                    text: "关闭"
                    onClicked: aiSentencePopup.close()
                }
            }
        }
    }

    onSelectedEntryIndexChanged: _clampSelectedEntry()
}
