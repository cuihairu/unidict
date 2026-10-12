// Carbon 热键桥（F10）：global_hotkeys.cpp 的 Q_OS_MACOS 面经本文件
// 落到 RegisterEventHotKey。Carbon 头只出现在这里；C++ 侧经
// global_hotkeys.h 的三个 unidict_mac_* 声明与本桥相遇。
// 真按键链路离屏不可测——CI 只断言注册成功/失败路径（接线级）。

#include "global_hotkeys.h"

#ifndef Q_OS_MACOS
#error "this bridge is macOS-only"
#else

#import <Carbon/Carbon.h>

namespace {

// 注册时装配一次：GlobalHotkeys 静态 trampoline → 按 handle 派发
void (*g_callback)(int hotkeyId, void* ctx) = nullptr;
void* g_callbackCtx = nullptr;

// EventModifiers 位（Events.h）：与 Qt::Modifier 一一对应
EventModifiers qtModsToMac(int qtMods) {
    EventModifiers m = 0;
    if (qtMods & Qt::ControlModifier) m |= controlKey;
    if (qtMods & Qt::ShiftModifier) m |= shiftKey;
    if (qtMods & Qt::AltModifier) m |= optionKey;
    if (qtMods & Qt::MetaModifier) m |= cmdKey;
    return m;
}

// Qt 主键 → Mac 虚拟键码（Events.h kVK_*；与 Windows 同一安全集，
// 但 Mac 键盘只到 F20 且 kVK_ANSI 字母位按 QWERTY 排布必须显式列）
bool qtKeyToMacCode(int qtKey, UInt32* code) {
    static const UInt32 fCodes[20] = {
        kVK_F1, kVK_F2, kVK_F3, kVK_F4, kVK_F5, kVK_F6, kVK_F7, kVK_F8,
        kVK_F9, kVK_F10, kVK_F11, kVK_F12, kVK_F13, kVK_F14, kVK_F15,
        kVK_F16, kVK_F17, kVK_F18, kVK_F19, kVK_F20};
    switch (qtKey) {
    case Qt::Key_A: *code = kVK_ANSI_A; return true;
    case Qt::Key_B: *code = kVK_ANSI_B; return true;
    case Qt::Key_C: *code = kVK_ANSI_C; return true;
    case Qt::Key_D: *code = kVK_ANSI_D; return true;
    case Qt::Key_E: *code = kVK_ANSI_E; return true;
    case Qt::Key_F: *code = kVK_ANSI_F; return true;
    case Qt::Key_G: *code = kVK_ANSI_G; return true;
    case Qt::Key_H: *code = kVK_ANSI_H; return true;
    case Qt::Key_I: *code = kVK_ANSI_I; return true;
    case Qt::Key_J: *code = kVK_ANSI_J; return true;
    case Qt::Key_K: *code = kVK_ANSI_K; return true;
    case Qt::Key_L: *code = kVK_ANSI_L; return true;
    case Qt::Key_M: *code = kVK_ANSI_M; return true;
    case Qt::Key_N: *code = kVK_ANSI_N; return true;
    case Qt::Key_O: *code = kVK_ANSI_O; return true;
    case Qt::Key_P: *code = kVK_ANSI_P; return true;
    case Qt::Key_Q: *code = kVK_ANSI_Q; return true;
    case Qt::Key_R: *code = kVK_ANSI_R; return true;
    case Qt::Key_S: *code = kVK_ANSI_S; return true;
    case Qt::Key_T: *code = kVK_ANSI_T; return true;
    case Qt::Key_U: *code = kVK_ANSI_U; return true;
    case Qt::Key_V: *code = kVK_ANSI_V; return true;
    case Qt::Key_W: *code = kVK_ANSI_W; return true;
    case Qt::Key_X: *code = kVK_ANSI_X; return true;
    case Qt::Key_Y: *code = kVK_ANSI_Y; return true;
    case Qt::Key_Z: *code = kVK_ANSI_Z; return true;
    default: break;
    }
    static const UInt32 digitCodes[10] = {
        kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
        kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9};
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        *code = digitCodes[qtKey - Qt::Key_0];
        return true;
    }
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F20) {
        *code = fCodes[qtKey - Qt::Key_F1];
        return true;
    }
    switch (qtKey) {
    case Qt::Key_Space: *code = kVK_Space; return true;
    case Qt::Key_Backspace: *code = kVK_Delete; return true;
    case Qt::Key_Tab: *code = kVK_Tab; return true;
    case Qt::Key_Return: *code = kVK_Return; return true;
    case Qt::Key_Escape: *code = kVK_Escape; return true;
    case Qt::Key_Delete: *code = kVK_ForwardDelete; return true;
    case Qt::Key_Home: *code = kVK_Home; return true;
    case Qt::Key_End: *code = kVK_End; return true;
    case Qt::Key_PageUp: *code = kVK_PageUp; return true;
    case Qt::Key_PageDown: *code = kVK_PageDown; return true;
    case Qt::Key_Left: *code = kVK_LeftArrow; return true;
    case Qt::Key_Right: *code = kVK_RightArrow; return true;
    case Qt::Key_Up: *code = kVK_UpArrow; return true;
    case Qt::Key_Down: *code = kVK_DownArrow; return true;
    default: return false;
    }
}

OSStatus hotkeyEventHandler(EventHandlerCallRef, EventRef event, void*) {
    if (g_callback && ::GetEventClass(event) == kEventClassKeyboard) {
        EventHotKeyID id;
        if (::GetEventParameter(event, kEventParamDirectObject,
                                typeEventHotKeyID, nullptr, sizeof(id),
                                nullptr, &id) == noErr) {
            g_callback(int(id.id), g_callbackCtx);
        }
    }
    return noErr;
}

} // namespace

bool unidict_mac_register_hotkey(int qtKey, int qtMods, int hotkeyId,
                                 quintptr* refOut) {
    UInt32 code = 0;
    if (!qtKeyToMacCode(qtKey, &code)) return false;
    EventHotKeyID id;
    id.signature = 'UNID';
    id.id = UInt32(hotkeyId);
    EventHotKeyRef ref = nullptr;
    const OSStatus st = ::RegisterEventHotKey(
        code, qtModsToMac(qtMods), id,
        ::GetApplicationEventTarget(), 0, &ref);
    if (st != noErr || !ref) return false;
    *refOut = reinterpret_cast<quintptr>(ref);
    return true;
}

void unidict_mac_unregister_hotkey(quintptr ref) {
    if (ref) ::UnregisterEventHotKey(reinterpret_cast<EventHotKeyRef>(ref));
}

void unidict_mac_install_callback(void (*cb)(int hotkeyId, void* ctx),
                                  void* ctx) {
    if (g_callback) {  // 已装配：仅换指针（id/ctx 同源）
        g_callback = cb;
        g_callbackCtx = ctx;
        return;
    }
    g_callback = cb;
    g_callbackCtx = ctx;
    const EventTypeSpec spec = {kEventClassKeyboard, kEventHotKeyPressed};
    ::InstallEventHandler(::GetApplicationEventTarget(),
                          hotkeyEventHandler, 1, &spec, nullptr, nullptr);
}

#endif // Q_OS_MACOS
