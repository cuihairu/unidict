// Theme —— QML 侧集中主题单例（C++ 上下文对象形态）。
//
// 用法（main.cpp）：engine.rootContext()->setContextProperty("Theme", &Theme::instance());
// QML 任意文件直接 `color: Theme.text`，无需 import；token 值唯一来源是
// theme_tokens.h。亮/暗切换只拨 `Theme.dark` 一个属性。
//
// 形状单一规则（按 Qt 尺度定档）：
//   radiusS = 4（小件：标签/徽标/小卡片内圆角）
//   radiusM = 8（按钮/输入框等控件级圆角——自定义 background 处使用）
//   radiusL = 12（卡片/面板）
// 字体保持系统栈：本类不提供字体 token，页面也不许引入自定义字体。
#ifndef UNIDICT_QML_THEME_H
#define UNIDICT_QML_THEME_H

#include <QObject>
#include <QString>

#include "theme_tokens.h"

namespace UnidictQml {

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)
    Q_PROPERTY(QString window READ window NOTIFY darkChanged)
    Q_PROPERTY(QString card READ card NOTIFY darkChanged)
    Q_PROPERTY(QString text READ text NOTIFY darkChanged)
    Q_PROPERTY(QString textSecondary READ textSecondary NOTIFY darkChanged)
    Q_PROPERTY(QString textTertiary READ textTertiary NOTIFY darkChanged)
    Q_PROPERTY(QString textDisabled READ textDisabled NOTIFY darkChanged)
    Q_PROPERTY(QString accent READ accent NOTIFY darkChanged)
    Q_PROPERTY(QString accentHover READ accentHover NOTIFY darkChanged)
    Q_PROPERTY(QString accentPressed READ accentPressed NOTIFY darkChanged)
    Q_PROPERTY(QString accentText READ accentText NOTIFY darkChanged)
    Q_PROPERTY(QString divider READ divider NOTIFY darkChanged)
    Q_PROPERTY(QString hoverOverlay READ hoverOverlay NOTIFY darkChanged)
    Q_PROPERTY(QString danger READ danger NOTIFY darkChanged)
    Q_PROPERTY(QString success READ success NOTIFY darkChanged)
    Q_PROPERTY(QString warning READ warning NOTIFY darkChanged)
    Q_PROPERTY(QString info READ info NOTIFY darkChanged)
    Q_PROPERTY(int radiusS READ radiusS CONSTANT)
    Q_PROPERTY(int radiusM READ radiusM CONSTANT)
    Q_PROPERTY(int radiusL READ radiusL CONSTANT)

public:
    static Theme& instance();

    bool dark() const { return m_dark; }
    void setDark(bool dark);

    QString window() const { return m_tokens.window; }
    QString card() const { return m_tokens.card; }
    QString text() const { return m_tokens.text; }
    QString textSecondary() const { return m_tokens.textSecondary; }
    QString textTertiary() const { return m_tokens.textTertiary; }
    QString textDisabled() const { return m_tokens.textDisabled; }
    QString accent() const { return m_tokens.accent; }
    QString accentHover() const { return m_tokens.accentHover; }
    QString accentPressed() const { return m_tokens.accentPressed; }
    QString accentText() const { return m_tokens.accentText; }
    QString divider() const { return m_tokens.divider; }
    QString hoverOverlay() const { return m_tokens.hoverOverlay; }
    QString danger() const { return m_tokens.danger; }
    QString success() const { return m_tokens.success; }
    QString warning() const { return m_tokens.warning; }
    QString info() const { return m_tokens.info; }

    int radiusS() const { return kRadiusS; }
    int radiusM() const { return kRadiusM; }
    int radiusL() const { return kRadiusL; }

signals:
    void darkChanged();

private:
    Theme();
    bool m_dark = false;
    ThemeTokens m_tokens;
};

} // namespace UnidictQml

#endif // UNIDICT_QML_THEME_H
