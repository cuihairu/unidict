# core/CMakeLists.txt

# Create a static library for our core logic.
# A static library is linked into the final executable at compile time.
add_library(unidict_core STATIC
    unidict_core.cpp
    unidict_core.h
)

# The core library needs access to Qt's Core module.
target_link_libraries(unidict_core PRIVATE
    Qt6::Core
)

# This makes sure that other targets that link against this library
# can find its header files.
target_include_directories(unidict_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
