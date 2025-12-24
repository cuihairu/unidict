#include "clipboard_monitor.h"

#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>

ClipboardMonitor::ClipboardMonitor(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this)) {

    connect(m_timer, &QTimer::timeout, this, &ClipboardMonitor::checkClipboard);
}

void ClipboardMonitor::start() {
    if (m_isMonitoring) return;

    // Get initial clipboard content
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        m_lastClipboardText = clipboard->text();
    }

    m_timer->start(m_pollInterval);
    m_isMonitoring = true;
    emit monitoringChanged(true);
}

void ClipboardMonitor::stop() {
    if (!m_isMonitoring) return;

    m_timer->stop();
    m_isMonitoring = false;
    emit monitoringChanged(false);
}

void ClipboardMonitor::setPollInterval(int milliseconds) {
    m_pollInterval = qBound(100, milliseconds, 5000);  // 100ms - 5s range
    if (m_isMonitoring) {
        m_timer->setInterval(m_pollInterval);
    }
}

void ClipboardMonitor::setMinWordLength(int length) {
    m_minWordLength = qBound(1, length, 10);
}

void ClipboardMonitor::setMaxWordLength(int length) {
    m_maxWordLength = qBound(10, length, 200);
}

void ClipboardMonitor::addExcludePattern(const QString& pattern) {
    m_excludePatterns.append(pattern);
}

void ClipboardMonitor::clearExcludePatterns() {
    m_excludePatterns.clear();
}

void ClipboardMonitor::checkClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) return;

    QString currentText = clipboard->text();

    // Check if clipboard changed
    if (currentText == m_lastClipboardText) return;

    m_lastClipboardText = currentText;
    emit textChanged(currentText);

    // Extract and validate word
    QString word = extractWord(currentText);
    if (!word.isEmpty() && isValidWord(word) && !isExcluded(word)) {
        emit wordDetected(word);
    }
}

bool ClipboardMonitor::isValidWord(const QString& text) const {
    // Check length constraints
    if (text.length() < m_minWordLength || text.length() > m_maxWordLength) {
        return false;
    }

    // Should contain at least one letter
    QRegularExpression letterRegex("[a-zA-Z\\u4e00-\\u9fff]");
    if (!text.contains(letterRegex)) {
        return false;
    }

    // Not too many special characters
    int specialCount = 0;
    for (const QChar& c : text) {
        if (!c.isLetterOrNumber() && !c.isSpace() && c != '-' && c != '\'') {
            specialCount++;
        }
    }

    // Allow up to 30% special characters
    return (specialCount * 3 <= text.length());
}

QString ClipboardMonitor::extractWord(const QString& text) const {
    // Trim whitespace
    QString result = text.trimmed();

    // Extract first word if contains space/newline
    int spaceIndex = result.indexOf(QRegularExpression("[\\s\\n]"));
    if (spaceIndex > 0) {
        result = result.left(spaceIndex);
    }

    // Remove common punctuation
    result.remove(QRegularExpression("^[.,!?;:()\"]+"));
    result.remove(QRegularExpression("[.,!?;:()\"]+$"));

    return result;
}

bool ClipboardMonitor::isExcluded(const QString& text) const {
    for (const QString& pattern : m_excludePatterns) {
        QRegularExpression regex(pattern);
        if (regex.match(text).hasMatch()) {
            return true;
        }
    }
    return false;
}
