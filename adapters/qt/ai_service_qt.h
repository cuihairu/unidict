#ifndef UNIDICT_AI_SERVICE_QT_H
#define UNIDICT_AI_SERVICE_QT_H

#include <QObject>
#include <QString>

namespace UnidictAdaptersQt {

// Lightweight AI adapter that can call an external command if configured via env UNIDICT_AI_CMD
// or falls back to simple heuristics. This avoids bundling network logic in core.
class AiServiceQt : public QObject {
    Q_OBJECT
public:
    explicit AiServiceQt(QObject* parent = nullptr);

    Q_INVOKABLE void setCommand(const QString& cmd);
    Q_INVOKABLE QString command() const;

    // Translate text to targetLang (e.g., "zh", "en"). Uses external cmd when available.
    Q_INVOKABLE QString translate(const QString& text, const QString& targetLang) const;
    // Simple grammar check; returns suggestions or "OK" when no obvious issues (heuristic when no external cmd).
    Q_INVOKABLE QString grammarCheck(const QString& text) const;

private:
    QString cmd_;
    QString runExternal(const QStringList& args, const QString& input) const; // returns empty on failure
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_AI_SERVICE_QT_H

