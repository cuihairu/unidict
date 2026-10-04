#include "lookup_adapter.h"
#include "clipboard_monitor.h"
#include "global_hotkeys.h"

#include <QAudioOutput>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMediaPlayer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTextToSpeech>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtGlobal>

#include "lookup_service.h"
#include "unidict_core.h"
#include "data_store.h"
#include "std/html_renderer_std.h"
#include "std/ipa_to_arpabet_std.h"
#include "std/mdd_resource_std.h"
#include "std/online_pron_std.h"

using namespace UnidictCore;

// UNIDICT_DICTS 的多路径分隔符跟各平台 PATH 惯例保持一致：Windows 用 ';'、
// POSIX 用 ':'。不能两者都当分隔符——Windows 盘符 'C:\...' 自带冒号，
// 会被 ':' 劈碎成 'C' + '\Users\...'，导致字典全部加载失败
static QStringList splitEnvDictPaths(const QString& env) {
    return env.split(QDir::listSeparator(), Qt::SkipEmptyParts);
}

static QString stripHtmlForStorage(const QString& in) {
    // Keep vocabulary storage readable even when UI renders rich-text definitions.
    if (!in.contains('<') || !in.contains('>')) return in;
    QString s = in;
    s.replace(QRegularExpression("<\\s*br\\s*/?>", QRegularExpression::CaseInsensitiveOption), "\n");
    s.replace(QRegularExpression("</\\s*p\\s*>", QRegularExpression::CaseInsensitiveOption), "\n\n");
    s.replace(QRegularExpression("</\\s*div\\s*>", QRegularExpression::CaseInsensitiveOption), "\n");
    s.remove(QRegularExpression("<[^>]+>"));
    s.replace("&nbsp;", " ");
    return s.trimmed();
}

LookupAdapter::LookupAdapter(QObject* parent)
    : QObject(parent)
    , m_service(std::make_unique<LookupService>())
    , m_tts(std::make_unique<QTextToSpeech>(this))
    , m_clipboardMonitor(std::make_unique<ClipboardMonitor>(this))
    , m_globalHotkeys(std::make_unique<GlobalHotkeys>(this))
    , m_net(std::make_unique<QNetworkAccessManager>(this))
    , m_player(std::make_unique<QMediaPlayer>(this))
    , m_audioOut(std::make_unique<QAudioOutput>(this))
    , m_pronSource(std::make_unique<UnidictCoreStd::FreeDictionarySource>())
    , m_p0(std::make_unique<P0Modules>()) {

    // Connect clipboard monitor word detection
    m_clipboardWordConnection = connect(m_clipboardMonitor.get(), &ClipboardMonitor::wordDetected,
        [this](const QString& word) {
            if (m_clipboardAutoLookupEnabled) {
                emit clipboardWordDetected(word);
            }
        }
    );

    // Connect global hotkey handler
    m_hotkeyPressedConnection = connect(m_globalHotkeys.get(), &GlobalHotkeys::hotkeyPressed,
        [this](const QString& action) {
            if (action == "lookup_selection") {
                // TODO: Get selected text from focused window
                // This requires platform-specific implementation
            } else if (action == "show_window") {
                // TODO: Show/bring main window to front
            } else if (action == "quick_lookup") {
                // Trigger quick lookup mode
            }
        }
    );
    // Initialize default voice presets
    // 续行 78/81/84 是 gcov 在 -O2 下的归属假象：insert 首行命中、initializer_list
    // 临时量的构造块被内联进 <initializer_list>/<map> 头文件而挂在这三行上，
    // 实测构造函数跑 16 次这三行仍恒 0（预设内容本身有测试断言兜底）。
    m_voicePresets.insert("Default", QVariantMap{
        { "rate", 1.0 }, { "pitch", 0.0 }, { "volume", 0.8 }  // GCOVR_EXCL_LINE
    });
    m_voicePresets.insert("Calm Study", QVariantMap{
        { "rate", 0.8 }, { "pitch", -0.1 }, { "volume", 0.9 }  // GCOVR_EXCL_LINE
    });
    m_voicePresets.insert("Quick Review", QVariantMap{
        { "rate", 1.4 }, { "pitch", 0.1 }, { "volume", 0.8 }  // GCOVR_EXCL_LINE
    });
    if (m_tts) {
        setRate(m_currentRate);
        setPitch(m_currentPitch);
        setVolume(m_currentVolume);
    }

    // 在线发音播放管线：QMediaPlayer 必须挂 QAudioOutput 才出声；
    // 播放器错误/播完都推进状态行，用户知道链路走到了哪一步
    if (m_player && m_audioOut) {
        m_player->setAudioOutput(m_audioOut.get());
        connect(m_player.get(), &QMediaPlayer::errorOccurred, this,
                [this](QMediaPlayer::Error, const QString& errorString) {
                    emit pronOnlineStatus(
                        QStringLiteral("在线发音播放失败：%1").arg(errorString));
                });
        connect(m_player.get(), &QMediaPlayer::mediaStatusChanged, this,
                [this](QMediaPlayer::MediaStatus status) {
                    if (status == QMediaPlayer::EndOfMedia) {
                        emit pronOnlineStatus(QStringLiteral("在线发音播放完毕"));
                    }
                });
    }

    // 发音源三态与口音偏好持久化（与 SettingsQt 同一 ini 文件）。
    // 默认本地：隐私口径——开在线是显式动作
    const QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                             QCoreApplication::organizationName(),
                             QCoreApplication::applicationName());
    m_pronSourceMode =
        qBound(0, settings.value("pron/sourceMode", 0).toInt(), 2);
    m_pronAccent = qBound(0, settings.value("pron/accent", 0).toInt(), 3);
}

QString LookupAdapter::lookupDefinition(const QString& word) {
    // 记录导航历史
    navigateToWord(word);

    const QString def = m_service->lookupDefinition(word, true, 10);

    if (!def.startsWith("Word not found")) {
        DataStore::instance().addSearchHistory(word);
        if (m_autoPlayEnabled && m_tts && !word.trimmed().isEmpty()) {
            const QString spokenWord = word;
            const int delay = qMax(0, m_autoPlayDelay);
            if (delay <= 0) {
                speakText(spokenWord);
            } else {
                QTimer::singleShot(delay, this, [this, spokenWord]() {
                    speakText(spokenWord);
                });
            }
        }
    }
    return def;
}

QStringList LookupAdapter::suggestPrefix(const QString& prefix, int maxResults) const {
    return m_service->suggestPrefix(prefix, maxResults);
}

QStringList LookupAdapter::loadedDictionaries() const {
    return DictionaryManager::instance().getLoadedDictionaries();
}

QVariantList LookupAdapter::dictionariesMeta() const {
    QVariantList out;
    const auto metas = DictionaryManager::instance().getDictionariesMeta();
    for (const auto& m : metas) {
        QVariantMap vm;
        vm["name"] = m.name;
        vm["wordCount"] = m.wordCount;
        vm["description"] = m.description;
        out.push_back(vm);
    }
    return out;
}

bool LookupAdapter::loadDictionariesFromEnv() {
    const QString env = qEnvironmentVariable("UNIDICT_DICTS");
    if (env.isEmpty()) return false;
    const QStringList paths = splitEnvDictPaths(env);
    bool ok = false;
    for (const QString& p : paths) {
        ok |= DictionaryManager::instance().addDictionary(p.trimmed());
    }
    if (ok) {
        dictionariesStamp_++;
        emit dictionariesStampChanged();
    }
    return ok;
}

bool LookupAdapter::reloadDictionariesFromEnv() {
    DictionaryManager::instance().clearDictionaries();
    const QString env = qEnvironmentVariable("UNIDICT_DICTS");
    if (env.isEmpty()) {
        dictionariesStamp_++;
        emit dictionariesStampChanged();
        return false;
    }
    const QStringList paths = splitEnvDictPaths(env);
    bool ok = false;
    for (const QString& p : paths) {
        ok |= DictionaryManager::instance().addDictionary(p.trimmed());
    }
    dictionariesStamp_++;
    emit dictionariesStampChanged();
    return ok;
}

bool LookupAdapter::setMdictPassword(const QString& password) {
    if (password.isEmpty()) return false;
    qputenv("UNIDICT_MDICT_PASSWORD", password.toUtf8());
    return true;
}

void LookupAdapter::clearMdictPassword() {
    qunsetenv("UNIDICT_MDICT_PASSWORD");
    qunsetenv("UNIDICT_PASSWORD");
}

bool LookupAdapter::hasMdictPassword() const {
    const QString pw = qEnvironmentVariable("UNIDICT_MDICT_PASSWORD");
    if (!pw.isEmpty()) return true;
    const QString pw2 = qEnvironmentVariable("UNIDICT_PASSWORD");
    return !pw2.isEmpty();
}

void LookupAdapter::addToVocabulary(const QString& word, const QString& definition) {
    DictionaryEntry e;
    e.word = word;
    e.definition = stripHtmlForStorage(definition);
    DataStore::instance().addVocabularyItem(e);
}

QStringList LookupAdapter::suggestFuzzy(const QString& word, int maxResults) const {
    return m_service->suggestFuzzy(word, maxResults);
}

QStringList LookupAdapter::searchWildcard(const QString& pattern, int maxResults) const {
    return m_service->searchWildcard(pattern, maxResults);
}

QStringList LookupAdapter::searchRegex(const QString& pattern, int maxResults) const {
    return DictionaryManager::instance().regexSearch(pattern, maxResults);
}

QStringList LookupAdapter::searchHistory(int limit) const {
    return DataStore::instance().getSearchHistory(limit);
}

QVariantList LookupAdapter::vocabulary() const {
    QVariantList out;
    const auto items = DataStore::instance().getVocabulary();
    for (const auto& e : items) {
        QVariantMap m; m["word"] = e.word; m["definition"] = e.definition; out.push_back(m);
    }
    return out;
}

QVariantList LookupAdapter::vocabularyMeta() const {
    return DataStore::instance().getVocabularyMeta();
}

// —— M3-B 生词本编辑：标签/笔记（core DataStore M3 口径转发，标签与
// 笔记操作 core 内部自带落盘，这里不重复 save） ——

QVariantList LookupAdapter::vocabularyByTag(const QString& tag) const {
    return DataStore::instance().getVocabularyByTag(tag);
}

bool LookupAdapter::addVocabTag(const QString& word, const QString& tag) {
    return DataStore::instance().addVocabularyItemTag(word, tag);
}

bool LookupAdapter::removeVocabTag(const QString& word, const QString& tag) {
    return DataStore::instance().removeVocabularyItemTag(word, tag);
}

void LookupAdapter::setVocabNote(const QString& word, const QString& text) {
    DataStore::instance().setNote(word, text);
}

QString LookupAdapter::getVocabNote(const QString& word) const {
    return DataStore::instance().getNote(word);
}

void LookupAdapter::removeVocabularyWord(const QString& word) {
    DataStore::instance().removeVocabularyItem(word);
}

void LookupAdapter::clearHistory() {
    DataStore::instance().clearHistory();
}

void LookupAdapter::clearVocabulary() {
    DataStore::instance().clearVocabulary();
}

int LookupAdapter::indexedWordCount() const {
    return DictionaryManager::instance().getIndexedWordCount();
}

bool LookupAdapter::exportVocabCsv(const QString& path) const {
    return DataStore::instance().exportVocabularyCSV(path);
}

// ================= TTS功能实现 =================

// 朗读统一入口（结果页/生词本/自动播放都汇到这里）：按发音源三态
// 分发——本地直接 TTS；在线与自动走在线片段，自动态失败回落本地
void LookupAdapter::speakText(const QString& text) {
    const QString word = text.trimmed();
    if (word.isEmpty()) return;
    if (m_pronSourceMode == 0) {
        ttsSay(word);
        return;
    }
    fetchOnlinePron(word, /*fallbackLocal=*/ m_pronSourceMode == 2);
}

void LookupAdapter::ttsSay(const QString& text) {
    if (m_tts) {
        m_tts->say(text);
    }
}

void LookupAdapter::fetchOnlinePron(const QString& word, bool fallbackLocal) {
    if (!m_net || !m_pronSource) {
        if (fallbackLocal) ttsSay(word);
        return;
    }
    if (m_pronFetchActive) {
        emit pronOnlineStatus(QStringLiteral("上一个在线发音请求还在进行，稍后再试"));
        if (fallbackLocal) ttsSay(word);
        return;
    }
    // 唯一外发内容是查询词（core 层拼装保证，无历史/生词本等附加信息）
    const std::string url = m_pronSource->request_url(word.toStdString());
    if (url.empty()) {
        if (fallbackLocal) ttsSay(word);
        return;
    }
    m_pronFetchActive = true;
    emit pronOnlineStatus(QStringLiteral("正在获取在线发音（%1）…")
                              .arg(QString::fromLatin1(m_pronSource->name())));

    QNetworkRequest request{QUrl(QString::fromStdString(url))};
    request.setTransferTimeout(8000);
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, word, fallbackLocal]() {
        reply->deleteLater();
        m_pronFetchActive = false;

        const QVariant httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        std::vector<UnidictCoreStd::PronClip> clips;
        if (reply->error() == QNetworkReply::NoError && httpStatus.toInt() == 200) {
            clips = m_pronSource->parse_response(
                QString::fromUtf8(reply->readAll()).toStdString());
        }
        // 口音偏好：设置 0=自动（不指定），1/2/3 对应美/英/澳
        UnidictCoreStd::PronAccent prefer = UnidictCoreStd::PronAccent::Unknown;
        if (m_pronAccent == 1) prefer = UnidictCoreStd::PronAccent::US;
        else if (m_pronAccent == 2) prefer = UnidictCoreStd::PronAccent::UK;
        else if (m_pronAccent == 3) prefer = UnidictCoreStd::PronAccent::AU;
        const UnidictCoreStd::PronClip* clip =
            UnidictCoreStd::pick_clip(clips, prefer);
        if (!clip || !m_player) {
            // 失败口径：在线态只报状态；自动态回落本地 TTS
            if (fallbackLocal) {
                emit pronOnlineStatus(QStringLiteral("在线发音失败，已回落本地语音"));
                ttsSay(word);
            } else if (reply->error() != QNetworkReply::NoError) {
                emit pronOnlineStatus(QStringLiteral("在线发音失败：%1")
                                          .arg(reply->errorString()));
            } else {
                emit pronOnlineStatus(QStringLiteral("该词没有在线发音片段"));
            }
            return;
        }
        m_player->setSource(QUrl(QString::fromStdString(clip->url)));
        m_player->play();
        emit pronOnlineStatus(QStringLiteral("正在播放在线发音（%1）…")
                                  .arg(QString::fromLatin1(m_pronSource->name())));
    });
}

void LookupAdapter::setPronSourceMode(int mode) {
    const int v = qBound(0, mode, 2);
    if (v == m_pronSourceMode) return;
    m_pronSourceMode = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    settings.setValue("pron/sourceMode", v);
    settings.sync();
}

int LookupAdapter::pronSourceMode() const { return m_pronSourceMode; }

void LookupAdapter::setPronAccent(int accent) {
    const int v = qBound(0, accent, 3);
    if (v == m_pronAccent) return;
    m_pronAccent = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    settings.setValue("pron/accent", v);
    settings.sync();
}

int LookupAdapter::pronAccent() const { return m_pronAccent; }

void LookupAdapter::stopSpeaking() {
    if (m_tts) {
        m_tts->stop();
    }
    if (m_player) {
        m_player->stop();
    }
}

void LookupAdapter::pauseSpeaking() {
    if (m_tts) {
        m_tts->pause();
    }
}

void LookupAdapter::resumeSpeaking() {
    if (m_tts) {
        m_tts->resume();
    }
}

bool LookupAdapter::isSpeaking() const {
    if (m_player &&
        m_player->playbackState() == QMediaPlayer::PlayingState) {
        return true;
    }
    return m_tts && m_tts->state() == QTextToSpeech::Speaking;
}

bool LookupAdapter::isPaused() const {
    return m_tts && m_tts->state() == QTextToSpeech::Paused;
}

QStringList LookupAdapter::availableVoices() const {
    QStringList voices;
    if (m_tts) {
        const auto voiceList = m_tts->availableVoices();
        for (const auto& voice : voiceList) {
            // 无引擎的 headless 环境（offscreen CI 无 speech-dispatcher，
            // mock 引擎只认 QTextToSpeech("mock") 显式构造，adapter 走默认
            // 探测）恒空表，循环体不可确定性到达——仓库纪律也禁止对音频
            // 设备/引擎做假设。
            voices.append(voice.name());  // GCOVR_EXCL_LINE
        }
    }
    return voices;
}

void LookupAdapter::setVoice(const QString& voiceName) {
    if (m_tts) {
        const auto voiceList = m_tts->availableVoices();
        for (const auto& voice : voiceList) {
            // 同上：headless 无语音后端时 availableVoices() 恒空，匹配体
            // 不可确定性到达（循环本身的扫描行已由测试命中）。
            if (voice.name() == voiceName) {  // GCOVR_EXCL_LINE
                m_tts->setVoice(voice);       // GCOVR_EXCL_LINE
                break;                        // GCOVR_EXCL_LINE
            }
        }
    }
}

QString LookupAdapter::getCurrentVoice() const {
    if (!m_tts) return {};
    auto voice = m_tts->voice();
    return voice.name();
}

void LookupAdapter::setRate(double rate) {
    m_currentRate = qBound(0.1, rate, 2.0);
    if (m_tts) {
        // 将0.1-2.0映射到Qt的-1.0到1.0
        double qtRate = (m_currentRate - 1.0) * 2.0;
        qtRate = qBound(-1.0, qtRate, 1.0);
        m_tts->setRate(qtRate);
    }
}

double LookupAdapter::getRate() const {
    return m_currentRate;
}

void LookupAdapter::setPitch(double pitch) {
    m_currentPitch = qBound(-1.0, pitch, 1.0);
    if (m_tts) {
        m_tts->setPitch(m_currentPitch);
    }
}

double LookupAdapter::getPitch() const {
    return m_currentPitch;
}

void LookupAdapter::setVolume(double volume) {
    m_currentVolume = qBound(0.0, volume, 1.0);
    if (m_tts) {
        m_tts->setVolume(m_currentVolume);
    }
}

double LookupAdapter::getVolume() const {
    return m_currentVolume;
}

void LookupAdapter::applyVoicePreset(const QString& presetName) {
    auto it = m_voicePresets.constFind(presetName);
    if (it == m_voicePresets.constEnd()) return;
    const QVariantMap preset = it.value();
    if (preset.contains("rate")) setRate(preset.value("rate").toDouble());
    if (preset.contains("pitch")) setPitch(preset.value("pitch").toDouble());
    if (preset.contains("volume")) setVolume(preset.value("volume").toDouble());
    const QString voice = preset.value("voice").toString();
    if (!voice.isEmpty()) setVoice(voice);
}

QStringList LookupAdapter::getVoicePresets() const {
    return m_voicePresets.keys();
}

void LookupAdapter::setAutoPlayEnabled(bool enabled) {
    m_autoPlayEnabled = enabled;
}

bool LookupAdapter::isAutoPlayEnabled() const {
    return m_autoPlayEnabled;
}

void LookupAdapter::setAutoPlayDelay(int milliseconds) {
    m_autoPlayDelay = qMax(0, milliseconds);
}

int LookupAdapter::getAutoPlayDelay() const {
    return m_autoPlayDelay;
}

QVariantMap LookupAdapter::getVoiceInfo() const {
    QVariantMap info;
    info["speaking"] = isSpeaking();
    info["paused"] = isPaused();
    info["voice"] = getCurrentVoice();
    info["availableVoices"] = availableVoices();
    info["rate"] = m_currentRate;
    info["pitch"] = m_currentPitch;
    info["volume"] = m_currentVolume;
    info["autoPlayEnabled"] = m_autoPlayEnabled;
    info["autoPlayDelay"] = m_autoPlayDelay;
    return info;
}

// ============================================================================
// P0 专业词典功能实现
// ============================================================================

// 词条里出现媒体引用时扫的标签。排除项是"已经能自己加载"的 URL——
// 外链、data: 内嵌，以及上一轮已经重写过的 file:/res:/qrc:（重复调用
// 不该二次处理，否则第二次会把已解析的缓存路径再当资源键去查一遍）。
static const QRegularExpression& mediaSrcRe() {
    static const QRegularExpression re(
        QStringLiteral(R"((<\s*(?:img|audio|source|video)\b[^>]*?\bsrc\s*=\s*)"
                       R"((["'])(?!https?:|data:|file:|res:|qrc:)([^"']+)(["'])))"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

// 由词典源文件路径推导出同目录同名 .mdd（MDict 的资源包约定）。
// "book.mdx" -> "book.mdd"；无扩展名时 completeBaseName() 即整名。
static QString deriveMddPath(const QString& dictionaryPath) {
    if (dictionaryPath.isEmpty()) {
        return {};  // GCOVR_EXCL_LINE 唯一调用点 ensureMdd 已先对
                    // dictPath.isEmpty() 早退，此守卫按构造不可达
    }
    const QFileInfo fi(dictionaryPath);
    return QDir(fi.absolutePath()).filePath(
        fi.completeBaseName() + QStringLiteral(".mdd"));
}

// Pimpl 类封装 std 模块依赖
class LookupAdapter::P0Modules {
public:
    P0Modules() {
        // 资源缓存目录：.mdd 解出来的资源文件落在这里，供 QML 直接以
        // file:// 加载。Image/Audio 对 file:// 有本地读权限，而 qrc:/data:
        // 都塞不进一份按词典动态加载的二进制资源。
        const QString cacheDir =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
            QStringLiteral("/unidict_mdd");
        QDir().mkpath(cacheDir);
        resources.set_cache_directory(cacheDir.toStdString());
    }

    // HTML 渲染：走 core/std 的白名单清洗器（标签/属性/CSS 属性白名单、
    // URL 协议白名单、嵌套深度与文本长度护栏）。原先这里是手搓的一串
    // QRegularExpression，只挡得住 <script>/<iframe>/on*= 这几种整齐写法：
    //   - <script/src=x>、<script >、属性不带引号等变体一律漏过；
    //   - 完全没有协议校验，href="javascript:..." / "data:text/html" 原样放行；
    //   - 没有标签白名单，<style>/<form>/<base> 都能进富文本。
    // core/std 那一版是逐 token 白名单判定，顺带产出纯文本与链接目标。
    QString sanitize(const QString& html) const {
        return QString::fromStdString(
            renderer.render(html.toStdString()).html);
    }

    QString extractText(const QString& html) const {
        return QString::fromStdString(
            renderer.extract_text(html.toStdString()));
    }

    // 文本节点级命中词标注（P-4）：sanitize 后 HTML 的文本节点内按
    // 大小写不敏感匹配，命中片段包 <span class="udict-hl">——显示层
    // QML 负责把 marker 换成主题高亮色。标签/属性明文不动。
    QString highlight(const QString& html, const QString& word) const {
        if (word.isEmpty()) return html;
        return QString::fromStdString(
            renderer.highlight_term(html.toStdString(), word.toStdString()));
    }

    // 交叉引用链接重写：entry:// / bword:// 统一转成 unidict://lookup?word=，
    // @@@LINK=word 就地替换成目标词（它在纯文本上下文里出现，不该留标记）。
    // 三者都在清洗之后跑：清洗器的协议白名单含 entry，链接能活到这一步。
    QString rewriteLinks(const QString& html) const {
        static const QRegularExpression entryRe(
            QStringLiteral(R"(entry://([^<"\s]+))"));
        static const QRegularExpression bwordRe(
            QStringLiteral(R"(bword://([^<"\s]+))"));
        static const QRegularExpression atAtRe(
            QStringLiteral(R"(@@@LINK=([^\s<>"']+))"));
        QString result = html;
        result.replace(entryRe, QStringLiteral(R"(unidict://lookup?word=\1)"));
        result.replace(bwordRe, QStringLiteral(R"(unidict://lookup?word=\1)"));
        result.replace(atAtRe, QStringLiteral(R"(\1)"));
        return result;
    }

    // 确保该词典的 .mdd 已挂进 resources，返回是否可用。
    bool ensureMdd(const QString& dictionaryId) const;

    // 资源键 → 本地缓存文件 URL（未命中返回空串）
    QString resolveOne(const QString& dictionaryId, const QString& key) const;

    // 媒体 src 重写：命中的换成 file:// 缓存路径，命中不了的原样保留
    // （外链与 data: 本来就该留着；.mdd 里没有的相对路径也不该被改成空，
    //  由调用方按清单里的 found=false 决定是否兜底）。传 outRefs 时顺带
    // 收集（键, 解析结果）清单，避免为了拿清单再扫一遍 HTML。
    QString rewriteMediaSrc(const QString& html, const QString& dictionaryId,
                            QVector<QPair<QString, QString>>* outRefs) const;

    // 交叉引用导航状态
    QStringList backStack;
    QStringList forwardStack;
    QString currentWord;
    QString currentDictionary;

    mutable UnidictCoreStd::HtmlRendererStd renderer;
    mutable UnidictCoreStd::MddResourceManager resources;
    // 已挂载的 .mdd：dictId → .mdd 路径。mountedOrder 是 FIFO 淘汰序——
    // 多词典对照时来回切不该每次重开文件，但把用户所有词典的索引全留在
    // 内存里也不行（大 .mdd 的索引表不小），所以留最近 kMaxMountedDicts 个。
    mutable QHash<QString, QString> mounted;
    // 挂载时的文件指纹（lastModified ms + size）：大小写不敏感文件系统
    // （macOS/Windows）上换名重挂（book.mdd → Book.mdd）后，旧路径的
    // exists() 会对新文件误判"还在"，仅靠存在性复用会拿旧解析器去解析
    // 新文件（Windows/macOS CI 的 mdd_remount_after_file_swap 挂点即此，
    // Linux 大小写敏感恰好走换文件分支测不出）。复用必须指纹一致——
    // 同名覆盖的内容替换也能被 size/mtime 变化抓到。
    mutable QHash<QString, QPair<qint64, qint64>> mountedStamp;
    mutable QStringList mountedOrder;
    static constexpr int kMaxMountedDicts = 4;
};

bool LookupAdapter::P0Modules::ensureMdd(const QString& dictionaryId) const {
    if (dictionaryId.isEmpty()) {
        return false;
    }
    const auto it = mounted.constFind(dictionaryId);
    if (it != mounted.constEnd()) {
        const QFileInfo fi(*it);
        const auto stamp = mountedStamp.constFind(dictionaryId);
        if (fi.exists() && stamp != mountedStamp.constEnd()
            && fi.lastModified().toMSecsSinceEpoch() == stamp->first
            && fi.size() == stamp->second) {
            return true;  // 同一词典同一路径同一文件：复用
        }
    }

    // 找到该词典的源文件路径，推导 .mdd
    QString dictPath;
    const auto infos =
        UnidictCore::DictionaryManager::instance().getLoadedDictionaryInfos();
    for (const auto& info : infos) {
        if (info.id == dictionaryId) {
            dictPath = info.filePath;
            break;
        }
    }
    if (dictPath.isEmpty()) {
        return false;
    }

    // 淘汰：先把同 id 的旧记录摘掉（文件换过的情况）。必须在 load_mdd
    // **之前**做：load_mdd 按 id 覆盖写 dictionaries_，先 load 后 unload
    // 会把刚装上的新解析器删掉——ensureMdd 却返回 true，mounted 里还留着
    // 新路径，下次 495 行直接命中缓存早退，该词典的资源从此永久解析不出
    // （换过一次 .mdd 的词典图片/发音全黑）。
    if (it != mounted.constEnd()) {
        resources.unload_mdd(dictionaryId.toStdString());
        mountedStamp.remove(dictionaryId);
        mountedOrder.removeAll(dictionaryId);
    }

    const QString mddPath = deriveMddPath(dictPath);
    if (!QFile::exists(mddPath) ||
        !resources.load_mdd(mddPath.toStdString(), dictionaryId.toStdString())) {
        return false;
    }
    while (mountedOrder.size() >= kMaxMountedDicts) {
        const QString victim = mountedOrder.takeFirst();
        resources.unload_mdd(victim.toStdString());
        mounted.remove(victim);
        mountedStamp.remove(victim);
    }
    const QFileInfo loaded(mddPath);
    mounted.insert(dictionaryId, mddPath);
    mountedStamp.insert(dictionaryId,
                        {loaded.lastModified().toMSecsSinceEpoch(),
                         loaded.size()});
    mountedOrder.append(dictionaryId);
    return true;
}

QString LookupAdapter::P0Modules::resolveOne(const QString& dictionaryId,
                                             const QString& key) const {
    if (key.isEmpty() || !ensureMdd(dictionaryId)) {
        return {};
    }
    // core/std 的 normalize_key 已经处理了前导斜杠、\、协议前缀、?query、
    // #fragment 与大小写；这里只补它没做的 "./" 前缀——MDX 里的
    // <img src="./pic/a.png"> 很常见，而 .mdd 里的键是 "pic/a.png"。
    QString normalized = key;
    if (normalized.startsWith(QLatin1String("./"))) {
        normalized = normalized.mid(2);
    }
    const std::string path = resources.get_resource_path(
        normalized.toStdString(), dictionaryId.toStdString());
    if (path.empty()) {
        return {};
    }
    // 转成 file:// 绝对路径给 QML 的 Image/Audio 用
    return QUrl::fromLocalFile(QString::fromStdString(path)).toString();
}

QString LookupAdapter::P0Modules::rewriteMediaSrc(
    const QString& html, const QString& dictionaryId,
    QVector<QPair<QString, QString>>* outRefs) const {
    if (html.isEmpty() || dictionaryId.isEmpty()) {
        return html;
    }

    QString out;
    out.reserve(html.size());
    int pos = 0;
    QRegularExpressionMatchIterator it = mediaSrcRe().globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        // 复制到**开引号**为止：captured(1) 覆盖的正是 [<img src= 这一段，
        // 再单独 emit 一次会把标签前缀写两遍（<img src="a.png""file://..."）。
        out += html.mid(pos, m.capturedStart(2) - pos);
        const QString quote = m.captured(2);
        const QString key = m.captured(3);
        const QString resolved = resolveOne(dictionaryId, key);
        if (outRefs) {
            outRefs->append({key, resolved});  // resolved 空 = 未命中
        }
        out += quote + (resolved.isEmpty() ? key : resolved) + quote;
        pos = m.capturedEnd(4);
    }
    out += html.mid(pos);
    return out;
}

// 栈上对象在每个测试里都真实析构（unique_ptr 链已验证释放），但 -O2 把
// 析构体在所有调用点内联，链接器保留的这份独立函数体恒 0 命中——gcov 假缺口。
LookupAdapter::~LookupAdapter() = default;  // GCOVR_EXCL_LINE

QString LookupAdapter::sanitizeHtml(const QString& html) const {
    if (!m_p0) return html;
    return m_p0->sanitize(html);
}

QString LookupAdapter::extractTextFromHtml(const QString& html) const {
    if (!m_p0) return html;
    return m_p0->extractText(html);
}

QString LookupAdapter::highlightHtml(const QString& html, const QString& word) const {
    if (!m_p0) return html;
    return m_p0->highlight(html, word);
}

QString LookupAdapter::rewriteResourceUrls(const QString& html, const QString& dictionaryId) const {
    if (!m_p0) return html;
    return m_p0->rewriteMediaSrc(html, dictionaryId, nullptr);
}

// 一站式词条呈现管线。QML 侧一次调用拿全：清洗过的富文本、纯文本回退、
// 以及资源清单（哪些图片/音频在 .mdd 里、解析成了什么 URL）。顺序是
// 清洗 → 交叉引用 → 资源重写：清洗会按协议白名单剔 href/src，不先洗
// 的话重写出来的 URL 还要再过一遍白名单；资源重写必须最后做，因为它
// 要往 src 里填 file:// 路径。
QVariantMap LookupAdapter::presentEntry(const QString& rawDefinition,
                                        const QString& dictionaryId) const {
    QVariantMap out;
    QVariantList refs;
    QString html = rawDefinition;
    if (m_p0) {
        html = m_p0->rewriteLinks(m_p0->sanitize(html));
        QVector<QPair<QString, QString>> resolved;
        html = m_p0->rewriteMediaSrc(html, dictionaryId, &resolved);
        for (const auto& kv : resolved) {
            QVariantMap ref;
            ref.insert(QStringLiteral("key"), kv.first);
            ref.insert(QStringLiteral("url"), kv.second);
            ref.insert(QStringLiteral("found"), !kv.second.isEmpty());
            refs.append(ref);
        }
    }
    out.insert(QStringLiteral("html"), html);
    out.insert(QStringLiteral("text"),
               m_p0 ? m_p0->extractText(html) : rawDefinition);
    out.insert(QStringLiteral("resources"), refs);
    return out;
}

bool LookupAdapter::hasDictionaryResource(const QString& dictionaryId, const QString& key) const {
    if (!m_p0 || dictionaryId.isEmpty() || key.isEmpty()) return false;
    if (!m_p0->ensureMdd(dictionaryId)) return false;
    return m_p0->resources.has_resource(key.toStdString(), dictionaryId.toStdString());
}

QByteArray LookupAdapter::loadDictionaryResourceData(const QString& dictionaryId, const QString& key) const {
    if (!m_p0 || dictionaryId.isEmpty() || key.isEmpty()) return {};
    if (!m_p0->ensureMdd(dictionaryId)) return {};
    const auto data = m_p0->resources.get_resource_data(key.toStdString(),
                                                          dictionaryId.toStdString());
    if (data.empty()) return {};
    return QByteArray(reinterpret_cast<const char*>(data.data()),
                      static_cast<int>(data.size()));
}

QString LookupAdapter::dictionaryResourceUrl(const QString& dictionaryId, const QString& key) const {
    if (!m_p0) return {};
    return m_p0->resolveOne(dictionaryId, key);
}

QString LookupAdapter::rewriteCrossReferenceLinks(const QString& html, const QString& /*dictionaryId*/) const {
    if (!m_p0) return html;
    return m_p0->rewriteLinks(html);
}

bool LookupAdapter::canGoBack() const {
    return m_p0 && !m_p0->backStack.isEmpty();
}

bool LookupAdapter::canGoForward() const {
    return m_p0 && !m_p0->forwardStack.isEmpty();
}

QString LookupAdapter::goBack() {
    if (!m_p0 || m_p0->backStack.isEmpty()) return QString();

    // 保存当前状态
    if (!m_p0->currentWord.isEmpty()) {
        m_p0->forwardStack.append(m_p0->currentWord);
    }

    // 返回上一个
    QString word = m_p0->backStack.takeLast();
    m_p0->currentWord = word;
    return word;
}

QString LookupAdapter::goForward() {
    if (!m_p0 || m_p0->forwardStack.isEmpty()) return QString();

    // 保存当前状态
    if (!m_p0->currentWord.isEmpty()) {
        m_p0->backStack.append(m_p0->currentWord);
    }

    // 前进到下一个
    QString word = m_p0->forwardStack.takeLast();
    m_p0->currentWord = word;
    return word;
}

void LookupAdapter::navigateToWord(const QString& word, const QString& dictionaryId) {
    if (!m_p0) return;

    // 保存当前状态
    if (!m_p0->currentWord.isEmpty() && m_p0->currentWord != word) {
        m_p0->backStack.append(m_p0->currentWord);
    }

    m_p0->currentWord = word;
    m_p0->currentDictionary = dictionaryId;

    // 清空 forward stack
    m_p0->forwardStack.clear();
}

void LookupAdapter::clearNavigationHistory() {
    if (m_p0) {
        m_p0->backStack.clear();
        m_p0->forwardStack.clear();
    }
}

int LookupAdapter::navigationHistorySize() const {
    if (!m_p0) return 0;
    return m_p0->backStack.size() + m_p0->forwardStack.size();
}

QVariantList LookupAdapter::aggregateLookup(const QString& word, const QVariantMap& options) {
    const int maxTotal = options.value("maxTotalResults", -1).toInt();
    const bool sanitize = options.value("sanitizeHtml", true).toBool();
    const bool rewriteLinks = options.value("rewriteCrossRefs", true).toBool();

    // 按词典分组的聚合结果（core searchGrouped：词头精确 > 前缀 > 释义
    // 包含三层降级 + 组内 headword 去重）。每组：{dictionary, dictionaryId,
    // entries:[{word, definition, pronunciation, examples, metadata, relevance}]}
    QVariantList results;
    const auto groups = DictionaryManager::instance().searchGrouped(word);

    int emitted = 0;
    bool any = false;
    for (const auto& group : groups) {
        QVariantMap groupMap;
        groupMap["dictionary"] = group.dictionaryName;
        groupMap["dictionaryId"] = group.dictionaryId;
        QVariantList groupEntries;

        for (const auto& e : group.entries) {
            const QString dictId = e.metadata.value("dictionary").toString();
            QVariantMap entry;
            entry["word"] = e.word;
            QString def = e.definition;
            // 顺序契约与 presentEntry 一致：清洗在前，重写在后（白名单已含
            // 重写产物 unidict://，次序颠倒了链接也不会丢，但别依赖它）
            if (sanitize) {
                def = sanitizeHtml(def);
            }
            if (rewriteLinks) {
                def = rewriteCrossReferenceLinks(def, dictId);
            }
            entry["definition"] = def;
            entry["pronunciation"] = e.pronunciation;
            entry["examples"] = e.examples;
            entry["metadata"] = e.metadata;
            entry["dictionary"] = dictId;
            entry["relevance"] = e.metadata.value("relevance").toInt();
            groupEntries.append(entry);
            any = true;

            emitted++;
            if (maxTotal > 0 && emitted >= maxTotal) break;
        }
        groupMap["entries"] = groupEntries;
        results.append(groupMap);
        if (maxTotal > 0 && emitted >= maxTotal) break;
    }

    if (any) {
        navigateToWord(word);
        DataStore::instance().addSearchHistory(word);

        if (m_autoPlayEnabled && m_tts && !word.trimmed().isEmpty()) {
            const QString spokenWord = word;
            const int delay = qMax(0, m_autoPlayDelay);
            if (delay <= 0) {
                speakText(spokenWord);
            } else {
                QTimer::singleShot(delay, this, [this, spokenWord]() {
                    speakText(spokenWord);
                });
            }
        }
    }

    return results;
}

// 词条卡头的英/美音标：词典没有独立发音字段，音标按惯例写在释义开头
// （"英 […] 美 […]"），用 core/std 提取器拿双字段（gui 发音面板同源）
QVariantMap LookupAdapter::extractPhonetics(const QString& definition) const {
    QVariantMap out;
    const auto fields =
        UnidictCoreStd::extract_phonetic_variants(definition.toStdString());
    if (fields.british) {
        out["british"] = QString::fromStdString(*fields.british);
    }
    if (fields.american) {
        out["american"] = QString::fromStdString(*fields.american);
    }
    // 中文词头（汉英词典）：释义开头惯例是 [拼音]（CC-CEDICT 形态
    // "[shuo1 ming2] to explain; …"）——英/美音标提取不到时用它填卡头
    // 音标行，避免中文词条只有裸喇叭
    if (!fields.british && !fields.american) {
        static const QRegularExpression pinyinRe(
            QStringLiteral("^\\s*\\[([^\\[\\]]{1,60})\\]"));
        const auto m = pinyinRe.match(definition);
        if (m.hasMatch()) {
            out["pinyin"] = m.captured(1).trimmed();
        }
    }
    return out;
}

// 全文检索 tab：释义中包含目标词的词条（core 倒排，带来源词典）
QVariantList LookupAdapter::fullTextLookup(const QString& word, int maxResults) const {
    QVariantList out;
    const auto entries = DictionaryManager::instance().fullTextSearch(word, maxResults);
    for (const auto& e : entries) {
        QVariantMap entry;
        entry["word"] = e.word;
        entry["definition"] = sanitizeHtml(e.definition);
        entry["dictionary"] = e.metadata.value("dictionary").toString();
        out.append(entry);
    }
    return out;
}

// 内容 tab 数据：phrases=词组（以查询词开头的词组条目，带回释义）；
// related=近义/联想词（前缀+模糊候选词表，词头蓝色链接形态）
QVariantList LookupAdapter::relatedLookup(const QString& word, const QString& kind) const {
    QVariantList out;
    auto& manager = DictionaryManager::instance();
    if (kind == QLatin1String("phrases")) {
        const QStringList candidates = manager.prefixSearch(word, 30);
        for (const QString& candidate : candidates) {
            if (!candidate.contains(QLatin1Char(' '))) {
                continue; // 词组=含空格的复合词头
            }
            QVariantMap item;
            item["word"] = candidate;
            const auto entries = manager.searchAll(candidate);
            item["definition"] =
                entries.isEmpty() ? QString() : sanitizeHtml(entries.first().definition);
            out.append(item);
            if (out.size() >= 12) {
                break;
            }
        }
    } else if (kind == QLatin1String("related")) {
        QStringList candidates = manager.prefixSearch(word, 12);
        const QStringList fuzzy = manager.searchSimilar(word, 12);
        for (const QString& f : fuzzy) {
            if (!candidates.contains(f)) {
                candidates.append(f);
            }
        }
        candidates.removeAll(word);
        for (const QString& candidate : candidates) {
            QVariantMap item;
            item["word"] = candidate;
            out.append(item);
            if (out.size() >= 15) {
                break;
            }
        }
    }
    return out;
}

QVariantList LookupAdapter::getDictionariesByCategory(const QString& category) const {
    // 按分类获取词典列表
    QVariantList results;
    auto dicts = DictionaryManager::instance().getLoadedDictionaries();

    for (const auto& dictId : dicts) {
        // 简化：所有词典返回，实际应按分类过滤
        QVariantMap info;
        info["id"] = dictId;
        info["category"] = category;
        results.append(info);
    }

    return results;
}

void LookupAdapter::setDictionaryPriority(const QString& dictionaryId, int priority) {
    // 设置词典优先级
    // 完整实现应调用 DictionaryAggregator::set_dictionary_priority()
    Q_UNUSED(dictionaryId);
    Q_UNUSED(priority);
}

void LookupAdapter::setDictionaryEnabled(const QString& dictionaryId, bool enabled) {
    // 设置词典启用状态
    // 完整实现应调用 DictionaryAggregator::set_dictionary_enabled()
    Q_UNUSED(dictionaryId);
    Q_UNUSED(enabled);
}

// ============================================================================
// P1 剪贴板监听功能实现
// ============================================================================

void LookupAdapter::startClipboardMonitoring() {
    m_clipboardMonitor->start();
}

void LookupAdapter::stopClipboardMonitoring() {
    m_clipboardMonitor->stop();
}

bool LookupAdapter::isClipboardMonitoring() const {
    return m_clipboardMonitor->isMonitoring();
}

void LookupAdapter::setClipboardPollInterval(int milliseconds) {
    m_clipboardMonitor->setPollInterval(milliseconds);
}

void LookupAdapter::setClipboardMinWordLength(int length) {
    m_clipboardMonitor->setMinWordLength(length);
}

void LookupAdapter::setClipboardMaxWordLength(int length) {
    m_clipboardMonitor->setMaxWordLength(length);
}

void LookupAdapter::addClipboardExcludePattern(const QString& pattern) {
    m_clipboardMonitor->addExcludePattern(pattern);
}

void LookupAdapter::clearClipboardExcludePatterns() {
    m_clipboardMonitor->clearExcludePatterns();
}

void LookupAdapter::setClipboardAutoLookupEnabled(bool enabled) {
    m_clipboardAutoLookupEnabled = enabled;
}

bool LookupAdapter::isClipboardAutoLookupEnabled() const {
    return m_clipboardAutoLookupEnabled;
}

// ============================================================================
// P1 全局热键功能实现
// ============================================================================

bool LookupAdapter::registerGlobalHotkey(const QString& action, const QString& keySequence) {
    return m_globalHotkeys->registerHotkey(action, keySequence);
}

void LookupAdapter::unregisterGlobalHotkey(const QString& action) {
    m_globalHotkeys->unregisterHotkey(action);
}

void LookupAdapter::unregisterAllGlobalHotkeys() {
    m_globalHotkeys->unregisterAllHotkeys();
}

QStringList LookupAdapter::registeredHotkeyActions() const {
    return m_globalHotkeys->registeredActions();
}

QString LookupAdapter::getHotkeyForAction(const QString& action) const {
    return m_globalHotkeys->getHotkeyForAction(action);
}

void LookupAdapter::setGlobalHotkeysEnabled(bool enabled) {
    m_globalHotkeys->setEnabled(enabled);
}

bool LookupAdapter::isGlobalHotkeysEnabled() const {
    return m_globalHotkeys->isEnabled();
}

bool LookupAdapter::isGlobalHotkeysSupported() {
    return GlobalHotkeys::isPlatformSupported();
}
