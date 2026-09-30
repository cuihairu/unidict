package dev.unidict.mobile

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.List
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch

// M2 双页壳：查词（聚合结果 SearchHit 卡片）+ 词典管理（SAF 导入/启停/
// 删除/词量）。主题走 UnidictTheme（品牌 #b11964，与桌面 qmlui 同观感）。
// 启动自跑罐头冒烟（仓库层首启种子三格式演示词典），状态行打
// M2-SMOKE-OK / M2-SMOKE-FAIL 供 CI/装机验收 grep。
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val repo = DictRepository(applicationContext)
        setContent {
            UnidictTheme { AppRoot(repo) }
        }
    }
}

@Composable
private fun AppRoot(repo: DictRepository) {
    var tab by rememberSaveable { mutableIntStateOf(0) }
    var snapshot by remember { mutableStateOf<SessionSnapshot?>(null) }
    var smoke by remember { mutableStateOf("初始化…") }
    var busy by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()

    // 所有仓库操作都在 core 单线程上跑；失败进 smoke 行可见，不静默
    val runOp: (suspend () -> SessionSnapshot) -> Unit = { op ->
        busy = true
        scope.launch {
            runCatching { op() }
                .onSuccess { snapshot = it }
                .onFailure { smoke = "操作失败：${it.message}" }
            busy = false
        }
    }

    LaunchedEffect(Unit) {
        runCatching { repo.ensureDemoAndOpen() }
            .onSuccess { (snap, msg) ->
                snapshot = snap
                smoke = msg
            }
            .onFailure { smoke = "M2-SMOKE-FAIL: ${it.message}" }
    }

    val importLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri ->
        if (uri != null) runOp { repo.importFromUri(uri) }
    }

    Scaffold(
        bottomBar = {
            NavigationBar {
                NavigationBarItem(
                    selected = tab == 0,
                    onClick = { tab = 0 },
                    icon = { Icon(Icons.Filled.Search, contentDescription = null) },
                    label = { Text("查词") },
                )
                NavigationBarItem(
                    selected = tab == 1,
                    onClick = { tab = 1 },
                    icon = { Icon(Icons.Filled.List, contentDescription = null) },
                    label = { Text("词典") },
                )
            }
        },
    ) { pad ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(pad),
        ) {
            if (busy) {
                LinearProgressIndicator(Modifier.fillMaxWidth())
            }
            if (tab == 0) {
                SearchScreen(repo, snapshot, smoke, scope)
            } else {
                DictManagerScreen(repo, snapshot, smoke, runOp) {
                    importLauncher.launch(arrayOf("*/*"))
                }
            }
        }
    }
}

// —— 查词页（M2 增量 2：聚合 searchAll 卡片 + 前缀建议 + 生词本/历史） ——

@Composable
private fun SearchScreen(
    repo: DictRepository,
    snapshot: SessionSnapshot?,
    smoke: String,
    scope: kotlinx.coroutines.CoroutineScope,
) {
    var query by rememberSaveable { mutableStateOf("") }
    var mode by rememberSaveable { mutableStateOf("agg") }   // agg=词条聚合 / fulltext=全文
    var hits by remember { mutableStateOf(arrayOf<SearchHit>()) }
    var suggestions by remember { mutableStateOf(arrayOf<String>()) }
    var vocabLine by remember { mutableStateOf("生词本：-") }
    var historyLine by remember { mutableStateOf("历史：-") }
    var searched by remember { mutableStateOf(false) }

    fun performLookup(word: String, m: String) {
        if (word.isBlank()) return
        query = word
        scope.launch {
            hits = if (m == "agg") repo.searchAll(word) else repo.fullText(word, 20)
            suggestions = if (m == "agg") repo.prefixSuggest(word, 8) else emptyArray()
            historyLine = "历史：" + repo.addHistory(word).joinToString("、")
            searched = true
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
        Text(smoke, fontSize = 12.sp, fontWeight = FontWeight.Medium)

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedTextField(
                value = query,
                onValueChange = { query = it },
                label = { Text("词条") },
                modifier = Modifier.weight(1f),
                singleLine = true,
            )
            Button(onClick = { performLookup(query, mode) }) { Text("查询") }
        }

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(
                selected = mode == "agg",
                onClick = {
                    mode = "agg"
                    if (query.isNotBlank()) performLookup(query, "agg")
                },
                label = { Text("词条") },
            )
            FilterChip(
                selected = mode == "fulltext",
                onClick = {
                    mode = "fulltext"
                    if (query.isNotBlank()) performLookup(query, "fulltext")
                },
                label = { Text("全文") },
            )
        }

        if (suggestions.isNotEmpty()) {
            Text("前缀建议（点击查询）", fontWeight = FontWeight.SemiBold)
            // 建议 = 全量索引词表（core 语义不过滤启用态）；聚合结果才按启停过滤
            for (w in suggestions) {
                Text(
                    w,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { performLookup(w, mode) }
                        .padding(vertical = 2.dp),
                )
            }
        }

        if (hits.isEmpty()) {
            if (searched) Text("无结果", color = MaterialTheme.colorScheme.error)
        } else {
            val label = if (mode == "agg") "聚合结果 ${hits.size} 条" else "全文结果 ${hits.size} 条"
            Text(label, fontWeight = FontWeight.SemiBold)
            for (hit in hits) {
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text(
                            hit.dictName,
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.primary,
                            fontWeight = FontWeight.Medium,
                        )
                        Text(hit.word, fontSize = 17.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            hit.definition,
                            fontSize = 14.sp,
                            maxLines = 8,
                            overflow = TextOverflow.Ellipsis,
                        )
                        TextButton(onClick = {
                            scope.launch {
                                vocabLine = "生词本：" +
                                    repo.addVocab(hit.word, hit.definition).joinToString("、")
                            }
                        }) { Text("加入生词本") }
                    }
                }
            }
        }

        Text(vocabLine, fontSize = 13.sp)
        Text(historyLine, fontSize = 13.sp)
    }
}

// —— 词典管理页（SAF 导入 / 启停 / 删除 / 词量） ——

@Composable
private fun DictManagerScreen(
    repo: DictRepository,
    snapshot: SessionSnapshot?,
    smoke: String,
    runOp: (suspend () -> SessionSnapshot) -> Unit,
    onImport: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("词典管理", fontSize = 22.sp, fontWeight = FontWeight.Bold)
        val loaded = snapshot?.dicts?.count { it.loaded } ?: 0
        Text(
            "已装载 $loaded 部 · 索引 ${snapshot?.indexedWords ?: 0} 词",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.secondary,
        )

        Button(onClick = onImport, modifier = Modifier.fillMaxWidth()) {
            Text("导入词典文件（.mdx / .json / .dsl / .csv…）")
        }

        for (st in snapshot?.dicts.orEmpty()) {
            Card(Modifier.fillMaxWidth()) {
                Row(
                    modifier = Modifier.padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                        Text(
                            if (st.loaded) st.dictName else st.entry.display,
                            fontWeight = FontWeight.SemiBold,
                        )
                        Text(
                            if (st.loaded) "${st.wordCount} 词" else st.description,
                            fontSize = 12.sp,
                            color = if (st.loaded) MaterialTheme.colorScheme.secondary
                            else MaterialTheme.colorScheme.error,
                        )
                    }
                    Spacer(Modifier.width(8.dp))
                    Switch(
                        checked = st.entry.enabled,
                        // 未装载的词典开关无效（没名可禁），只对成功装载的生效
                        enabled = st.loaded,
                        onCheckedChange = { on -> runOp { repo.setEnabled(st.entry, on) } },
                    )
                    IconButton(onClick = { runOp { repo.remove(st.entry) } }) {
                        Icon(
                            Icons.Filled.Delete,
                            contentDescription = "删除 ${st.entry.display}",
                            tint = MaterialTheme.colorScheme.error,
                        )
                    }
                }
            }
        }

        Text(smoke, fontSize = 12.sp, color = MaterialTheme.colorScheme.outline)
    }
}
