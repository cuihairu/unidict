package dev.unidict.mobile

import android.media.AudioAttributes
import android.media.MediaPlayer
import android.util.Log
import java.net.HttpURLConnection
import java.net.URL
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.json.JSONArray

// M6：在线发音。口径与桌面 LookupAdapter 的在线发音面同源——同一
// 在线源（dictionaryapi.dev，免密钥无配额）、同一解析/挑选规则
// （core/std/online_pron_std 的镜像实现）、同一三态语义：
//   0 本地 TTS / 1 在线发音 / 2 自动（在线优先、失败回落本地）。
// 隐私口径：请求 URL 里唯一外发内容是查询词，不带历史/生词本。
// 状态行走 logcat + 回调双通道（M6-PRON-*），供 CI 与装机验收 grep；
// 网络失败/无片段不崩、不抛出 UI 层，fallback 回调决定是否回落 TTS。
class UnidictOnlinePron(private val onStatus: (String) -> Unit) {

    // 一条可播放的发音片段。accent 编码与 core 同源：
    // 0 自动/未知 1 美音 2 英音 3 澳音
    data class Clip(val url: String, val accent: Int, val label: String)

    private val player = MediaPlayer()
    private var attrsSet = false
    private val mutex = Mutex()  // 同一时刻只取一条（与桌面防重入口径一致）

    /**
     * 查询在线发音并播放。
     * @param accentPref 口音偏好（0 自动 / 1 美 / 2 英 / 3 澳）
     * @param fallbackLocal 在线失败时的本地回落回调（自动态传入，
     *   在线态传 null——只报状态不打断）
     * @return 是否走通了在线播放（回落/失败均为 false）
     */
    suspend fun speak(
        word: String,
        accentPref: Int,
        fallbackLocal: (() -> Unit)?,
    ): Boolean = mutex.withLock {
        if (word.isBlank()) {
            announce("M6-PRON-FAIL 空查询")
            return@withLock false
        }
        var error: String? = null
        val clips = withContext(Dispatchers.IO) {
            try {
                fetchClips(word)
            } catch (e: Exception) {
                error = e.message ?: e.javaClass.simpleName
                emptyList()
            }
        }
        val clip = pickClip(clips, accentPref)
        if (clip == null) {
            if (fallbackLocal != null) {
                announce("M6-PRON-FALLBACK 在线发音失败，回落本地语音")
                fallbackLocal()
            } else {
                announce(
                    if (error != null) "M6-PRON-FAIL $error"
                    else "M6-PRON-FAIL 没有在线发音片段",
                )
            }
            return@withLock false
        }
        // setDataSource(url) + prepare() 有网络 IO，整体压在 IO 线程
        withContext(Dispatchers.IO) { play(clip.url) }
    }

    fun release() {
        player.run {
            runCatching { stop() }
            runCatching { release() }
        }
    }

    // ---- 内部：取 / 解析 / 挑 / 播（规则与 core/std/online_pron_std 镜像） ----

    private fun fetchClips(word: String): List<Clip> {
        val conn = (URL(BASE_URL + escapePath(word)).openConnection()
            as HttpURLConnection).apply {
            connectTimeout = 8000
            readTimeout = 8000
            requestMethod = "GET"
        }
        try {
            val code = conn.responseCode
            if (code != 200) throw RuntimeException("HTTP $code")
            return parse(conn.inputStream.bufferedReader().use { it.readText() })
        } finally {
            conn.disconnect()
        }
    }

    /** org.json 严格解析：残缺/变体结构以已解析到的条目为准，不整体作废 */
    private fun parse(body: String): List<Clip> {
        val out = ArrayList<Clip>()
        val seen = HashSet<String>()
        try {
            val arr = JSONArray(body)
            for (i in 0 until arr.length()) {
                val phonetics = arr.getJSONObject(i).optJSONArray("phonetics")
                    ?: continue
                for (j in 0 until phonetics.length()) {
                    val p = phonetics.getJSONObject(j)
                    val audio = p.optString("audio").trim()
                    if (audio.isEmpty() || !seen.add(audio)) continue
                    val text = p.optString("text")
                    val accent = accentFromUrl(audio)
                        .takeIf { it != 0 } ?: accentFromLabel(text)
                    out.add(Clip(audio, accent, text))
                }
            }
        } catch (_: Exception) {
            // 截断 JSON：已完整解析的条目照常产出（容忍口径与 core 一致）
        }
        return out
    }

    private fun accentFromUrl(url: String): Int {
        val file = url.substringAfterLast('/').substringBefore('?').lowercase()
        return when {
            "-us" in file || "_us" in file -> 1
            "-uk" in file || "_uk" in file || "-gb" in file || "_gb" in file -> 2
            "-au" in file || "_au" in file -> 3
            else -> 0
        }
    }

    private fun accentFromLabel(text: String): Int {
        val t = text.lowercase()
        return when {
            "american" in t || t == "us" || t == "usa" -> 1
            "british" in t || t == "uk" || t == "gb" -> 2
            "australian" in t || t == "au" -> 3
            else -> 0
        }
    }

    /** 偏好命中优先，否则 US → UK → AU → Unknown → 首条（与 core 同序） */
    private fun pickClip(clips: List<Clip>, prefer: Int): Clip? {
        if (clips.isEmpty()) return null
        if (prefer != 0) clips.firstOrNull { it.accent == prefer }?.let { return it }
        for (a in intArrayOf(1, 2, 3, 0)) {
            clips.firstOrNull { it.accent == a }?.let { return it }
        }
        return clips.first()
    }

    /** RFC 3986 unreserved 之外的字节全部百分号转义（与 core 同规则） */
    private fun escapePath(raw: String): String {
        val sb = StringBuilder()
        for (b in raw.toByteArray(Charsets.UTF_8)) {
            val c = b.toInt() and 0xFF
            val unreserved = c in 0x41..0x5A || c in 0x61..0x7A ||
                c in 0x30..0x39 || c == 0x2D || c == 0x5F ||
                c == 0x2E || c == 0x7E
            if (unreserved) sb.append(c.toChar())
            else sb.append('%').append(String.format("%02X", c))
        }
        return sb.toString()
    }

    private fun play(url: String): Boolean {
        try {
            player.reset()
            if (!attrsSet) {
                player.setAudioAttributes(
                    AudioAttributes.Builder()
                        .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .build(),
                )
                attrsSet = true
            }
            player.setDataSource(url)
            player.setOnCompletionListener {
                announce("M6-PRON-OK 在线发音播放完毕")
            }
            player.prepare()
            player.start()
            announce("M6-PRON-PLAY 正在播放在线发音")
            return true
        } catch (e: Exception) {
            announce("M6-PRON-FAIL 播放失败 ${e.message ?: e.javaClass.simpleName}")
            return false
        }
    }

    private fun announce(msg: String) {
        Log.i(TAG, msg)
        onStatus(msg)
    }

    private companion object {
        const val TAG = "unidict"

        // 与 core/std/online_pron_std.cpp 的 FreeDictionarySource::kBaseUrl
        // 同源同端点（Free Dictionary API，免密钥无配额）。走 HTTPS，manifest
        // 未开明文流量。
        const val BASE_URL = "https://api.dictionaryapi.dev/api/v2/entries/en/"
    }
}
