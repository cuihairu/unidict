#pragma once

// 主题三态（LAY-004 自 gui/main.cpp 拆出）：跟随系统 / 浅色 / 深色，
// 记忆到 QSettings。纯 QApplication 调色，无窗口/控件依赖。

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleHints>
#include <QString>

// 深色 QPalette 模板（Qt 官方 Dark 样式示例值）
inline QPalette darkPalette() {
    QPalette p;
    p.setColor(QPalette::Window, QColor(53, 53, 53));
    p.setColor(QPalette::WindowText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    p.setColor(QPalette::Base, QColor(42, 42, 42));
    p.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, QColor(53, 53, 53));
    p.setColor(QPalette::Text, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    p.setColor(QPalette::Dark, QColor(35, 35, 35));
    p.setColor(QPalette::Shadow, QColor(20, 20, 20));
    p.setColor(QPalette::Button, QColor(53, 53, 53));
    p.setColor(QPalette::ButtonText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(42, 130, 218));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
    return p;
}

// 主题三态：跟随系统 / 浅色 / 深色，记忆到 QSettings
class ThemeManager {
public:
    explicit ThemeManager(QApplication& app) : app_(app) {
        mode_ = QSettings().value("ui/theme", 0).toInt();
    }

    void apply(int mode) {
        mode_ = mode;
        QSettings().setValue("ui/theme", mode_);
        applyCurrent();
    }

    void applyCurrent() {
        const bool dark = mode_ == 2
            || (mode_ == 0 && app_.styleHints()->colorScheme() == Qt::ColorScheme::Dark);
        if (dark) {
            app_.setPalette(darkPalette());
        } else {
            app_.setPalette(app_.style()->standardPalette());
        }
    }

    [[nodiscard]] int mode() const { return mode_; }

    [[nodiscard]] QString label() const {
        switch (mode_) {
        case 1: return QStringLiteral("主题: 浅色");
        case 2: return QStringLiteral("主题: 深色");
        default: return QStringLiteral("主题: 跟随系统");
        }
    }

private:
    QApplication& app_;
    int mode_ = 0;
};
