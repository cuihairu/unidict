# Crashpad compile options interface target.
add_library(crashpad_common INTERFACE)

if(NOT MSVC)
    target_compile_options(crashpad_common INTERFACE
        -Wall
        -Wendif-labels
        -Wextra
        # 适配改动（2026-10，vendor 自 TheAssemblyArmada/crashpad-cmake 后
        # 按宿主编译器收敛）：
        #  - 去 -Werror：上游 2020 年代源码撞 gcc 15 新告警不应打断构建
        #  - -include cstdint：gcc 13+ 不再传递包含 <cstdint>，上游头
        #    缺它（uint32_t 未声明连环错）——强制预包含，不动上游源码
        -Wno-error
        $<$<COMPILE_LANGUAGE:CXX>:-include cstdint>
        -Wno-missing-field-initializers
		-Wno-noexcept-type
        -Wno-unused-parameter
        -Wno-implicit-fallthrough
        -Wsign-compare
        -Wno-multichar
        -fno-exceptions
        -fno-rtti
        -fno-strict-aliasing
        -fstack-protector-all
        -fdata-sections
        -ffunction-sections
    )
	
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_compile_options(crashpad_common INTERFACE
			-Wno-attributes
			-Wno-ignored-qualifiers
			-Wno-stringop-truncation
			-Wno-restrict
		)
	endif()
else()
    target_compile_options(crashpad_common INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:
        /FS
        /W4
        # 去 /WX、加 /FIcstdint：同非 MSVC 分支的适配理由（新编译器
        # 告警不打断构建；MSVC 头同样不保证传递 <cstdint>）
        /FIcstdint
        /Zi
        /bigobj
        /wd4100
        /wd4127
        /wd4324
        /wd4351
        /wd4577
        /wd4996
        /wd4201
        /Zc:inline
        /d2Zi+
        >
    )
endif()

if(WIN32)
    target_compile_definitions(crashpad_common INTERFACE
        NOMINMAX
        UNICODE
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _HAS_EXCEPTIONS=0
        _UNICODE
    )

    target_link_libraries(crashpad_common INTERFACE
        advapi32.lib
        winhttp.lib
        version.lib
        user32.lib
        PowrProf.lib
    )
else()
    target_link_libraries(crashpad_common INTERFACE
      Threads::Threads
      ${CMAKE_DL_LIBS}
    )
endif()

if(APPLE)
    target_link_options(crashpad_common INTERFACE -Wl,-dead_strip)
endif()

if(ANDROID)
    target_link_libraries(crashpad_common INTERFACE -llog)
endif()

target_compile_definitions(crashpad_common INTERFACE
    -D_FILE_OFFSET_BITS=64
    -DCRASHPAD_ZLIB_SOURCE_SYSTEM
    -DCRASHPAD_LSS_SOURCE_EXTERNAL
)

target_include_directories(crashpad_common INTERFACE
    ${crashpad_git_SOURCE_DIR}
    ${mini_chromium_git_SOURCE_DIR}
)
