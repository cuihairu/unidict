#include "ai_service_qt.h"
#include <QProcess>
#include <QByteArray>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace UnidictAdaptersQt {

namespace {

// L99 prompts：各操作默认提示词模板（外部命令桥 / 启发式 provider 的
// promptFor 默认出口；真 LLM provider 接入后替换为服务商模板）。
const char* kDefaultPromptTranslate =
    "Translate the following text into the target language. Output only the translation.";
const char* kDefaultPromptGrammar =
    "Check the grammar of the following text. Report issues and suggestions.";
const char* kDefaultPromptSentences =
    "Generate a few natural example sentences using the given word.";

QString defaultPromptFor(const QString& op) {
    if (op == AiOp::translate()) return QString::fromLatin1(kDefaultPromptTranslate);
    if (op == AiOp::grammar()) return QString::fromLatin1(kDefaultPromptGrammar);
    if (op == AiOp::sentences()) return QString::fromLatin1(kDefaultPromptSentences);
    return {};
}

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

    // L99 prompts：活动模板（覆盖优先，无覆盖 → 该操作默认模板）。
    QString promptFor(const QString& op) const override {
        auto it = prompts_.constFind(op);
        if (it != prompts_.constEnd()) return it.value();
        return defaultPromptFor(op);
    }
    // L99 prompts：服务层下发活动模板（空串=清除覆盖，回落默认）。
    void setPromptFor(const QString& op, const QString& templateText) override {
        if (templateText.isEmpty())
            prompts_.remove(op);
        else
            prompts_.insert(op, templateText);
    }

private:
    // 模板（若被服务层启用）在 stdin 首行前导；输入在下一行。外部命令
    // 契约保持向后兼容：无活动模板时原样转发 stdin（P-9 cat 回显不变）。
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
        QByteArray payload;
        const QString op = args.value(0);
        const auto it = prompts_.constFind(op);
        if (it != prompts_.constEnd() && !it.value().isEmpty()) {
            payload += it.value().toUtf8();
            payload += '\n';
        }
        if (!input.isEmpty()) {
            payload += input.toUtf8();
        }
        p.write(payload);
        p.closeWriteChannel();
        p.waitForFinished(15000);
        if (p.exitStatus() != QProcess::NormalExit) return {};
        QByteArray out = p.readAllStandardOutput();
        if (out.isEmpty()) out = p.readAllStandardError();
        return QString::fromUtf8(out).trimmed();
    }

    QString cmd_;
    QHash<QString, QString> prompts_;
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

    // L99 prompts：启发式 provider 的默认模板（无覆盖采纳——纯展示面）。
    QString promptFor(const QString& op) const override { return defaultPromptFor(op); }
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
    clearCache();
}

QString AiServiceQt::command() const { return cmd_; }

void AiServiceQt::rebuildProviders() {
    providers_.clear();
    if (!cmd_.isEmpty()) {
        providers_.push_back(std::make_unique<CommandAiProvider>(cmd_));
    }
    providers_.push_back(std::make_unique<HeuristicAiProvider>());
    // L99 prompts：重建后把活动覆盖（若转发启用）重推给链。
    if (promptForwarding_) {
        for (auto it = promptOverrides_.constBegin(); it != promptOverrides_.constEnd(); ++it) {
            for (const auto& p : providers_) p->setPromptFor(it.key(), it.value());
        }
    }
}

QStringList AiServiceQt::providerNames() const {
    QStringList names;
    for (const auto& p : providers_) names << p->name();
    return names;
}

// L99 prompts：有效模板 = 服务层覆盖优先，否则 provider 默认（取链首）。
QString AiServiceQt::promptFor(const QString& op) const {
    const auto it = promptOverrides_.constFind(op);
    if (it != promptOverrides_.constEnd()) return it.value();
    if (!providers_.empty()) return providers_.front()->promptFor(op);
    return defaultPromptFor(op);
}

void AiServiceQt::setPrompt(const QString& op, const QString& templateText) {
    if (templateText.isEmpty())
        promptOverrides_.remove(op);
    else
        promptOverrides_.insert(op, templateText);
    if (promptForwarding_) {
        for (const auto& p : providers_) p->setPromptFor(op, templateText);
    }
    // 覆盖变更会改变 command provider 的输出（转发开启时），旧结果已失效。
    clearCache();
}

bool AiServiceQt::promptForwardingEnabled() const { return promptForwarding_; }

void AiServiceQt::setPromptForwarding(bool enabled) {
    promptForwarding_ = enabled;
    // 开关切换即按当前状态同步链：开→重推覆盖；关→清链上模板。
    for (const auto& p : providers_) {
        for (auto it = promptOverrides_.constBegin(); it != promptOverrides_.constEnd(); ++it) {
            p->setPromptFor(it.key(), enabled ? it.value() : QString());
        }
    }
    // 转发开关改变链上模板生效状态 → 输出可能变化，旧结果已失效。
    clearCache();
}

bool AiServiceQt::cacheEnabled() const { return cacheEnabled_; }

void AiServiceQt::setCacheEnabled(bool enabled) {
    cacheEnabled_ = enabled;
    if (!enabled) clearCache();
}

int AiServiceQt::cacheSize() const { return cache_.size(); }

void AiServiceQt::clearCache() { cache_.clear(); }

bool AiServiceQt::streamingSupported() const {
    for (const auto& p : providers_) {
        if (p->streamingSupported()) return true;
    }
    return false;
}

QString AiServiceQt::resolve(const QString& op, const QString& input,
                             const QString& lang) const {
    if (input.isEmpty()) return {};
    const QString key = cacheKey(op, input, lang);
    if (cacheEnabled_) {
        const auto it = cache_.constFind(key);
        if (it != cache_.constEnd()) return it.value();
    }
    QString out;
    for (const auto& p : providers_) {
        if (op == AiOp::translate())
            out = p->translate(input, lang);
        else if (op == AiOp::grammar())
            out = p->grammarCheck(input);
        else if (op == AiOp::sentences())
            out = p->generateSentences(input);
        if (!out.isEmpty()) break;
    }
    if (cacheEnabled_) cache_.insert(key, out);
    return out;
}

QString AiServiceQt::cacheKey(const QString& op, const QString& input, const QString& lang) {
    return op + QLatin1Char('\x1f') + lang + QLatin1Char('\x1f') + input;
}

QString AiServiceQt::translate(const QString& text, const QString& targetLang) const {
    return resolve(AiOp::translate(), text, targetLang);
}

QString AiServiceQt::grammarCheck(const QString& text) const {
    return resolve(AiOp::grammar(), text);
}

QString AiServiceQt::generateSentences(const QString& word) const {
    return resolve(AiOp::sentences(), word);
}

} // namespace UnidictAdaptersQt
