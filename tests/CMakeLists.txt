# Test suite — Unity-based.

include(${CMAKE_SOURCE_DIR}/cmake/deps/unity.cmake)

# Helper: register a test executable + ctest entry.
function(sf_add_test name source label)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE souls_formats_static unity bcrypt)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/${label}")
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
sf_add_test(souls_formats_test_sf_util       core/test_sf_util.c       "core")
sf_add_test(souls_formats_test_dds_header    core/test_dds_header.c    "core")

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
sf_add_test(souls_formats_test_sl2_kat crypto/test_sl2_kat.c "crypto")
sf_add_test(souls_formats_test_rsa crypto/test_rsa.c "crypto")
target_link_libraries(souls_formats_test_rsa PRIVATE crypt32)

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
# Phase 3 — archive containers
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_binder_common archive/test_binder_common.c "archive")
sf_add_test(souls_formats_test_bhd5_synthetic archive/test_bhd5_synthetic.c "archive")
sf_add_test(souls_formats_test_bnd3_synthetic archive/test_bnd3_synthetic.c "archive")
sf_add_test(souls_formats_test_bnd4_synthetic archive/test_bnd4_synthetic.c "archive")
sf_add_test(souls_formats_test_bxf3_synthetic archive/test_bxf3_synthetic.c "archive")
sf_add_test(souls_formats_test_bxf4_synthetic archive/test_bxf4_synthetic.c "archive")
sf_add_test(souls_formats_test_enfl_synthetic archive/test_enfl_synthetic.c "archive")
sf_add_test(souls_formats_test_tpf_synthetic  archive/test_tpf_synthetic.c  "archive")

#-----------------------------------------------------------------------------
# Phase 3 — e2e tests (require ER copy + Oodle DLL; gracefully skip otherwise)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_er_helper_smoke e2e/test_er_helper_smoke.c "e2e")
target_include_directories(souls_formats_test_er_helper_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_sources(souls_formats_test_er_helper_smoke PRIVATE
    e2e/er_test_helper.c)
target_compile_definitions(souls_formats_test_er_helper_smoke PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_bhd5_e2e_er e2e/test_bhd5_e2e_er.c "e2e")
target_include_directories(souls_formats_test_bhd5_e2e_er PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_sources(souls_formats_test_bhd5_e2e_er PRIVATE
    e2e/er_test_helper.c)
target_compile_definitions(souls_formats_test_bhd5_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_bnd4_e2e_er e2e/test_bnd4_e2e_er.c "e2e")
target_include_directories(souls_formats_test_bnd4_e2e_er PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_sources(souls_formats_test_bnd4_e2e_er PRIVATE
    e2e/er_test_helper.c)
target_compile_definitions(souls_formats_test_bnd4_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_bxf4_e2e_er e2e/test_bxf4_e2e_er.c "e2e")
target_include_directories(souls_formats_test_bxf4_e2e_er PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_sources(souls_formats_test_bxf4_e2e_er PRIVATE
    e2e/er_test_helper.c)
target_compile_definitions(souls_formats_test_bxf4_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_tpf_e2e_er e2e/test_tpf_e2e_er.c "e2e")
target_include_directories(souls_formats_test_tpf_e2e_er PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_sources(souls_formats_test_tpf_e2e_er PRIVATE
    e2e/er_test_helper.c)
target_compile_definitions(souls_formats_test_tpf_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

#-----------------------------------------------------------------------------
# Phase 4 — PARAM/PARAMDEF
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_paramdef_bitstream param/test_paramdef_bitstream.c "param")
sf_add_test(souls_formats_test_param_binary_read param/test_param_binary_read.c "param")
sf_add_test(param_binary_write param/test_param_binary_write.c "param")
sf_add_test(param_apply_paramdef param/test_param_apply_paramdef.c "param")
sf_add_test(souls_formats_test_paramdef_binary_read param/test_paramdef_binary_read.c "param")
sf_add_test(paramdef_binary_write param/test_paramdef_binary_write.c "param")
sf_add_test(paramdef_xml_read param/test_paramdef_xml_read.c "param")
sf_add_test(paramtdf_read param/test_paramtdf_read.c "param")
sf_add_test(paramtdf_write param/test_paramtdf_write.c "param")
sf_add_test(fmg_read param/test_fmg_read.c "param")
sf_add_test(fmg_write param/test_fmg_write.c "param")
sf_add_test(synthetic_roundtrip param/test_synthetic_roundtrip.c "param")
sf_add_test(paramdef_xml_e2e param/test_paramdef_xml_e2e.c "param")
sf_add_test(param_apply_paramdef_e2e param/test_param_apply_paramdef_e2e.c "param")
sf_add_test(fmg_e2e_er param/test_fmg_e2e_er.c "param")

#-----------------------------------------------------------------------------
# Phase 4 — Script
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_emevd_read script/test_emevd_read.c "script")
sf_add_test(emevd_write script/test_emevd_write.c "script")
sf_add_test(emevd_synthetic script/test_emevd_synthetic.c "script")
sf_add_test(emevd_e2e_er script/test_emevd_e2e_er.c "script")

#-----------------------------------------------------------------------------
# Phase 4 — pre-flight empirical probe (T0.1)
# Reads real ER data and writes findings to
# .sisyphus/evidence/phase4-pre-flight.md. No format parsing here.
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_emevd_format_probe script/test_emevd_format_probe.c "script")
target_include_directories(souls_formats_test_emevd_format_probe PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_sources(souls_formats_test_emevd_format_probe PRIVATE
    e2e/er_test_helper.c)
# Escape backslashes for both CMake and the C preprocessor so the macro
# value reaches the compiler as L"...\\Ubuntu\\..." verbatim.
target_compile_definitions(souls_formats_test_emevd_format_probe PRIVATE
    SF_E2E_REPO_DIR=L"${CMAKE_SOURCE_DIR}"
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

#-----------------------------------------------------------------------------
# Pre-refactor SHA-256 baseline — pins deterministic byte outputs across
# the api-realignment effort. See tests/golden/golden_hashes.h.
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_golden golden/capture_golden.c "golden")
target_include_directories(souls_formats_test_golden
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/golden)
