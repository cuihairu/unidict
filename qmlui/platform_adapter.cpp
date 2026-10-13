#include "platform_adapter.h"

#include "clipboard_monitor.h"
#include "global_hotkeys.h"
#include "selection_monitor.h"
#include "win_selection_reader.h"

#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

PlatformAdapter::~PlatformAdapter() = default;

bool PlatformAdapter::openContainingFolder(const QString& path) const {
    // 文件 → 所在目录；目录 → 本体。QFileInfo 对 '/' 与 '\\' 混排路径
    // 都归一（Windows 反斜杠分隔符 QML 侧 substring 不可靠，故收在 C++）
    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists()) return false;
    const QString dir = info.isDir() ? info.absoluteFilePath()
                                     : info.absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

PlatformAdapter::PlatformAdapter(QObject* parent)
    : QObject(parent)
    , m_clipboardMonitor(std::make_unique<ClipboardMonitor>(this))
    , m_selectionMonitor(std::make_unique<SelectionMonitor>(this))
    , m_globalHotkeys(std::make_unique<GlobalHotkeys>(this)) {

    // Connect clipboard monitor word detection
    m_clipboardWordConnection = connect(m_clipboardMonitor.get(), &ClipboardMonitor::wordDetected,
        [this](const QString& word) {
            if (m_clipboardAutoLookupEnabled) {
                emit clipboardWordDetected(word);
            }
        }
    );

    // 划词取词（X11）：命中即转发，开关分流在 QML 侧（与剪贴板同款）
    connect(m_selectionMonitor.get(), &SelectionMonitor::selectionDetected,
        this, &PlatformAdapter::selectionWordDetected);

    // Connect global hotkey handler
    // P-5 收口三个分支中的两个：show_window/quick_lookup 转成信号交 QML
    // 主窗处理（窗口前置、读剪贴板取词都在 UI 侧）；lookup_selection
    // （选中文本取词）需要跨进程读焦点窗口选区（X11/Win32 专属抓取
    // SendInput/GetGUIThreadInfo），P-5 裁剪为剪贴板取词代为承载——
    // 该分支保留为平台化工作的落点。Q-4 测试驱动四条分支全走一遍。
    m_hotkeyPressedConnection = connect(m_globalHotkeys.get(), &GlobalHotkeys::hotkeyPressed,
        [this](const QString& action) {
            if (action == "lookup_selection") {
                // 已决不做（非待办）：X11 划词走 SelectionMonitor 直达；热键
                // 触发的跨进程选区抓取属 P-5 裁剪面，剪贴板取词承载同一意图
            } else if (action == "show_window") {
                emit showMainWindowRequested();
            } else if (action == "quick_lookup") {
                // 取词窗入口：QML 侧收到信号后自行读 clip.text() 掐首尾
                // 标点再查（同进程剪贴板访问，跨平台无特权问题）
                emit quickLookupRequested();
            }
        }
    );
}

// ============================================================================
// F13a 焦点窗选区读取（quick_lookup 热键文本源）
// ============================================================================

QString PlatformAdapter::readFocusedSelection() const {
    // 委托平台 reader（非 Windows 编译为恒空实现，调用方零分支）
    return WinSelectionReader::readFocusedSelection();
}

// ============================================================================
// P1 剪贴板监听功能实现
// ============================================================================

void PlatformAdapter::startClipboardMonitoring() {
    m_clipboardMonitor->start();
}

void PlatformAdapter::stopClipboardMonitoring() {
    m_clipboardMonitor->stop();
}

bool PlatformAdapter::isClipboardMonitoring() const {
    return m_clipboardMonitor->isMonitoring();
}

void PlatformAdapter::setClipboardPollInterval(int milliseconds) {
    m_clipboardMonitor->setPollInterval(milliseconds);
}

void PlatformAdapter::setClipboardMinWordLength(int length) {
    m_clipboardMonitor->setMinWordLength(length);
}

void PlatformAdapter::setClipboardMaxWordLength(int length) {
    m_clipboardMonitor->setMaxWordLength(length);
}

void PlatformAdapter::addClipboardExcludePattern(const QString& pattern) {
    m_clipboardMonitor->addExcludePattern(pattern);
}

void PlatformAdapter::clearClipboardExcludePatterns() {
    m_clipboardMonitor->clearExcludePatterns();
}

bool PlatformAdapter::isSelectionMonitoringSupported() const {
    return m_selectionMonitor->isSupported();
}

void PlatformAdapter::startSelectionMonitoring() {
    // 非 xcb 平台 start 也不允许跑：文本源恒空虽不会触发，但状态行
    // 会谎报「监控中」——设置面开关仅在 supported 时可见，双保险
    if (!m_selectionMonitor->isSupported()) return;
    m_selectionMonitor->start();
}

void PlatformAdapter::stopSelectionMonitoring() {
    m_selectionMonitor->stop();
}

bool PlatformAdapter::isSelectionMonitoring() const {
    return m_selectionMonitor->isMonitoring();
}

void PlatformAdapter::setSelectionPollInterval(int milliseconds) {
    m_selectionMonitor->setPollInterval(milliseconds);
}

void PlatformAdapter::setSelectionMinLength(int length) {
    m_selectionMonitor->setMinLength(length);
}

void PlatformAdapter::setSelectionMaxLength(int length) {
    m_selectionMonitor->setMaxLength(length);
}

void PlatformAdapter::setClipboardAutoLookupEnabled(bool enabled) {
    m_clipboardAutoLookupEnabled = enabled;
}

bool PlatformAdapter::isClipboardAutoLookupEnabled() const {
    return m_clipboardAutoLookupEnabled;
}

QPoint PlatformAdapter::cursorScreenPos() const {
    return QCursor::pos();
}

// ============================================================================
// P1 全局热键功能实现
// ============================================================================

bool PlatformAdapter::registerGlobalHotkey(const QString& action, const QString& keySequence) {
    return m_globalHotkeys->registerHotkey(action, keySequence);
}

void PlatformAdapter::unregisterGlobalHotkey(const QString& action) {
    m_globalHotkeys->unregisterHotkey(action);
}

void PlatformAdapter::unregisterAllGlobalHotkeys() {
    m_globalHotkeys->unregisterAllHotkeys();
}

QStringList PlatformAdapter::registeredHotkeyActions() const {
    return m_globalHotkeys->registeredActions();
}

QString PlatformAdapter::getHotkeyForAction(const QString& action) const {
    return m_globalHotkeys->getHotkeyForAction(action);
}

void PlatformAdapter::setGlobalHotkeysEnabled(bool enabled) {
    m_globalHotkeys->setEnabled(enabled);
}

bool PlatformAdapter::isGlobalHotkeysEnabled() const {
    return m_globalHotkeys->isEnabled();
}

bool PlatformAdapter::isGlobalHotkeysSupported() {
    return GlobalHotkeys::isPlatformSupported();
}
