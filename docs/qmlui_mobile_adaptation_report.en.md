> Language: **English** | [简体中文](./qmlui_mobile_adaptation_report.md)

# Unidict Mobile Platform Adaptation Completion Report

> **Note (2026-09-29)**: this report describes mobile-friendly refactoring inside
> qmlui (responsive layout / build configuration / file and permission adaptation),
> **not a phone app**. For the mobile version selection, feasibility measurements, and
> batch planning, see `docs/mobile_plan.md` and the "mobile version" section of
> todo.md; claims in this report such as "Qt for HarmonyOS, officially supported by
> Huawei" do not match public facts -- defer to the verification in mobile_plan.md.

## Completed Work

### 1. Responsive UI Design
- **Adaptive layout system**: `ResponsiveLayout.qml` provides device type detection (Phone/Tablet/Desktop)
- **Touch optimization**: all interactive elements have a minimum 44dp touch target, conforming to iOS and Android design guidelines
- **Flow layout**: uses `Flow` layout instead of a fixed `Row`, wrapping automatically on small screens
- **Font scaling**: font size adjusts automatically by device type
- **Safe areas**: reserves space for the mobile status bar and bottom region

### 2. Platform-Specific Build Configuration
- **Android**:
  - Complete `AndroidManifest.xml` configuration
  - Permission management (storage, microphone)
  - Material Design theme
  - File provider configuration
- **iOS**:
  - `Info.plist` configuration
  - Permission description strings
  - Document type support
  - iOS native framework linking
- **CMake**: smart platform detection and conditional compilation

### 3. Mobile File Management
- **Unified interface**: `MobileFileDialog.qml` provides cross-platform file selection
- **Native integration**: the `MobileUtils` C++ class handles platform-specific operations
- **Android**: native document picker integration
- **iOS**: document picker framework prepared (requires an Objective-C++ implementation)

### 4. Permission Management System
- **Runtime permissions**: Android dynamic permission requests
- **Storage access**: read/write external storage permissions
- **Microphone access**: voice feature permissions
- **iOS permissions**: usage descriptions in Info.plist

## Core Architecture Improvements

### SOLID Principles
- **S (Single Responsibility)**: each component does only one thing
  - `ResponsiveLayout`: only layout response
  - `MobileFileDialog`: only file selection
  - `MobileUtils`: only platform-specific operations
- **O (Open/Closed)**: adding a platform does not modify existing components
- **D (Dependency Inversion)**: program to abstract interfaces, not concrete implementations

### DRY Principle
- Responsive component reuse (`DefinitionContent.qml`)
- Unified touch-target size calculation
- Platform detection logic concentrated in one place

### KISS Principle
- Simple file structure: `mobile/{android,ios,common}/`
- Straightforward API: `MobileUtils.isAndroid()`
- Few dependencies

## Mobile Platform Features

### Android Adaptation Points
- Responsive layout (Phone/Tablet breakpoints)
- Material Design theme
- Native document picker
- Runtime permission requests
- Full-screen display and status bar handling
- APK packaging configuration

### iOS Adaptation Points
- Human Interface Guidelines layout
- iOS Controls theme
- Document type registration
- Permission description configuration
- Safe area handling
- Bundle configuration

### HarmonyOS Support Path
1. **Qt for HarmonyOS** (originally recommended by this report): this item does not match public facts -- defer to the verification in `docs/mobile_plan.md`
2. **Web base**: QML to WebAssembly
3. **Native rewrite**: reimplement the UI layer in ArkTS

## Tech Stack

### Cross-Platform Unification
- **90% code sharing**: core logic is the same set on desktop and mobile
- **Native performance**: Qt compiles to native machine code
- **Platform integration**: access platform-specific APIs through Qt modules

### Development Efficiency
- Mobile UI can be tested in the desktop environment
- Desktop-first, mobile-second development order
- Modular: each component can be tested independently

## File Structure

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

## Remaining To-Dos (Original Record, Not Individually Implemented)

The items below were candidates at the time this report was written and were never aligned with the later roadmap. `docs/mobile_plan.md` and the "mobile version" section of todo.md are the current position; where they conflict, defer to those.

### Immediately Feasible
1. iOS native implementation: add the iOS document picker to `MobileUtils`
2. Theme switching: dynamic switching between Material / iOS themes
3. Offline dictionaries: optimize the mobile dictionary loading strategy

### Medium Term
1. HarmonyOS adaptation
2. PWA version: WebAssembly deployment
3. Sync feature: cross-device vocabulary sync

### Long Term (Conflicts with the §A Do-Not-Do List, Most Already Dropped)
1. Voice recognition
2. Camera word capture
3. Learning community (the product positioning already excludes the social/learning-platform surface, see product-principles §7)

---

## Eudic Feature Reference

A comparison breakdown made when this report was written. Per-item trade-off judgments are in section 4 of `docs/research/dictionary-ui-research.md` and section 5 of `docs/site/research/dictionary-market-survey.md`.

1. **Learning Management System**
   - Smart vocabulary grouping
   - Review reminder algorithm
   - Learning progress statistics (not introduced in this section; the positioning excludes the learning-platform surface)

2. **Multi-Dictionary Integration**
   - Offline dictionary support
   - Online dictionary APIs (no online store introduced in this section, P-10 user-driven)
   - Custom dictionary import

3. **Intelligent Lookup**
   - Screen word capture (desktop version)
   - Browser extension (not introduced in this section)
   - PDF/image word capture (already dropped per §A)

4. **Pronunciation System**
   - Real human voice library
   - Phonetic symbol display
   - Pronunciation practice (delivered M1–M9, see pronunciation-plan)

### Comparison Conclusion

- **Technical side (this section)**: Qt native performance and responsive layout are landed; the WebAssembly no-install version was not done.
- **Experience side**: AI-driven learning paths, AR real-time translation, and voice conversation practice were all not done and are not on the roadmap after §A either.
- **Ecosystem side**: community dictionary formats go through the `core/std` parser factory registry, with no third-party runtime; Anki export is abolished per §A.
