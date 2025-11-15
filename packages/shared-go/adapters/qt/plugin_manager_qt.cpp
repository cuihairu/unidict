#include "plugin_manager_qt.h"

#include <QFileInfo>
#include <QDebug>

#include "stardict_parser_qt.h"
#include "mdict_parser_qt.h"
#include "json_parser_qt.h"

namespace UnidictAdaptersQt {

PluginManagerQt& PluginManagerQt::instance() {
    static PluginManagerQt pm;
    return pm;
}

void PluginManagerQt::registerFactory(const QStringList& extensions, FactoryFn factory) {
    for (QString ext : extensions) {
        const QString low = ext.toLower();
        m_extToFactories[low].push_back(factory);
    }
}

std::vector<PluginManagerQt::FactoryFn> PluginManagerQt::factoriesForExtension(const QString& ext) const {
    const QString low = ext.toLower();
    auto it = m_extToFactories.find(low);
    if (it == m_extToFactories.end()) return {};
    return it.value();
}

std::vector<std::unique_ptr<UnidictCore::DictionaryParser>> PluginManagerQt::createCandidatesForFile(const QString& filePath) const {
    QFileInfo fi(filePath);
    const QString ext = fi.suffix().toLower();
    std::vector<std::unique_ptr<UnidictCore::DictionaryParser>> out;
    auto fns = factoriesForExtension(ext);
    for (const auto& fn : fns) out.push_back(fn());
    return out;
}

void PluginManagerQt::ensureBuiltinsRegistered() {
    if (m_builtinsRegistered) return;
    registerFactory({"ifo", "idx", "dict", "dz"}, [](){ return std::make_unique<UnidictAdaptersQt::StarDictParserQt>(); });
    registerFactory({"mdx", "mdd"}, [](){ return std::make_unique<UnidictAdaptersQt::MdictParserQt>(); });
    registerFactory({"json"}, [](){ return std::make_unique<UnidictAdaptersQt::JsonParserQt>(); });
    m_builtinsRegistered = true;
}

QMap<QString, int> PluginManagerQt::extensionStats() const {
    QMap<QString, int> out;
    for (auto it = m_extToFactories.begin(); it != m_extToFactories.end(); ++it) {
        out[it.key()] = static_cast<int>(it.value().size());
    }
    return out;
}

}
