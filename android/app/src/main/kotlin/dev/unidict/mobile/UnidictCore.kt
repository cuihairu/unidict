package dev.unidict.mobile

// JNI 面（mobile_plan §4）：UTF-8 字符串 ↔ jstring，句柄 jlong + close()
// 显式释放，单线程调用起步。三域（dict/lookup/store）与 adapters/android/
// jni/{dict,lookup,store}_jni.cpp 一一对应；新增绑定先改那边的 C++。
object UnidictCore {
    init {
        System.loadLibrary("unidict_jni")
    }

    // —— dict 域：装载/启停/元信息 ——
    @JvmStatic external fun dictOpen(paths: Array<String>): Long
    @JvmStatic external fun dictClose(handle: Long)
    @JvmStatic external fun dictAdd(handle: Long, path: String): Boolean
    @JvmStatic external fun dictRebuildIndex(handle: Long)
    @JvmStatic external fun dictNames(handle: Long): Array<String>
    @JvmStatic external fun setDictEnabled(handle: Long, name: String, enabled: Boolean): Boolean
    @JvmStatic external fun isDictEnabled(handle: Long, name: String): Boolean
    @JvmStatic external fun indexedWordCount(handle: Long): Int
    @JvmStatic external fun dictionariesMeta(handle: Long): Array<DictMetaInfo>

    // —— lookup 域：精确/前缀/模糊/全文/聚合/定义 ——
    @JvmStatic external fun exactSearch(handle: Long, word: String): Array<String>
    @JvmStatic external fun prefixSearch(handle: Long, prefix: String, limit: Int): Array<String>
    @JvmStatic external fun fuzzySearch(handle: Long, word: String, limit: Int): Array<String>
    @JvmStatic external fun fullTextSearch(handle: Long, query: String, limit: Int): Array<String>
    @JvmStatic external fun searchAll(handle: Long, word: String, limit: Int): Array<SearchHit>
    @JvmStatic external fun fullTextSearchEntries(handle: Long, query: String, limit: Int): Array<SearchHit>
    @JvmStatic external fun searchDefinition(handle: Long, word: String): String

    // —— store 域：生词本/历史（M3 扩全标签/笔记/CSV） ——
    @JvmStatic external fun storeOpen(path: String): Long
    @JvmStatic external fun storeClose(handle: Long)
    @JvmStatic external fun addVocab(handle: Long, word: String, definition: String)
    @JvmStatic external fun vocabWords(handle: Long): Array<String>
    @JvmStatic external fun addHistory(handle: Long, word: String)
    @JvmStatic external fun history(handle: Long, limit: Int): Array<String>
    @JvmStatic external fun saveStore(handle: Long): Boolean
}

// 结构化条目（字段序与 jni_util.h 的 JNI 构造签名严格对齐，动一头必动另一头）
data class SearchHit(val dictName: String, val word: String, val definition: String)
data class DictMetaInfo(val name: String, val wordCount: Int, val description: String)

// 会话壳：一对句柄（词典管理 + 生词本）的 Closeable 封装，UI 只碰它。
// M2 起词典走逐个 addDictionary（仓库层拿到逐条装载成败）+ rebuildIndex。
class UnidictSession(storePath: String) : AutoCloseable {
    private val dictHandle = UnidictCore.dictOpen(emptyArray())
    private val storeHandle = UnidictCore.storeOpen(storePath)

    fun addDictionary(path: String): Boolean = UnidictCore.dictAdd(dictHandle, path)
    fun rebuildIndex() = UnidictCore.dictRebuildIndex(dictHandle)

    fun dictNames(): Array<String> = UnidictCore.dictNames(dictHandle)
    fun setDictEnabled(name: String, enabled: Boolean): Boolean =
        UnidictCore.setDictEnabled(dictHandle, name, enabled)

    fun isDictEnabled(name: String): Boolean = UnidictCore.isDictEnabled(dictHandle, name)
    fun indexedWordCount(): Int = UnidictCore.indexedWordCount(dictHandle)
    fun dictionariesMeta(): Array<DictMetaInfo> = UnidictCore.dictionariesMeta(dictHandle)
    fun exactSearch(word: String): Array<String> = UnidictCore.exactSearch(dictHandle, word)
    fun prefixSearch(prefix: String, limit: Int = 10): Array<String> =
        UnidictCore.prefixSearch(dictHandle, prefix, limit)

    fun fuzzySearch(word: String, limit: Int = 10): Array<String> =
        UnidictCore.fuzzySearch(dictHandle, word, limit)

    fun fullTextSearch(query: String, limit: Int = 10): Array<String> =
        UnidictCore.fullTextSearch(dictHandle, query, limit)

    fun searchAll(word: String, limit: Int = 50): Array<SearchHit> =
        UnidictCore.searchAll(dictHandle, word, limit)

    fun fullTextSearchEntries(query: String, limit: Int = 20): Array<SearchHit> =
        UnidictCore.fullTextSearchEntries(dictHandle, query, limit)

    fun definition(word: String): String = UnidictCore.searchDefinition(dictHandle, word)

    fun addVocab(word: String, definition: String) =
        UnidictCore.addVocab(storeHandle, word, definition)

    fun vocabWords(): Array<String> = UnidictCore.vocabWords(storeHandle)
    fun addHistory(word: String) = UnidictCore.addHistory(storeHandle, word)
    fun history(limit: Int = 20): Array<String> = UnidictCore.history(storeHandle, limit)
    fun save(): Boolean = UnidictCore.saveStore(storeHandle)

    override fun close() {
        UnidictCore.dictClose(dictHandle)
        UnidictCore.storeClose(storeHandle)
    }
}
