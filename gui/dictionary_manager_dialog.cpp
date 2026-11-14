// 词典管理对话框（LAY-004 自 gui/main.cpp 拆出）——列表/启停/排序/
// 分组/失败隔离重试遗忘/导入，入口见 dictionary_manager_dialog.h。

#include "dictionary_manager_dialog.h"

#include "dict_state.h"

#include <QBrush>
#include <QColor>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QObject>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

#include <string>
#include <utility>
#include <vector>

void runDictionaryManagerDialog(QWidget* parent,
                                const std::function<void()>& onTagChanged,
                                const std::function<void()>& onFinished) {
    auto& manager = dictMgr();

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("词典管理"));
    dialog.resize(560, 420);
    auto* layout = new QVBoxLayout(&dialog);

    auto* list = new QListWidget(&dialog);
    layout->addWidget(list);

    auto refreshList = [&] {
        list->clear();
        const auto metas = manager.dictionaries_meta();
        for (const auto& m : metas) {
            QStringList tagList;
            for (const auto& t : m.tags) {
                tagList.append(QString::fromStdString(t));
            }
            QString label = QStringLiteral("%1  (%2 · %3 词")
                                .arg(QString::fromStdString(m.name),
                                     QString::fromStdString(m.format))
                                .arg(m.word_count);
            if (!tagList.isEmpty()) {
                label += QStringLiteral(" · 分组: %1").arg(tagList.join(QStringLiteral("/")));
            }
            label += QLatin1Char(')');
            auto* item = new QListWidgetItem(label, list);
            item->setData(Qt::UserRole, QString::fromStdString(m.name));
            item->setToolTip(QString::fromStdString(m.file_path));
            item->setForeground(m.enabled ? QBrush() : QBrush(Qt::gray));
        }
        // 加载失败的词典（损坏隔离/文件丢失）：诊断 + 重试/移除入口
        const auto failures = manager.failed_dictionaries();
        for (const auto& failure : failures) {
            const QString filePath = QString::fromStdString(failure.file_path);
            auto* item = new QListWidgetItem(
                QStringLiteral("⚠ %1（无法加载）：%2")
                    .arg(QFileInfo(filePath).fileName(),
                         QString::fromStdString(failure.reason)),
                list);
            item->setData(Qt::UserRole,
                          QStringLiteral("failed:%1").arg(filePath));
            item->setToolTip(filePath);
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

    QObject::connect(addFile, &QPushButton::clicked, &dialog, [&] {
        const QString path = QFileDialog::getOpenFileName(
            &dialog, QStringLiteral("选择词典文件"), QString(),
            QStringLiteral("词典文件 (*.ifo *.mdx *.json *.epub);;所有文件 (*)"));
        if (!path.isEmpty() && !manager.add_dictionary(path.toStdString())) {
            QMessageBox::warning(&dialog, QStringLiteral("Unidict"),
                                 QString::fromStdString(manager.last_error()).isEmpty()
                                     ? QStringLiteral("无法加载该词典")
                                     : QString::fromStdString(manager.last_error()));
        } else {
            manager.build_index();
            // 导入即建全文索引：导入完成立即可全文检索，首查不再付
            // 全量构建（与 qmlui finishImport 同口径）
            manager.warm_fulltext_index();
            persistDictState();
        }
        refreshList();
    });
    QObject::connect(addDir, &QPushButton::clicked, &dialog, [&] {
        const QString dir =
            QFileDialog::getExistingDirectory(&dialog, QStringLiteral("选择词典目录"));
        if (!dir.isEmpty()) {
            bool added = false;
            for (const std::string& p : manager.scan_directory(dir.toStdString())) {
                added |= manager.add_dictionary(p);
            }
            if (added) {
                manager.build_index();
                manager.warm_fulltext_index();
            }
            persistDictState();
            refreshList();
        }
    });
    QObject::connect(toggle, &QPushButton::clicked, &dialog, [&] {
        if (auto* item = list->currentItem()) {
            if (!failedPathOf(item).isEmpty()) {
                return; // 失败行没有启用/禁用语义（还没加载）
            }
            const QString name = item->data(Qt::UserRole).toString();
            for (const auto& m : manager.dictionaries_meta()) {
                if (QString::fromStdString(m.name) == name) {
                    manager.set_dictionary_enabled(name.toStdString(), !m.enabled);
                    break;
                }
            }
            persistDictState();
            refreshList();
        }
    });
    // 上移/下移：视图序相邻交换后全量重排显式优先级（去并列保序确定）；
    // std 查询视图 = priority 降序稳定，语义与 legacy 列表位次一致
    auto moveDict = [&](const QString& name, bool up) {
        const auto metas = manager.dictionaries_meta();
        int idx = -1;
        for (int i = 0; i < (int)metas.size(); ++i) {
            if (QString::fromStdString(metas[i].name) == name) {
                idx = i;
                break;
            }
        }
        const int other = up ? idx - 1 : idx + 1;
        if (idx < 0 || other < 0 || other >= (int)metas.size()) {
            return;
        }
        std::vector<std::pair<std::string, int>> order;
        order.reserve(metas.size());
        for (int i = 0; i < (int)metas.size(); ++i) {
            order.emplace_back(metas[i].name, i);
        }
        std::swap(order[idx], order[other]);
        const int n = (int)order.size();
        for (int i = 0; i < n; ++i) {
            manager.set_dictionary_priority(order[i].first, n - i);
        }
        persistDictState();
        refreshList();
    };
    QObject::connect(up, &QPushButton::clicked, &dialog, [&] {
        if (auto* item = list->currentItem(); item && failedPathOf(item).isEmpty()) {
            moveDict(item->data(Qt::UserRole).toString(), true);
        }
    });
    QObject::connect(down, &QPushButton::clicked, &dialog, [&] {
        if (auto* item = list->currentItem(); item && failedPathOf(item).isEmpty()) {
            moveDict(item->data(Qt::UserRole).toString(), false);
        }
    });
    QObject::connect(remove, &QPushButton::clicked, &dialog, [&] {
        if (auto* item = list->currentItem()) {
            // 失败行走“遗忘”（从隔离区移除，不再重试）；正常行按名移除
            const QString failedPath = failedPathOf(item);
            if (!failedPath.isEmpty()) {
                manager.forget_failed_dictionary(failedPath.toStdString());
            } else {
                manager.remove_dictionary(
                    item->data(Qt::UserRole).toString().toStdString());
            }
            persistDictState();
            refreshList();
        }
    });
    QObject::connect(retry, &QPushButton::clicked, &dialog, [&] {
        if (auto* item = list->currentItem()) {
            const QString failedPath = failedPathOf(item);
            if (failedPath.isEmpty()) {
                return;
            }
            if (manager.retry_failed_dictionary(failedPath.toStdString())) {
                manager.build_index();
            } else {
                QMessageBox::warning(&dialog, QStringLiteral("Unidict"),
                                     QString::fromStdString(manager.last_error()).isEmpty()
                                         ? QStringLiteral("重试失败：词典仍无法加载")
                                         : QString::fromStdString(manager.last_error()));
            }
            persistDictState();
            refreshList();
        }
    });
    QObject::connect(tags, &QPushButton::clicked, &dialog, [&] {
        if (auto* item = list->currentItem()) {
            if (!failedPathOf(item).isEmpty()) {
                return; // 失败行没有分组语义
            }
            const QString name = item->data(Qt::UserRole).toString();
            QString current;
            for (const auto& m : manager.dictionaries_meta()) {
                if (QString::fromStdString(m.name) == name) {
                    QStringList tagList;
                    for (const auto& t : m.tags) {
                        tagList.append(QString::fromStdString(t));
                    }
                    current = tagList.join(QStringLiteral(","));
                    break;
                }
            }
            bool ok = false;
            const QString text = QInputDialog::getText(
                &dialog, QStringLiteral("分组标签"),
                QStringLiteral("逗号分隔，留空清除（例：en,zh）"),
                QLineEdit::Normal, current, &ok);
            if (ok) {
                std::vector<std::string> parsed;
                for (const QString& tag : text.split(QLatin1Char(','))) {
                    const QString t = tag.trimmed();
                    if (!t.isEmpty()) {
                        parsed.push_back(t.toStdString());
                    }
                }
                manager.set_dictionary_tags(name.toStdString(), std::move(parsed));
                persistDictState();
                refreshList();
                onTagChanged();
            }
        }
    });

    QObject::connect(&dialog, &QDialog::finished, parent, [onFinished](int) { onFinished(); });

    dialog.exec();
}
