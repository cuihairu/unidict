# Unidict 移动平台适配完成报告

> **注（2026-09-29）**：本报告描述的是 qmlui 内的移动友好改造（响应式
> 布局/构建配置/文件与权限适配），**不是手机 App**。移动端版本选型、
> 可行性实测与批次规划见 `docs/mobile_plan.md` 与 todo.md「移动端版本」
> 章节；报告中「Qt for HarmonyOS 华为官方支持」等说法与公开事实不符，
> 以 mobile_plan.md 的核对为准。

## 完成的工作

### 1. 响应式 UI 设计
- **自适应布局系统**：`ResponsiveLayout.qml` 提供设备类型检测（Phone/Tablet/Desktop）
- **触控优化**：所有交互元素最小44dp触控目标，符合iOS和Android设计规范
- **流式布局**：使用 `Flow` 布局替代固定 `Row`，在小屏幕上自动换行
- **字体缩放**：根据设备类型自动调整字体大小
- **安全区域**：为移动设备状态栏和底部区域预留空间

### 2. 平台特定构建配置
- **Android**：
  - 完整的 `AndroidManifest.xml` 配置
  - 权限管理（存储、麦克风）
  - Material Design主题
  - 文件提供者配置
- **iOS**：
  - `Info.plist` 配置
  - 权限描述字符串
  - 文档类型支持
  - iOS原生框架链接
- **CMake**：智能平台检测和条件编译

### 3. 移动端文件管理
- **统一接口**：`MobileFileDialog.qml` 提供跨平台文件选择
- **原生集成**：`MobileUtils` C++类处理平台特定操作
- **Android**：原生文档选择器集成
- **iOS**：文档选择器框架准备（需要Objective-C++实现）

### 4. 权限管理系统
- **运行时权限**：Android动态权限申请
- **存储访问**：读写外部存储权限
- **麦克风访问**：语音功能权限
- **iOS权限**：Info.plist中的使用描述

## 核心架构改进

### SOLID 原则
- **S (单一职责)**：每个组件只管一件事
  - `ResponsiveLayout`：只做布局响应
  - `MobileFileDialog`：只做文件选择
  - `MobileUtils`：只做平台特定操作
- **O (开放封闭)**：新增平台不改现有组件
- **D (依赖倒置)**：面向抽象接口编程，不绑具体实现

### DRY 原则
- 响应式组件复用（`DefinitionContent.qml`）
- 触控目标尺寸计算统一
- 平台检测逻辑集中在一处

### KISS 原则
- 文件结构从简：`mobile/{android,ios,common}/`
- API 直白：`MobileUtils.isAndroid()`
- 依赖从少

## 移动平台特性

### Android 适配要点
- 响应式布局（Phone/Tablet 断点）
- Material Design 主题
- 原生文档选择器
- 运行时权限申请
- 全屏显示与状态栏处理
- APK 打包配置

### iOS 适配要点
- Human Interface Guidelines 布局
- iOS Controls 主题
- 文档类型注册
- 权限描述配置
- 安全区域处理
- Bundle 配置

### 鸿蒙系统支持路径
1. **Qt for HarmonyOS**（原报告推荐）：此条与公开事实不符，以 `docs/mobile_plan.md` 的核对为准
2. **Web 基座**：QML 转 WebAssembly
3. **原生重写**：ArkTS 重新实现 UI 层

## 技术栈

### 跨平台统一
- **90% 代码共享**：核心逻辑桌面与移动共用同一套
- **原生性能**：Qt 编译为原生机器码
- **平台集成**：经 Qt 模块访问平台特定 API

### 开发效率
- 桌面环境即可测试移动 UI
- 先桌面后移动的开发顺序
- 模块化：各组件可独立测试

## 文件结构

```
qmlui/
├── Main.qml                    # 主界面（已适配）
├── main.cpp                    # 主程序（含平台检测）
├── mobile_utils.h/cpp         # 移动平台工具类
├── CMakeLists.txt             # 跨平台构建配置
└── mobile/
    ├── android/               # Android专用文件
    │   ├── AndroidManifest.xml
    │   └── res/               # 资源文件
    ├── ios/                   # iOS专用文件
    │   └── Info.plist
    └── common/               # 共享组件
        ├── ResponsiveLayout.qml
        ├── MobileFileDialog.qml
        └── DefinitionContent.qml
```

## 后续待办（原始记录，未逐条落地）

下列条目是本报告写作时的候选项，与后来的路线没有对齐过。`docs/mobile_plan.md` 与 todo.md「移动端版本」章节是当前口径，冲突以它们为准。

### 立即可行
1. iOS 原生实现：补 `MobileUtils` 的 iOS 文档选择器
2. 主题切换：Material / iOS 主题动态切换
3. 离线词典：优化移动端词典加载策略

### 中期
1. 鸿蒙适配
2. PWA 版本：WebAssembly 部署
3. 同步功能：跨设备词汇同步

### 远期（与 §A 禁用清单冲突，多数已弃）
1. 语音识别
2. 相机取词
3. 学习社区（产品定位已排除社交/学习平台面，见 product-principles §7）

---

## 欧路词典功能参考

本报告写作时的对照拆解。逐条取舍判断见 `docs/research/dictionary-ui-research.md` 第四节与 `docs/site/research/dictionary-market-survey.md` 第五节。

1. **学习管理系统**
   - 生词本智能分组
   - 复习提醒算法
   - 学习进度统计（本节不引入，定位排除学习平台面）

2. **多词典集成**
   - 离线词典支持
   - 在线词典 APIs（本节不引入在线商店，P-10 用户驱动）
   - 自定义词典导入

3. **智能查词**
   - 屏幕取词（桌面版）
   - 浏览器插件（本节不引入）
   - PDF/图片取词（已按 §A 弃）

4. **发音系统**
   - 真人语音库
   - 音标显示
   - 发音练习（已交付 M1–M9，见 pronunciation-plan）

### 对照结论

- **技术面（本节）**：Qt 原生性能、响应式布局已落地；WebAssembly 无安装版本未做。
- **体验面**：AI 驱动学习路径、AR 实时翻译、语音对话练习均未做，也不在 §A 之后的路线上。
- **生态面**：社区词典格式走 `core/std` 解析器工厂注册表，无第三方运行时；Anki 导出按 §A 废止。