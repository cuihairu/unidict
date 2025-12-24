# qmlui/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# 如果不是作为子目录被包含，则设置项目
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(UnidictQML VERSION 1.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick Qml QuickControls2 TextToSpeech QuickDialogs2)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

# 移动平台检测
set(IS_MOBILE FALSE)
if(ANDROID OR IOS)
    set(IS_MOBILE TRUE)
endif()

# 可执行文件创建
if(ANDROID)
    # Android应用
    qt_add_executable(unidict_qml
        main.cpp
        lookup_adapter.cpp
        lookup_adapter.h
        clipboard_monitor.cpp
        clipboard_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        mobile_utils.cpp
        mobile_utils.h
        learning_manager.cpp
        learning_manager.h
        ../adapters/qt/sync_service_qt.cpp
        resources.qrc
    )
    set_target_properties(unidict_qml PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mobile/android
    )
elseif(IOS)
    # iOS应用
    qt_add_executable(unidict_qml
        main.cpp
        lookup_adapter.cpp
        lookup_adapter.h
        clipboard_monitor.cpp
        clipboard_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        mobile_utils.cpp
        mobile_utils.h
        learning_manager.cpp
        learning_manager.h
        ../adapters/qt/sync_service_qt.cpp
        resources.qrc
    )
    set_target_properties(unidict_qml PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/mobile/ios/Info.plist
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "Your Team ID"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.yourcompany.unidict"
    )
else()
    # 桌面应用
    add_executable(unidict_qml
        main.cpp
        lookup_adapter.cpp
        lookup_adapter.h
        clipboard_monitor.cpp
        clipboard_monitor.h
        global_hotkeys.cpp
        global_hotkeys.h
        ../adapters/qt/ai_service_qt.cpp
        ../adapters/qt/clipboard_qt.cpp
        ../adapters/qt/settings_qt.cpp
        mobile_utils.cpp
        mobile_utils.h
        learning_manager.cpp
        learning_manager.h
        ../adapters/qt/sync_service_qt.cpp
        resources.qrc
    )
endif()

# 平台特定的链接库
target_link_libraries(unidict_qml PRIVATE
    unidict_core_qt
    unidict_fulltext_qt
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::TextToSpeech
)

# Android特定配置
if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS AndroidExtras)
    target_link_libraries(unidict_qml PRIVATE Qt6::AndroidExtras)

    # Android权限和特性
    set_target_properties(unidict_qml PROPERTIES
        QT_ANDROID_MINIMUM_SDK_VERSION 23
        QT_ANDROID_TARGET_SDK_VERSION 31
    )
endif()

# iOS特定配置
if(IOS)
    target_link_libraries(unidict_qml PRIVATE
        "-framework Foundation"
        "-framework UIKit"
        "-framework AVFoundation"
    )
endif()

target_include_directories(unidict_qml PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# QML模块注册
if(IS_MOBILE)
    qt_add_qml_module(unidict_qml
        URI UniDict.Mobile
        VERSION 1.0
        QML_FILES
            mobile/common/ResponsiveLayout.qml
            mobile/common/MobileFileDialog.qml
            mobile/common/DefinitionContent.qml
    )
endif()
