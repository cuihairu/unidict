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

QString Theme::noteHighlight(int index) const {
    switch (index) {
    case 1: return m_tokens.noteYellow;
    case 2: return m_tokens.noteGreen;
    case 3: return m_tokens.noteBlue;
    case 4: return m_tokens.notePink;
    default: return QStringLiteral("transparent");
    }
}

} // namespace UnidictQml
