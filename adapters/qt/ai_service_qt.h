#ifndef UNIDICT_AI_SERVICE_QT_H
#define UNIDICT_AI_SERVICE_QT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

namespace UnidictAdaptersQt {

// P-9 AI Provider 抽象：能力（translate/grammarCheck）与来源解耦——
// provider 可替换（外部命令桥 / 离线启发式兜底），core 不带 AI，
// 任一 provider 失败对上层不可见（返回空串即让位下一家）。
class AiProvider {
public:
    virtual ~AiProvider() = default;
    // 稳定小写标识（providerNames 断言/设置面可见性用）
    virtual QString name() const = 0;
    virtual QString translate(const QString& text, const QString& targetLang) const = 0;
    virtual QString grammarCheck(const QString& text) const = 0;
};

// Lightweight AI adapter that can call an external command if configured via env UNIDICT_AI_CMD
// or falls back to simple heuristics. This avoids bundling network logic in core.
class AiServiceQt : public QObject {
    Q_OBJECT
public:
    explicit AiServiceQt(QObject* parent = nullptr);

    Q_INVOKABLE void setCommand(const QString& cmd);
    Q_INVOKABLE QString command() const;

    // P-9：当前 provider 链名（配置命令 → ["command","heuristic"]，
    // 未配置 → ["heuristic"]）——provider 可替换的可观察面
    Q_INVOKABLE QStringList providerNames() const;

    // Translate text to targetLang (e.g., "zh", "en"). Uses external cmd when available.
    Q_INVOKABLE QString translate(const QString& text, const QString& targetLang) const;
    // Simple grammar check; returns suggestions or "OK" when no obvious issues (heuristic when no external cmd).
    Q_INVOKABLE QString grammarCheck(const QString& text) const;

private:
    // 命令串变化即重建链：有命令 = command + heuristic，无 = 仅 heuristic
    void rebuildProviders();

    QString cmd_;
    std::vector<std::unique_ptr<AiProvider>> providers_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_AI_SERVICE_QT_H
