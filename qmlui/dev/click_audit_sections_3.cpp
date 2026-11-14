// 离屏 UI 真点审计场景（LAY-008 自 ui_click_audit.cpp 拆出，第 3/6 批）：
// 场景清单与调用顺序见 click_audit_sections.h，共享面见 click_audit_helpers.h。

#include "click_audit_sections.h"
#include "click_audit_helpers.h"
#include "../theme.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariant>

using namespace ui_audit;
using UnidictQml::Theme;

void audit_s14(AuditCtx& c) {
// ---- S14 头部三入口 + 页脚清历史 ----
c.drawer = popupObj(c.win, "toolsDrawer");
clickItem(c.win, item(c.win, "headerHistoryButton"));
audit(lpProp(c.win, "currentTabIndex").toInt() == 1,
      "头部「历史」按钮", "currentTabIndex=1");
clickItem(c.win, item(c.win, "headerVocabButton"));
audit(lpProp(c.win, "currentTabIndex").toInt() == 2,
      "头部「生词本」按钮", "currentTabIndex=2");
clickItem(c.win, item(c.win, "headerSettingsButton"));
settle(c.win);   // Drawer 开启是动画过渡，settle 后内容才在屏上可点
audit(popupVisible(c.drawer), "头部「设置」按钮开抽屉",
      "toolsDrawer visible");

}

void audit_s15(AuditCtx& c) {
// ---- S15 抽屉：先测默认「取词」页控件，再切「语音」页测语音控件
// （StackLayout 切页后隐藏页控件点不到，顺序不能反）----
if (auto* clipSwitch = item(c.win, "clipboardSwitch")) {
    const bool clipBefore = clipSwitch->property("checked").toBool();
    clickItem(c.win, clipSwitch);
    audit(clipSwitch->property("checked").toBool() != clipBefore
              && c.platform.isClipboardAutoLookupEnabled() != clipBefore,
          "剪贴板取词开关点击（UI+core 双证）",
          QString("checked %1→%2").arg(clipBefore)
              .arg(!clipBefore));
    clickItem(c.win, clipSwitch);   // 还原
    audit(c.platform.isClipboardAutoLookupEnabled() == clipBefore,
          "剪贴板取词开关还原", "core 状态回原");
} else {
    audit(false, "剪贴板取词开关点击（UI+core 双证）", "clipboardSwitch 不存在");
}

if (auto* qlSwitch = item(c.win, "quickLookupSwitch")) {
    const bool qlBefore = qlSwitch->property("checked").toBool();
    clickItem(c.win, qlSwitch);
    audit(qlSwitch->property("checked").toBool() != qlBefore,
          "取词悬浮窗开关点击", QString("checked %1→%2").arg(qlBefore).arg(!qlBefore));
    clickItem(c.win, qlSwitch);   // 还原
} else {
    audit(false, "取词悬浮窗开关点击", "quickLookupSwitch 不存在");
}

if (auto* poll = item(c.win, "pollSlider")) {
    const qreal pollBefore = poll->property("value").toReal();
    clickItemRel(c.win, poll, 0.9, 0.5);
    audit(poll->property("value").toReal() != pollBefore,
          "轮询间隔 Slider 点击改值",
          QString("value %1→%2").arg(pollBefore).arg(poll->property("value").toReal()));
} else {
    audit(false, "轮询间隔 Slider 点击改值", "pollSlider 不存在");
}

// SpinBox 真实用户路径=点 +/- 步进钮；点击控件中央落焦在内部
// TextInput，Key_Up 会被光标移动消费而非步进
auto spinStepTest = [&](QQuickWindow* w, const char* name, const QString& label) {
    if (auto* spin = item(w, name)) {
        const int spinBefore = spin->property("value").toInt();
        QObject* upGroup = spin->property("up").value<QObject*>();
        QQuickItem* upInd = upGroup
                                ? upGroup->property("indicator").value<QQuickItem*>()
                                : nullptr;
        if (upInd) {
            clickItem(w, upInd);
            audit(spin->property("value").toInt() == spinBefore + 1,
                  label + "「+」步进钮点击",
                  QString("value %1→%2").arg(spinBefore)
                      .arg(spin->property("value").toInt()));
            QObject* downGroup = spin->property("down").value<QObject*>();
            QQuickItem* downInd = downGroup
                                      ? downGroup->property("indicator").value<QQuickItem*>()
                                      : nullptr;
            if (downInd) clickItem(w, downInd);   // 还原
        } else {
            audit(false, label + "「+」步进钮点击", "up indicator 不存在");
        }
    } else {
        audit(false, label + "「+」步进钮点击", QString(name) + " 不存在");
    }
};
spinStepTest(c.win, "minLenSpin", "最短词长 SpinBox");
spinStepTest(c.win, "maxLenSpin", "最长词长 SpinBox");

}

void audit_s32(AuditCtx& c) {
// ---- S32 开机自启（巡检续批④ F9）：真点开关 → 落盘物生灭 +
// isEnabled 翻面 + 状态行。Linux/macOS 经 UNIDICT_AUTOSTART_DIR
// 密封断言文件内容；Windows 走真实 HKCU Run 键（开→关自还原）----
if (auto* stSwitch = item(c.win, "startupToggle")) {
    if (!stSwitch->isVisible()) {
        audit(false, "开机自启开关可见（平台支持面）",
              "startupToggle 不可见（平台应支持）");
    } else {
        if (c.startup.isEnabled()) c.startup.setEnabled(false);  // 基线清残留
        scrollIntoView(c.win, stSwitch);
        clickItem(c.win, stSwitch);
        settle(c.win);
        const bool on = c.startup.isEnabled();
        const QString exec =
            QCoreApplication::applicationFilePath();
#ifndef Q_OS_WIN
        const QString fPath =
            QDir(qEnvironmentVariable("UNIDICT_AUTOSTART_DIR"))
                .absoluteFilePath(c.startup.entryFileName());
        QFileInfo fi(fPath);
        QString content;
        if (fi.exists()) {
            QFile f(fPath);
            f.open(QIODevice::ReadOnly);
            content = QString::fromUtf8(f.readAll());
        }
        const bool execHit = content.contains(exec);
        const QString detailOn =
            QString("on=%1 file=%2 execHit=%3")
                .arg(on).arg(fi.exists()).arg(execHit);
#else
        const QString detailOn =
            QString("on=%1 (HKCU Run)").arg(on);
#endif
        audit(on && stSwitch->property("checked").toBool()
                  && statusText(c.win).startsWith("已开启开机自启")
#ifdef Q_OS_WIN
                  ,
              "开机自启开启（真点：注册物+状态行）", detailOn);
#else
                  && fi.exists() && execHit,
              "开机自启开启（真点：注册物+状态行）", detailOn);
#endif
        clickItem(c.win, stSwitch);
        settle(c.win);
#ifndef Q_OS_WIN
        const bool gone = !QFileInfo::exists(fPath);
#else
        const bool gone = !c.startup.isEnabled();
#endif
        audit(!c.startup.isEnabled() && gone
                  && statusText(c.win).startsWith("已关闭开机自启"),
              "开机自启关闭（真点：注册物移除）",
              QString("enabled=%1 gone=%2")
                  .arg(c.startup.isEnabled()).arg(gone));
    }
} else {
    audit(false, "开机自启开启（真点：注册物+状态行）",
          "startupToggle 不存在");
}

}

void audit_s33(AuditCtx& c) {
// ---- S33 取词窗热键（F10）：真点应用钮 → 注册结果两态如实回显 +
// 落盘联动 + 清空路径。真按键链路（系统级按下→弹取词窗）离屏不可测，
// 本项止步接线级：只断言注册成功/失败两态之一且 UI/存储联动一致
//（Linux offscreen 恒走失败态；Windows 视 runner 环境两态皆可）----
{
    clickItem(c.win, item(c.win, "toolsTab3"));
    settle(c.win);
    const QString hkBaseline =
        c.settings.getString(QStringLiteral("quicklookup/hotkey"),
                           QStringLiteral("Alt+Q"));
    auto* hkField = item(c.win, "quickHotkeyField");
    auto* hkApply = item(c.win, "hotkeyApplyButton");
    auto* hkState = item(c.win, "hotkeyState");
    if (hkField && hkApply && hkState) {
        hkField->setProperty("text", QStringLiteral("Ctrl+Alt+U"));
        scrollIntoView(c.win, hkApply);
        clickItem(c.win, hkApply);
        settle(c.win);
        const QString state1 = hkState->property("text").toString();
        const bool ok1 = state1.startsWith(QStringLiteral("已生效: "));
        const bool fail1 = state1.startsWith(QStringLiteral("注册失败"));
        const bool stored1 =
            c.settings.getString(QStringLiteral("quicklookup/hotkey"))
                == QStringLiteral("Ctrl+Alt+U");
        audit((ok1 || fail1) && stored1 == ok1,
              "取词窗热键注册回显与落盘联动（接线级）",
              QString("state=%1, stored=%2").arg(state1).arg(stored1));
        // 清空路径全平台确定：只注销+状态行，不触 native 注册
        hkField->setProperty("text", QString());
        clickItem(c.win, hkApply);
        settle(c.win);
        audit(hkState->property("text").toString()
                      == QStringLiteral("已清除取词热键"),
              "取词窗热键清空（真点：注销+状态行）",
              QString("state=%1")
                  .arg(hkState->property("text").toString()));
        // 还原存储基线（native 注册态随审计进程退出即弃，无需回注册）
        c.settings.setString(QStringLiteral("quicklookup/hotkey"),
                           hkBaseline);
    } else {
        audit(false, "取词窗热键注册回显与落盘联动（接线级）",
              "quickHotkeyField/hotkeyApplyButton/hotkeyState 不存在");
    }
}

}

void audit_s34(AuditCtx& c) {
// ---- S34 关闭到托盘（候选#2 F11）：托盘可用性如实上报 + 开关翻面
// 落盘 +（托盘可用时）真 close→hide→showMainWindow 还原链。托盘
// 不可用（Linux offscreen）只验决策面；图标/菜单真交互离屏不可测，
// 如实注明 ----
{
    audit(c.tray.available() == QSystemTrayIcon::isSystemTrayAvailable(),
          "托盘可用性如实上报",
          QString("available=%1").arg(c.tray.available()));
    const bool cttBaseline =
        c.settings.getBool(QStringLiteral("ui/close_to_tray"), true);
    c.tray.setCloseToTrayEnabled(!cttBaseline);
    audit(c.tray.closeToTrayEnabled() == !cttBaseline
              && c.settings.getBool(QStringLiteral("ui/close_to_tray"),
                                  true) == !cttBaseline,
          "关闭到托盘开关翻面+落盘",
          QString("enabled=%1 stored=%2")
              .arg(c.tray.closeToTrayEnabled())
              .arg(c.settings.getBool(QStringLiteral("ui/close_to_tray"),
                                    true)));
    if (c.tray.available()) {
        c.tray.setCloseToTrayEnabled(true);
        c.win->close();   // → QML onClosing：shouldHideToTray 命中即拦
        settle(c.win);
        const bool hidden = !c.win->isVisible();
        if (hidden) c.tray.showMainWindow();
        settle(c.win);
        audit(hidden && c.win->isVisible(),
              "close→托盘隐藏→showMainWindow 还原",
              QString("hidden=%1 restored=%2")
                  .arg(hidden)
                  .arg(c.win->isVisible()));
        c.tray.setCloseToTrayEnabled(false);
        audit(!c.tray.shouldHideToTray(),
              "开关关→不拦 close（决策面翻面）",
              "shouldHideToTray=false");
    } else {
        audit(!c.tray.shouldHideToTray(),
              "托盘不可用→不拦 close（保持关窗即退出）",
              "shouldHideToTray=false");
    }
    c.tray.setCloseToTrayEnabled(cttBaseline);   // 还原基线
}

// 切「语音」页：StackLayout 布局传递需要一帧，settle 后再断言可见
clickItem(c.win, item(c.win, "toolsTab1"));
settle(c.win);
QQuickItem* volumeSlider = item(c.win, "volumeSlider");
audit(volumeSlider && volumeSlider->isVisible(),
      "抽屉「语音」tab 切换", "volumeSlider 可见");
clickItem(c.win, item(c.win, "stopSpeakButton"));
audit(true, "「停止」按钮点击", "执行无异常（停止播放副作用离屏不可观测）");
clickItem(c.win, item(c.win, "refreshVoicesButton"));
audit(true, "「刷新」语音按钮点击", "执行无异常（重扫语音副作用离屏不可观测）");

// combo popup 离屏无法真点菜单项：以同签名 activated 信号验证 handler
// 接线。QML handler 读的是 currentIndex 属性（非信号参数）——先设
// currentIndex 再发 activated，才等价于真点菜单项
QObject* accentCombo = c.win->findChild<QObject*>("pronAccentCombo");
if (accentCombo) {
    accentCombo->setProperty("currentIndex", 2);
    QMetaObject::invokeMethod(accentCombo, "activated", Q_ARG(int, 2));
    audit(c.voice.pronAccent() == 2, "口音 ComboBox 选择接线（signal 级）",
          QString("pronAccent=%1").arg(c.voice.pronAccent()));
    accentCombo->setProperty("currentIndex", 0);
    QMetaObject::invokeMethod(accentCombo, "activated", Q_ARG(int, 0));
} else {
    audit(false, "口音 ComboBox 选择接线（signal 级）", "pronAccentCombo 不存在");
}
QObject* sourceCombo = c.win->findChild<QObject*>("pronSourceCombo");
if (sourceCombo) {
    sourceCombo->setProperty("currentIndex", 1);
    QMetaObject::invokeMethod(sourceCombo, "activated", Q_ARG(int, 1));
    audit(c.voice.pronSourceMode() == 1, "发音源 ComboBox 选择接线（signal 级）",
          QString("pronSourceMode=%1").arg(c.voice.pronSourceMode()));
    sourceCombo->setProperty("currentIndex", 0);
    QMetaObject::invokeMethod(sourceCombo, "activated", Q_ARG(int, 0));
    audit(c.voice.pronSourceMode() == 0, "发音源还原本地态", "pronSourceMode=0");
} else {
    audit(false, "发音源 ComboBox 选择接线（signal 级）", "pronSourceCombo 不存在");
}

if (c.drawer) c.drawer->setProperty("visible", false);
settle(c.win);   // Drawer 关闭过渡未完会挡住页脚点击

}
