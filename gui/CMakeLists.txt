# gui/CMakeLists.txt

# Create the executable for our GUI application.
add_executable(unidict_gui
    main.cpp
)

# Link the GUI application against our core library and Qt's GUI modules.
target_link_libraries(unidict_gui PRIVATE
    unidict_core_qt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
