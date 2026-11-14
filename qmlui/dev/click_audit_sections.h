// 离屏 UI 真点审计场景面（LAY-008 自 ui_click_audit.cpp main 拆出）：
// AuditCtx 携带主 TU 装配的窗口与适配器引用，36 个场景函数按 main 原
// 顺序调用（click_audit_sections_{1..6}.cpp 提供）。场景间唯一共享
// 状态是 hasTts（S1 测得、S17 消费）；registerAuditContext 是 main 的
// 上下文装配 lambda 的自由函数化（S37 第二引擎复用同一装配）。

#ifndef UNIDICT_CLICK_AUDIT_SECTIONS_H
#define UNIDICT_CLICK_AUDIT_SECTIONS_H

#include <QString>

#include "../lookup_adapter.h"
#include "../platform_adapter.h"
#include "../voice_adapter.h"
#include "../vocab_store_adapter.h"
#include "../adapters/qt/fulltext_manager_qt.h"
#include "../adapters/qt/sync_service_qt.h"
#include "../adapters/qt/sync_manager_qt.h"
#include "../adapters/qt/ai_service_qt.h"
#include "../adapters/qt/clipboard_qt.h"
#include "../adapters/qt/settings_qt.h"
#include "../startup_launcher.h"
#include "../tray_controller.h"
#include "../mobile_utils.h"

class QQuickWindow;
class QQmlApplicationEngine;
class QObject;

struct AuditCtx {
    QQuickWindow* win = nullptr;
    QString scratch;
    QString outDir;
    bool hasTts = false;
    QObject* drawer = nullptr;   // S14 打开的工具抽屉，尾部场景关回
    LookupAdapter& adapter;
    PlatformAdapter& platform;
    VoiceAdapter& voice;
    VocabStoreAdapter& vocab;
    UnidictAdaptersQt::FullTextManagerQt& fulltext;
    UnidictAdaptersQt::SyncServiceQt& sync;
    UnidictAdaptersQt::SyncManagerQt& syncManager;
    UnidictAdaptersQt::AiServiceQt& ai;
    UnidictAdaptersQt::ClipboardQt& clip;
    UnidictAdaptersQt::SettingsQt& settings;
    StartupLauncher& startup;
    TrayController& tray;
    MobileUtils& mobileUtils;
};

// main 的 registerCtx lambda 自由函数化（参数 e 与 lambda 同名）：两引擎（MainDesktop / S37 的
// Main.qml）注册同一批适配器实例为 QML 上下文属性。
void registerAuditContext(QQmlApplicationEngine& e, AuditCtx& c);

void audit_s0(AuditCtx& c);
void audit_s1(AuditCtx& c);
void audit_s2(AuditCtx& c);
void audit_s3(AuditCtx& c);
void audit_s4(AuditCtx& c);
void audit_s5(AuditCtx& c);
void audit_s6(AuditCtx& c);
void audit_s7(AuditCtx& c);
void audit_s8(AuditCtx& c);
void audit_s9(AuditCtx& c);
void audit_s10(AuditCtx& c);
void audit_s11(AuditCtx& c);
void audit_s12(AuditCtx& c);
void audit_s13(AuditCtx& c);
void audit_s25(AuditCtx& c);
void audit_s13b(AuditCtx& c);
void audit_s27(AuditCtx& c);
void audit_s29(AuditCtx& c);
void audit_s14(AuditCtx& c);
void audit_s15(AuditCtx& c);
void audit_s32(AuditCtx& c);
void audit_s33(AuditCtx& c);
void audit_s34(AuditCtx& c);
void audit_s16(AuditCtx& c);
void audit_s17(AuditCtx& c);
void audit_s28(AuditCtx& c);
void audit_s30(AuditCtx& c);
void audit_s38(AuditCtx& c);
void audit_s18(AuditCtx& c);
void audit_s19(AuditCtx& c);
void audit_s20(AuditCtx& c);
void audit_s21(AuditCtx& c);
void audit_s21b(AuditCtx& c);
void audit_s22(AuditCtx& c);
void audit_s23(AuditCtx& c);
void audit_s37(AuditCtx& c);

#endif // UNIDICT_CLICK_AUDIT_SECTIONS_H
