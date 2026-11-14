// 离屏 UI 真点审计（BUG-010 口径：可点元素全接线，点不动的不许留）。
// 先例：dev/ui_sandbox.cpp——同一套离屏装配；区别在于本程序对每个可点
// 元素发真实 QMouseEvent（press+release 进 QQuickWindow 走完整交付链，
// 与用户点击同路径），并断言点击前后存在可观测状态差（状态行/模型数/
// 可见性/currentWord/口音设置…），逐项输出 PASS/FAIL 证据，任一 FAIL
// 退出码非 0。
//
// 用法：unidict_ui_click_audit <报告输出目录>
//   词典用 UNIDICT_DICTS 注入（ctest 指向 dev/fixtures/click_audit_dict.json，
//   fixture 保证 例句/词组/近义联想/全文 四个内容 tab 都有数据）。
// 证据分级：signal 级（combo popup 离屏无法真点菜单项，以激活同签名
//   信号验证 handler 接线）会如实标注；其余全部为真实点击路径。
#include <QGuiApplication>
#include <functional>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QFont>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QThread>

#include "../lookup_adapter.h"
#include "../platform_adapter.h"
#include "../voice_adapter.h"
#include "../vocab_store_adapter.h"
#include "../adapters/qt/data_store_qt.h"
#include "../adapters/qt/fulltext_manager_qt.h"
#include "../mobile_utils.h"
#include "../adapters/qt/sync_service_qt.h"
#include "../adapters/qt/sync_manager_qt.h"
#include "../adapters/qt/ai_service_qt.h"
#include "../adapters/qt/clipboard_qt.h"
#include "../adapters/qt/settings_qt.h"
#include "../startup_launcher.h"
#include "../tray_controller.h"

#include <QApplication>
#include "../theme.h"

using UnidictQml::Theme;

#include "click_audit_helpers.h"
#include "click_audit_sections.h"

using namespace ui_audit;

int main(int argc, char* argv[]) {
    // Windows 下 Qt 默认 message handler 不写 stderr（走 OutputDebugString），
    // ctest --output-on-failure 捕不到任何审计输出——自装 handler 把每条
    // PASS/FAIL 实时打进 stderr/stdout 并立刻 flush，失败时 CI 才有现场
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&,
                              const QString& msg) {
        const QByteArray raw = msg.toUtf8();
        fwrite(raw.constData(), 1, raw.size(), stderr);
        fputc('\n', stderr);
        fflush(stderr);
        fwrite(raw.constData(), 1, raw.size(), stdout);
        fputc('\n', stdout);
        fflush(stdout);
    });
    // 离屏软件渲染 + 用户目录隔离：必须在 QGuiApplication 之前设置
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Material");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    // 数据/配置全落临时目录：审计会写生词本/历史/设置，不污染真实用户数据
    const QString scratch = QDir::tempPath() + "/unidict_click_audit";
    QDir().mkpath(scratch + "/data");
    QDir().mkpath(scratch + "/xdg-config");
    QDir().mkpath(scratch + "/xdg-data");
    qputenv("UNIDICT_DATA_DIR", (scratch + "/data").toUtf8());
    // DataStoreQt 默认落 CWD 相对 ./data/unidict.json（不走 UNIDICT_DATA_DIR），
    // 显式钉到 scratch，审计写入绝不碰真实用户数据
    UnidictAdaptersQt::DataStoreQt::instance().setStoragePath(
        scratch + "/data/unidict.json");
    qputenv("XDG_CONFIG_HOME", (scratch + "/xdg-config").toUtf8());
    qputenv("XDG_DATA_HOME", (scratch + "/xdg-data").toUtf8());
    qputenv("HOME", scratch.toUtf8());
    // F9 自启密封：落盘目录钉进 scratch（Linux/macOS 实现读此缝；
    // Windows 走注册表不受影响，开→关自还原）
    qputenv("UNIDICT_AUTOSTART_DIR", (scratch + "/autostart").toUtf8());
    // QApplication：TrayController 的 QMenu 是 QWidget 系（qmlui/main.cpp
    // 同口径），纯 QGuiApplication 下构造菜单直接 fatal
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Unidict"));
    app.setOrganizationName(QStringLiteral("YourCompany"));
    // 同步开关缺省态隔离：审计沙盒目录跨次复用，清掉历史开关位，
    // 「同步默认关闭」断言不依赖上次运行（须与 SyncManagerQt 同为
    // IniFormat——NativeFormat 指向 Unidict.conf，清不到真实存储）
    {
        QSettings s(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("YourCompany"),
                    QStringLiteral("Unidict"));
        s.remove(QStringLiteral("sync/enabled"));
        s.sync();
    }

    QString outDir = scratch;
    if (app.arguments().size() > 1) outDir = app.arguments().at(1);
    QDir().mkpath(outDir);

    // 与 main.cpp/ui_sandbox 同构的适配器装配
    LookupAdapter adapter;
    if (!adapter.loadDictionariesFromEnv()) {
        qCritical("UNIDICT_DICTS 未指向可加载词典（审计前置条件）");
        return 2;
    }
    PlatformAdapter platform;
    VoiceAdapter voice;
    VocabStoreAdapter vocab;
    voice.setPronSourceMode(0);   // 本地 TTS 态（离屏环境可预期）
    voice.setPronAccent(0);
    UnidictAdaptersQt::FullTextManagerQt fulltext;
    fulltext.loadDictionariesFromEnv();
    UnidictAdaptersQt::SyncServiceQt sync;
    UnidictAdaptersQt::SyncManagerQt syncManager;
    UnidictAdaptersQt::AiServiceQt ai;
    UnidictAdaptersQt::ClipboardQt clip;
    UnidictAdaptersQt::SettingsQt settings;
    StartupLauncher startup;
    TrayController tray(&settings);
    MobileUtils mobileUtils;

    // 上下文装配双引擎复用：桌面面（MainDesktop.qml，B 系列同步）与
    AuditCtx ctx{/*win*/ nullptr, scratch, outDir, /*hasTts*/ false,
                 /*drawer*/ nullptr,
                 adapter, platform, voice, vocab, fulltext, sync,
                 syncManager, ai, clip, settings, startup, tray,
                 mobileUtils};

    QQmlApplicationEngine engine;
    registerAuditContext(engine, ctx);

    engine.load(QUrl("qrc:/MainDesktop.qml"));
    if (engine.rootObjects().isEmpty()) {
        qCritical("QML load failed");
        return 2;
    }
    auto* win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!win) {
        qCritical("root is not a QQuickWindow");
        return 2;
    }
    win->show();
    settle(win);
    ctx.win = win;

    // 生词本种子：保证 筛选 chips / 卡片 / +标签 / 笔记 可点
    vocab.addToVocabulary("hello", "A greeting; used when meeting someone.");
    vocab.addToVocabulary("world", "The earth and all its peoples.");
    vocab.addVocabTag("hello", "CET4");
    vocab.addVocabTag("hello", "greeting");
    vocab.addVocabTag("world", "CET4");
    vocab.setVocabNote("hello", "常用问候语");

    // ---- S0 词典卡头渲染管线（decorateHtml 主题拼接，BUG-010 之一） ----
    audit_s0(ctx);

    // ---- S1 朗读点击反馈（BUG-010：点击必有状态行反馈） ----
    audit_s1(ctx);

    // ---- S2 生词本/复制轻入口 ----
    audit_s2(ctx);

    // ---- S3 笔记弹层开关 ----
    audit_s3(ctx);

    // ---- S4 内容 tab：例句（重组加载反馈 + 喇叭读整句 + 词链跳转） ----
    audit_s4(ctx);

    // ---- S5 词组 tab ----
    audit_s5(ctx);

    // ---- S6 近义联想 tab ----
    audit_s6(ctx);

    // ---- S7 全文检索 tab（重组路径，加载状态行） ----
    audit_s7(ctx);

    // ---- S8 词典 tab：分组折叠 ----
    audit_s8(ctx);

    // ---- S9 组内词头链跳转（前缀/释义组的非当前词头） ----
    audit_s9(ctx);

    // ---- S10 释义区导航 ←/→ ----
    audit_s10(ctx);

    // ---- S11 侧栏：搜索+查按钮 ----
    audit_s11(ctx);

    // ---- S12 侧栏 tab：历史 ----
    audit_s12(ctx);

    // ---- S13 侧栏 tab：生词本（chips/卡/对话框） ----
    audit_s13(ctx);

    // ---- S25 生词本 CSV 导入（导入导出批）：按钮 signal 级（对话框
    audit_s25(ctx);

    // ---- S13-b 笔记内检索（Search within notes）：输入非空 → 列表切
    audit_s13b(ctx);

    // ---- S27 历史置顶（历史面最后一公里）：真点 pin → 置顶块插队双证
    audit_s27(ctx);

    // ---- S29 历史逐条删除（F5）：真点 ✕ → 行消失 + adapter entries
    audit_s29(ctx);

    // ---- S14 头部三入口 + 页脚清历史 ----
    audit_s14(ctx);

    // ---- S15 抽屉：先测默认「取词」页控件，再切「语音」页测语音控件
    audit_s15(ctx);

    // ---- S32 开机自启（巡检续批④ F9）：真点开关 → 落盘物生灭 +
    audit_s32(ctx);

    // ---- S33 取词窗热键（F10）：真点应用钮 → 注册结果两态如实回显 +
    audit_s33(ctx);

    // ---- S34 关闭到托盘（候选#2 F11）：托盘可用性如实上报 + 开关翻面
    audit_s34(ctx);

    // ---- S16 页脚清历史 ----
    audit_s16(ctx);

    // ---- S17 悬浮取词窗全控件 ----
    audit_s17(ctx);

    // ---- S28 外观三态（F4）：锁定深色 → Theme 翻暗 + 记忆落盘；
    audit_s28(ctx);

    // ---- S30 释义字号（F7）：radio 真点 → defFontSize 翻面 + 落盘 +
    audit_s30(ctx);

    // ---- S38 查询键归一化（P1 text-norm）：radio 真点 → core 档位翻面
    audit_s38(ctx);

    // ---- S18 同步备份 tab：红线开关面 + 自救口 ----
    audit_s18(ctx);

    // ---- S19 词典库 tab（P-6）：目录导入 + 装载列表 + 隔离重试 ----
    audit_s19(ctx);

    // ---- S20 AI 造句弹层（roadmap AI sentence generation）----
    audit_s20(ctx);

    // ---- S21 划词取词（X11 面）：非 xcb（offscreen）开关整块隐藏 ----
    audit_s21(ctx);

    // ---- S21b F13b macOS 焦点窗选区读取：非 macOS 整块隐藏 + 授权契约 ----
    audit_s21b(ctx);

    // ---- S22 UI 批一：命中词典 chip / 内容 tab 分区折叠 / 建议分组 /
    audit_s22(ctx);

    // ---- S23 UI 批二：生词本表格/卡片双视图 + 词典元信息面板 + 笔记荧光笔多色 ----
    audit_s23(ctx);

    // ---- S37 移动端根（Main.qml）同步预览冲突裁决（remoteOnly 选择链）：
    audit_s37(ctx);

    // ---- 汇总 ----
    const QString summary = QString("==== click audit: %1 passed, %2 failed ====")
                                .arg(g_pass).arg(g_fail);
    g_log << summary;
    qInfo("%s", qPrintable(summary));
    QFile report(outDir + "/click_audit_report.txt");
    if (report.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream ts(&report);
        for (const QString& line : g_log) ts << line << "\n";
    }
    return g_fail == 0 ? 0 : 3;
}