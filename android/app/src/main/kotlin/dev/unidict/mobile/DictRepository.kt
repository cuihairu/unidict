package dev.unidict.mobile

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject

// 词典仓库（M2）：SAF 导入 → app 私有目录 + 导入清单持久化（桌面
// UNIDICT_DICTS 环境变量的移动等价物，mobile_plan §2.2）。
// core 句柄无内部锁（§4）——所有 core 调用固定在单线程执行器上。
data class DictEntry(
    val file: String,        // files/dicts/ 下的文件名（含扩展名）
    val display: String,     // 导入时的展示名
    val enabled: Boolean,
)

// 装载快照：loaded=false 即 add_dictionary 失败（坏文件/不支持格式），
// 保留条目在管理页可见可删，不做静默吞掉。
data class DictStatus(
    val entry: DictEntry,
    val loaded: Boolean,
    val dictName: String,    // 装载成功 = 词典自带 name（≠文件名）
    val wordCount: Int,
    val description: String,
)

data class SessionSnapshot(
    val dicts: List<DictStatus>,
    val indexedWords: Int,
)

class DictRepository(private val context: Context) {
    private val exec = java.util.concurrent.Executors.newSingleThreadExecutor { r ->
        Thread(r, "unidict-core").apply { isDaemon = true }
    }
    private val coreDispatcher = exec.asCoroutineDispatcher()

    private val dictsDir: File get() = File(context.filesDir, "dicts").apply { mkdirs() }
    private val manifestFile: File get() = File(context.filesDir, "manifest.json")
    private val storeFile: File get() = File(context.filesDir, "store.json")

    // core 线程独占；UI 侧只在快照/结果对象上读
    private var session: UnidictSession? = null
    private var entries: MutableList<DictEntry> = mutableListOf()

    // ---- 清单持久化（org.json 系框架 API，不引第三方） ----

    private fun loadManifest() {
        entries = mutableListOf()
        val f = manifestFile
        if (!f.exists()) return
        runCatching {
            val root = JSONObject(f.readText())
            val arr = root.optJSONArray("dicts") ?: JSONArray()
            for (i in 0 until arr.length()) {
                val o = arr.getJSONObject(i)
                entries += DictEntry(
                    file = o.getString("file"),
                    display = o.optString("display", o.getString("file")),
                    enabled = o.optBoolean("enabled", true),
                )
            }
        } // 坏清单按空清单起步（文件还在，可经管理页重新导入）
    }

    private fun saveManifest() {
        val root = JSONObject()
        val arr = JSONArray()
        for (e in entries) {
            arr.put(JSONObject().put("file", e.file).put("display", e.display)
                .put("enabled", e.enabled))
        }
        root.put("version", 1).put("dicts", arr)
        manifestFile.writeText(root.toString(2))
    }

    // ---- 首启演示种子（M1 冒烟口径延续：CI/模拟器可 grep M2-SMOKE-OK） ----

    private fun copyAsset(name: String): File {
        val out = File(dictsDir, name)
        if (!out.exists()) {
            context.assets.open(name).use { input ->
                out.outputStream().use { input.copyTo(it) }
            }
        }
        return out
    }

    // ---- 会话重建：逐词典装载拿成败 → 建索引 → 应用启停 ----

    private fun rebuildLocked(): SessionSnapshot {
        session?.close()
        val s = UnidictSession(storeFile.absolutePath)
        session = s
        val loaded = entries.map { e ->
            runCatching { s.addDictionary(File(dictsDir, e.file).absolutePath) }
                .getOrDefault(false)
        }
        s.rebuildIndex()
        // meta 顺序 == 装载成功顺序（dicts_ 只在成功时 push）
        val metas = s.dictionariesMeta().iterator()
        val statuses = entries.mapIndexed { i, e ->
            if (loaded[i]) {
                val m = metas.next()
                DictStatus(e, loaded = true, dictName = m.name,
                    wordCount = m.wordCount, description = m.description)
            } else {
                DictStatus(e, loaded = false, dictName = "", wordCount = 0,
                    description = "装载失败（文件损坏或格式不支持）")
            }
        }
        // 启停应用（禁用词典 search_all/full_text 自动跳过，M2 门禁点）
        for (st in statuses) {
            if (st.loaded && !st.entry.enabled) s.setDictEnabled(st.dictName, false)
        }
        return SessionSnapshot(statuses, s.indexedWordCount())
    }

    private fun smokeCheckLocked(snap: SessionSnapshot): String {
        val s = session ?: return "M2-SMOKE-FAIL: no session"
        return runCatching {
            val def = s.definition("hello")
            val pref = s.prefixSearch("wo", 5)
            val hits = s.searchAll("hello")
            val ft = s.fullTextSearchEntries("greeting", 5)
            val fz = s.fuzzySearch("helo", 5)
            check(def.isNotEmpty()) { "def empty" }
            check(pref.contains("world")) { "prefix miss" }
            check(hits.isNotEmpty()) { "agg empty" }
            check(ft.isNotEmpty()) { "fulltext empty" }
            check(fz.isNotEmpty()) { "fuzzy empty" }
            s.addVocab("hello", def)
            s.addHistory("hello")
            check(s.save()) { "store save failed" }
            check(s.vocabWords().contains("hello")) { "vocab miss" }
            "M2-SMOKE-OK dicts=${snap.dicts.count { it.loaded }} " +
                "indexed=${snap.indexedWords} agg=${hits.size} " +
                "fulltext=${ft.size} vocab=${s.vocabWords().size}"
        }.getOrElse { "M2-SMOKE-FAIL: ${it.message}" }
    }

    // ---- 对 UI 的挂起 API ----

    suspend fun ensureDemoAndOpen(): Pair<SessionSnapshot, String> =
        withContext(coreDispatcher) {
            loadManifest()
            if (entries.isEmpty() && !manifestFile.exists()) {
                for (name in listOf("dict.json", "test.dsl", "test.csv")) {
                    copyAsset(name)
                    entries += DictEntry(name, name, enabled = true)
                }
                saveManifest()
            }
            val snap = rebuildLocked()
            Pair(snap, smokeCheckLocked(snap))
        }

    suspend fun refresh(): SessionSnapshot = withContext(coreDispatcher) {
        rebuildLocked()
    }

    suspend fun importFromUri(uri: Uri): SessionSnapshot = withContext(coreDispatcher) {
        val name = resolveDisplayName(uri)
        val dest = File(dictsDir, name)
        context.contentResolver.openInputStream(uri)?.use { input ->
            dest.outputStream().use { input.copyTo(it) }
        } ?: throw IllegalStateException("无法读取所选文件")
        entries.removeAll { it.file == name }
        entries += DictEntry(name, name, enabled = true)
        saveManifest()
        rebuildLocked()
    }

    suspend fun setEnabled(entry: DictEntry, on: Boolean): SessionSnapshot =
        withContext(coreDispatcher) {
            // 原位替换保持清单顺序（removeAll+append 会让卡片跳位）
            val i = entries.indexOfFirst { it.file == entry.file }
            if (i >= 0) entries[i] = entry.copy(enabled = on)
            saveManifest()
            val snap = rebuildLocked()  // 启停走重建：语义与 core 一致且免双路径
            snap
        }

    suspend fun remove(entry: DictEntry): SessionSnapshot = withContext(coreDispatcher) {
        File(dictsDir, entry.file).delete()
        entries.removeAll { it.file == entry.file }
        saveManifest()
        rebuildLocked()
    }

    suspend fun searchAll(word: String): Array<SearchHit> = withContext(coreDispatcher) {
        session?.searchAll(word) ?: emptyArray()
    }

    suspend fun prefixSuggest(prefix: String, limit: Int = 8): Array<String> =
        withContext(coreDispatcher) {
            // 索引词表不过滤启用态（core 语义），启用集合过滤在调用侧做
            session?.prefixSearch(prefix, limit) ?: emptyArray()
        }

    suspend fun fuzzySuggest(word: String, limit: Int = 8): Array<String> =
        withContext(coreDispatcher) {
            session?.fuzzySearch(word, limit) ?: emptyArray()
        }

    suspend fun fullText(query: String, limit: Int = 20): Array<SearchHit> =
        withContext(coreDispatcher) {
            session?.fullTextSearchEntries(query, limit) ?: emptyArray()
        }

    suspend fun addVocab(word: String, definition: String): Array<String> =
        withContext(coreDispatcher) {
            session?.let {
                if (word.isNotBlank()) {
                    it.addVocab(word, definition)
                    it.save()
                }
                it.vocabWords()
            } ?: emptyArray()
        }

    suspend fun addHistory(word: String): Array<String> = withContext(coreDispatcher) {
        session?.let {
            if (word.isNotBlank()) it.addHistory(word)
            it.history(8)
        } ?: emptyArray()
    }

    suspend fun history(limit: Int = 8): Array<String> = withContext(coreDispatcher) {
        session?.history(limit) ?: emptyArray()
    }

    private fun resolveDisplayName(uri: Uri): String {
        var name: String? = null
        runCatching {
            context.contentResolver.query(
                uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null,
            )?.use { c ->
                if (c.moveToFirst()) name = c.getString(0)
            }
        }
        val raw = name ?: uri.lastPathSegment ?: "imported.dict"
        // 只留文件名部分；不支持无扩展名（core 按扩展名分发解析器）
        val clean = raw.substringAfterLast('/').replace(Regex("[\\\\\"']"), "_")
        return if (clean.contains('.')) clean else "$clean.txt"
    }
}
