# gui/CMakeLists.txt

# Create the executable for our GUI application.
add_executable(unidict_gui
    main.cpp
    # 剪贴板取词监控（纯 QObject，qmlui 侧同源复用，无 QML 依赖）
    ${CMAKE_SOURCE_DIR}/qmlui/clipboard_monitor.cpp
)

target_include_directories(unidict_gui PRIVATE ${CMAKE_SOURCE_DIR}/qmlui)

# Link the GUI application against our core library and Qt's GUI modules.
target_link_libraries(unidict_gui PRIVATE
    unidict_core_qt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
