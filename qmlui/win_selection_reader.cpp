#include "win_selection_reader.h"

#ifdef Q_OS_WIN
// MSVC 直接以 pragma 挂 COM 库（Windows 构建面已锁死 MSVC 工具链，见
// ci.yml 锁 MSVC 的注释；免给 5 个消费 target 逐一补 target_link_libraries）
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")

#include <uiautomationclient.h>

QString WinSelectionReader::readFocusedSelection() {
    QString selection;

    // 热键路径低频调用：线程无 COM 则本次初始化 STA；线程已是 MTA 返回
    // RPC_E_CHANGED_MODE，此时 COM 已就绪但不能替别人 CoUninitialize
    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool mustUninit = SUCCEEDED(initHr);

    IUIAutomation* uia = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_IUIAutomation,
                                   reinterpret_cast<void**>(&uia))) && uia) {
        IUIAutomationElement* focused = nullptr;
        if (SUCCEEDED(uia->GetFocusedElement(&focused)) && focused) {
            IUIAutomationTextPattern* text = nullptr;
            if (SUCCEEDED(focused->GetCurrentPatternAs(
                    UIA_TextPatternId, IID_IUIAutomationTextPattern,
                    reinterpret_cast<void**>(&text))) && text) {
                IUIAutomationTextRangeArray* ranges = nullptr;
                if (SUCCEEDED(text->GetSelection(&ranges)) && ranges) {
                    // 属性式 getter（get_Length），不是 Length() 方法
                    int count = 0;
                    if (FAILED(ranges->get_Length(&count))) {
                        count = 0;
                    }
                    for (int i = 0; i < count; ++i) {
                        IUIAutomationTextRange* range = nullptr;
                        if (FAILED(ranges->GetElement(i, &range)) || !range) {
                            continue;
                        }
                        BSTR raw = nullptr;
                        if (SUCCEEDED(range->GetText(-1, &raw)) && raw) {
                            selection += QString::fromWCharArray(raw);
                            SysFreeString(raw);
                        }
                        range->Release();
                    }
                    ranges->Release();
                }
                text->Release();
            }
            focused->Release();
        }
        uia->Release();
    }

    if (mustUninit) {
        CoUninitialize();
    }
    return selection;
}

#else  // !Q_OS_WIN

// 非 Windows：恒空串（QML 侧据此回落剪贴板路径）
QString WinSelectionReader::readFocusedSelection() {
    return {};
}

#endif // Q_OS_WIN
