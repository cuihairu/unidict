# §7 B5：C++ 版自带中转 unidict-relay（单二进制，Qt-free，随 std core 构建；
# 与 dev/worker 形态同一契约，契约符合性套件见 run_conformance.py）
add_executable(unidict-relay
    main.cpp
    relay_http.cpp
)

target_link_libraries(unidict-relay PRIVATE
    unidict_std_core
)

target_include_directories(unidict-relay PRIVATE
    "${CMAKE_SOURCE_DIR}/core"
)

if(WIN32)
    target_link_libraries(unidict-relay PRIVATE ws2_32)
endif()
