// Clipboard monitor for automatic word lookup (Qt-only).
// Monitors system clipboard for text changes and triggers lookups.

#ifndef CLIPBOARD_MONITOR_H
#define CLIPBOARD_MONITOR_H

#include <QObject>
#include <QTimer>
#include <QString>

class ClipboardMonitor : public QObject {
    Q_OBJECT

public:
    explicit ClipboardMonitor(QObject* parent = nullptr);
    ~ClipboardMonitor() = default;

    // Control monitoring
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool isMonitoring() const { return m_isMonitoring; }

    // Configuration
    Q_INVOKABLE void setPollInterval(int milliseconds);
    Q_INVOKABLE int getPollInterval() const { return m_pollInterval; }

    Q_INVOKABLE void setMinWordLength(int length);
    Q_INVOKABLE int getMinWordLength() const { return m_minWordLength; }

    Q_INVOKABLE void setMaxWordLength(int length);
    Q_INVOKABLE int getMaxWordLength() const { return m_maxWordLength; }

    // Exclude patterns (e.g., URLs, file paths)
    Q_INVOKABLE void addExcludePattern(const QString& pattern);
    Q_INVOKABLE void clearExcludePatterns();

signals:
    // Emitted when a word is detected in clipboard
    void wordDetected(const QString& word);

    // Emitted when clipboard text changes but doesn't match word criteria
    void textChanged(const QString& text);

    // Monitoring state changed
    void monitoringChanged(bool active);

private slots:
    void checkClipboard();

private:
    bool isValidWord(const QString& text) const;
    QString extractWord(const QString& text) const;
    bool isExcluded(const QString& text) const;

    QTimer* m_timer;
    QString m_lastClipboardText;

    bool m_isMonitoring = false;
    int m_pollInterval = 500;      // Check every 500ms
    int m_minWordLength = 2;       // Minimum 2 characters
    int m_maxWordLength = 50;      // Maximum 50 characters

    QStringList m_excludePatterns = {
        "^https?://",           // URLs
        "^file://",             // File URLs
        "^/.*",                 // Unix paths
        "^[A-Za-z]:\\\\.*",     // Windows paths
        "^\\d+$",               // Pure numbers
        "^[^\\w\\s]+$"          // Special characters only
    };
};

#endif // CLIPBOARD_MONITOR_H
