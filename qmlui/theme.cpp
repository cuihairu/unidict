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

void Theme::setMode(int mode) {
    if (mode < 0 || mode > 2 || m_mode == mode) return;
    m_mode = mode;
    emit modeChanged();
    // 锁定/解锁即刻生效：浅=亮、深=暗、跟随=最近一次系统亮暗
    if (mode == 1) {
        setDark(false);
    } else if (mode == 2) {
        setDark(true);
    } else {
        setDark(m_systemDark);
    }
}

void Theme::applySystemScheme(bool systemDark) {
    m_systemDark = systemDark;
    if (m_mode != 0) return;  // 手动锁定不跟随
    setDark(systemDark);
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
