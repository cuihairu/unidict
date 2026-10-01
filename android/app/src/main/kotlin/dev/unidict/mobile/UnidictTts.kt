package dev.unidict.mobile

import android.content.Context
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import android.util.Log
import java.util.Locale

// M4：系统 TextToSpeech 朗读包装。口径与桌面 qmlui 的 TTS 面同源——
// 「查得到就读得出」：查词聚合卡片、生词本卡片一键朗读词条。
// 引擎缺失/初始化失败不崩、不挡其余功能：状态行与 logcat 同步打
// M4-TTS-READY / M4-TTS-OK / M4-TTS-FAIL 供 CI 与装机验收 grep
// （模拟器上合成不可闻听，utterance onDone 回调是唯一可靠证据链）。
// 语音参数（音色/语速）走系统默认，M5 打磨再开口径。
class UnidictTts(context: Context, private val onStatus: (String) -> Unit) {

    private var engine: TextToSpeech? = null
    private var ready = false
    private var seq = 0

    init {
        engine = TextToSpeech(context.applicationContext) { code ->
            ready = code == TextToSpeech.SUCCESS
            if (!ready) {
                announce("M4-TTS-FAIL 初始化失败 code=$code")
                return@TextToSpeech
            }
            // 语言不设硬约束：缺语言数据只降级发音质量，不挡朗读功能
            engine?.language = Locale.getDefault()
            engine?.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
                override fun onStart(utteranceId: String?) = Unit
                override fun onDone(utteranceId: String?) {
                    if (utteranceId?.startsWith(UTT_PREFIX) == true) {
                        announce("M4-TTS-OK 朗读完成")
                    }
                }

                @Deprecated("Deprecated in Java")
                override fun onError(utteranceId: String?) {
                    announce("M4-TTS-FAIL 合成出错 utterance=$utteranceId")
                }
            })
            announce("M4-TTS-READY 引擎=${engine?.defaultEngine ?: "system"}")
        }
    }

    /** 朗读词条；未就绪/空文本给 FAIL 行，不抛异常打断 UI 流程 */
    fun speak(text: String) {
        val e = engine
        if (!ready || e == null) {
            announce("M4-TTS-FAIL 未就绪")
            return
        }
        if (text.isBlank()) {
            announce("M4-TTS-FAIL 空文本")
            return
        }
        // QUEUE_FLUSH：连点发音时打断上一条，行为与桌面一致
        val r = e.speak(text, TextToSpeech.QUEUE_FLUSH, null, "$UTT_PREFIX${seq++}")
        if (r != TextToSpeech.SUCCESS) announce("M4-TTS-FAIL 入队失败 code=$r")
    }

    fun stop() {
        engine?.stop()
    }

    fun shutdown() {
        engine?.apply {
            stop()
            shutdown()
        }
        engine = null
        ready = false
    }

    private fun announce(msg: String) {
        Log.i(TAG, msg)
        onStatus(msg)
    }

    private companion object {
        const val TAG = "unidict"
        const val UTT_PREFIX = "unidict-m4-"
    }
}
