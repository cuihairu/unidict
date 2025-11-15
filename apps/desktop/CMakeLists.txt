cmake_minimum_required(VERSION 3.20)

project(UniDictDesktop
    VERSION 1.0.0
    DESCRIPTION "UniDict Desktop Application"
    LANGUAGES CXX
)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Quick
    Qml
    QuickControls2
    TextToSpeech
    QuickDialogs2
    Multimedia
    Network
    Concurrent
)

# 自动处理Qt的MOC、UIC、RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 源文件
set(DESKTOP_SOURCES
    main.cpp
    lookup_adapter.cpp
    learning_manager.cpp
    mobile_utils.cpp
)

# 头文件
set(DESKTOP_HEADERS
    lookup_adapter.h
    learning_manager.h
    mobile_utils.h
)

# 资源文件
set(DESKTOP_RESOURCES
    resources.qrc
)

# 创建可执行文件
qt_add_executable(unidict_desktop
    ${DESKTOP_SOURCES}
    ${DESKTOP_HEADERS}
    ${DESKTOP_RESOURCES}
)

# 设置目标属性
set_target_properties(unidict_desktop PROPERTIES
    OUTPUT_NAME "UniDict"
    WIN32_EXECUTABLE TRUE
)

# macOS Bundle配置
if(APPLE)
    set_target_properties(unidict_desktop PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "UniDict"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_IDENTIFIER "com.unidict.desktop"
    )
endif()

# 包含目录
target_include_directories(unidict_desktop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../packages
)

# 链接Qt库
target_link_libraries(unidict_desktop PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::TextToSpeech
    Qt6::QuickDialogs2
    Qt6::Multimedia
    Qt6::Network
    Qt6::Concurrent
)

# 链接UniDict核心库
if(TARGET unidict_core_qt)
    target_link_libraries(unidict_desktop PRIVATE unidict_core_qt)
elseif(TARGET UniDict::CoreQt)
    target_link_libraries(unidict_desktop PRIVATE UniDict::CoreQt)
elseif(TARGET UniDict::Core)
    target_link_libraries(unidict_desktop PRIVATE UniDict::Core)
endif()

# 尝试链接全文索引库
if(TARGET unidict_fulltext_qt)
    target_link_libraries(unidict_desktop PRIVATE unidict_fulltext_qt)
endif()

# 安装规则
if(APPLE)
    install(TARGETS unidict_desktop
        BUNDLE DESTINATION .
    )
elseif(WIN32)
    install(TARGETS unidict_desktop
        RUNTIME DESTINATION bin
    )
else()
    install(TARGETS unidict_desktop
        RUNTIME DESTINATION bin
    )
endif()

# QML模块注册（如果需要）
qt_add_qml_module(unidict_desktop
    URI UniDict.Desktop
    VERSION 1.0
    # QML文件会在这里注册
)
