// Selection monitor for select-to-lookup (Qt-only, X11 face).
// 轮询 PRIMARY selection（鼠标划选的文本）触发查词——roadmap Quick Access
// 里 lookup_selection 的 X11 落地面（pro_dictionary_gap P1 收尾之一）。
// Qt 仅在 xcb 平台提供独立 selection；Windows/macOS 上 text(Selection)
// 等同 text(Clipboard)（会与剪贴板监控双触发），因此默认文本源钉
// xcb，其余平台恒空串（监控可跑但永不触发，诚实不假装工作）。
// 文本源可注入（setSource），离屏单测经假源直驱私有槽（Q-8 同款）。

#ifndef SELECTION_MONITOR_H
#define SELECTION_MONITOR_H

#include <QObject>
#include <QTimer>
#include <QString>

#include <functional>

class SelectionMonitor : public QObject {
    Q_OBJECT

public:
    explicit SelectionMonitor(QObject* parent = nullptr);
    ~SelectionMonitor() = default;

    // 当前平台是否具备独立 selection（xcb；设置面据此显隐开关）
    Q_INVOKABLE bool isSupported() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool isMonitoring() const { return m_isMonitoring; }

    Q_INVOKABLE void setPollInterval(int milliseconds);
    Q_INVOKABLE int getPollInterval() const { return m_pollInterval; }

    Q_INVOKABLE void setMinLength(int length);
    Q_INVOKABLE int getMinLength() const { return m_minLength; }

    Q_INVOKABLE void setMaxLength(int length);
    Q_INVOKABLE int getMaxLength() const { return m_maxLength; }

    // 测试缝：替换默认（xcb 钉死的）文本源
    void setSource(std::function<QString()> source);

signals:
    // 划选文本变化且通过长度/排除校验
    void selectionDetected(const QString& text);
    void monitoringChanged(bool active);

private slots:
    void checkSelection();

private:
    bool isExcluded(const QString& text) const;

    QTimer* m_timer;
    std::function<QString()> m_source;
    QString m_lastText;

    bool m_isMonitoring = false;
    int m_pollInterval = 400;   // 划词比剪贴板更贴手：400ms
    int m_minLength = 2;        // 单字符划选多为误划
    int m_maxLength = 80;       // 整段划选不是查词意图

    QStringList m_excludePatterns = {
        "^https?://",           // URLs
        "^file://"              // File URLs
    };
};

#endif // SELECTION_MONITOR_H
