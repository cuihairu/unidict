// Unidict GUI —— Qt Widgets 壳。
// 结构按 docs/gui-ui-structure.md：顶部搜索框(QCompleter 补全) + 释义主区
// + 历史/收藏侧栏 + 状态栏/词典管理对话框，深浅色三态切换。

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QCloseEvent>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QStringListModel>
#include <QStyle>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWidget>

#include <optional>

#include "clipboard_monitor.h"
#include "data_store.h"
#include "global_hotkeys.h"
#include "pronunciation_panel.h"
#include "startup_launcher.h"
#include "std/html_renderer_std.h"
#include "std/ipa_to_arpabet_std.h"
#include "unidict_core.h"

namespace {

// 深色 QPalette 模板（Qt 官方 Dark 样式示例值）
QPalette darkPalette() {
    QPalette p;
    p.setColor(QPalette::Window, QColor(53, 53, 53));
    p.setColor(QPalette::WindowText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    p.setColor(QPalette::Base, QColor(42, 42, 42));
    p.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, QColor(53, 53, 53));
    p.setColor(QPalette::Text, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    p.setColor(QPalette::Dark, QColor(35, 35, 35));
    p.setColor(QPalette::Shadow, QColor(20, 20, 20));
    p.setColor(QPalette::Button, QColor(53, 53, 53));
    p.setColor(QPalette::ButtonText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(42, 130, 218));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
    return p;
}

// 主题三态：跟随系统 / 浅色 / 深色，记忆到 QSettings
class ThemeManager {
public:
    explicit ThemeManager(QApplication& app) : app_(app) {
        mode_ = QSettings().value("ui/theme", 0).toInt();
    }

    void apply(int mode) {
        mode_ = mode;
        QSettings().setValue("ui/theme", mode_);
        applyCurrent();
    }

    void applyCurrent() {
        const bool dark = mode_ == 2
            || (mode_ == 0 && app_.styleHints()->colorScheme() == Qt::ColorScheme::Dark);
        if (dark) {
            app_.setPalette(darkPalette());
        } else {
            app_.setPalette(app_.style()->standardPalette());
        }
    }

    [[nodiscard]] int mode() const { return mode_; }

    [[nodiscard]] QString label() const {
        switch (mode_) {
        case 1: return QStringLiteral("主题: 浅色");
        case 2: return QStringLiteral("主题: 深色");
        default: return QStringLiteral("主题: 跟随系统");
        }
    }

private:
    QApplication& app_;
    int mode_ = 0;
};

void loadDefaultDictionaryLocations() {
    auto& manager = UnidictCore::DictionaryManager::instance();
    manager.loadState();

    const QString envDir = qEnvironmentVariable("UNIDICT_DICT_DIR");
    if (!envDir.isEmpty()) {
        manager.addDictionariesFromDirectory(envDir);
    }
    const QString envDicts = qEnvironmentVariable("UNIDICT_DICTS");
    for (const QString& path : envDicts.split(QDir::listSeparator(), Qt::SkipEmptyParts)) {
        manager.addDictionary(path.trimmed());
    }
    const QString localDir = QDir(QApplication::applicationDirPath()).filePath("dictionaries");
    if (QFileInfo::exists(localDir)) {
        manager.addDictionariesFromDirectory(localDir);
    }
}

// ---------- 词条渲染管线（HTML 子集） ----------
// MDX 释义是 HTML：走 core/std HtmlRendererStd 白名单清洗（script/on*/
// javascript: 等一律剥除），站内跳转链接转 #w: 锚点，相对资源引用转
// res:/// 供 ResultBrowser 从 .mdd 取数据。JSON/CSV/DSL/EPUB 是纯文本，
// 沿用转义 + 换行转 <br/> 的旧路径。
QString renderRichDefinition(const QString& raw, const QString& dictionaryId) {
    const UnidictCoreStd::HtmlRendererStd renderer;
    QString html = QString::fromStdString(renderer.render(raw.toStdString()).html);

    // entry://word / bword://word → 站内锚点（与全文命中 #ft: 同机制）
    static const QRegularExpression linkRe(
        QStringLiteral(R"(href\s*=\s*["'](?:entry|bword)://([^"']+)["'])"));
    html.replace(linkRe, QStringLiteral(R"(href="#w:\1")"));

    // img/audio 等的相对资源引用 → res:///<key>?dict=<词典id 百分号编码>
    static const QRegularExpression resourceRe(
        QStringLiteral(R"((<\s*(?:img|audio|source|video)\b[^>]*?\bsrc\s*=\s*)"
                       R"((["'])(?!https?:|data:|res:)([^"']+)(["'])))"),
        QRegularExpression::CaseInsensitiveOption);
    const QString substitution =
        QStringLiteral(R"(\1\2res:///\3?dict=%1\4)")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(dictionaryId)));
    html.replace(resourceRe, substitution);
    return html;
}

// QTextBrowser 派生：res:// 资源回调 DictionaryManager 的 .mdd 解析，
// 图片按原始字节解码成 QImage 交给富文本引擎
class ResultBrowser : public QTextBrowser {
public:
    using QTextBrowser::QTextBrowser;

    QVariant loadResource(int type, const QUrl& url) override {
        if (url.scheme() == QLatin1String("res")) {
            const QUrlQuery query(url);
            const QString dictionaryId = query.queryItemValue(QStringLiteral("dict"));
            const QByteArray data =
                UnidictCore::DictionaryManager::instance()
                    .loadDictionaryResource(dictionaryId, url.path());
            if (!data.isEmpty()) {
                if (type == QTextDocument::ImageResource) {
                    QImage image;
                    image.loadFromData(data);
                    return image;
                }
                return data;
            }
            return {};
        }
        return QTextBrowser::loadResource(type, url);
    }
};

class MainWindow : public QWidget {
public:
    explicit MainWindow(QApplication& app) : app_(app), theme_(app) {
        setWindowTitle(QStringLiteral("Unidict"));

        // 窗口几何记忆：读不回（首次启动）用默认尺寸
        const QByteArray geo = QSettings().value("ui/windowGeometry").toByteArray();
        if (geo.isEmpty()) {
            resize(1080, 700);
        } else {
            restoreGeometry(geo);
        }

        auto* rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        buildToolbar(rootLayout);
        rootLayout->addWidget(buildSearchBar());
        rootLayout->addWidget(buildCentral(), 1);
        buildStatusBar(rootLayout);
        wireActions();
        theme_.applyCurrent();
        themeAction_->setText(theme_.label());
        applyResultFont();
        refreshAll();
        setupTray();
    }

protected:
    // 关窗：托盘可用且开关开启 → 隐藏到托盘常驻（全局热键/剪贴板取词继续
    // 可用，几何照存）；否则保存几何按默认关闭退出。托盘不可用（部分 Linux
    // 会话/offscreen）保持原行为
    void closeEvent(QCloseEvent* event) override {
        saveWindowGeometry();
        if (trayIcon_ && trayIcon_->isVisible() &&
            QSettings().value("ui/closeToTray", true).toBool()) {
            if (!trayHintShown_) {
                trayIcon_->showMessage(
                    QStringLiteral("Unidict"),
                    QStringLiteral("已隐藏到托盘，双击托盘图标恢复；退出请用托盘菜单。"),
                    QSystemTrayIcon::Information, 3000);
                trayHintShown_ = true;
            }
            hide();
            event->ignore();
            return;
        }
        QWidget::closeEvent(event);
    }

public:
    // ---------- 系统托盘 ----------
    // 托盘不可用时不创建托盘对象，关窗即退出（与托盘出现前的行为一致）。
    // “关闭时隐藏到托盘”开关记忆在 QSettings ui/closeToTray，默认开。
    void setupTray() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            return;
        }
        trayIcon_ = new QSystemTrayIcon(trayIconPixmap(), this);
        auto* menu = new QMenu(this);
        QAction* showAction = menu->addAction(QStringLiteral("显示主窗"));
        connect(showAction, &QAction::triggered, this, [this] {
            showNormal();
            activateWindow();
            raise();
        });
        QAction* trayToggle = menu->addAction(QStringLiteral("关闭时隐藏到托盘"));
        trayToggle->setCheckable(true);
        trayToggle->setChecked(QSettings().value("ui/closeToTray", true).toBool());
        connect(trayToggle, &QAction::toggled, this, [](bool checked) {
            QSettings().setValue("ui/closeToTray", checked);
        });
        // 开机自启（Windows 注册表 Run 键；其余平台 stub 不可勾选）
        StartupLauncher launcher;
        QAction* startupToggle = menu->addAction(QStringLiteral("开机自启"));
        startupToggle->setCheckable(true);
        startupToggle->setChecked(launcher.isEnabled());
        if (!StartupLauncher::isPlatformSupported()) {
            startupToggle->setEnabled(false);
            startupToggle->setToolTip(QStringLiteral("当前平台暂不支持开机自启"));
        }
        connect(startupToggle, &QAction::toggled, this,
                [this](bool checked) {
                    StartupLauncher l;
                    if (!l.setEnabled(checked)) {
                        QMessageBox::warning(this, QStringLiteral("开机自启"),
                                             QStringLiteral("设置开机自启失败"));
                    }
                });
        menu->addSeparator();
        QAction* quitAction = menu->addAction(QStringLiteral("退出"));
        connect(quitAction, &QAction::triggered, this, [this] {
            saveWindowGeometry();
            qApp->quit();
        });
        trayIcon_->setContextMenu(menu);
        trayIcon_->setToolTip(QStringLiteral("Unidict"));
        connect(trayIcon_, &QSystemTrayIcon::activated, this,
                [this](QSystemTrayIcon::ActivationReason reason) {
                    if (reason == QSystemTrayIcon::Trigger ||
                        reason == QSystemTrayIcon::DoubleClick) {
                        showNormal();
                        activateWindow();
                        raise();
                    }
                });
        trayIcon_->show();
    }

    void saveWindowGeometry() {
        QSettings().setValue("ui/windowGeometry", saveGeometry());
    }

private:
    // 自绘托盘图标（无资源文件依赖）：圆角底 + “U”
    static QPixmap trayIconPixmap() {
        QPixmap pm(32, 32);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x3a, 0x6e, 0xa5));
        p.drawRoundedRect(2, 2, 28, 28, 6, 6);
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(20);
        p.setFont(f);
        p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("U"));
        return pm;
    }

public:
    void runLookup(const QString& text) {
        const QString query = text.trimmed();
        if (query.isEmpty()) {
            return;
        }
        searchInput_->setText(query);
        ftHits_.clear();

        auto& manager = UnidictCore::DictionaryManager::instance();
        lastResult_ = manager.searchWord(query, activeTagFilter_);
        lastSuccess_ = lastResult_->success;
        statusLabel_->setText(lastResult_->message);

        QString html;
        if (lastResult_->success) {
            html += QStringLiteral("<h2>%1</h2>").arg(lastResult_->entry.word.toHtmlEscaped());
            for (const auto& match : lastResult_->matches) {
                html += QStringLiteral("<hr/><p style='color:gray'><small>%1</small></p>")
                            .arg(match.dictionaryName.toHtmlEscaped());
                if (match.entry.metadata.value(QStringLiteral("format")).toString()
                    == QLatin1String("MDict")) {
                    // MDX 释义本身是 HTML：走渲染管线（白名单清洗 + 链接/资源改写）
                    html += renderRichDefinition(match.entry.definition,
                                                 match.dictionaryId);
                } else {
                    html += match.entry.definition.toHtmlEscaped()
                                .replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
                }
            }
        } else if (!lastResult_->suggestions.isEmpty()) {
            html += QStringLiteral("<p>相近词条：</p><ul>");
            for (const QString& s : lastResult_->suggestions) {
                html += QStringLiteral("<li>%1</li>").arg(s.toHtmlEscaped());
            }
            html += QStringLiteral("</ul>");
        }

        // 精确未命中时回落全文检索：列出释义中出现该词的词条（#ft:<i> 锚点回查）
        if (!lastResult_->success) {
            ftHits_ = manager.fullTextSearch(query, 20, activeTagFilter_);
            if (!ftHits_.isEmpty()) {
                html += QStringLiteral("<hr/><p><b>全文命中（释义中出现该词）：</b></p><ul>");
                for (int i = 0; i < ftHits_.size(); ++i) {
                    const auto& entry = ftHits_.at(i);
                    html += QStringLiteral(
                                "<li><a href=\"#ft:%1\">%2</a> "
                                "<span style='color:gray'>— %3</span></li>")
                                .arg(i)
                                .arg(entry.word.toHtmlEscaped(),
                                     entry.metadata.value(QStringLiteral("dictionary"))
                                         .toString()
                                         .toHtmlEscaped());
                }
                html += QStringLiteral("</ul>");
            }
        }

        // 笔记展示闭环：该词有笔记就在释义末尾追加区块（Markdown 渲染）
        if (lastSuccess_) {
            const QString note = UnidictCore::DataStore::instance().getNote(
                lastResult_->entry.word);
            if (!note.isEmpty()) {
                html += QStringLiteral(
                            "<hr/><p><b>📝 笔记</b></p>"
                            "<p style='background-color:rgba(255,200,0,0.15)'>%1</p>")
                            .arg(renderNoteMarkdown(note));
            }
        }

        resultView_->setHtml(html);
        starButton_->setEnabled(lastSuccess_);
        noteAction_->setEnabled(lastSuccess_);

        refreshHistory();
    }

private:
    // ---------- 构建 ----------
    void buildToolbar(QVBoxLayout* root) {
        toolbar_ = new QToolBar(this);
        toolbar_->setMovable(false);
        QAction* title = toolbar_->addAction(QStringLiteral("Unidict"));
        title->setEnabled(false);
        clipboardAction_ = toolbar_->addAction(QStringLiteral("剪贴板取词"));
        clipboardAction_->setCheckable(true);
        hotkeyAction_ = toolbar_->addAction(QStringLiteral("全局热键"));
        hotkeyAction_->setCheckable(true);
        hotkeyAction_->setToolTip(QStringLiteral("注册系统级热键 Ctrl+Alt+U，任意应用中按下即唤起 Unidict"));
        if (!GlobalHotkeys::isPlatformSupported()) {
            hotkeyAction_->setEnabled(false);
            hotkeyAction_->setToolTip(QStringLiteral("当前平台暂不支持全局热键（仅 Windows 已实现）"));
        }
        // 分组（词典 tag）选择器：查询范围限定到选中分组的词典
        groupBox_ = new QComboBox(toolbar_);
        groupBox_->setToolTip(QStringLiteral("按分组过滤查询（分组在“词典管理”里设置）"));
        groupBox_->addItem(QStringLiteral("全部分组"));
        groupBox_->setMinimumContentsLength(12);
        toolbar_->addWidget(groupBox_);
        QWidget* spacer = new QWidget(toolbar_);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar_->addWidget(spacer);
        QAction* fontAction = toolbar_->addAction(QStringLiteral("释义字体…"));
        fontAction->setToolTip(QStringLiteral("调整释义阅读区字体与字号"));
        connect(fontAction, &QAction::triggered, this, [this] { pickResultFont(); });
        noteAction_ = toolbar_->addAction(QStringLiteral("笔记"));
        noteAction_->setToolTip(QStringLiteral("为当前查询词条添加/编辑学习笔记（清空保存即删除）"));
        noteAction_->setEnabled(false);
        connect(noteAction_, &QAction::triggered, this, [this] { editCurrentNote(); });
        QAction* pronAction = toolbar_->addAction(QStringLiteral("发音练习"));
        pronAction->setToolTip(QStringLiteral("跟读录音、回放与逐音素评分"));
        connect(pronAction, &QAction::triggered, this, [this] {
            // M4：词条音标随面板带入，评分据此换算目标音素。词典没有
            // 独立发音字段，音标按惯例写在释义开头（/ˈkæt/）——用
            // core/std 的提取器从释义里拿（主词条优先，空则依次看前
            // 几个释义源；不同词典标音格式不同）。提取失败退回 M2 跟读
            QString phonetics;
            const auto tryExtract = [](const QString& definition) {
                if (definition.isEmpty()) {
                    return QString();
                }
                const auto span = UnidictCoreStd::extract_phonetic_text(
                    definition.toStdString());
                return span.has_value() ? QString::fromStdString(*span)
                                        : QString();
            };
            if (lastResult_) {
                phonetics = tryExtract(lastResult_->entry.definition);
                const int n = qMin(lastResult_->matches.size(), 5);
                for (int i = 0; phonetics.isEmpty() && i < n; ++i) {
                    phonetics = tryExtract(
                        lastResult_->matches.at(i).entry.definition);
                }
            }
            PronunciationPanel panel(searchInput_->text(), phonetics, this);
            panel.exec();
        });
        themeAction_ = toolbar_->addAction(theme_.label());
        root->addWidget(toolbar_);
    }

    // ---------- 释义字体 ----------
    // 只动释义阅读区（resultView_），列表/工具栏跟随系统主题；设置记忆在
    // QSettings ui/resultFont（序列化 QFont），未设置时跟随默认
    void pickResultFont() {
        QFont current = resultView_->font();
        const QVariant saved = QSettings().value("ui/resultFont");
        if (saved.isValid()) {
            current.fromString(saved.toString());
        }
        bool ok = false;
        const QFont chosen = QFontDialog::getFont(&ok, current, this,
                                                  QStringLiteral("释义字体"));
        if (!ok) {
            return;
        }
        QSettings().setValue("ui/resultFont", chosen.toString());
        applyResultFont();
    }

    void applyResultFont() {
        const QVariant saved = QSettings().value("ui/resultFont");
        if (saved.isValid()) {
            QFont f;
            f.fromString(saved.toString());
            resultView_->setFont(f);
        }
    }

    // ---------- 词条笔记 ----------
    // 笔记正文按 GitHub 风格 Markdown 渲染（QTextDocument 转换后剥 body 壳
    // 嵌进释义 HTML）；转换失败/无结构时回落纯文本转义
    static QString renderNoteMarkdown(const QString& note) {
        QTextDocument doc;
        doc.setMarkdown(note, QTextDocument::MarkdownDialectGitHub);
        QString html = doc.toHtml();
        static const QRegularExpression bodyRe(
            QStringLiteral("<body[^>]*>(.*)</body>"),
            QRegularExpression::DotMatchesEverythingOption);
        const auto match = bodyRe.match(html);
        if (match.hasMatch() && !match.captured(1).trimmed().isEmpty()) {
            return match.captured(1);
        }
        return note.toHtmlEscaped().replace(QLatin1Char('\n'),
                                            QStringLiteral("<br/>"));
    }

    // 对当前查询成功的词条 upsert 笔记：已有则预填，清空保存即删除
    void editCurrentNote() {
        if (!lastSuccess_ || !lastResult_) {
            return;
        }
        const QString word = lastResult_->entry.word;
        auto& store = UnidictCore::DataStore::instance();
        const QString existing = store.getNote(word);
        bool ok = false;
        const QString text = QInputDialog::getMultiLineText(
            this, QStringLiteral("词条笔记"),
            QStringLiteral("「%1」的学习笔记，支持 Markdown（清空保存即删除）：")
                .arg(word),
            existing, &ok);
        if (!ok) {
            return;
        }
        store.setNote(word, text.trimmed());
        statusLabel_->setText(text.trimmed().isEmpty()
                                  ? QStringLiteral("已删除「%1」的笔记").arg(word)
                                  : QStringLiteral("已保存「%1」的笔记").arg(word));
    }

    QWidget* buildSearchBar() {
        auto* holder = new QWidget(this);
        auto* layout = new QHBoxLayout(holder);
        layout->setContentsMargins(12, 10, 12, 6);

        searchInput_ = new QLineEdit(holder);
        searchInput_->setPlaceholderText(QStringLiteral("输入单词或词组…"));
        searchInput_->setClearButtonEnabled(true);
        QFont f = searchInput_->font();
        f.setPointSize(f.pointSize() + 2);
        searchInput_->setFont(f);
        layout->addWidget(searchInput_, 1);

        completer_ = new QCompleter(&wordListModel_, searchInput_);
        completer_->setCaseSensitivity(Qt::CaseInsensitive);
        // model 即补全结果（textChanged 按需前缀查询填充），弹窗不再自行过滤
        completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        searchInput_->setCompleter(completer_);

        return holder;
    }

    QWidget* buildCentral() {
        auto* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);

        resultView_ = new ResultBrowser(splitter);
        resultView_->setPlaceholderText(
            QStringLiteral("释义会显示在这里。加载词典后输入单词开始查询。"));
        resultView_->setOpenExternalLinks(true);
        splitter->addWidget(resultView_);

        auto* sidePanel = new QWidget(splitter);
        auto* sideLayout = new QVBoxLayout(sidePanel);
        sideLayout->setContentsMargins(0, 0, 0, 0);

        sideTabs_ = new QTabWidget(sidePanel);
        sideTabs_->setTabPosition(QTabWidget::South);

        historyList_ = new QListWidget(sideTabs_);
        historyList_->setContextMenuPolicy(Qt::CustomContextMenu);
        sideTabs_->addTab(historyList_, QStringLiteral("历史"));

        // 收藏面板：顶部分组过滤（生词标签聚合），下列表项；与词典分组同语义
        auto* vocabPanel = new QWidget(sideTabs_);
        auto* vocabLayout = new QVBoxLayout(vocabPanel);
        vocabLayout->setContentsMargins(0, 0, 0, 0);
        vocabLayout->setSpacing(2);
        vocabGroupBox_ = new QComboBox(vocabPanel);
        vocabGroupBox_->addItem(QStringLiteral("全部分组"));
        connect(vocabGroupBox_, &QComboBox::activated, this, [this](int index) {
            vocabGroupFilter_ =
                index <= 0 ? QString() : vocabGroupBox_->itemText(index);
            refreshVocabulary();
        });
        vocabLayout->addWidget(vocabGroupBox_);

        vocabList_ = new QListWidget(vocabPanel);
        vocabList_->setContextMenuPolicy(Qt::CustomContextMenu);
        vocabLayout->addWidget(vocabList_, 1);
        sideTabs_->addTab(vocabPanel, QStringLiteral("收藏"));

        sideLayout->addWidget(sideTabs_);
        starButton_ = new QPushButton(QStringLiteral("☆ 收藏当前词"), sidePanel);
        starButton_->setEnabled(false);
        sideLayout->addWidget(starButton_);
        sidePanel->setMaximumWidth(320);

        splitter->addWidget(sidePanel);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 0);
        return splitter;
    }

    void buildStatusBar(QVBoxLayout* root) {
        auto* bar = new QStatusBar(this);
        statusLabel_ = new QLabel(this);
        bar->addWidget(statusLabel_, 1);
        auto* manageButton = new QPushButton(QStringLiteral("词典管理…"), this);
        manageButton->setFlat(true);
        bar->addPermanentWidget(manageButton);
        connect(manageButton, &QPushButton::clicked, this, &MainWindow::showDictionaryManager);
        root->addWidget(bar);
    }

    // ---------- 行为 ----------
    void wireActions() {
        connect(themeAction_, &QAction::triggered, this, [this] {
            theme_.apply((theme_.mode() + 1) % 3);
            themeAction_->setText(theme_.label());
        });

        // 剪贴板取词：开关记忆到 QSettings；取到词后弹窗并回填查询
        clipboardAction_->setChecked(QSettings().value("ui/clipboardLookup", false).toBool());
        if (clipboardAction_->isChecked()) {
            clipboardMonitor_.start();
        }
        connect(clipboardAction_, &QAction::toggled, this, [this](bool on) {
            QSettings().setValue("ui/clipboardLookup", on);
            if (on) {
                clipboardMonitor_.start();
            } else {
                clipboardMonitor_.stop();
            }
        });
        connect(&clipboardMonitor_, &ClipboardMonitor::wordDetected, this,
                [this](const QString& word) {
                    showNormal();
                    raise();
                    activateWindow();
                    runLookup(word);
                });

        // 全局热键（Windows）：Ctrl+Alt+U 唤起主窗口；开关记忆到 QSettings。
        // 恢复延迟到事件循环：注册需要主窗口的平台句柄已创建
        connect(hotkeyAction_, &QAction::toggled, this, [this](bool on) {
            QSettings().setValue("ui/globalHotkey", on);
            if (on) {
                if (!hotkeys_.registerHotkey(QStringLiteral("quickLookup"),
                                             QStringLiteral("Ctrl+Alt+U"))) {
                    QSignalBlocker blocker(hotkeyAction_);
                    hotkeyAction_->setChecked(false);
                    QSettings().setValue("ui/globalHotkey", false);
                    statusLabel_->setText(QStringLiteral("全局热键注册失败（快捷键可能被占用）"));
                }
            } else {
                hotkeys_.unregisterHotkey(QStringLiteral("quickLookup"));
            }
        });
        connect(&hotkeys_, &GlobalHotkeys::hotkeyPressed, this, [this](const QString& action) {
            if (action == QLatin1String("quickLookup")) {
                showNormal();
                raise();
                activateWindow();
                searchInput_->setFocus();
                searchInput_->selectAll();
            }
        });
        if (hotkeyAction_->isEnabled() &&
            QSettings().value("ui/globalHotkey", false).toBool()) {
            QTimer::singleShot(0, this, [this] { hotkeyAction_->setChecked(true); });
        }

        // 分组切换：查询/补全立即跟随新范围；选择记忆到 QSettings
        connect(groupBox_, &QComboBox::currentIndexChanged, this, [this](int index) {
            const QString tag = index > 0 ? groupBox_->itemText(index) : QString();
            activeTagFilter_ = tag.isEmpty() ? QStringList() : QStringList{tag};
            QSettings().setValue("ui/activeTagGroup", tag);
            refreshCompletions(searchInput_->text());
            if (!searchInput_->text().trimmed().isEmpty()) {
                runLookup(searchInput_->text());
            } else {
                refreshStatus();
            }
        });

        connect(searchInput_, &QLineEdit::returnPressed, this,
                [this] { runLookup(searchInput_->text()); });

        // 释义内锚点跳转：#w:<word>（MDX entry:/bword: 链接转换而来）回查词条；
        // #ft:<i> 全文命中回查。都复位 source 防止滚动跳动
        connect(resultView_, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
            const QString fragment = url.fragment();
            if (fragment.startsWith(QLatin1String("w:"))) {
                const QString word = fragment.mid(2);
                if (!word.isEmpty()) {
                    runLookup(word);
                }
                resultView_->setSource(QUrl());
                return;
            }
            if (!fragment.startsWith(QLatin1String("ft:"))) {
                return;
            }
            bool ok = false;
            const int index = fragment.mid(3).toInt(&ok);
            if (ok && index >= 0 && index < ftHits_.size()) {
                runLookup(ftHits_.at(index).word);
            }
            resultView_->setSource(QUrl());
        });

        connect(searchInput_, &QLineEdit::textChanged, this, [this](const QString& text) {
            starButton_->setEnabled(!text.trimmed().isEmpty() && lastSuccess_);
            noteAction_->setEnabled(!text.trimmed().isEmpty() && lastSuccess_);
            refreshCompletions(text);
        });

        // 补全选中（回车/点击弹窗项）即查询；抑制选中回填再次触发弹窗
        connect(completer_, qOverload<const QString&>(&QCompleter::activated), this,
                [this](const QString& word) {
                    suppressCompletionRefresh_ = true;
                    runLookup(word);
                    QTimer::singleShot(0, this, [this] { suppressCompletionRefresh_ = false; });
                });

        connect(starButton_, &QPushButton::clicked, this, [this] {
            if (!lastResult_ || !lastSuccess_) {
                return;
            }
            UnidictCore::DataStore::instance().addVocabularyItem(lastResult_->entry);
            UnidictCore::DataStore::instance().save();
            refreshVocabulary();
        });

        // 历史：双击回填查询；右键 置顶/删除
        connect(historyList_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
            if (item) {
                runLookup(item->data(Qt::UserRole).toString());
            }
        });
        connect(historyList_, &QListWidget::customContextMenuRequested, this,
                [this](const QPoint& pos) {
                    QListWidgetItem* item = historyList_->itemAt(pos);
                    if (!item) {
                        return;
                    }
                    QMenu menu(this);
                    QAction* pin = menu.addAction(QStringLiteral("置顶/取消置顶"));
                    QAction* remove = menu.addAction(QStringLiteral("删除该条"));
                    QAction* chosen = menu.exec(historyList_->mapToGlobal(pos));
                    if (chosen == nullptr) {
                        return;
                    }
                    auto& manager = UnidictCore::DictionaryManager::instance();
                    const QString query = item->data(Qt::UserRole).toString();
                    if (chosen == pin) {
                        manager.setSearchHistoryPinned(query, !item->data(Qt::UserRole + 1).toBool());
                    } else if (chosen == remove) {
                        manager.removeSearchHistoryItem(query);
                    }
                    refreshHistory();
                });

        // 收藏：双击查询；右键设置标签 / 移除
        connect(vocabList_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
            if (item) {
                runLookup(item->data(Qt::UserRole).toString());
            }
        });
        connect(vocabList_, &QListWidget::customContextMenuRequested, this,
                [this](const QPoint& pos) {
                    QListWidgetItem* item = vocabList_->itemAt(pos);
                    if (!item) {
                        return;
                    }
                    const QString word = item->data(Qt::UserRole).toString();
                    QMenu menu(this);
                    QAction* tagAction = menu.addAction(QStringLiteral("设置标签…"));
                    QAction* remove = menu.addAction(QStringLiteral("移除收藏"));
                    QAction* chosen = menu.exec(vocabList_->mapToGlobal(pos));
                    if (chosen == tagAction) {
                        const QStringList current =
                            item->data(Qt::UserRole + 1).toStringList();
                        bool ok = false;
                        const QString text = QInputDialog::getText(
                            this, QStringLiteral("设置标签"),
                            QStringLiteral("逗号分隔，清空即移除全部标签："),
                            QLineEdit::Normal, current.join(QStringLiteral(", ")), &ok);
                        if (!ok) {
                            return;
                        }
                        QStringList tags;
                        for (const QString& t : text.split(',')) {
                            const QString trimmed = t.trimmed();
                            if (!trimmed.isEmpty()) {
                                tags.append(trimmed);
                            }
                        }
                        if (!UnidictCore::DataStore::instance()
                                 .setVocabularyItemTags(word, tags)) {
                            QMessageBox::warning(this, QStringLiteral("设置标签"),
                                                 QStringLiteral("词条已不在收藏中：%1").arg(word));
                        }
                        refreshVocabulary();
                    } else if (chosen == remove) {
                        UnidictCore::DataStore::instance().removeVocabularyItem(word);
                        UnidictCore::DataStore::instance().save();
                        refreshVocabulary();
                    }
                });
    }

    // ---------- 刷新 ----------
    void refreshAll() {
        refreshGroupBox();
        refreshHistory();
        refreshVocabulary();
        refreshStatus();
    }

    // 分组下拉框重建：聚合全部词典的 tags（去重保序），恢复记忆的选择；
    // 记忆的分组词典全被移除后回落“全部分组”
    void refreshGroupBox() {
        QStringList tags;
        for (const auto& info :
             UnidictCore::DictionaryManager::instance().getLoadedDictionaryInfos()) {
            for (const QString& tag : info.tags) {
                if (!tag.isEmpty() && !tags.contains(tag)) {
                    tags.append(tag);
                }
            }
        }

        const QSignalBlocker blocker(groupBox_);
        groupBox_->clear();
        groupBox_->addItem(QStringLiteral("全部分组"));
        groupBox_->addItems(tags);

        QString remembered = QSettings().value("ui/activeTagGroup").toString();
        if (!remembered.isEmpty() && !tags.contains(remembered)) {
            remembered.clear();
            QSettings().setValue("ui/activeTagGroup", QString());
        }
        activeTagFilter_ = remembered.isEmpty() ? QStringList() : QStringList{remembered};
        groupBox_->setCurrentIndex(remembered.isEmpty() ? 0 : tags.indexOf(remembered) + 1);
    }

    // QCompleter 前缀补全：按需查询前缀索引（prefixSearch 二分），替代
    // 启动时全量拉词表——大词典免 20 万词全载与截断
    void refreshCompletions(const QString& text) {
        if (suppressCompletionRefresh_) {
            return;
        }
        const QString query = text.trimmed();
        QStringList words;
        if (!query.isEmpty() && query.size() <= 64) {
            words = UnidictCore::DictionaryManager::instance()
                        .prefixSearch(query, 20, activeTagFilter_);
        }
        wordListModel_.setStringList(words);
        if (!words.isEmpty() && searchInput_->hasFocus()) {
            completer_->complete();
        }
    }

    void refreshHistory() {
        const QString current = historyList_->currentItem()
                                    ? historyList_->currentItem()->data(Qt::UserRole).toString()
                                    : QString();
        historyList_->clear();
        const auto items = UnidictCore::DictionaryManager::instance().getSearchHistory(50);
        for (const auto& entry : items) {
            const QString pinMark = entry.pinned ? QStringLiteral("📌 ") : QString();
            const QString text = entry.success
                ? QStringLiteral("%1%2 — %3").arg(pinMark, entry.query, entry.dictionaryName)
                : QStringLiteral("%1%2（未找到）").arg(pinMark, entry.query);
            auto* item = new QListWidgetItem(text, historyList_);
            item->setData(Qt::UserRole, entry.query);
            item->setData(Qt::UserRole + 1, entry.pinned);
            if (!entry.success) {
                item->setForeground(QBrush(Qt::gray));
            }
            if (entry.query == current) {
                item->setSelected(true);
            }
        }
    }

    void refreshVocabulary() {
        const auto items = UnidictCore::DataStore::instance().getVocabularyMeta();

        // 分组下拉重建：全部生词标签聚合（去重保序），保持当前过滤；
        // 记忆的过滤标签已不存在（标签被清/词被移除）则回落“全部分组”
        QStringList tags;
        for (const auto& meta : items) {
            const QVariantList itemTags =
                meta.toMap().value(QStringLiteral("tags")).toList();
            for (const QVariant& tag : itemTags) {
                const QString t = tag.toString();
                if (!t.isEmpty() && !tags.contains(t)) {
                    tags.append(t);
                }
            }
        }
        if (!vocabGroupFilter_.isEmpty() && !tags.contains(vocabGroupFilter_)) {
            vocabGroupFilter_.clear();
        }
        vocabGroupBox_->blockSignals(true);
        vocabGroupBox_->clear();
        vocabGroupBox_->addItem(QStringLiteral("全部分组"));
        vocabGroupBox_->addItems(tags);
        vocabGroupBox_->setCurrentIndex(vocabGroupFilter_.isEmpty()
                                            ? 0
                                            : vocabGroupBox_->findText(vocabGroupFilter_));
        vocabGroupBox_->blockSignals(false);

        vocabList_->clear();
        for (const auto& meta : items) {
            const QVariantMap m = meta.toMap();
            const QString word = m.value(QStringLiteral("word")).toString();
            QStringList itemTags;
            for (const QVariant& tag : m.value(QStringLiteral("tags")).toList()) {
                itemTags.append(tag.toString());
            }
            if (!vocabGroupFilter_.isEmpty() && !itemTags.contains(vocabGroupFilter_)) {
                continue;
            }
            QString label = word;
            if (!itemTags.isEmpty()) {
                label += QStringLiteral("  ·  %1")
                             .arg(itemTags.join(QStringLiteral("/")));
            }
            auto* item = new QListWidgetItem(label, vocabList_);
            item->setData(Qt::UserRole, word);
            item->setData(Qt::UserRole + 1, itemTags);
            item->setToolTip(
                m.value(QStringLiteral("definition")).toString().left(200));
        }
    }

    void refreshStatus() {
        const int count = (int)UnidictCore::DictionaryManager::instance()
                              .getLoadedDictionaryInfos()
                              .size();
        statusLabel_->setText(QStringLiteral("%1 部词典 · 就绪").arg(count));
    }

    // ---------- 词典管理对话框 ----------
    void showDictionaryManager() {
        auto& manager = UnidictCore::DictionaryManager::instance();

        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("词典管理"));
        dialog.resize(560, 420);
        auto* layout = new QVBoxLayout(&dialog);

        auto* list = new QListWidget(&dialog);
        layout->addWidget(list);

        auto refreshList = [&] {
            list->clear();
            const auto infos = manager.getLoadedDictionaryInfos();
            for (const auto& info : infos) {
                QString label = QStringLiteral("%1  (%2 · %3 词")
                                    .arg(info.name, info.format)
                                    .arg(info.wordCount);
                if (!info.tags.isEmpty()) {
                    label += QStringLiteral(" · 分组: %1").arg(info.tags.join(QStringLiteral("/")));
                }
                label += QLatin1Char(')');
                auto* item = new QListWidgetItem(label, list);
                item->setData(Qt::UserRole, info.id);
                item->setToolTip(info.filePath);
                item->setForeground(info.enabled ? QBrush() : QBrush(Qt::gray));
            }
            // 加载失败的词典（损坏隔离/文件丢失）：诊断 + 重试/移除入口
            const auto failures = manager.getFailedDictionaries();
            for (const auto& failure : failures) {
                auto* item = new QListWidgetItem(
                    QStringLiteral("⚠ %1（无法加载）：%2")
                        .arg(QFileInfo(failure.filePath).fileName(), failure.reason),
                    list);
                item->setData(Qt::UserRole,
                              QStringLiteral("failed:%1").arg(failure.filePath));
                item->setToolTip(failure.filePath);
                item->setForeground(QBrush(QColor(0xc0, 0x3a, 0x2b)));
            }
        };
        refreshList();

        // 失败行的 UserRole 带 failed: 前缀——命中则返回路径，否则空
        auto failedPathOf = [](const QListWidgetItem* item) -> QString {
            const QString tag = item ? item->data(Qt::UserRole).toString() : QString();
            return tag.startsWith(QLatin1String("failed:")) ? tag.mid(7) : QString();
        };

        auto* buttons = new QHBoxLayout;
        auto* addFile = new QPushButton(QStringLiteral("添加词典文件…"), &dialog);
        auto* addDir = new QPushButton(QStringLiteral("添加词典目录…"), &dialog);
        auto* toggle = new QPushButton(QStringLiteral("启用/禁用"), &dialog);
        auto* up = new QPushButton(QStringLiteral("上移"), &dialog);
        auto* down = new QPushButton(QStringLiteral("下移"), &dialog);
        auto* remove = new QPushButton(QStringLiteral("移除"), &dialog);
        auto* tags = new QPushButton(QStringLiteral("设置分组标签…"), &dialog);
        auto* retry = new QPushButton(QStringLiteral("重试加载"), &dialog);
        for (QPushButton* b : {addFile, addDir, toggle, up, down, remove, tags, retry}) {
            buttons->addWidget(b);
        }
        layout->addLayout(buttons);

        connect(addFile, &QPushButton::clicked, &dialog, [&] {
            const QString path = QFileDialog::getOpenFileName(
                &dialog, QStringLiteral("选择词典文件"), QString(),
                QStringLiteral("词典文件 (*.ifo *.mdx *.json *.epub);;所有文件 (*)"));
            if (!path.isEmpty() && !manager.addDictionary(path)) {
                QMessageBox::warning(&dialog, QStringLiteral("Unidict"),
                                     manager.lastError().isEmpty()
                                         ? QStringLiteral("无法加载该词典")
                                         : manager.lastError());
            }
            refreshList();
        });
        connect(addDir, &QPushButton::clicked, &dialog, [&] {
            const QString dir =
                QFileDialog::getExistingDirectory(&dialog, QStringLiteral("选择词典目录"));
            if (!dir.isEmpty()) {
                manager.addDictionariesFromDirectory(dir);
                refreshList();
            }
        });
        connect(toggle, &QPushButton::clicked, &dialog, [&] {
            if (auto* item = list->currentItem()) {
                if (!failedPathOf(item).isEmpty()) {
                    return; // 失败行没有启用/禁用语义（还没加载）
                }
                const QString id = item->data(Qt::UserRole).toString();
                for (const auto& info : manager.getLoadedDictionaryInfos()) {
                    if (info.id == id) {
                        manager.setDictionaryEnabled(id, !info.enabled);
                        break;
                    }
                }
                refreshList();
            }
        });
        connect(up, &QPushButton::clicked, &dialog, [&] {
            if (auto* item = list->currentItem(); item && failedPathOf(item).isEmpty()) {
                manager.moveDictionaryUp(item->data(Qt::UserRole).toString());
                refreshList();
            }
        });
        connect(down, &QPushButton::clicked, &dialog, [&] {
            if (auto* item = list->currentItem(); item && failedPathOf(item).isEmpty()) {
                manager.moveDictionaryDown(item->data(Qt::UserRole).toString());
                refreshList();
            }
        });
        connect(remove, &QPushButton::clicked, &dialog, [&] {
            if (auto* item = list->currentItem()) {
                // 失败行走“遗忘”（从隔离区移除，不再重试）；正常行按 id 移除
                const QString failedPath = failedPathOf(item);
                if (!failedPath.isEmpty()) {
                    manager.forgetFailedDictionary(failedPath);
                } else {
                    manager.removeDictionary(item->data(Qt::UserRole).toString());
                }
                refreshList();
            }
        });
        connect(retry, &QPushButton::clicked, &dialog, [&] {
            if (auto* item = list->currentItem()) {
                const QString failedPath = failedPathOf(item);
                if (failedPath.isEmpty()) {
                    return;
                }
                if (!manager.retryFailedDictionary(failedPath)) {
                    QMessageBox::warning(&dialog, QStringLiteral("Unidict"),
                                         manager.lastError().isEmpty()
                                             ? QStringLiteral("重试失败：词典仍无法加载")
                                             : manager.lastError());
                }
                refreshList();
            }
        });
        connect(tags, &QPushButton::clicked, &dialog, [&] {
            if (auto* item = list->currentItem()) {
                if (!failedPathOf(item).isEmpty()) {
                    return; // 失败行没有分组语义
                }
                const QString id = item->data(Qt::UserRole).toString();
                QString current;
                for (const auto& info : manager.getLoadedDictionaryInfos()) {
                    if (info.id == id) {
                        current = info.tags.join(QStringLiteral(","));
                        break;
                    }
                }
                bool ok = false;
                const QString text = QInputDialog::getText(
                    &dialog, QStringLiteral("分组标签"),
                    QStringLiteral("逗号分隔，留空清除（例：en,zh）"),
                    QLineEdit::Normal, current, &ok);
                if (ok) {
                    QStringList parsed;
                    for (QString tag : text.split(QLatin1Char(','))) {
                        tag = tag.trimmed();
                        if (!tag.isEmpty()) {
                            parsed.append(tag);
                        }
                    }
                    manager.setDictionaryTags(id, parsed);
                    refreshList();
                    refreshGroupBox();
                }
            }
        });

        connect(&dialog, &QDialog::finished, this, [this](int) { refreshAll(); });

        dialog.exec();
    }

    QApplication& app_;
    ThemeManager theme_;
    ClipboardMonitor clipboardMonitor_;
    GlobalHotkeys hotkeys_;

    QToolBar* toolbar_ = nullptr;
    QAction* themeAction_ = nullptr;
    QAction* noteAction_ = nullptr;
    QAction* clipboardAction_ = nullptr;
    QAction* hotkeyAction_ = nullptr;
    QComboBox* groupBox_ = nullptr;
    QStringList activeTagFilter_;   // 非空时查询限定到该分组的词典
    QLineEdit* searchInput_ = nullptr;
    QCompleter* completer_ = nullptr;
    QStringListModel wordListModel_;
    ResultBrowser* resultView_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
    bool trayHintShown_ = false;
    QTabWidget* sideTabs_ = nullptr;
    QListWidget* historyList_ = nullptr;
    QListWidget* vocabList_ = nullptr;
    QComboBox* vocabGroupBox_ = nullptr;
    QString vocabGroupFilter_;
    QPushButton* starButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    std::optional<UnidictCore::LookupResult> lastResult_;
    bool lastSuccess_ = false;
    QVector<UnidictCore::DictionaryEntry> ftHits_;   // 最近一次全文命中（锚点回查用）
    bool suppressCompletionRefresh_ = false;         // 补全选中回填期间抑制再弹窗
};

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Unidict"));
    QApplication::setApplicationVersion(QStringLiteral("1.0"));

    loadDefaultDictionaryLocations();

    MainWindow window(app);
    window.show();

    // 命令行参数里的词直接查询：unidict_gui hello
    const QStringList args = app.arguments().mid(1);
    if (!args.isEmpty()) {
        window.runLookup(args.last());
    }

    return QApplication::exec();
}
