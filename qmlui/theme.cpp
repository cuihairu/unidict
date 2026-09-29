#include "theme.h"

namespace UnidictQml {

Theme::Theme() : QObject(nullptr), m_tokens(tokensFor(false)) {}

Theme& Theme::instance() {
    static Theme t;
    return t;
}

void Theme::setDark(bool dark) {
    if (m_dark == dark) return;
    m_dark = dark;
    m_tokens = tokensFor(dark);
    emit darkChanged();
}

} // namespace UnidictQml
