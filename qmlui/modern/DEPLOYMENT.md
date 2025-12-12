# Unidict 现代化界面部署指南

## 🚀 快速开始

### 1. 集成到现有项目

```cpp
// main.cpp 中的修改
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    // 注册现代化组件
    qmlRegisterType<ModernNotificationManager>("Unidict.Modern", 1, 0, "NotificationManager");
    qmlRegisterType<ModernPerformance>("Unidict.Modern", 1, 0, "Performance");

    // 加载现代化界面
    engine.load(QUrl(QStringLiteral("qrc:/qmlui/modern/MainModern.qml")));

    return app.exec();
}
```

### 2. CMakeLists.txt 添加现代化文件

```cmake
# 现代化界面文件
set(MODERN_QML_FILES
    qmlui/modern/MainModern.qml
    qmlui/modern/ModernBridge.qml
    qmlui/modern/Theme.qml
    qmlui/modern/ModernPerformance.qml
    qmlui/modern/components/ModernCard.qml
    qmlui/modern/components/ModernButton.qml
    qmlui/modern/components/ModernSearchBox.qml
    qmlui/modern/components/ModernAnimations.qml
    qmlui/modern/components/ModernNavigationItem.qml
    qmlui/modern/components/ModernSearchResultsPage.qml
    qmlui/modern/components/ModernHistoryPage.qml
    qmlui/modern/components/ModernVocabularyPage.qml
    qmlui/modern/components/ModernVoicePage.qml
    qmlui/modern/components/ModernLearningPage.qml
    qmlui/modern/components/ModernSettingsPage.qml
    qmlui/modern/components/ModernToast.qml
    qmlui/modern/components/ModernNotificationManager.qml
)

qt5_add_qml_module(unidict_qmlui
    URI "Unidict.QmlUI"
    VERSION 1.0
    QML_FILES ${MODERN_QML_FILES}
)
```

## 📁 文件结构

```
qmlui/modern/
├── MainModern.qml                    # 现代化主界面
├── ModernBridge.qml                  # 功能桥接层
├── Theme.qml                        # 统一主题配置
├── ModernPerformance.qml             # 性能优化组件
├── README.md                        # 设计说明文档
├── DEPLOYMENT.md                   # 部署指南（本文件）
└── components/
    ├── ModernCard.qml               # 现代化卡片
    ├── ModernButton.qml             # 现代化按钮
    ├── ModernSearchBox.qml          # 现代化搜索框
    ├── ModernAnimations.qml         # 动画效果库
    ├── ModernNavigationItem.qml     # 导航项
    ├── ModernSearchResultsPage.qml  # 搜索结果页
    ├── ModernHistoryPage.qml        # 历史记录页
    ├── ModernVocabularyPage.qml    # 词汇管理页
    ├── ModernVoicePage.qml          # 语音功能页
    ├── ModernLearningPage.qml       # 学习进度页
    ├── ModernSettingsPage.qml      # 设置页面
    ├── ModernToast.qml             # 通知提示
    └── ModernNotificationManager.qml # 通知管理器
```

## ⚙️ 配置选项

### 1. 主题配置

```qml
// 在 Theme.qml 中自定义颜色
readonly property color primary: "#6366F1"        // 主色调
readonly property color secondary: "#8B5CF6"      // 次要色
readonly property color accent: "#EC4899"           // 强调色
readonly property color background: "#FAFBFC"       // 背景色
readonly property color surface: "#FFFFFF"           // 卡片背景色
```

### 2. 性能配置

```qml
// 在 MainModern.qml 中配置性能选项
ModernPerformance {
    id: performance

    enableGpuAcceleration: true
    enableLazyLoading: true
    maxCacheSize: 100
    memoryThreshold: 512 // MB

    Component.onCompleted: {
        setPerformanceProfile("high") // high, medium, low
    }
}
```

### 3. 通知配置

```qml
// 在应用根组件中添加通知管理器
ModernNotificationManager {
    id: notificationManager

    maxConcurrentNotifications: 3
    maxNotificationHeight: 120
}
```

## 🔧 适配现有功能

### 1. LookupAdapter 桥接

```qml
// 在 ModernBridge.qml 中连接现有功能
LookupAdapter {
    id: lookupAdapter

    // 现有信号连接
    onSearchCompleted: function(results) {
        searchResultsPage.updateResults(results)
    }

    onError: function(error) {
        notificationManager.showError(error.message, "查询错误")
    }
}
```

### 2. 数据模型适配

```qml
// 适配现有的数据模型
property var existingModel: lookupAdapter.searchHistory(50)

ListView {
    model: existingModel
    delegate: modernDelegate
}
```

### 3. 设置集成

```qml
// 读取现有设置
function loadExistingSettings() {
    var settings = lookupAdapter.getSettings()

    // 映射到现代化设置
    internal.settings.general.language = settings.language || "zh-CN"
    internal.settings.appearance.theme = settings.theme || "light"
}
```

## 🎨 自定义指南

### 1. 创建新主题

```qml
// 新建 CustomTheme.qml
import QtQuick 2.15

Theme {
    // 覆盖颜色
    readonly property color primary: "#FF6B6B"
    readonly property color secondary: "#4ECDC4"
    readonly property color accent: "#FFE66D"

    // 覆盖字体
    readonly property font titleFont: Qt.font({ family: "Arial", weight: Font.Bold })
    readonly property font bodyFont: Qt.font({ family: "Arial", weight: Font.Normal })
}
```

### 2. 创建自定义组件

```qml
// CustomComponent.qml
import QtQuick 2.15
import "Theme.qml"

ModernCard {
    // 继承现代化样式
    title: "自定义组件"

    // 添加自定义内容
    Text {
        anchors.centerIn: parent
        text: "自定义内容"
        color: theme.textPrimary
    }
}
```

### 3. 添加新的页面

```qml
// CustomPage.qml
import QtQuick 2.15
import "../Theme.qml"
import "../components"

Item {
    Theme { id: theme }

    // 使用现代化组件构建页面
    ModernCard {
        width: parent.width
        title: "自定义页面"

        // 页面内容
    }
}
```

## 📱 移动端适配

### 1. 响应式布局

```qml
// 使用现有的 ResponsiveLayout
ResponsiveLayout {
    id: responsive

    // 根据设备类型调整布局
    Column {
        width: parent.width
        spacing: responsive.isMobile ? theme.spacingSM : theme.spacingMD

        // 响应式内容
    }
}
```

### 2. 触控优化

```qml
ModernButton {
    size: responsive.isMobile ? ModernButton.Large : ModernButton.Medium

    // 确保触控目标足够大
    MouseArea {
        anchors.fill: parent
        minimumTouchTarget: 44 // iOS/Android 推荐最小值
    }
}
```

### 3. 安全区域适配

```qml
ApplicationWindow {
    anchors.margins: responsive.safeAreaTop + responsive.baseMargin
    anchors.bottomMargin: responsive.safeAreaBottom + responsive.baseMargin

    // 适配状态栏和底部导航
}
```

## 🔍 测试指南

### 1. 组件测试

```qml
// ComponentTest.qml
import QtQuick 2.15
import QtTest 1.15
import "../components"

TestCase {
    name: "ModernComponentTests"

    ModernButton {
        id: testButton
    }

    function test_button_click() {
        compare(testButton.buttonType, ModernButton.Primary)

        mouseClick(testButton)
        verify(testButton.wasClicked)
    }
}
```

### 2. 性能测试

```qml
// PerformanceTest.qml
TestCase {
    name: "PerformanceTests"

    function test_animation_performance() {
        var startTime = Date.now()

        // 执行动画
        testAnimation.running = true

        var duration = Date.now() - startTime
        verify(duration < 300, "Animation should complete within 300ms")
    }
}
```

### 3. 集成测试

```cpp
// tst_integration.cpp
void TestModernUI::testSearchIntegration()
{
    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/qmlui/modern/MainModern.qml"));

    // 测试搜索功能集成
    auto searchBox = engine.rootObjects()[0]->findChild<QObject*>("modernSearch");
    QVERIFY(searchBox != nullptr);
}
```

## 📦 打包和分发

### 1. 资源文件

```qrc
<!DOCTYPE RCC><RCC version="1.0">
    <qresource prefix="/">
        <file>qmlui/modern/MainModern.qml</file>
        <file>qmlui/modern/Theme.qml</file>
        <file>qmlui/modern/components/*.qml</file>
        <file>qmlui/modern/icons/*.svg</file>
    </qresource>
</RCC>
```

### 2. 部署脚本

```bash
#!/bin/bash
# deploy.sh

echo "🚀 部署 Unidict 现代化界面..."

# 检查依赖
echo "📦 检查依赖..."
if ! command -v qmake &> /dev/null; then
    echo "❌ qmake 未找到"
    exit 1
fi

# 清理构建
echo "🧹 清理构建..."
rm -rf build/
mkdir build
cd build/

# 配置项目
echo "⚙️ 配置项目..."
qmake ../unidict.pro
make

# 运行测试
echo "🧪 运行测试..."
make check

# 打包应用
echo "📱 打包应用..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    macdeployqt unidict.app
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    linuxdeploy --appdir unidict.AppDir
fi

echo "✅ 部署完成！"
```

### 3. 持续集成

```yaml
# .github/workflows/ci.yml
name: CI

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v2

    - name: Install Qt
      uses: jurplel/install-qt-action@v2
      with:
        version: '6.2.0'

    - name: Build
      run: |
        mkdir build
        cd build
        qmake ..
        make

    - name: Test
      run: |
        cd build
        make check
```

## 🐛 故障排除

### 常见问题

1. **组件无法加载**
   ```qml
   // 检查组件路径是否正确
   import "components/ModernCard.qml"
   ```

2. **主题颜色不生效**
   ```qml
   // 确保主题实例正确
   Theme { id: theme }
   color: theme.primary  // 而不是直接引用
   ```

3. **性能问题**
   ```qml
   // 启用性能监控
   ModernPerformance {
       Component.onCompleted: {
           setPerformanceProfile("medium")
       }
   }
   ```

4. **移动端显示问题**
   ```qml
   // 检查响应式配置
   anchors.margins: responsive.baseMargin
   height: responsive.isMobile ? 60 : 48
   ```

### 调试工具

```qml
// 启用调试模式
ModernPerformance {
    Component.onCompleted: {
        // 显示性能统计
        console.log("Performance Stats:", getPerformanceStats())
    }
}
```

## 🔄 升级指南

### 从旧版本升级

1. **备份现有界面**
   ```bash
   cp -r qmlui/Main.qml qmlui/Main.qml.backup
   ```

2. **渐进式替换**
   ```qml
   // 可以混合使用新旧组件
   Column {
       // 新的现代化组件
       ModernCard {
           title: "新功能"
       }

       // 旧的组件（逐步替换）
       Frame {
           // 旧的内容
       }
   }
   ```

3. **数据迁移**
   ```qml
   // 迁移设置
   function migrateSettings() {
       var oldSettings = oldSettingsComponent.getSettings()
       var newSettings = mapLegacySettings(oldSettings)
       applyNewSettings(newSettings)
   }
   ```

---

## 📞 支持

如果在使用过程中遇到问题，请：

1. 查阅 `README.md` 了解设计理念
2. 检查 `DEPLOYMENT.md` 中的配置说明
3. 运行内置的性能诊断工具
4. 查看控制台输出的错误信息

**现代化界面团队** 🎨