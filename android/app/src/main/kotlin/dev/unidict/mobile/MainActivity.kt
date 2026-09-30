package dev.unidict.mobile

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.io.File
import kotlin.concurrent.thread

// M1 冒烟页（mobile_plan 验收：装机冒烟页可查词）。
// 启动即自跑一遍罐头冒烟：示例词典（assets/dict.json → filesDir）装载 →
// 精确查 hello → 前缀 wo → 全文 greeting → 生词本写入读回 → 状态行打
// M1-SMOKE-OK / M1-SMOKE-FAIL；上方查询框则供人手交互。
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme { SmokePage() }
        }
    }

    private fun copySampleDict(): String {
        val out = File(filesDir, "sample_dict.json")
        if (!out.exists()) {
            assets.open("dict.json").use { input ->
                out.outputStream().use { input.copyTo(it) }
            }
        }
        return out.absolutePath
    }

    @Composable
    private fun SmokePage() {
        var status by remember { mutableStateOf("smoke running…") }
        var query by remember { mutableStateOf("") }
        var definition by remember { mutableStateOf("") }
        var suggestions by remember { mutableStateOf(arrayOf<String>()) }
        var vocabLine by remember { mutableStateOf("生词本：-") }
        var historyLine by remember { mutableStateOf("历史：-") }
        var session by remember { mutableStateOf<UnidictSession?>(null) }

        fun performLookup(word: String) {
            val s = session ?: return
            if (word.isBlank()) return
            definition = s.definition(word)
            suggestions = s.prefixSearch(word, 8)
            s.addHistory(word)
            historyLine = "历史：" + s.history(5).joinToString("、")
        }

        // 罐头冒烟：后台线程跑 core（单线程句柄假设），状态回投 UI 线程
        LaunchedEffect(Unit) {
            thread {
                try {
                    val dictPath = copySampleDict()
                    val s = UnidictSession(
                        arrayOf(dictPath),
                        File(filesDir, "store.json").absolutePath,
                    )
                    session = s
                    val exact = s.exactSearch("hello")
                    val def = s.definition("hello")
                    val pref = s.prefixSearch("wo", 5)
                    val full = s.fullTextSearch("greeting", 5)
                    check(exact.isNotEmpty() && def.isNotEmpty()) { "exact/def empty" }
                    s.addVocab("hello", def)
                    s.addHistory("hello")
                    check(s.save()) { "store save failed" }
                    val vocab = s.vocabWords()
                    val hist = s.history(5)
                    runOnUiThread {
                        status = "M1-SMOKE-OK dicts=${s.dictNames().size} " +
                            "indexed=${s.indexedWordCount()} prefix=${pref.size} " +
                            "fulltext=${full.size} vocab=${vocab.size}"
                        definition = def
                        suggestions = pref
                        vocabLine = "生词本：${vocab.joinToString("、")}"
                        historyLine = "历史：${hist.joinToString("、")}"
                        query = "hello"
                    }
                } catch (e: Throwable) {
                    runOnUiThread { status = "M1-SMOKE-FAIL: ${e.message}" }
                }
            }
        }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text("Unidict Mobile", fontSize = 22.sp, fontWeight = FontWeight.Bold)
            Text(status, fontSize = 13.sp, fontWeight = FontWeight.Medium)

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = query,
                    onValueChange = { query = it },
                    label = { Text("词条") },
                    modifier = Modifier.weight(1f),
                    singleLine = true,
                )
                Button(onClick = { performLookup(query) }) { Text("查询") }
            }

            if (definition.isNotEmpty()) {
                Text("释义", fontWeight = FontWeight.SemiBold)
                Text(definition)
            }

            if (suggestions.isNotEmpty()) {
                Text("前缀建议（点击查询）", fontWeight = FontWeight.SemiBold)
                for (w in suggestions) {
                    Text(
                        w,
                        color = MaterialTheme.colorScheme.primary,
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { performLookup(w) }
                            .padding(vertical = 2.dp),
                    )
                }
            }

            Button(onClick = {
                val s = session ?: return@Button
                if (query.isNotBlank()) {
                    s.addVocab(query, definition)
                    s.save()
                    vocabLine = "生词本：" + s.vocabWords().joinToString("、")
                }
            }) { Text("加入生词本") }

            Text(vocabLine)
            Text(historyLine)
        }
    }
}
