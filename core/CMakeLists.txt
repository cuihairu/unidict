# core/CMakeLists.txt

# Create a static library for our core logic.
add_library(unidict_core STATIC
    unidict_core.cpp
    unidict_core.h
    stardict_parser.cpp
    stardict_parser.h
    mdict_parser.cpp
    mdict_parser.h
    index_engine.cpp
    index_engine.h
)

# The core library needs access to Qt's Core module.
target_link_libraries(unidict_core PRIVATE
    Qt6::Core
)

# Add zlib for MDict decompression
find_package(ZLIB REQUIRED)
target_link_libraries(unidict_core PRIVATE ZLIB::ZLIB)

# This makes sure that other targets that link against this library
# can find its header files.
target_include_directories(unidict_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
