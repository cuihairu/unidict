package dev.unidict.mobile

import android.app.Application

// 进程级仓库单例：MainActivity 与 FloatingLookupService（M8 悬浮取词窗）
// 共享同一 core 会话。core 句柄无内部锁（mobile_plan §4），生词本落盘
// store.json——各开各的会话会互相覆盖写入，必须单会话单写者。
class UnidictApp : Application() {
    val repo: DictRepository by lazy { DictRepository(this) }
}
