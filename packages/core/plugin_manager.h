#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <functional>
#include <memory>
#include <vector>

#include "unidict_core.h"

namespace UnidictCore {

class PluginManager {
public:
    static PluginManager& instance();

    using FactoryFn = std::function<std::unique_ptr<DictionaryParser>()>;

    void registerFactory(const QStringList& extensions, FactoryFn factory);
    std::vector<FactoryFn> factoriesForExtension(const QString& ext) const;
    std::vector<std::unique_ptr<DictionaryParser>> createCandidatesForFile(const QString& filePath) const;
    void ensureBuiltinsRegistered();
    QMap<QString, int> extensionStats() const;

private:
    PluginManager() = default;
};

}

#endif // PLUGIN_MANAGER_H
