#ifndef UNIDICT_AI_SERVICE_QT_H
#define UNIDICT_AI_SERVICE_QT_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

namespace UnidictAdaptersQt {

// L99: 稳定 AI 操作标识（prompt 模板键 / 缓存键前缀共用）。
namespace AiOp {
inline QString translate() { return QStringLiteral("translate"); }
inline QString grammar() { return QStringLiteral("grammar"); }
inline QString sentences() { return QStringLiteral("sentences"); }
}

// P-9 AI Provider 抽象：能力（translate/grammarCheck/generateSentences）与
// 来源解耦——provider 可替换（外部命令桥 / 离线启发式兜底），core 不带 AI，
// 任一 provider 失败对上层不可见（返回空串即让位下一家）。
// L99 扩展：prompt 模板面 + streaming 能力面（auth-gated 真 LLM 预留）。
class AiProvider {
public:
    virtual ~AiProvider() = default;
    // 稳定小写标识（providerNames 断言/设置面可见性用）
    virtual QString name() const = 0;
    virtual QString translate(const QString& text, const QString& targetLang) const = 0;
    virtual QString grammarCheck(const QString& text) const = 0;
    // 语境造句：给词生成若干上下文例句（roadmap AI sentence generation）
    virtual QString generateSentences(const QString& word) const = 0;

    // L99 prompts：该 provider 对某操作的默认提示词模板。服务层可
    // setPromptFor 覆盖（不改变子方法签名——覆盖经本虚函数下发）。
    virtual QString promptFor(const QString& op) const = 0;
    // L99 prompts：服务层下发的活动提示词模板（provider 在子调用时
    // 采纳；无覆盖时等于默认）。基类默认 no-op——不采纳覆盖的 provider
    // （如离线启发式）不受影响。
    virtual void setPromptFor(const QString& op, const QString& templateText) {
        Q_UNUSED(op);
        Q_UNUSED(templateText);
    }
    // L99 streaming：仅能增量输出的 provider 报 true（当前 command /
    // heuristic 均单发整段输出 → false；真 LLM/SSE provider 属 auth-gated）。
    virtual bool streamingSupported() const { return false; }
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
    // Contextual example sentences for a word (heuristic mock when no external cmd).
    Q_INVOKABLE QString generateSentences(const QString& word) const;

    // L99 prompts：某操作的有效提示词模板（覆盖优先，无覆盖 → provider 默认）。
    Q_INVOKABLE QString promptFor(const QString& op) const;
    // L99 prompts：设置/清除（空串清除）某操作的提示词覆盖，并随链下发。
    Q_INVOKABLE void setPrompt(const QString& op, const QString& templateText);
    // L99 prompts：是否把提示词模板转发给外部命令桥（stdin 前缀）。
    // 默认关——保持 P-9 cat 回显契约；开时 command provider 采纳。
    Q_INVOKABLE bool promptForwardingEnabled() const;
    Q_INVOKABLE void setPromptForwarding(bool enabled);

    // L99 caching：结果 memo（op+输入+目标语言 键）。provider 链失败
    // （空串）也缓存，避免重复拉起外部进程；命令/覆盖变更即清缓存。
    Q_INVOKABLE bool cacheEnabled() const;
    Q_INVOKABLE void setCacheEnabled(bool enabled);
    Q_INVOKABLE int cacheSize() const;
    Q_INVOKABLE void clearCache();

    // L99 streaming：当前链任一 provider 支持增量输出（今日恒 false，
    // 真 LLM/SSE provider 接入后反映能力）。
    Q_INVOKABLE bool streamingSupported() const;

private:
    // 命令串变化即重建链：有命令 = command + heuristic，无 = 仅 heuristic
    void rebuildProviders();
    // 沿链求值 op（translate/grammar/sentences）并 memo；空输入短路返回空。
    QString resolve(const QString& op, const QString& input,
                    const QString& lang = QString()) const;
    // 缓存键（op + lang + input）
    static QString cacheKey(const QString& op, const QString& input, const QString& lang);

    QString cmd_;
    std::vector<std::unique_ptr<AiProvider>> providers_;
    QHash<QString, QString> promptOverrides_;
    bool promptForwarding_ = false;
    mutable QHash<QString, QString> cache_;
    bool cacheEnabled_ = true;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_AI_SERVICE_QT_H
