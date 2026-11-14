#pragma once

// 词典库状态入口（LAY-004 自 gui/main.cpp 匿名命名空间拆出）：dictMgr
// 单例、状态文件定位、环境装载与落盘。inline 函数的静态局部对象全 TU
// 唯一（C++ ODR）——click_audit / ui_sandbox 以 #include "main.cpp" 复用
// MainWindow，与主程序仍拿到同一实例。

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

#include "std/dictionary_manager_std.h"

// 词典管理面已切 core/std（TD-101 残余收口）：装载/查询/索引/资源/词典库
// 都走 DictionaryManagerStd，与 qmlui 主链同源；历史/生词本/笔记仍经
// DataStore 门面（P-7 双存储收敛后同落 DataStoreStd）
inline UnidictCoreStd::DictionaryManagerStd& dictMgr() {
    static UnidictCoreStd::DictionaryManagerStd manager;
    return manager;
}

// 状态文件定位（与 legacy defaultStateFilePathValue 同口径）：显式 env
// 重定向优先（测试隔离口子），未设置走 AppDataLocation
inline QString resolveStateFilePath() {
    const QByteArray envDir = qgetenv("UNIDICT_STATE_DIR");
    if (!envDir.isEmpty()) {
        return QDir(QString::fromLocal8Bit(envDir))
            .filePath(QStringLiteral("dictionary_state.json"));
    }
    QString baseDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        baseDir = QDir::homePath() + QDir::separator() + QStringLiteral(".unidict");
    }
    return QDir(baseDir).filePath(QStringLiteral("dictionary_state.json"));
}

inline void loadDefaultDictionaryLocations() {
    auto& manager = dictMgr();
    manager.load_state(resolveStateFilePath().toStdString());

    bool added = false;
    const QString envDir = qEnvironmentVariable("UNIDICT_DICT_DIR");
    if (!envDir.isEmpty()) {
        for (const std::string& path : manager.scan_directory(envDir.toStdString())) {
            added |= manager.add_dictionary(path);
        }
    }
    const QString envDicts = qEnvironmentVariable("UNIDICT_DICTS");
    for (const QString& path : envDicts.split(QDir::listSeparator(), Qt::SkipEmptyParts)) {
        added |= manager.add_dictionary(path.trimmed().toStdString());
    }
    const QString localDir = QDir(QApplication::applicationDirPath()).filePath("dictionaries");
    if (QFileInfo::exists(localDir)) {
        for (const std::string& path : manager.scan_directory(localDir.toStdString())) {
            added |= manager.add_dictionary(path);
        }
    }
    // 前缀/通配/正则走索引（std 面显式 build，时机由装载方统一把握）；
    // 环境装载结果落盘（legacy addDictionary 自动 saveState 口径）
    if (added) {
        manager.build_index();
        manager.save_state(resolveStateFilePath().toStdString());
    }
}

// 状态落盘（std 面无自动落盘纪律，变更后显式保存）
inline void persistDictState() {
    dictMgr().save_state(resolveStateFilePath().toStdString());
}
