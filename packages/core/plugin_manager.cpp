#include "plugin_manager.h"
#include "plugin_manager_qt.h"

namespace UnidictCore {

PluginManager& PluginManager::instance() {
    static PluginManager inst;
    return inst;
}

void PluginManager::registerFactory(const QStringList& extensions, FactoryFn factory) {
    ::UnidictAdaptersQt::PluginManagerQt::instance().registerFactory(extensions, factory);
}

std::vector<PluginManager::FactoryFn> PluginManager::factoriesForExtension(const QString& ext) const {
    return ::UnidictAdaptersQt::PluginManagerQt::instance().factoriesForExtension(ext);
}

std::vector<std::unique_ptr<DictionaryParser>> PluginManager::createCandidatesForFile(const QString& filePath) const {
    return ::UnidictAdaptersQt::PluginManagerQt::instance().createCandidatesForFile(filePath);
}

void PluginManager::ensureBuiltinsRegistered() {
    ::UnidictAdaptersQt::PluginManagerQt::instance().ensureBuiltinsRegistered();
}

QMap<QString, int> PluginManager::extensionStats() const {
    return ::UnidictAdaptersQt::PluginManagerQt::instance().extensionStats();
}

}
