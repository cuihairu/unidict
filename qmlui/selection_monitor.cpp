#include "selection_monitor.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QRegularExpression>

SelectionMonitor::SelectionMonitor(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this)) {
    // 默认文本源：xcb 平台读 PRIMARY selection；其余平台恒空串——
    // Windows/macOS 的 text(Selection) 会回落到剪贴板内容，开了会与
    // ClipboardMonitor 双触发，宁可不做也不假装支持
    m_source = []() {
        if (QGuiApplication::platformName() != QStringLiteral("xcb")) {
            return QString();
        }
        const QClipboard* clipboard = QGuiApplication::clipboard();
        return clipboard ? clipboard->text(QClipboard::Selection) : QString(); // GCOVR_EXCL_LINE
    };
    connect(m_timer, &QTimer::timeout, this, &SelectionMonitor::checkSelection);
}

bool SelectionMonitor::isSupported() const {
    return QGuiApplication::platformName() == QStringLiteral("xcb");
}

void SelectionMonitor::start() {
    if (m_isMonitoring) return;
    m_lastText = m_source();
    m_timer->start(m_pollInterval);
    m_isMonitoring = true;
    emit monitoringChanged(true);
}

void SelectionMonitor::stop() {
    if (!m_isMonitoring) return;
    m_timer->stop();
    m_isMonitoring = false;
    emit monitoringChanged(false);
}

void SelectionMonitor::setPollInterval(int milliseconds) {
    m_pollInterval = qBound(100, milliseconds, 5000);
    if (m_isMonitoring) {
        m_timer->setInterval(m_pollInterval);
    }
}

void SelectionMonitor::setMinLength(int length) {
    m_minLength = qBound(1, length, 40);
}

void SelectionMonitor::setMaxLength(int length) {
    m_maxLength = qBound(20, length, 300);
}

void SelectionMonitor::setSource(std::function<QString()> source) {
    m_source = std::move(source);
    m_lastText = m_source();
}

void SelectionMonitor::checkSelection() {
    const QString text = m_source();
    if (text == m_lastText) return;

    m_lastText = text;
    const QString trimmed = text.trimmed();
    if (trimmed.length() < m_minLength || trimmed.length() > m_maxLength) return;
    if (isExcluded(trimmed)) return;
    emit selectionDetected(trimmed);
}

bool SelectionMonitor::isExcluded(const QString& text) const {
    for (const QString& pattern : m_excludePatterns) {
        if (QRegularExpression(pattern).match(text).hasMatch()) return true;
    }
    return false;
}
