#include "ai_service_qt.h"
#include <QProcess>
#include <QByteArray>
#include <QRegularExpression>

namespace UnidictAdaptersQt {

AiServiceQt::AiServiceQt(QObject* parent) : QObject(parent) {
    const QByteArray env = qgetenv("UNIDICT_AI_CMD");
    if (!env.isEmpty()) cmd_ = QString::fromUtf8(env);
}

void AiServiceQt::setCommand(const QString& cmd) { cmd_ = cmd; }
QString AiServiceQt::command() const { return cmd_; }

QString AiServiceQt::runExternal(const QStringList& args, const QString& input) const {
    if (cmd_.isEmpty()) return {};
    QProcess p;
    p.start(cmd_, args);
    if (!p.waitForStarted(3000)) return {};
    if (!input.isEmpty()) {
        p.write(input.toUtf8());
    }
    p.closeWriteChannel();
    p.waitForFinished(15000);
    if (p.exitStatus() != QProcess::NormalExit) return {};
    QByteArray out = p.readAllStandardOutput();
    if (out.isEmpty()) out = p.readAllStandardError();
    return QString::fromUtf8(out);
}

QString AiServiceQt::translate(const QString& text, const QString& targetLang) const {
    // Try external: ai_cmd translate --to <lang>
    QString ext = runExternal(QStringList() << "translate" << "--to" << targetLang, text);
    if (!ext.isEmpty()) return ext.trimmed();
    // Fallback heuristics: very simple wrapper
    QString t = targetLang.toLower();
    if (t.startsWith("zh")) {
        return QString("[Mock Translation to Chinese]\n%1").arg(text);
    } else if (t.startsWith("en")) {
        return QString("[Mock Translation to English]\n%1").arg(text);
    }
    return QString("[Mock Translation to %1]\n%2").arg(targetLang, text);
}

QString AiServiceQt::grammarCheck(const QString& text) const {
    // Try external: ai_cmd grammar
    QString ext = runExternal(QStringList() << "grammar", text);
    if (!ext.isEmpty()) return ext.trimmed();
    // Fallback heuristic: trivial checks
    QString s = text.trimmed();
    if (s.isEmpty()) return "Input is empty.";
    QStringList issues;
    if (!s.endsWith(".") && !s.endsWith("!") && !s.endsWith("?")) {
        issues << "Consider ending the sentence with punctuation.";
    }
    if (s.size() > 0 && s[0].isLower()) {
        issues << "Sentence may start with a capital letter.";
    }
    if (issues.isEmpty()) return "No obvious issues (mock).";
    return "Suggestions:\n- " + issues.join("\n- ");
}

} // namespace UnidictAdaptersQt

