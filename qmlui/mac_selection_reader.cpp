#include "mac_selection_reader.h"

#ifdef Q_OS_MACOS

#include <ApplicationServices/ApplicationServices.h>

QString MacSelectionReader::readFocusedSelection() {
    QString selection;

    // 热键路径低频调用：系统级元素取当前焦点元素，再取其 AXSelectedText。
    // 任一步失败（无焦点元素 / 元素不支持该属性 / 未授权）→ 空串，
    // QML 侧据空回落剪贴板（诚实不假装）
    AXUIElementRef systemWide = AXUIElementCreateSystemWide();
    if (!systemWide) {
        return selection;
    }
    CFTypeRef focused = nullptr;
    if (AXUIElementCopyAttributeValue(systemWide, kAXFocusedUIElementAttribute,
                                      &focused) == kAXErrorSuccess
            && focused) {
        CFTypeRef sel = nullptr;
        if (AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(focused),
                                          kAXSelectedTextAttribute,
                                          &sel) == kAXErrorSuccess
                && sel) {
            if (CFGetTypeID(sel) == CFStringGetTypeID()) {
                selection = QString::fromCFString(static_cast<CFStringRef>(sel));
            }
            CFRelease(sel);
        }
        CFRelease(focused);
    }
    CFRelease(systemWide);
    return selection;
}

bool MacSelectionReader::isAccessibilityTrusted() {
    return AXIsProcessTrusted();
}

void MacSelectionReader::requestAccessibilityPrompt() {
    // prompt:true 时若仍未授权，macOS 自己弹一次「想控制这台电脑」；
    // 已授权则为空操作。字典建在栈上，用完即释放
    const void* keys[] = { kAXTrustedCheckOptionPrompt };
    const void* values[] = { kCFBooleanTrue };
    CFDictionaryRef opts = CFDictionaryCreate(
        nullptr, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (opts) {
        AXIsProcessTrustedWithOptions(opts);
        CFRelease(opts);
    }
}

#else  // !Q_OS_MACOS

// 非 macOS：恒空串（QML 侧据此回落剪贴板路径）、恒未授权、提示 no-op
QString MacSelectionReader::readFocusedSelection() {
    return {};
}

bool MacSelectionReader::isAccessibilityTrusted() {
    return false;
}

void MacSelectionReader::requestAccessibilityPrompt() {}

#endif // Q_OS_MACOS

QString MacSelectionReader::accessibilitySettingsUrl() {
    // 平台无关常量：非 macOS 也返回同值（QML 仅在 macOS 分支使用）
    return QStringLiteral(
        "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility");
}
