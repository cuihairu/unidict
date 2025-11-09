#ifndef UNIDICT_PLUGIN_MANAGER_QT_H
#define UNIDICT_PLUGIN_MANAGER_QT_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <functional>
#include <memory>
#include <vector>

#include "core/unidict_core.h"

namespace UnidictAdaptersQt {

class PluginManagerQt {
public:
    static PluginManagerQt& instance();

    using FactoryFn = std::function<std::unique_ptr<UnidictCore::DictionaryParser>()>;

    void registerFactory(const QStringList& extensions, FactoryFn factory);

    std::vector<FactoryFn> factoriesForExtension(const QString& ext) const;
    std::vector<std::unique_ptr<UnidictCore::DictionaryParser>> createCandidatesForFile(const QString& filePath) const;

    void ensureBuiltinsRegistered();
    QMap<QString, int> extensionStats() const;

private:
    PluginManagerQt() = default;
    QMap<QString, std::vector<FactoryFn>> m_extToFactories;
    bool m_builtinsRegistered = false;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_PLUGIN_MANAGER_QT_H

