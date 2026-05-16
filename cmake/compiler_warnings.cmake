# sf_apply_warnings(<target>)  — strict warnings + warnings-as-errors
# (suppressing a small allowlist that exists only to make MSVC quieter)

function(sf_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /WX
            /permissive-
            /utf-8
            /D_CRT_SECURE_NO_WARNINGS
            /D_CRT_NONSTDC_NO_WARNINGS
            /wd4200  # nonstandard extension: zero-sized array in struct/union
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wshadow
            -Wcast-align
            -Wstrict-prototypes
            -Wmissing-prototypes
            -Wpointer-arith
            -Wundef
            -Wno-unused-parameter
        )
        # GCC-only — -Wshadow is enough for clang
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE -Wlogical-op -Wduplicated-cond)
        endif()
        # Clang 16+ promotes UINT32_MAX enum sentinels to a C23 extension
        # warning. Our enums intentionally use UINT32_MAX as an "unknown/other"
        # sentinel; suppress the pedantic diagnostic rather than change the API.
        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE -Wno-c23-extensions)
        endif()
    endif()
endfunction()
