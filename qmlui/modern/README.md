# Unidict 现代化界面设计方案

## 概述

本设计完全重新定义了 Unidict 词典的用户界面，采用了现代设计理念，提供了更好的用户体验和视觉效果。

## 设计理念

### 1. 现代化美学
- **毛玻璃效果**：导航栏采用背景模糊，增强层次感
- **卡片式布局**：内容以卡片形式组织，清晰明了
- **渐变背景**：淡雅的渐变色增加视觉深度
- **圆角设计**：统一的圆角系统，营造柔和现代感

### 2. 响应式设计
- **多设备适配**：完美支持桌面、平板、手机
- **弹性布局**：自适应不同屏幕尺寸
- **触控优化**：移动端友好的触控目标尺寸

### 3. 交互动效
- **微交互动画**：悬停、点击等状态有细腻的动画反馈
- **页面转场**：流畅的页面切换动画
- **加载状态**：优雅的加载和过渡动画

## 组件架构

### 主题系统 (Theme.qml)
- 统一的颜色定义和管理
- 阴影系统（4个等级）
- 圆角系统（5个等级）
- 间距系统（6个等级）
- 动画时长配置
- 字体系统配置

### 核心组件

#### 1. ModernCard.qml
- 现代化卡片容器
- 支持阴影、边框、圆角
- 内置标题、副标题、内容区域
- 支持点击交互和悬停效果

#### 2. ModernButton.qml
- 5种按钮类型：Primary, Secondary, Outline, Ghost, Icon
- 3种尺寸：Small, Medium, Large
- 内置加载状态和禁用状态
- 支持图标和文本组合

#### 3. ModernSearchBox.qml
- 现代化搜索输入框
- 内置搜索图标和清除按钮
- 支持加载状态和防抖搜索
- 键盘快捷键支持

#### 4. ModernAnimations.qml
- 丰富的动画效果库
- 淡入、滑入、缩放、弹跳、脉冲等动画
- 页面转场动画
- 数值变化动画

### 页面组件

#### ModernSearchResultsPage.qml
- 现代化搜索结果展示
- 卡片式结果项
- 内置朗读、收藏、分享功能
- 空状态设计

## 使用方法

### 1. 替换现有主界面

```cpp
// 在 main.cpp 中使用现代化界面
// QQmlApplicationEngine engine;
// engine.load(QUrl(QStringLiteral("qrc:/qmlui/MainModern.qml")));
```

### 2. 集成现有功能

使用 `ModernBridge.qml` 桥接现有功能：

```qml
ModernBridge {
    anchors.fill: parent
    // 自动桥接现有的 LookupAdapter 和 ResponsiveLayout
}
```

### 3. 自定义主题

修改 `Theme.qml` 中的颜色配置：

```qml
// 主色调配置
readonly property color primary: "#6366F1"           // 主色
readonly property color primaryLight: "#818CF8"     // 亮主色
readonly property color primaryDark: "#4F46E5"      // 暗主色
```

## 文件结构

```
qmlui/
├── modern/
│   ├── Theme.qml                    # 主题配置
│   ├── MainModern.qml              # 现代化主界面
│   ├── ModernBridge.qml            # 功能桥接
│   ├── README.md                   # 说明文档
│   └── components/
│       ├── ModernCard.qml          # 现代化卡片
│       ├── ModernButton.qml        # 现代化按钮
│       ├── ModernSearchBox.qml     # 现代化搜索框
│       ├── ModernAnimations.qml    # 动画效果库
│       ├── ModernNavigationItem.qml # 导航项
│       └── ModernSearchResultsPage.qml # 搜索结果页
```

## 设计特点

### 1. 色彩系统
- **主色**：现代蓝紫色调 (#6366F1)
- **辅助色**：紫色 (#8B5CF6)、粉红 (#EC4899)
- **中性色**：柔和的灰色系统
- **状态色**：成功绿、警告橙、错误红

### 2. 字体系统
- **主字体**：SF Pro Display (macOS) / 系统字体 (其他平台)
- **字重**：Light, Regular, Medium, Semibold, Bold
- **字号**：适配不同场景的尺寸等级

### 3. 阴影系统
- 4个等级的阴影效果
- 基于真实物理的光影模型
- 适配卡片、按钮等不同组件

### 4. 动画系统
- **时长**：快速(150ms)、标准(250ms)、慢速(350ms)
- **缓动**：OutCubic、InCubic、InOutCubic
- **类型**：属性动画、序列动画、并行动画

## 兼容性

- **Qt版本**：Qt 5.15+ (兼容 Qt 6.x)
- **平台支持**：Windows、macOS、Linux、iOS、Android
- **屏幕密度**：支持标准和高DPI显示器

## 性能优化

1. **按需加载**：页面内容按需显示
2. **动画优化**：使用属性动画而非JavaScript动画
3. **内存管理**：合理的对象生命周期管理
4. **GPU加速**：利用layer.enabled进行硬件加速

## 扩展指南

### 添加新组件
1. 继承现有的设计系统
2. 使用统一的颜色、字体、间距
3. 遵循命名规范：Modern + 组件名.qml

### 自定义动画
1. 在 ModernAnimations.qml 中添加新动画
2. 使用标准的时长和缓动函数
3. 考虑性能和用户体验

### 主题定制
1. 修改 Theme.qml 中的配置
2. 支持深色/浅色主题切换
3. 可考虑添加品牌定制功能

## 设计原则

1. **一致性**：所有组件遵循统一的设计语言
2. **可访问性**：支持键盘导航和屏幕阅读器
3. **性能**：动画流畅，不影响核心功能
4. **可维护性**：模块化设计，易于扩展和维护

## 未来改进

1. **深色模式**：完整的深色主题支持
2. **个性化**：用户自定义颜色和布局
3. **国际化**：多语言界面支持
4. **无障碍**：增强可访问性功能
5. **插件系统**：第三方组件集成能力

---

这套现代化设计将显著提升 Unidict 的用户体验，使其在同类应用中具有更强的竞争力和吸引力。