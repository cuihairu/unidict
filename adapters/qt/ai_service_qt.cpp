#include "ai_service_qt.h"
#include <QProcess>
#include <QByteArray>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace UnidictAdaptersQt {

namespace {

// P-9 外部命令桥 provider：UNIDICT_AI_CMD/setCommand 配置的命令行桥。
// 任何失败（未配置/起不来/被信号杀/超时/无输出）都返回空串——
// 失败对上层不可见，由链上下一家接手
class CommandAiProvider : public AiProvider {
public:
    explicit CommandAiProvider(QString cmd) : cmd_(std::move(cmd)) {}
    QString name() const override { return QStringLiteral("command"); }

    QString translate(const QString& text, const QString& targetLang) const override {
        return runExternal(QStringList() << "translate" << "--to" << targetLang, text);
    }
    QString grammarCheck(const QString& text) const override {
        return runExternal(QStringList() << "grammar", text);
    }
    QString generateSentences(const QString& word) const override {
        return runExternal(QStringList() << "sentences", word);
    }

private:
    QString runExternal(const QStringList& args, const QString& input) const {
        if (cmd_.isEmpty()) return {};
        QProcess p;
#ifdef Q_OS_WIN
        // GUI 子系统下拉起控制台程序会闪 terminal 窗口（BUGS.md BUG-001）：
        // CREATE_NO_WINDOW 静默拉起，AI 桥输出仍经管道收
        p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#endif
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
        return QString::fromUtf8(out).trimmed();
    }

    QString cmd_;
};

// 离线启发式兜底 provider：无外部能力时的最后一环（永不为空 → 失败无感）
class HeuristicAiProvider : public AiProvider {
public:
    QString name() const override { return QStringLiteral("heuristic"); }

    QString translate(const QString& text, const QString& targetLang) const override {
        QString t = targetLang.toLower();
        if (t.startsWith("zh")) {
            return QString("[Mock Translation to Chinese]\n%1").arg(text);
        } else if (t.startsWith("en")) {
            return QString("[Mock Translation to English]\n%1").arg(text);
        }
        return QString("[Mock Translation to %1]\n%2").arg(targetLang, text);
    }

    QString grammarCheck(const QString& text) const override {
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

    // 离线兜底：固定模板注入词头，输出确定（ui_click_audit/单测可逐字断言）。
    // 文案自标 [Mock sentences]——未配 UNIDICT_AI_CMD 时诚实告知来源。
    QString generateSentences(const QString& word) const override {
        const QString w = word.trimmed();
        if (w.isEmpty()) return {};
        return QStringLiteral("[Mock sentences]\n"
                              "1. She said \"%1\" when she opened the door.\n"
                              "2. He greeted everyone with a friendly \"%1\".\n"
                              "3. Try using \"%1\" in your next conversation.")
            .arg(w);
    }
};

} // namespace

AiServiceQt::AiServiceQt(QObject* parent) : QObject(parent) {
    const QByteArray env = qgetenv("UNIDICT_AI_CMD");
    if (!env.isEmpty()) cmd_ = QString::fromUtf8(env);
    rebuildProviders();
}

void AiServiceQt::setCommand(const QString& cmd) {
    cmd_ = cmd;
    rebuildProviders();
}

QString AiServiceQt::command() const { return cmd_; }

void AiServiceQt::rebuildProviders() {
    providers_.clear();
    if (!cmd_.isEmpty()) {
        providers_.push_back(std::make_unique<CommandAiProvider>(cmd_));
    }
    providers_.push_back(std::make_unique<HeuristicAiProvider>());
}

QStringList AiServiceQt::providerNames() const {
    QStringList names;
    for (const auto& p : providers_) names << p->name();
    return names;
}

QString AiServiceQt::translate(const QString& text, const QString& targetLang) const {
    for (const auto& p : providers_) {
        const QString out = p->translate(text, targetLang);
        if (!out.isEmpty()) return out;
    }
    return {};
}

QString AiServiceQt::grammarCheck(const QString& text) const {
    for (const auto& p : providers_) {
        const QString out = p->grammarCheck(text);
        if (!out.isEmpty()) return out;
    }
    return {};
}

QString AiServiceQt::generateSentences(const QString& word) const {
    for (const auto& p : providers_) {
        const QString out = p->generateSentences(word);
        if (!out.isEmpty()) return out;
    }
    return {};
}

} // namespace UnidictAdaptersQt
