# cli/CMakeLists.txt

# Create the executable for our command-line interface.
add_executable(unidict_cli
    main.cpp
)

# Link the CLI application against our core library and Qt's Core module.
# Note that we don't need Qt's GUI modules here.
target_link_libraries(unidict_cli PRIVATE
    unidict_core_qt
    Qt6::Core
)
