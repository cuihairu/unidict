# qmlui/CMakeLists.txt

add_executable(unidict_qml
    main.cpp
    lookup_adapter.cpp
    lookup_adapter.h
    resources.qrc
)

target_link_libraries(unidict_qml PRIVATE
    unidict_core
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

target_include_directories(unidict_qml PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
