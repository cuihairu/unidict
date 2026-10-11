package dev.unidict.mobile

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.os.IBinder
import android.util.Log
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch

// M8 悬浮取词窗（roadmap Mobile: floating lookup widget）：可拖动悬浮球，
// 点开成迷你查词面板——聚合最优组首条释义 + 朗读/加生词/主窗打开/收起。
// 与桌面 QuickLookupPane 同语义（「快」，不打断当前应用），查词走共享
// DictRepository（UnidictApp 单例，单会话单写者）。
// 权限：SYSTEM_ALERT_WINDOW 经设置页引导授权（Settings.canDrawOverlays）；
// FGS specialUse（targetSdk 34+ 强制类型声明），常驻通知一条（33+ 无
// POST_NOTIFICATIONS 运行时授权时通知被隐藏、服务照常运行）。
// 验收令牌：M8-FLOAT-OK ball_shown|ball_rect=|panel_shown|panel_rect=|
// lookup=<w> hits=<n>|speak|vocab_added|open_main|collapsed|service_stopped，
// 失败打 M8-FLOAT-FAIL（logcat 单通道，供 adb 验收 grep）。
class FloatingLookupService : Service() {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val repo get() = (application as UnidictApp).repo
    private var tts: UnidictTts? = null

    private var wm: WindowManager? = null
    private var ball: View? = null
    private var panel: View? = null
    private var queryInput: EditText? = null
    private var resultView: TextView? = null
    private var lastWord = ""
    private var lastDefinition = ""

    // 设置页状态行读这个（同进程静态，拉起/停止双向同步）
    companion object {
        const val ACTION_OPEN_WORD = "dev.unidict.mobile.action.OPEN_WORD"
        const val TAG = "unidict"
        var running = false
            private set
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        running = true
        startForeground(NOTIFY_ID, buildNotification())
        tts = UnidictTts(this) { /* 悬浮窗无状态行，logcat 单通道 */ }
        wm = getSystemService(WINDOW_SERVICE) as WindowManager
        showBall()
        Log.i(TAG, "M8-FLOAT-OK ball_shown")
    }

    override fun onDestroy() {
        running = false
        tts?.shutdown()
        tts = null
        scope.cancel()
        wm?.let { w ->
            ball?.let { runCatching { w.removeView(it) } }
            panel?.let { runCatching { w.removeView(it) } }
        }
        ball = null
        panel = null
        Log.i(TAG, "M8-FLOAT-OK service_stopped")
        super.onDestroy()
    }

    // —— 悬浮球：可拖动，轻点展开面板 ——

    private fun showBall() {
        val size = dp(44)
        val box = FrameLayout(this).apply {
            background = GradientDrawable().apply {
                shape = GradientDrawable.OVAL
                setColor(BRAND)
            }
            elevation = dp(4).toFloat()
        }
        val label = TextView(this).apply {
            text = "查"
            setTextColor(Color.WHITE)
            textSize = 18f
            gravity = Gravity.CENTER
        }
        box.addView(
            label,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )

        val params = WindowManager.LayoutParams(
            size,
            size,
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
            PixelFormat.TRANSLUCENT,
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            x = dp(12)
            y = dp(160)
        }

        var downX = 0f
        var downY = 0f
        var moved = false
        box.setOnTouchListener { _, ev ->
            when (ev.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    downX = ev.rawX
                    downY = ev.rawY
                    moved = false
                    true
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = ev.rawX - downX
                    val dy = ev.rawY - downY
                    if (moved || dx * dx + dy * dy > slop()) {
                        moved = true
                        params.x += (ev.rawX - downX).toInt().also { downX = ev.rawX }
                        params.y += (ev.rawY - downY).toInt().also { downY = ev.rawY }
                        wm?.updateViewLayout(box, params)
                    }
                    true
                }
                MotionEvent.ACTION_UP -> {
                    if (!moved) showPanel(params.x, params.y)
                    true
                }
                else -> false
            }
        }
        wm?.addView(box, params)
        ball = box
        Log.i(TAG, "M8-FLOAT-OK ball_rect=${params.x},${params.y},$size,$size")
    }

    private fun slop(): Float {
        val c = ViewConfigurationCompat.scaledTouchSlop(this)
        return (c * c).toFloat()
    }

    // —— 迷你查词面板 ——

    private fun showPanel(ballX: Int, ballY: Int) {
        if (panel != null) return
        val dark =
            (resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) ==
                Configuration.UI_MODE_NIGHT_YES
        val bg = if (dark) 0xFF1E1B18.toInt() else Color.WHITE
        val fg = if (dark) 0xFFEDE7E0.toInt() else 0xFF201A17.toInt()
        val sub = if (dark) 0xFF9E948A.toInt() else 0xFF8A8078.toInt()

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = GradientDrawable().apply {
                setColor(bg)
                cornerRadius = dp(12).toFloat()
            }
            elevation = dp(6).toFloat()
            setPadding(dp(12), dp(10), dp(12), dp(10))
        }

        // 头行：词 + 收起
        val titleRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val title = TextView(this).apply {
            text = "悬浮查词"
            setTextColor(fg)
            textSize = 15f
            gravity = Gravity.CENTER_VERTICAL
        }
        val collapse = Button(this).apply {
            text = "—"
            contentDescription = "float_collapse"
            setTextColor(fg)
            backgroundTintList = android.content.res.ColorStateList.valueOf(Color.TRANSPARENT)
        }
        titleRow.addView(
            title,
            LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f),
        )
        titleRow.addView(collapse)
        root.addView(titleRow)

        // 输入行：词框 + 查
        val inputRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val input = EditText(this).apply {
            hint = "输入单词"
            setTextColor(fg)
            setHintTextColor(sub)
            textSize = 14f
            setSingleLine(true)
            contentDescription = "float_input"
            setOnEditorActionListener { _, _, _ ->
                lookup(text.toString())
                true
            }
        }
        val go = Button(this).apply {
            text = "查"
            contentDescription = "float_search"
            setTextColor(fg)
        }
        inputRow.addView(
            input,
            LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f),
        )
        inputRow.addView(go)
        root.addView(inputRow)
        queryInput = input

        // 结果区：聚合最优组首条（与桌面 QuickLookupPane 同口径）
        val result = TextView(this).apply {
            setTextColor(fg)
            textSize = 13f
            setLineSpacing(dp(2).toFloat(), 1f)
        }
        val scroller = ScrollView(this).apply { isVerticalScrollBarEnabled = true }
        scroller.addView(
            result,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
            ),
        )
        root.addView(
            scroller,
            LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f,
            ).apply { setMargins(0, dp(8), 0, dp(8)) },
        )
        resultView = result

        // 动作行：朗读 / 加生词 / 主窗打开
        val actionRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val speak = Button(this).apply {
            text = "朗读"
            contentDescription = "float_speak"
            setTextColor(fg)
        }
        val vocab = Button(this).apply {
            text = "生词"
            contentDescription = "float_vocab"
            setTextColor(fg)
        }
        val openMain = Button(this).apply {
            text = "主窗"
            contentDescription = "float_open_main"
            setTextColor(fg)
        }
        for (b in listOf(speak, vocab, openMain)) {
            actionRow.addView(
                b,
                LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f),
            )
        }
        root.addView(actionRow)

        go.setOnClickListener { lookup(input.text.toString()) }
        speak.setOnClickListener { speakLast() }
        vocab.setOnClickListener { addVocabLast() }
        openMain.setOnClickListener { openInMain() }
        collapse.setOnClickListener { collapse() }

        val params = WindowManager.LayoutParams(
            dp(300),
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            0,  // 有焦点：EditText 可输入
            PixelFormat.TRANSLUCENT,
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            x = ballX.coerceAtLeast(dp(8))
            y = ballY.coerceAtLeast(dp(8))
        }

        wm?.addView(root, params)
        panel = root
        result.text = if (lastDefinition.isEmpty()) "输入单词开始查词" else result.text
        Log.i(TAG, "M8-FLOAT-OK panel_shown")
        input.requestFocus()
        // 越屏回钳（与桌面 QuickLookupPane 同口径）：悬浮球拖到屏幕右/下
        // 缘时面板（825px 宽）会出屏，等布局量完实际尺寸后钳回安全区
        root.post {
            val p = panel ?: return@post
            val w = resources.displayMetrics.widthPixels
            val h = resources.displayMetrics.heightPixels
            params.x = params.x.coerceIn(dp(8), (w - p.width - dp(8)).coerceAtLeast(dp(8)))
            params.y = params.y.coerceIn(dp(8), (h - p.height - dp(8)).coerceAtLeast(dp(8)))
            runCatching { wm?.updateViewLayout(p, params) }
        }
    }

    private fun collapse() {
        val p = panel ?: return
        wm?.removeView(p)
        panel = null
        queryInput = null
        resultView = null
        Log.i(TAG, "M8-FLOAT-OK collapsed")
    }

    // —— 查词与动作 ——

    private fun lookup(rawWord: String) {
        val word = rawWord.trim()
        if (word.isEmpty()) return
        lastWord = word
        scope.launch {
            runCatching {
                // 主界面没跑过（冷启直拉服务）也保证会话就绪（首启种子照走）
                if (!repo.sessionReady) repo.ensureDemoAndOpen()
                val hits = repo.searchAll(word)
                if (hits.isNotEmpty()) {
                    val h = hits[0]
                    lastDefinition = h.definition
                    resultView?.text = "【${h.dictName}】\n${h.definition}"
                    repo.addHistory(word)
                    Log.i(TAG, "M8-FLOAT-OK lookup=$word hits=${hits.size}")
                } else {
                    lastDefinition = ""
                    resultView?.text = "未收录：$word"
                    Log.i(TAG, "M8-FLOAT-OK lookup=$word hits=0")
                }
            }.onFailure {
                lastDefinition = ""
                resultView?.text = "查询失败：${it.message}"
                Log.i(TAG, "M8-FLOAT-FAIL lookup=$word err=${it.message}")
            }
        }
    }

    private fun speakLast() {
        if (lastWord.isEmpty()) return
        tts?.speak(lastWord)
        Log.i(TAG, "M8-FLOAT-OK speak word=$lastWord")
    }

    private fun addVocabLast() {
        if (lastWord.isEmpty() || lastDefinition.isEmpty()) return
        val word = lastWord
        val def = lastDefinition
        scope.launch {
            runCatching {
                repo.addVocab(word, def)
                Log.i(TAG, "M8-FLOAT-OK vocab_added word=$word")
            }.onFailure {
                Log.i(TAG, "M8-FLOAT-FAIL vocab word=$word err=${it.message}")
            }
        }
    }

    private fun openInMain() {
        if (lastWord.isEmpty()) return
        val i = Intent(this, MainActivity::class.java)
            .setAction(ACTION_OPEN_WORD)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP)
            .putExtra(Intent.EXTRA_TEXT, lastWord)
        startActivity(i)
        collapse()
        Log.i(TAG, "M8-FLOAT-OK open_main word=$lastWord")
    }

    // —— 常驻通知 ——

    private fun buildNotification(): Notification {
        val nm = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            nm.createNotificationChannel(
                NotificationChannel(
                    CHANNEL_ID,
                    "悬浮取词",
                    NotificationManager.IMPORTANCE_MIN,
                ).apply { setShowBadge(false) },
            )
        }
        val open = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE,
        )
        val b = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, CHANNEL_ID)
        } else {
            @Suppress("DEPRECATION")
            Notification.Builder(this)
        }
        return b
            .setContentTitle("Unidict 悬浮取词")
            .setContentText("点按返回主界面")
            .setSmallIcon(android.R.drawable.ic_menu_search)
            .setContentIntent(open)
            .setOngoing(true)
            .build()
    }

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()

    private val NOTIFY_ID = 808
    private val CHANNEL_ID = "unidict_float"

    // 品牌色与 UnidictTheme 同源（#b11964）
    private val BRAND = 0xFFB11964.toInt()
}

// ViewConfiguration 拿系统触摸误判阈值（不引 androidx.core 的薄读法）
private object ViewConfigurationCompat {
    fun scaledTouchSlop(c: Context): Int =
        android.view.ViewConfiguration.get(c).scaledTouchSlop
}
