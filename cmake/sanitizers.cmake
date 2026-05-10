# sf_apply_sanitizers(<target>)  — opt-in via -DSF_ENABLE_SANITIZERS=ON
# Only effective on Clang / clang-cl / MinGW GCC. MSVC ASan integration is
# possible but quirky; we don't enable it here.

function(sf_apply_sanitizers target)
    if(NOT SF_ENABLE_SANITIZERS)
        return()
    endif()

    if(MSVC)
        message(STATUS "SF_ENABLE_SANITIZERS=ON ignored on MSVC; use clang-cl")
        return()
    endif()

    set(_sans "address,undefined")
    target_compile_options(${target} PRIVATE
        -fsanitize=${_sans}
        -fno-omit-frame-pointer
        -g
    )
    target_link_options(${target} PRIVATE
        -fsanitize=${_sans}
    )
endfunction()
