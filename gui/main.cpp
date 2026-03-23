#include <QApplication>
#include <QBrush>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

#include "unidict_core.h"

namespace {

void loadDefaultDictionaryLocations() {
    auto& manager = UnidictCore::DictionaryManager::instance();
    manager.loadState();
    const QString envDir = qEnvironmentVariable("UNIDICT_DICT_DIR");
    if (!envDir.isEmpty()) {
        manager.addDictionariesFromDirectory(envDir);
    }

    const QString localDir = QDir(QApplication::applicationDirPath()).filePath("dictionaries");
    if (QFileInfo::exists(localDir)) {
        manager.addDictionariesFromDirectory(localDir);
    }
}

QString buildDictionaryLabel(const UnidictCore::DictionaryInfo& info) {
    return QString("#%1 %2\n%3 words | %4 | %5")
        .arg(info.priority)
        .arg(info.name)
        .arg(info.wordCount)
        .arg(info.format)
        .arg(info.enabled ? "enabled" : "disabled");
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Unidict");
    QApplication::setApplicationVersion("1.0");

    auto& manager = UnidictCore::DictionaryManager::instance();
    loadDefaultDictionaryLocations();

    QWidget window;
    window.setWindowTitle("Unidict");
    window.resize(1100, 720);

    auto *rootLayout = new QVBoxLayout(&window);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    auto *title = new QLabel("Unidict");
    title->setStyleSheet("font-size: 28px; font-weight: 700;");
    auto *subtitle = new QLabel("Open-source offline dictionary workspace");
    subtitle->setStyleSheet("color: #666;");

    auto *headerLayout = new QVBoxLayout();
    headerLayout->setSpacing(2);
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    rootLayout->addLayout(headerLayout);

    auto *splitter = new QSplitter();
    splitter->setChildrenCollapsible(false);
    rootLayout->addWidget(splitter, 1);

    auto *sidebar = new QWidget();
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(10);

    auto *dictionaryBox = new QGroupBox("Dictionaries");
    auto *dictionaryLayout = new QVBoxLayout(dictionaryBox);

    auto *importFileButton = new QPushButton("Import File");
    auto *importDirButton = new QPushButton("Import Folder");
    auto *toggleEnabledButton = new QPushButton("Enable / Disable");
    auto *moveUpButton = new QPushButton("Move Up");
    auto *moveDownButton = new QPushButton("Move Down");
    auto *removeButton = new QPushButton("Remove");
    auto *applyTagsButton = new QPushButton("Apply Tags");
    auto *dictionaryFilterInput = new QLineEdit();
    auto *dictionaryTagsInput = new QLineEdit();
    auto *dictionaryScopeFilter = new QComboBox();
    auto *dictionaryList = new QListWidget();
    auto *dictionaryStatus = new QLabel();
    dictionaryStatus->setWordWrap(true);
    dictionaryStatus->setStyleSheet("color: #666;");
    dictionaryFilterInput->setPlaceholderText("Filter dictionaries by name, path, or format");
    dictionaryTagsInput->setPlaceholderText("Comma-separated tags for selected dictionary");
    dictionaryScopeFilter->addItem("All");
    dictionaryScopeFilter->addItem("Enabled Only");
    dictionaryScopeFilter->addItem("Disabled Only");

    dictionaryLayout->addWidget(importFileButton);
    dictionaryLayout->addWidget(importDirButton);
    dictionaryLayout->addWidget(toggleEnabledButton);
    dictionaryLayout->addWidget(moveUpButton);
    dictionaryLayout->addWidget(moveDownButton);
    dictionaryLayout->addWidget(removeButton);
    dictionaryLayout->addWidget(dictionaryTagsInput);
    dictionaryLayout->addWidget(applyTagsButton);
    dictionaryLayout->addWidget(dictionaryFilterInput);
    dictionaryLayout->addWidget(dictionaryScopeFilter);
    dictionaryLayout->addWidget(dictionaryList, 1);
    dictionaryLayout->addWidget(dictionaryStatus);
    sidebarLayout->addWidget(dictionaryBox, 1);

    auto *historyBox = new QGroupBox("Recent Searches");
    auto *historyLayout = new QVBoxLayout(historyBox);
    auto *clearHistoryButton = new QPushButton("Clear History");
    auto *togglePinnedHistoryButton = new QPushButton("Pin / Unpin");
    auto *removeHistoryButton = new QPushButton("Remove Selected");
    auto *exportHistoryButton = new QPushButton("Export History");
    auto *importHistoryButton = new QPushButton("Import History");
    auto *historyFilterInput = new QLineEdit();
    auto *historyList = new QListWidget();
    historyFilterInput->setPlaceholderText("Filter history by query or dictionary");
    historyLayout->addWidget(clearHistoryButton);
    historyLayout->addWidget(togglePinnedHistoryButton);
    historyLayout->addWidget(removeHistoryButton);
    historyLayout->addWidget(exportHistoryButton);
    historyLayout->addWidget(importHistoryButton);
    historyLayout->addWidget(historyFilterInput);
    historyLayout->addWidget(historyList, 1);
    sidebarLayout->addWidget(historyBox, 1);

    auto *detailsBox = new QGroupBox("Dictionary Details");
    auto *detailsLayout = new QVBoxLayout(detailsBox);
    auto *detailsView = new QTextBrowser();
    detailsView->setOpenExternalLinks(false);
    detailsView->setPlaceholderText("Select a dictionary to inspect its metadata.");
    detailsLayout->addWidget(detailsView);
    sidebarLayout->addWidget(detailsBox, 1);

    auto *settingsBox = new QGroupBox("Workspace");
    auto *settingsLayout = new QVBoxLayout(settingsBox);
    auto *workspaceInfoView = new QTextBrowser();
    auto *saveWorkspaceButton = new QPushButton("Save Workspace");
    auto *reloadWorkspaceButton = new QPushButton("Reload Workspace");
    workspaceInfoView->setOpenExternalLinks(false);
    workspaceInfoView->setPlaceholderText("Workspace details appear here.");
    settingsLayout->addWidget(saveWorkspaceButton);
    settingsLayout->addWidget(reloadWorkspaceButton);
    settingsLayout->addWidget(workspaceInfoView);
    sidebarLayout->addWidget(settingsBox, 1);

    auto *workspace = new QWidget();
    auto *workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(10);

    auto *searchBox = new QGroupBox("Lookup");
    auto *searchLayout = new QGridLayout(searchBox);

    auto *searchInput = new QLineEdit();
    searchInput->setPlaceholderText("Search a word");
    auto *searchButton = new QPushButton("Search");
    auto *statusLabel = new QLabel("Import a StarDict dictionary to begin.");
    statusLabel->setWordWrap(true);
    statusLabel->setStyleSheet("color: #666;");

    searchLayout->addWidget(searchInput, 0, 0);
    searchLayout->addWidget(searchButton, 0, 1);
    searchLayout->addWidget(statusLabel, 1, 0, 1, 2);
    workspaceLayout->addWidget(searchBox);

    auto *resultBox = new QGroupBox("Result");
    auto *resultLayout = new QVBoxLayout(resultBox);
    auto *resultView = new QTextBrowser();
    resultView->setOpenExternalLinks(true);
    resultView->setPlaceholderText("Definition appears here.");
    resultLayout->addWidget(resultView);
    workspaceLayout->addWidget(resultBox, 1);

    auto *suggestionBox = new QGroupBox("Suggestions");
    auto *suggestionLayout = new QVBoxLayout(suggestionBox);
    auto *suggestionList = new QListWidget();
    suggestionLayout->addWidget(suggestionList);
    workspaceLayout->addWidget(suggestionBox);

    splitter->addWidget(sidebar);
    splitter->addWidget(workspace);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    auto refreshDictionaryList = [&]() {
        dictionaryList->clear();
        const auto infos = manager.getLoadedDictionaryInfos();
        const QString filterText = dictionaryFilterInput->text().trimmed().toLower();
        const int scopeIndex = dictionaryScopeFilter->currentIndex();
        int visibleCount = 0;
        for (const auto& info : infos) {
            if (scopeIndex == 1 && !info.enabled) {
                continue;
            }
            if (scopeIndex == 2 && info.enabled) {
                continue;
            }

            const QString haystack =
                QString("%1\n%2\n%3\n%4\n%5")
                    .arg(info.name, info.filePath, info.format, info.description, info.tags.join(", "))
                    .toLower();
            if (!filterText.isEmpty() && !haystack.contains(filterText)) {
                continue;
            }

            auto *item = new QListWidgetItem(buildDictionaryLabel(info));
            item->setToolTip(info.filePath);
            item->setData(Qt::UserRole, info.id);
            item->setForeground(info.enabled ? QBrush() : QBrush(Qt::gray));
            dictionaryList->addItem(item);
            ++visibleCount;
        }

        if (infos.isEmpty()) {
            dictionaryStatus->setText("No dictionaries loaded. Supported now: StarDict (.ifo/.idx/.dict).");
        } else {
            int enabledCount = 0;
            for (const auto& info : infos) {
                if (info.enabled) {
                    ++enabledCount;
                }
            }
            dictionaryStatus->setText(
                QString("%1 dictionaries loaded, %2 enabled, %3 visible.")
                    .arg(infos.size())
                    .arg(enabledCount)
                    .arg(visibleCount));
        }
    };

    auto refreshDictionaryDetails = [&]() {
        auto *item = dictionaryList->currentItem();
        if (item == nullptr) {
            detailsView->setPlainText("Select a dictionary to inspect its metadata.");
            return;
        }

        const QString dictionaryId = item->data(Qt::UserRole).toString();
        const auto infos = manager.getLoadedDictionaryInfos();
        for (const auto& info : infos) {
            if (info.id != dictionaryId) {
                continue;
            }

            QString details;
            details += QString("Name: %1\n").arg(info.name);
            details += QString("Format: %1\n").arg(info.format);
            details += QString("Priority: %1\n").arg(info.priority);
            details += QString("Enabled: %1\n").arg(info.enabled ? "Yes" : "No");
            details += QString("Word Count: %1\n").arg(info.wordCount);
            details += QString("Path: %1\n").arg(info.filePath);
            details += QString("Tags: %1\n").arg(info.tags.isEmpty() ? "-" : info.tags.join(", "));
            if (!info.description.trimmed().isEmpty()) {
                details += QString("\nDescription:\n%1").arg(info.description.trimmed());
            }
            detailsView->setPlainText(details);
            dictionaryTagsInput->setText(info.tags.join(", "));
            return;
        }

        detailsView->setPlainText("Dictionary metadata is unavailable.");
        dictionaryTagsInput->clear();
    };

    auto refreshWorkspaceInfo = [&]() {
        const auto infos = manager.getLoadedDictionaryInfos();
        const auto history = manager.getSearchHistory();
        int enabledCount = 0;
        for (const auto& info : infos) {
            if (info.enabled) {
                ++enabledCount;
            }
        }

        QString details;
        details += QString("State File:\n%1\n\n").arg(manager.defaultStateFilePath());
        details += QString("Loaded Dictionaries: %1\n").arg(infos.size());
        details += QString("Enabled Dictionaries: %1\n").arg(enabledCount);
        details += QString("History Items: %1\n").arg(history.size());
        details += QString("\nWorkspace state is auto-saved after dictionary and history changes.");
        workspaceInfoView->setPlainText(details);
    };

    auto refreshHistoryList = [&]() {
        historyList->clear();
        const auto items = manager.getSearchHistory(20);
        const QString filterText = historyFilterInput->text().trimmed().toLower();
        for (const auto& entry : items) {
            const QString haystack = QString("%1\n%2").arg(entry.query, entry.dictionaryName).toLower();
            if (!filterText.isEmpty() && !haystack.contains(filterText)) {
                continue;
            }

            auto *item = new QListWidgetItem(
                entry.success
                    ? QString("%1%2\n%3").arg(entry.pinned ? "[Pinned] " : "", entry.query, entry.dictionaryName)
                    : QString("%1%2\nnot found").arg(entry.pinned ? "[Pinned] " : "", entry.query));
            item->setData(Qt::UserRole, entry.query);
            item->setData(Qt::UserRole + 1, entry.pinned);
            if (!entry.success) {
                item->setForeground(QBrush(Qt::gray));
            }
            historyList->addItem(item);
        }
    };

    auto runLookup = [&]() {
        const auto result = UnidictCore::lookupWord(searchInput->text());
        statusLabel->setText(result.message);
        resultView->setPlainText(UnidictCore::formatLookupResult(result));

        suggestionList->clear();
        for (const QString& suggestion : result.suggestions) {
            suggestionList->addItem(suggestion);
        }
        refreshHistoryList();
        refreshWorkspaceInfo();
    };

    QObject::connect(searchButton, &QPushButton::clicked, runLookup);
    QObject::connect(searchInput, &QLineEdit::returnPressed, runLookup);
    QObject::connect(searchInput, &QLineEdit::textChanged, [&](const QString& text) {
        if (text.trimmed().isEmpty()) {
            suggestionList->clear();
            resultView->clear();
            statusLabel->setText(manager.hasDictionaries()
                                     ? "Ready."
                                     : "Import a StarDict dictionary to begin.");
            return;
        }

        suggestionList->clear();
        for (const QString& suggestion : manager.searchSimilar(text, 10)) {
            suggestionList->addItem(suggestion);
        }
    });

    QObject::connect(suggestionList, &QListWidget::itemActivated, [&](QListWidgetItem *item) {
        if (item == nullptr) {
            return;
        }
        searchInput->setText(item->text());
        runLookup();
    });

    QObject::connect(historyList, &QListWidget::itemActivated, [&](QListWidgetItem *item) {
        if (item == nullptr) {
            return;
        }
        searchInput->setText(item->data(Qt::UserRole).toString());
        runLookup();
    });
    QObject::connect(historyFilterInput, &QLineEdit::textChanged, [&](const QString &) {
        refreshHistoryList();
    });

    QObject::connect(dictionaryList, &QListWidget::currentItemChanged,
                     [&](QListWidgetItem *, QListWidgetItem *) { refreshDictionaryDetails(); });
    QObject::connect(dictionaryFilterInput, &QLineEdit::textChanged, [&](const QString &) {
        refreshDictionaryList();
        refreshDictionaryDetails();
    });
    QObject::connect(dictionaryScopeFilter, &QComboBox::currentIndexChanged, [&](int) {
        refreshDictionaryList();
        refreshDictionaryDetails();
    });
    QObject::connect(applyTagsButton, &QPushButton::clicked, [&]() {
        auto *item = dictionaryList->currentItem();
        if (item == nullptr) {
            statusLabel->setText("Select a dictionary first.");
            return;
        }

        const QStringList tags = dictionaryTagsInput->text().split(',', Qt::SkipEmptyParts);
        if (!manager.setDictionaryTags(item->data(Qt::UserRole).toString(), tags)) {
            statusLabel->setText(manager.lastError());
            return;
        }

        refreshDictionaryList();
        refreshDictionaryDetails();
        refreshWorkspaceInfo();
        statusLabel->setText("Dictionary tags updated.");
    });

    QObject::connect(importFileButton, &QPushButton::clicked, [&]() {
        const QString filePath = QFileDialog::getOpenFileName(
            &window,
            "Import dictionary",
            QString(),
            "Dictionary Files (*.ifo *.mdx)");

        if (filePath.isEmpty()) {
            return;
        }

        if (!manager.addDictionary(filePath)) {
            QMessageBox::warning(&window, "Import failed", manager.lastError());
            return;
        }

        refreshDictionaryList();
        refreshDictionaryDetails();
        refreshHistoryList();
        refreshWorkspaceInfo();
        statusLabel->setText("Dictionary imported.");
    });

    QObject::connect(importDirButton, &QPushButton::clicked, [&]() {
        const QString dirPath = QFileDialog::getExistingDirectory(&window, "Import dictionary folder");
        if (dirPath.isEmpty()) {
            return;
        }

        const int count = manager.addDictionariesFromDirectory(dirPath);
        if (count <= 0) {
            QMessageBox::warning(&window, "Import failed", manager.lastError());
            return;
        }

        refreshDictionaryList();
        refreshDictionaryDetails();
        refreshHistoryList();
        refreshWorkspaceInfo();
        statusLabel->setText(QString("Imported %1 dictionaries.").arg(count));
    });

    QObject::connect(toggleEnabledButton, &QPushButton::clicked, [&]() {
        auto *item = dictionaryList->currentItem();
        if (item == nullptr) {
            statusLabel->setText("Select a dictionary first.");
            return;
        }

        const QString dictionaryId = item->data(Qt::UserRole).toString();
        const auto infos = manager.getLoadedDictionaryInfos();
        for (const auto& info : infos) {
            if (info.id == dictionaryId) {
                manager.setDictionaryEnabled(dictionaryId, !info.enabled);
                refreshDictionaryList();
                refreshDictionaryDetails();
                refreshHistoryList();
                refreshWorkspaceInfo();
                statusLabel->setText(info.enabled ? "Dictionary disabled." : "Dictionary enabled.");
                return;
            }
        }
    });

    QObject::connect(moveUpButton, &QPushButton::clicked, [&]() {
        auto *item = dictionaryList->currentItem();
        if (item == nullptr) {
            statusLabel->setText("Select a dictionary first.");
            return;
        }

        if (!manager.moveDictionaryUp(item->data(Qt::UserRole).toString())) {
            statusLabel->setText(manager.lastError());
            return;
        }

        refreshDictionaryList();
        refreshDictionaryDetails();
        refreshHistoryList();
        refreshWorkspaceInfo();
        statusLabel->setText("Dictionary priority updated.");
    });

    QObject::connect(moveDownButton, &QPushButton::clicked, [&]() {
        auto *item = dictionaryList->currentItem();
        if (item == nullptr) {
            statusLabel->setText("Select a dictionary first.");
            return;
        }

        if (!manager.moveDictionaryDown(item->data(Qt::UserRole).toString())) {
            statusLabel->setText(manager.lastError());
            return;
        }

        refreshDictionaryList();
        refreshDictionaryDetails();
        refreshHistoryList();
        refreshWorkspaceInfo();
        statusLabel->setText("Dictionary priority updated.");
    });

    QObject::connect(removeButton, &QPushButton::clicked, [&]() {
        auto *item = dictionaryList->currentItem();
        if (item == nullptr) {
            statusLabel->setText("Select a dictionary first.");
            return;
        }

        if (!manager.removeDictionary(item->data(Qt::UserRole).toString())) {
            statusLabel->setText(manager.lastError());
            return;
        }

        refreshDictionaryList();
        refreshDictionaryDetails();
        refreshHistoryList();
        refreshWorkspaceInfo();
        statusLabel->setText("Dictionary removed.");
    });

    QObject::connect(clearHistoryButton, &QPushButton::clicked, [&]() {
        manager.clearSearchHistory();
        refreshHistoryList();
        refreshWorkspaceInfo();
        statusLabel->setText("Search history cleared.");
    });

    QObject::connect(togglePinnedHistoryButton, &QPushButton::clicked, [&]() {
        auto *item = historyList->currentItem();
        if (item == nullptr) {
            statusLabel->setText("Select a history item first.");
            return;
        }

        const QString query = item->data(Qt::UserRole).toString();
        const bool pinned = item->data(Qt::UserRole + 1).toBool();
        if (!manager.setSearchHistoryPinned(query, !pinned)) {
            statusLabel->setText(manager.lastError());
            return;
        }

        refreshHistoryList();
        refreshWorkspaceInfo();
        statusLabel->setText(pinned ? "History item unpinned." : "History item pinned.");
    });

    QObject::connect(removeHistoryButton, &QPushButton::clicked, [&]() {
        auto *item = historyList->currentItem();
        if (item == nullptr) {
            statusLabel->setText("Select a history item first.");
            return;
        }

        const QString query = item->data(Qt::UserRole).toString();
        if (!manager.removeSearchHistoryItem(query)) {
            statusLabel->setText(manager.lastError());
            return;
        }

        refreshHistoryList();
        refreshWorkspaceInfo();
        statusLabel->setText("History item removed.");
    });

    QObject::connect(exportHistoryButton, &QPushButton::clicked, [&]() {
        const QString filePath = QFileDialog::getSaveFileName(
            &window,
            "Export search history",
            QString(),
            "JSON Files (*.json)");
        if (filePath.isEmpty()) {
            return;
        }

        if (!manager.exportSearchHistory(filePath)) {
            statusLabel->setText("Failed to export search history.");
            return;
        }

        statusLabel->setText("Search history exported.");
    });

    QObject::connect(importHistoryButton, &QPushButton::clicked, [&]() {
        const QString filePath = QFileDialog::getOpenFileName(
            &window,
            "Import search history",
            QString(),
            "JSON Files (*.json)");
        if (filePath.isEmpty()) {
            return;
        }

        if (!manager.importSearchHistory(filePath, false)) {
            statusLabel->setText(manager.lastError());
            return;
        }

        refreshHistoryList();
        refreshWorkspaceInfo();
        statusLabel->setText("Search history imported.");
    });

    QObject::connect(saveWorkspaceButton, &QPushButton::clicked, [&]() {
        if (!manager.saveState()) {
            statusLabel->setText("Failed to save workspace.");
            return;
        }
        refreshWorkspaceInfo();
        statusLabel->setText("Workspace saved.");
    });

    QObject::connect(reloadWorkspaceButton, &QPushButton::clicked, [&]() {
        if (!manager.loadState()) {
            statusLabel->setText(manager.lastError());
            return;
        }
        refreshDictionaryList();
        refreshDictionaryDetails();
        refreshHistoryList();
        refreshWorkspaceInfo();
        statusLabel->setText("Workspace reloaded.");
    });

    refreshDictionaryList();
    refreshDictionaryDetails();
    refreshHistoryList();
    refreshWorkspaceInfo();
    window.show();
    return app.exec();
}
