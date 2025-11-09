// Minimal core type aliases for a Qt-free core. Kept small by design.
// This header is used by new std-only modules and does not pull Qt.

#ifndef UNIDICT_CORE_TYPES_H
#define UNIDICT_CORE_TYPES_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace UnidictCoreTypes {

using String = std::string;               // UTF-8 by convention
template <typename T>
using Vector = std::vector<T>;
template <typename K, typename V>
using UnorderedMap = std::unordered_map<K, V>;
template <typename T>
using UnorderedSet = std::unordered_set<T>;

} // namespace UnidictCoreTypes

#endif // UNIDICT_CORE_TYPES_H

