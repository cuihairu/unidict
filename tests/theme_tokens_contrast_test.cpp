// 设计语言 token 的 WCAG AA 回归门禁（qmlui/theme_tokens.h 双主题全表）。
// 机器实算相对亮度与对比度，正文级配对一律 ≥ 4.5:1；textDisabled 按
// WCAG 豁免不判定。改色值若掉出门槛这里直接红——先调色再落表。
// 纯 token 文件测试：不拖 Theme QObject，QString 足够。
#include <QtTest>
#include <algorithm>
#include <cmath>

#include "theme_tokens.h"

using UnidictQml::ThemeTokens;
using UnidictQml::tokensFor;

namespace {

double srgbChannel(double c) {
    // WCAG 2.x 相对亮度：sRGB 通道线性化
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double relativeLuminance(const QString& hex) {
    const int r = hex.mid(1, 2).toInt(nullptr, 16);
    const int g = hex.mid(3, 2).toInt(nullptr, 16);
    const int b = hex.mid(5, 2).toInt(nullptr, 16);
    return 0.2126 * srgbChannel(r / 255.0) + 0.7152 * srgbChannel(g / 255.0) +
           0.0722 * srgbChannel(b / 255.0);
}

double contrast(const QString& a, const QString& b) {
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

} // namespace

class ThemeTokensContrastTest : public QObject {
    Q_OBJECT

private slots:
    // 两个主题的全部 hex 色值格式守门（含 hoverOverlay：曾用 rgba() 浮点
    // alpha 串，QML color 渲染成不透明纯黑/纯白，已改实体色）
    void hexFormat();

    // 正文级配对全表：文字三级 × 底两级 + 强调色系 + 语义色系，AA 门槛 4.5
    void aaContrast_data();
    void aaContrast();

    // 形状档位单一规则的守门：小件 4 / 控件 8 / 卡片 12
    void radiusTiers();
};

void ThemeTokensContrastTest::hexFormat() {
    const QRegularExpression hexRe(QStringLiteral("^#[0-9a-f]{6}$"));
    for (bool dark : {false, true}) {
        const ThemeTokens t = tokensFor(dark);
        const auto hexFields = {
            t.window, t.card, t.surfaceSunken, t.text, t.textSecondary,
            t.textTertiary, t.textDisabled, t.accent, t.accentHover,
            t.accentPressed, t.accentText, t.divider, t.hoverOverlay,
            t.danger, t.success, t.warning, t.info, t.link,
        };
        for (const QString& v : hexFields) {
            QVERIFY2(hexRe.match(v).hasMatch(),
                     qPrintable(QString("not 6-digit hex: %1").arg(v)));
        }
    }
}

void ThemeTokensContrastTest::aaContrast_data() {
    QTest::addColumn<QString>("fg");
    QTest::addColumn<QString>("bg");

    for (bool dark : {false, true}) {
        const ThemeTokens t = tokensFor(dark);
        const QString tag = dark ? "dark" : "light";
        QTest::newRow(qPrintable(tag + "-text-window")) << t.text << t.window;
        QTest::newRow(qPrintable(tag + "-text-card")) << t.text << t.card;
        QTest::newRow(qPrintable(tag + "-secondary-card"))
            << t.textSecondary << t.card;
        QTest::newRow(qPrintable(tag + "-tertiary-card"))
            << t.textTertiary << t.card;
        // 凹陷面（surfaceSunken，P-7 UI 重设计批新增）：内嵌输入条底。
        // tertiary 在 sunken 上掉出 4.5（4.3:1）——凹陷面文字只用 text/
        // secondary 两档，placeholder 归 secondary
        QTest::newRow(qPrintable(tag + "-text-sunken"))
            << t.text << t.surfaceSunken;
        QTest::newRow(qPrintable(tag + "-secondary-sunken"))
            << t.textSecondary << t.surfaceSunken;
        QTest::newRow(qPrintable(tag + "-accent-sunken"))
            << t.accent << t.surfaceSunken;
        QTest::newRow(qPrintable(tag + "-accent-window")) << t.accent << t.window;
        QTest::newRow(qPrintable(tag + "-accent-card")) << t.accent << t.card;
        QTest::newRow(qPrintable(tag + "-accentText-accent"))
            << t.accentText << t.accent;
        QTest::newRow(qPrintable(tag + "-danger-card")) << t.danger << t.card;
        QTest::newRow(qPrintable(tag + "-success-card")) << t.success << t.card;
        QTest::newRow(qPrintable(tag + "-warning-card")) << t.warning << t.card;
        QTest::newRow(qPrintable(tag + "-info-card")) << t.info << t.card;
        QTest::newRow(qPrintable(tag + "-link-card")) << t.link << t.card;
    }
}

void ThemeTokensContrastTest::aaContrast() {
    QFETCH(QString, fg);
    QFETCH(QString, bg);
    const double ratio = contrast(fg, bg);
    QVERIFY2(ratio >= 4.5,
             qPrintable(QString("%1 on %2 = %3:1 < 4.5")
                            .arg(fg, bg).arg(ratio, 0, 'f', 2)));
}

void ThemeTokensContrastTest::radiusTiers() {
    QCOMPARE(UnidictQml::kRadiusS, 4);
    QCOMPARE(UnidictQml::kRadiusM, 8);
    QCOMPARE(UnidictQml::kRadiusL, 12);
}

QTEST_MAIN(ThemeTokensContrastTest)
#include "theme_tokens_contrast_test.moc"
