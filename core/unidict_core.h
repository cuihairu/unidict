#ifndef UNIDICT_CORE_H
#define UNIDICT_CORE_H

#include <QVariant>
#include <QString>
#include <QStringList>

// legacy 词典门面已退役（core/unidict_core.cpp 与四套 Qt 解析器随
// DictionaryManager 一并删除；qmlui/gui 全走 core/std 的
// DictionaryManagerStd）。此头只剩词本条目 DTO——adapters/qt 的
// DataStoreQt（core/data_store 双跳门面已拆，TD-106）词本 API 共用
// 它的形状，故保留在此处做单一真源。
namespace UnidictCore {

struct DictionaryEntry {
    QString word;
    QString definition;
    QString pronunciation;
    QStringList examples;
    QVariantMap metadata;
};

} // namespace UnidictCore

#endif // UNIDICT_CORE_H
