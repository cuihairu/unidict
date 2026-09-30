// Theme 单例（qmlui）全量测试。界面重设计批次（21685d4）引入时 0% 覆盖，
// 把 Qt 层覆盖率闸门从 99.7% 拉回 100%：单例身份、亮/暗两套 token 全量
// 切换、重复设值短路、darkChanged 信号、Q_PROPERTY 元对象路径。
//
// token 期望值直接对照 theme_tokens.h 的 tokensFor()（同一数值源，主题
// 语义配对的 WCAG 实测门禁在 theme_tokens_contrast_test.cpp，不在这里重复）。

#include <QtTest>
#include <QSignalSpy>

#include "theme.h"
#include "theme_tokens.h"

using namespace UnidictQml;

namespace {

// 逐 token 对照 tokensFor(dark)——getter 与表值必须同源一致
bool tokensMatch(const Theme& t, bool dark) {
    const ThemeTokens exp = tokensFor(dark);
    return t.window() == exp.window && t.card() == exp.card
        && t.text() == exp.text && t.textSecondary() == exp.textSecondary
        && t.textTertiary() == exp.textTertiary
        && t.textDisabled() == exp.textDisabled && t.accent() == exp.accent
        && t.accentHover() == exp.accentHover
        && t.accentPressed() == exp.accentPressed
        && t.accentText() == exp.accentText && t.divider() == exp.divider
        && t.hoverOverlay() == exp.hoverOverlay && t.danger() == exp.danger
        && t.success() == exp.success && t.warning() == exp.warning
        && t.info() == exp.info;
}

} // namespace

class ThemeTest : public QObject {
    Q_OBJECT

private slots:
    // 单例身份 + 亮色默认：getter 全表 == tokensFor(false)，半径常量同源
    void singleton_identity_and_light_defaults() {
        Theme& t = Theme::instance();
        QVERIFY(&Theme::instance() == &t); // 单例同一性
        QVERIFY(!t.dark());
        QVERIFY(tokensMatch(t, false));
        QCOMPARE(t.radiusS(), kRadiusS);
        QCOMPARE(t.radiusM(), kRadiusM);
        QCOMPARE(t.radiusL(), kRadiusL);
    }

    // 暗/亮切换：全表翻面 + darkChanged 信号 + 重复设值短路分支
    void dark_toggle_signals_and_roundtrip() {
        Theme& t = Theme::instance();
        t.setDark(false); // 归零起点
        QSignalSpy spy(&t, &Theme::darkChanged);

        t.setDark(true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(t.dark());
        QVERIFY(tokensMatch(t, true));

        t.setDark(true); // 同值短路：不发信号
        QCOMPARE(spy.count(), 1);

        t.setDark(false);
        QCOMPARE(spy.count(), 2);
        QVERIFY(!t.dark());
        QVERIFY(tokensMatch(t, false));
    }

    // Q_PROPERTY 元对象路径：QML 侧就是走 property 读写的，守一遍
    void qproperty_metadata_path() {
        Theme& t = Theme::instance();
        t.setDark(true);
        QCOMPARE(t.property("dark").toBool(), true);
        QCOMPARE(t.property("window").toString(), tokensFor(true).window);
        QCOMPARE(t.property("accent").toString(), tokensFor(true).accent);
        QCOMPARE(t.property("radiusL").toInt(), kRadiusL);
        t.setProperty("dark", false); // WRITE 走 setDark
        QCOMPARE(t.property("dark").toBool(), false);
        QCOMPARE(t.property("window").toString(), tokensFor(false).window);
    }
};

QTEST_MAIN(ThemeTest)
#include "theme_test.moc"
