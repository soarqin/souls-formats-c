# Test suite — Unity-based.

include(${CMAKE_SOURCE_DIR}/cmake/deps/unity.cmake)

# Helper: register a test executable + ctest entry.
function(sf_add_test name source label)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE souls_formats_static unity bcrypt)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    sf_apply_warnings(${name})
    sf_apply_sanitizers(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "${label}" TIMEOUT 60)
endfunction()

#-----------------------------------------------------------------------------
# Phase 0 — smoke (BCrypt + ABI sanity)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_smoke unity_runner.c "smoke")

#-----------------------------------------------------------------------------
# Phase 1 — core runtime
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_filename_hash core/test_filename_hash.c "core")
sf_add_test(souls_formats_test_encoding      core/test_encoding.c      "core")
sf_add_test(souls_formats_test_binary_reader core/test_binary_reader.c "core")
sf_add_test(souls_formats_test_binary_writer core/test_binary_writer.c "core")
sf_add_test(souls_formats_test_path          core/test_path.c          "core")

#-----------------------------------------------------------------------------
# Phase 2 — compression + crypto
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_dcx_dflt compression/test_dcx_dflt.c "compression")
sf_add_test(souls_formats_test_dcx_edge compression/test_dcx_edge.c "compression")
sf_add_test(souls_formats_test_dcx_zstd compression/test_dcx_zstd.c "compression")
sf_add_test(souls_formats_test_dcx_krak compression/test_dcx_krak.c "compression")
sf_add_test(souls_formats_test_dcx_sniff compression/test_dcx_sniff.c "compression")

sf_add_test(souls_formats_test_aes_kat crypto/test_aes_kat.c "crypto")
sf_add_test(souls_formats_test_md5_kat crypto/test_md5_kat.c "crypto")
sf_add_test(souls_formats_test_regulation_decrypt crypto/test_regulation_decrypt.c "crypto")

add_custom_target(souls_formats_test_compression
    DEPENDS
        souls_formats_test_dcx_dflt
        souls_formats_test_dcx_edge
        souls_formats_test_dcx_zstd
        souls_formats_test_dcx_krak
        souls_formats_test_dcx_sniff)

add_custom_target(souls_formats_test_crypto
    DEPENDS
        souls_formats_test_aes_kat
        souls_formats_test_md5_kat
        souls_formats_test_regulation_decrypt)

#-----------------------------------------------------------------------------
# Pre-refactor SHA-256 baseline — pins deterministic byte outputs across
# the api-realignment effort. See tests/golden/golden_hashes.h.
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_golden golden/capture_golden.c "golden")
target_include_directories(souls_formats_test_golden
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/golden)
