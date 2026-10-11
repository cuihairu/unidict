package dev.unidict.mobile

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.util.Log
import android.widget.RemoteViews

// M8-B 桌面可缩放 widget（roadmap Mobile: homescreen resizable widget）：
// 常驻一个可拖缩的小部件——展示最近查过的词（取自共享 DictRepository 历史），
// 点按进主窗快速查词页（与 M7 快捷方式同路径，光标就位弹键盘）。
// RemoteViews 只支持受限视图集（无 EditText），故widget只作「展示+入口」。
// 验收令牌：M8-WIDGET-OK bound|updated|launch，失败打 M8-WIDGET-FAIL。
class QuickLookupWidgetProvider : AppWidgetProvider() {

    override fun onUpdate(
        context: Context,
        appWidgetManager: AppWidgetManager,
        appWidgetIds: IntArray,
    ) {
        for (id in appWidgetIds) {
            updateOne(context, appWidgetManager, id)
        }
        Log.i(TAG, "M8-WIDGET-OK updated ids=${appWidgetIds.size}")
    }

    override fun onReceive(context: Context, intent: Intent) {
        super.onReceive(context, intent)
        if (intent.action == ACTION_REFRESH) {
            val mgr = AppWidgetManager.getInstance(context)
            val ids = mgr.getAppWidgetIds(ComponentName(context, QuickLookupWidgetProvider::class.java))
            if (ids.isNotEmpty()) {
                onUpdate(context, mgr, ids)
                Log.i(TAG, "M8-WIDGET-OK bound ids=${ids.size}")
            }
        }
    }

    private fun updateOne(context: Context, mgr: AppWidgetManager, id: Int) {
        val views = RemoteViews(context.packageName, R.layout.widget_quick_lookup)
        // 最近查词读 core 历史（单线程执行器，onUpdate 主线程短任务）
        val lastWord = runCatching {
            val app = context.applicationContext as UnidictApp
            kotlinx.coroutines.runBlocking {
                app.repo.history(1).firstOrNull().orEmpty()
            }
        }.getOrDefault("")
        views.setTextViewText(R.id.widget_word, lastWord.ifEmpty { "点按开始查词" })
        views.setOnClickPendingIntent(R.id.widget_root, launchIntent(context))
        mgr.updateAppWidget(id, views)
    }

    // 点按 → 主窗快速查词（与桌面快捷方式同意图）
    private fun launchIntent(context: Context): PendingIntent {
        val i = Intent(context, MainActivity::class.java)
            .setAction(MainActivity.ACTION_QUICK_LOOKUP)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP)
        return PendingIntent.getActivity(
            context, 0, i, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
    }

    companion object {
        const val TAG = "unidict"
        const val ACTION_REFRESH = "dev.unidict.mobile.action.WIDGET_REFRESH"

        // 查词后主界面广播一次 → 所有已绑定 widget 刷新最近词
        fun broadcastRefresh(context: Context) {
            val i = Intent(context, QuickLookupWidgetProvider::class.java).setAction(ACTION_REFRESH)
            context.sendBroadcast(i)
        }
    }
}
