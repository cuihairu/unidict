// Unidict GUI —— Qt Widgets 壳。
// 结构按 docs/gui-ui-structure.md：顶部搜索框(QCompleter 补全) + 释义主区
// + 历史/收藏侧栏 + 状态栏/词典管理对话框，深浅色三态切换。

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QCompleter>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QStringListModel>
#include <QStyle>
#include <QStyleHints>
#include <QTabWidget>
#include <QTextBrowser>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <optional>

#include "clipboard_monitor.h"
#include "data_store.h"
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
        refreshAll();
    }

protected:
    // 关窗时保存窗口几何（位置 + 尺寸），下次启动恢复
    void closeEvent(QCloseEvent* event) override {
        QSettings().setValue("ui/windowGeometry", saveGeometry());
        QWidget::closeEvent(event);
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
        lastResult_ = manager.searchWord(query);
        lastSuccess_ = lastResult_->success;
        statusLabel_->setText(lastResult_->message);

        QString html;
        if (lastResult_->success) {
            html += QStringLiteral("<h2>%1</h2>").arg(lastResult_->entry.word.toHtmlEscaped());
            for (const auto& match : lastResult_->matches) {
                html += QStringLiteral("<hr/><p style='color:gray'><small>%1</small></p>")
                            .arg(match.dictionaryName.toHtmlEscaped());
                html += match.entry.definition.toHtmlEscaped()
                            .replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
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
            ftHits_ = manager.fullTextSearch(query, 20);
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

        resultView_->setHtml(html);
        starButton_->setEnabled(lastSuccess_);

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
        QWidget* spacer = new QWidget(toolbar_);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar_->addWidget(spacer);
        themeAction_ = toolbar_->addAction(theme_.label());
        root->addWidget(toolbar_);
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
        completer_->setCompletionMode(QCompleter::PopupCompletion);
        completer_->setFilterMode(Qt::MatchContains);
        searchInput_->setCompleter(completer_);

        return holder;
    }

    QWidget* buildCentral() {
        auto* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);

        resultView_ = new QTextBrowser(splitter);
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

        vocabList_ = new QListWidget(sideTabs_);
        vocabList_->setContextMenuPolicy(Qt::CustomContextMenu);
        sideTabs_->addTab(vocabList_, QStringLiteral("收藏"));

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

        connect(searchInput_, &QLineEdit::returnPressed, this,
                [this] { runLookup(searchInput_->text()); });

        // 全文命中锚点（#ft:<i>）：回查对应词条；复位 source 防止滚动跳动
        connect(resultView_, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
            const QString fragment = url.fragment();
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

        // 收藏：双击查询；右键移除
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
                    QMenu menu(this);
                    QAction* remove = menu.addAction(QStringLiteral("移除收藏"));
                    if (menu.exec(vocabList_->mapToGlobal(pos)) != remove) {
                        return;
                    }
                    UnidictCore::DataStore::instance().removeVocabularyItem(
                        item->data(Qt::UserRole).toString());
                    UnidictCore::DataStore::instance().save();
                    refreshVocabulary();
                });
    }

    // ---------- 刷新 ----------
    void refreshAll() {
        refreshWordListModel();
        refreshHistory();
        refreshVocabulary();
        refreshStatus();
    }

    void refreshWordListModel() {
        const QStringList words = UnidictCore::DictionaryManager::instance().getAllWords();
        wordListModel_.setStringList(words);
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
        vocabList_->clear();
        const auto items = UnidictCore::DataStore::instance().getVocabulary();
        for (const auto& entry : items) {
            auto* item = new QListWidgetItem(entry.word, vocabList_);
            item->setData(Qt::UserRole, entry.word);
            item->setToolTip(entry.definition.left(200));
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
                auto* item = new QListWidgetItem(
                    QStringLiteral("%1  (%2 · %3 词)").arg(info.name, info.format).arg(info.wordCount),
                    list);
                item->setData(Qt::UserRole, info.id);
                item->setToolTip(info.filePath);
                item->setForeground(info.enabled ? QBrush() : QBrush(Qt::gray));
            }
        };
        refreshList();

        auto* buttons = new QHBoxLayout;
        auto* addFile = new QPushButton(QStringLiteral("添加词典文件…"), &dialog);
        auto* addDir = new QPushButton(QStringLiteral("添加词典目录…"), &dialog);
        auto* toggle = new QPushButton(QStringLiteral("启用/禁用"), &dialog);
        auto* up = new QPushButton(QStringLiteral("上移"), &dialog);
        auto* down = new QPushButton(QStringLiteral("下移"), &dialog);
        auto* remove = new QPushButton(QStringLiteral("移除"), &dialog);
        for (QPushButton* b : {addFile, addDir, toggle, up, down, remove}) {
            buttons->addWidget(b);
        }
        layout->addLayout(buttons);

        connect(addFile, &QPushButton::clicked, &dialog, [&] {
            const QString path = QFileDialog::getOpenFileName(
                &dialog, QStringLiteral("选择词典文件"), QString(),
                QStringLiteral("词典文件 (*.ifo *.mdx);;所有文件 (*)"));
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
            if (auto* item = list->currentItem()) {
                manager.moveDictionaryUp(item->data(Qt::UserRole).toString());
                refreshList();
            }
        });
        connect(down, &QPushButton::clicked, &dialog, [&] {
            if (auto* item = list->currentItem()) {
                manager.moveDictionaryDown(item->data(Qt::UserRole).toString());
                refreshList();
            }
        });
        connect(remove, &QPushButton::clicked, &dialog, [&] {
            if (auto* item = list->currentItem()) {
                manager.removeDictionary(item->data(Qt::UserRole).toString());
                refreshList();
            }
        });

        connect(&dialog, &QDialog::finished, this, [this](int) { refreshAll(); });

        dialog.exec();
    }

    QApplication& app_;
    ThemeManager theme_;
    ClipboardMonitor clipboardMonitor_;

    QToolBar* toolbar_ = nullptr;
    QAction* themeAction_ = nullptr;
    QAction* clipboardAction_ = nullptr;
    QLineEdit* searchInput_ = nullptr;
    QCompleter* completer_ = nullptr;
    QStringListModel wordListModel_;
    QTextBrowser* resultView_ = nullptr;
    QTabWidget* sideTabs_ = nullptr;
    QListWidget* historyList_ = nullptr;
    QListWidget* vocabList_ = nullptr;
    QPushButton* starButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    std::optional<UnidictCore::LookupResult> lastResult_;
    bool lastSuccess_ = false;
    QVector<UnidictCore::DictionaryEntry> ftHits_;   // 最近一次全文命中（锚点回查用）
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
