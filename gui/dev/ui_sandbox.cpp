// gui 端离屏截图沙盒（镜像 qmlui dev/ui_sandbox.cpp 的定位）：
// 无显示环境组装真实 MainWindow，按状态（初始/查词/例句/历史/收藏/
// 词典管理对话框）× 亮暗主题截图落盘，作 BUG-010 逐屏留档。
// gui 端设计基准是 gui-ui-structure.md（Qt Widgets 一屏一焦点），与
// docs/ui 的 QML 原型稿非同一设计基准——对照口径见 compare/README.md。
//
// 用法：unidict_gui_sandbox <输出目录>   （UNIDICT_DICTS 由调用方注入）
// 构建：cmake -DUNIDICT_BUILD_UI_SANDBOX=ON（默认 OFF）。
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QtTest/QTest>

#include "../../core/data_store.h"

#define UNIDICT_GUI_AUDIT_NO_MAIN
#include "../main.cpp"

namespace {

void settle(int ms = 150) {
    QApplication::processEvents(QEventLoop::AllEvents);
    QThread::msleep(ms);
    QApplication::processEvents(QEventLoop::AllEvents);
}

bool grab(QWidget* w, const QString& path) {
    const QPixmap pm = w->grab();
    if (pm.isNull()) {
        qWarning("grab null: %s", qPrintable(path));
        return false;
    }
    QDir().mkpath(QFileInfo(path).path());
    if (!pm.save(path)) {
        qWarning("save failed: %s", qPrintable(path));
        return false;
    }
    qInfo("saved %s (%dx%d)", qPrintable(path), pm.width(), pm.height());
    return true;
}

QToolButton* themeButton(QWidget* win) {
    for (QToolButton* b : win->findChildren<QToolButton*>()) {
        if (b->text().contains(QStringLiteral("主题"))) {
            return b;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char* argv[]) {
    // 存储/设置钉临时目录，不碰真实 ~/.config 与用户数据
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("unidict-sandbox"));
    QApplication::setApplicationName(QStringLiteral("gui-sandbox"));
    QSettings::setDefaultFormat(QSettings::IniFormat);

    const QStringList args = app.arguments();
    const QString outDir =
        args.size() > 1 ? args[1] : QStringLiteral("/tmp/gui_sandbox_out");
    const QString scratch = QDir::tempPath() + QStringLiteral("/unidict_gui_sandbox");
    QDir(scratch).removeRecursively();
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .removeRecursively();  // manager state.json 残留会把词典全禁
    QDir().mkpath(scratch);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, scratch);
    UnidictCore::DataStore::instance().setStoragePath(
        scratch + QStringLiteral("/data.json"));

    loadDefaultDictionaryLocations();  // 读 UNIDICT_DICTS/UNIDICT_DICT_DIR

    const QStringList themeNames = {QStringLiteral("浅色"), QStringLiteral("深色")};
    const QStringList tags = {QStringLiteral("light"), QStringLiteral("dark")};
    bool ok = true;

    // 亮暗各起一个窗：查询后结果区没有复位路径，空态 home 只有新窗才有
    for (int pass = 0; pass < 2; ++pass) {
        MainWindow win(app);
        win.resize(1200, 760);
        win.show();
        settle();

        QLineEdit* input = win.findChild<QLineEdit*>(QStringLiteral("searchInput"));
        QTabWidget* content =
            win.findChild<QTabWidget*>(QStringLiteral("contentTabs"));
        QTabWidget* side = win.findChild<QTabWidget*>(QStringLiteral("sideTabs"));
        QToolButton* theme = themeButton(&win);
        if (!input || !content || !side || !theme) {
            qCritical("anchor widgets missing");
            return 2;
        }

        // 主题循环：跟随系统 → 浅色 → 深色。第一轮从跟随系统点一次到
        // 浅色；第二轮新窗恢复记忆的浅色，再点一次到深色
        theme->click();
        settle();
        if (!theme->text().contains(themeNames[pass])) {
            qWarning("theme not %s: %s", qPrintable(themeNames[pass]),
                     qPrintable(theme->text()));
            ok = false;
        }

        // 初始空态（搜索框 + 空释义引导）
        grab(&win, outDir + QStringLiteral("/home_") + tags[pass] + ".png");

        // 查一次词（真实键盘链路），结果区/历史/收藏有内容
        QTest::keyClicks(input, QStringLiteral("hello"));
        QTest::keyClick(input, Qt::Key_Return);
        settle();
        ok &= grab(&win, outDir + QStringLiteral("/result_") + tags[pass] + ".png");

        // 例句页签（五视图的代表帧；词典页签即 result）
        content->setCurrentIndex(1);
        settle();
        ok &= grab(&win, outDir + QStringLiteral("/examples_") + tags[pass] + ".png");
        content->setCurrentIndex(0);

        // 历史 / 收藏侧栏
        side->setCurrentIndex(0);
        settle();
        ok &= grab(&win, outDir + QStringLiteral("/history_") + tags[pass] + ".png");
        side->setCurrentIndex(1);
        settle();
        ok &= grab(&win, outDir + QStringLiteral("/vocab_") + tags[pass] + ".png");
        side->setCurrentIndex(0);

        // 词典管理对话框（模态 exec：排 timer 进对话框循环截图后关闭）
        if (QPushButton* manage =
                win.findChild<QPushButton*>(QStringLiteral("manageButton"))) {
            const QString path =
                outDir + QStringLiteral("/manage_") + tags[pass] + ".png";
            QTimer::singleShot(200, [&win, path] {
                if (QWidget* dlg = QApplication::activeModalWidget()) {
                    grab(dlg, path);
                    dlg->close();
                }
            });
            manage->click();
            settle(300);
            if (!QFile::exists(path)) {
                qWarning("manage dialog not captured (%s)",
                         qPrintable(tags[pass]));
                ok = false;
            }
        }
        settle(50);
    }
    return ok ? 0 : 3;
}
