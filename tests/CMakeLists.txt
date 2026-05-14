# Test suite — Unity-based.

include(${CMAKE_SOURCE_DIR}/cmake/deps/unity.cmake)

# Helper: register a test executable + ctest entry.
#
# Tests link souls_formats_STATIC so they do not depend on the DLL at
# runtime.  However, Windows DLL search also scans the directory that
# contains the .exe, and the shared library (libsouls_formats.dll) is
# built into CMAKE_BINARY_DIR.  If a user double-clicks a test .exe from
# Explorer the OS may still complain about a missing DLL.  We therefore:
#   1. Copy libsouls_formats.dll into every test output directory after
#      each build so that direct execution works without PATH changes.
#   2. Set the ctest ENVIRONMENT to prepend CMAKE_BINARY_DIR to PATH so
#      that ctest-driven runs also find any other runtime DLLs (e.g. the
#      Oodle DLL loaded at runtime by sf_oodle_set_search_path).
function(sf_add_test name source label)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE souls_formats_static unity bcrypt)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/${label}")
    sf_apply_warnings(${name})
    sf_apply_sanitizers(${name})

    # Copy the shared DLL alongside the test .exe so direct execution works.
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:souls_formats_shared>"
            "${CMAKE_BINARY_DIR}/tests/${label}/$<TARGET_FILE_NAME:souls_formats_shared>"
        COMMENT "Copying souls_formats DLL to tests/${label}/"
    )

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        LABELS "${label}"
        TIMEOUT 60
        ENVIRONMENT "PATH=${CMAKE_BINARY_DIR};$ENV{PATH}"
    )
endfunction()

# Labels are free-form; lighting will be used by the new phase-8 test targets.

# Helper: register an e2e test with both the umbrella 'e2e' label and a
# per-game label. Use this for any future e2e test registration so that
# both `ctest -L '^e2e$'` (run all e2e) and `ctest -L 'e2e_<game>'` (run a
# single game's e2e) work consistently.
function(sf_add_e2e_test name source game)
    sf_add_test(${name} ${source} "e2e_${game}")
    set_tests_properties(${name} PROPERTIES LABELS "e2e;e2e_${game}")
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
sf_add_test(souls_formats_test_no_absolute_paths core/test_no_absolute_paths.c "hygiene")
target_compile_definitions(souls_formats_test_no_absolute_paths PRIVATE
    SOULS_FORMATS_ROOT_DIR="${CMAKE_SOURCE_DIR}")

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
# Legacy binder formats (BND + BND2)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_bnd_synthetic  archive/test_bnd_synthetic.c  "legacy-binder")
sf_add_test(souls_formats_test_bnd2_synthetic archive/test_bnd2_synthetic.c "legacy-binder")

#-----------------------------------------------------------------------------
# T2.4 — shared e2e helper library
#
# Each per-game helper (er_test_helper.c, nightreign_test_helper.c,
# ac6_test_helper.c, sekiro_test_helper.c) used to be added to every
# consuming e2e test via target_sources(...), which recompiled the helper
# once per test target (~13 ER tests + others). The static lib below
# compiles each helper exactly once and lets consuming tests link against
# the resulting archive.
#
# Helper sources use #ifndef SF_E2E_* fallbacks that match the canonical
# paths each test target uses, so the precompiled object code holds the
# same path values that the prior target_sources approach produced. Each
# consuming *test source* still receives its own SF_E2E_* defines via
# target_compile_definitions() because some tests (e.g. test_bhd5_e2e_er)
# also reference those macros directly from the test body.
#
# PUBLIC include of tests/e2e propagates the helper *.h files to every
# consumer transitively. PUBLIC link of souls_formats_static + bcrypt
# matches sf_add_test() so adding souls_formats_e2e_helpers to a test
# target adds nothing redundant on top of what sf_add_test already linked.
#-----------------------------------------------------------------------------
add_library(souls_formats_e2e_helpers STATIC
    e2e/er_test_helper.c
    e2e/nightreign_test_helper.c
    e2e/ac6_test_helper.c
    e2e/sekiro_test_helper.c
    e2e/ds1r_test_helper.c
    e2e/ds2s_test_helper.c
    e2e/ds3_test_helper.c)
target_link_libraries(souls_formats_e2e_helpers PUBLIC souls_formats_static bcrypt)
target_include_directories(souls_formats_e2e_helpers PUBLIC
    ${CMAKE_SOURCE_DIR}/tests/e2e
    ${CMAKE_SOURCE_DIR}/src)
sf_apply_warnings(souls_formats_e2e_helpers)
sf_apply_sanitizers(souls_formats_e2e_helpers)

#-----------------------------------------------------------------------------
# Legacy Souls real-game e2e tests (DS1R loose files, DS2S loose PARAM, DS3 BHD5)
#-----------------------------------------------------------------------------
sf_add_e2e_test(souls_formats_test_ds1r_helper_smoke
    e2e/test_ds1r_helper_smoke.c "ds1r")
target_link_libraries(souls_formats_test_ds1r_helper_smoke PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_ds1r_helper_smoke PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

sf_add_e2e_test(souls_formats_test_msb1_e2e_ds1r
    map/msb1/test_msb1_e2e_ds1r.c "ds1r")
target_link_libraries(souls_formats_test_msb1_e2e_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_msb1_e2e_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

sf_add_e2e_test(souls_formats_test_bnd3_e2e_ds1r
    archive/test_bnd3_e2e_ds1r.c "ds1r")
target_link_libraries(souls_formats_test_bnd3_e2e_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_bnd3_e2e_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

sf_add_e2e_test(souls_formats_test_param_e2e_ds1r
    param/test_param_e2e_ds1r.c "ds1r")
target_link_libraries(souls_formats_test_param_e2e_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_param_e2e_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

sf_add_e2e_test(souls_formats_test_fmg_e2e_ds1r
    param/test_fmg_e2e_ds1r.c "ds1r")
target_link_libraries(souls_formats_test_fmg_e2e_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_fmg_e2e_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

sf_add_e2e_test(souls_formats_test_emevd_e2e_ds1r
    script/test_emevd_e2e_ds1r.c "ds1r")
target_link_libraries(souls_formats_test_emevd_e2e_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_emevd_e2e_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

sf_add_e2e_test(souls_formats_test_flver2_e2e_ds1r
    geom/test_flver2_e2e_ds1r.c "ds1r")
target_link_libraries(souls_formats_test_flver2_e2e_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_flver2_e2e_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

sf_add_e2e_test(souls_formats_test_mtd_e2e_ds1r
    geom/test_mtd_e2e_ds1r.c "ds1r")
target_link_libraries(souls_formats_test_mtd_e2e_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_mtd_e2e_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

sf_add_e2e_test(souls_formats_test_ds2s_helper_smoke
    e2e/test_ds2s_helper_smoke.c "ds2s")
target_link_libraries(souls_formats_test_ds2s_helper_smoke PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_ds2s_helper_smoke PRIVATE
    SF_E2E_DS2S_DIR=L"C:/Games/Dark Souls II Scholar of the First Sin/Game")

sf_add_e2e_test(souls_formats_test_param_e2e_ds2s
    param/test_param_e2e_ds2s.c "ds2s")
target_link_libraries(souls_formats_test_param_e2e_ds2s PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_param_e2e_ds2s PRIVATE
    SF_E2E_DS2S_DIR=L"C:/Games/Dark Souls II Scholar of the First Sin/Game")

sf_add_e2e_test(souls_formats_test_ds3_helper_smoke
    e2e/test_ds3_helper_smoke.c "ds3")
target_link_libraries(souls_formats_test_ds3_helper_smoke PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_ds3_helper_smoke PRIVATE
    SF_E2E_DS3_DIR=L"C:/Games/DARK SOULS III/Game"
    SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")

sf_add_e2e_test(souls_formats_test_msb3_e2e_ds3
    map/msb3/test_msb3_e2e_ds3.c "ds3")
target_link_libraries(souls_formats_test_msb3_e2e_ds3 PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_msb3_e2e_ds3 PRIVATE
    SF_E2E_DS3_DIR=L"C:/Games/DARK SOULS III/Game"
    SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")

sf_add_e2e_test(souls_formats_test_emevd_e2e_ds3
    script/test_emevd_e2e_ds3.c "ds3")
target_link_libraries(souls_formats_test_emevd_e2e_ds3 PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_emevd_e2e_ds3 PRIVATE
    SF_E2E_DS3_DIR=L"C:/Games/DARK SOULS III/Game"
    SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")

sf_add_e2e_test(souls_formats_test_flver2_e2e_ds3
    geom/test_flver2_e2e_ds3.c "ds3")
target_link_libraries(souls_formats_test_flver2_e2e_ds3 PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_flver2_e2e_ds3 PRIVATE
    SF_E2E_DS3_DIR=L"C:/Games/DARK SOULS III/Game"
    SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")

sf_add_e2e_test(souls_formats_test_fmg_e2e_ds3
    param/test_fmg_e2e_ds3.c "ds3")
target_link_libraries(souls_formats_test_fmg_e2e_ds3 PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_fmg_e2e_ds3 PRIVATE
    SF_E2E_DS3_DIR=L"C:/Games/DARK SOULS III/Game"
    SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")

#-----------------------------------------------------------------------------
# Phase 3 — e2e tests (require ER copy + Oodle DLL; gracefully skip otherwise)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_er_helper_smoke e2e/test_er_helper_smoke.c "e2e")
target_link_libraries(souls_formats_test_er_helper_smoke PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_er_helper_smoke PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")
 
# Phase 5 — Nightreign e2e helper
sf_add_test(souls_formats_test_nightreign_helper_smoke e2e/test_nightreign_helper_smoke.c "e2e_nightreign")
target_link_libraries(souls_formats_test_nightreign_helper_smoke PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_nightreign_helper_smoke PRIVATE
    SF_E2E_NIGHTREIGN_DIR=L"C:/Games/ELDEN RING NIGHTREIGN/Game"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")

sf_add_test(souls_formats_test_bhd5_e2e_er e2e/test_bhd5_e2e_er.c "e2e")
target_link_libraries(souls_formats_test_bhd5_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_bhd5_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

# Phase 5 — AC6 e2e helper
sf_add_test(souls_formats_test_ac6_helper_smoke e2e/test_ac6_helper_smoke.c "e2e_ac6")
target_link_libraries(souls_formats_test_ac6_helper_smoke PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_ac6_helper_smoke PRIVATE
    SF_E2E_AC6_DIR=L"C:/Games/ARMORED CORE VI FIRES OF RUBICON/Game"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_msbvi_e2e_ac6 map/msbvi/test_msbvi_e2e_ac6.c "e2e_ac6")
target_link_libraries(souls_formats_test_msbvi_e2e_ac6 PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_msbvi_e2e_ac6 PRIVATE
    SF_E2E_AC6_DIR=L"C:/Games/ARMORED CORE VI FIRES OF RUBICON/Game"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_bnd4_e2e_er e2e/test_bnd4_e2e_er.c "e2e")
target_link_libraries(souls_formats_test_bnd4_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_bnd4_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_bxf4_e2e_er e2e/test_bxf4_e2e_er.c "e2e")
target_link_libraries(souls_formats_test_bxf4_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_bxf4_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_tpf_e2e_er e2e/test_tpf_e2e_er.c "e2e")
target_link_libraries(souls_formats_test_tpf_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_tpf_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

#-----------------------------------------------------------------------------
# Phase 5 — script formats
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_esd_read script/test_esd_read.c "script")
sf_add_test(souls_formats_test_esd_write script/test_esd_write.c "script")
sf_add_test(souls_formats_test_esd_synthetic_rt script/test_esd_synthetic_rt.c "script")

#-----------------------------------------------------------------------------
# Phase 4 — PARAM/PARAMDEF
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_paramdef_bitstream param/test_paramdef_bitstream.c "param")
sf_add_test(souls_formats_test_param_binary_read param/test_param_binary_read.c "param")
sf_add_test(souls_formats_test_param_binary_write param/test_param_binary_write.c "param")
sf_add_test(souls_formats_test_param_apply_paramdef param/test_param_apply_paramdef.c "param")
sf_add_test(souls_formats_test_param_setters_int param/test_param_setters_int.c "param")
sf_add_test(souls_formats_test_param_setters_ext param/test_param_setters_ext.c "param")
sf_add_test(souls_formats_test_paramdef_binary_read param/test_paramdef_binary_read.c "param")
sf_add_test(souls_formats_test_paramdef_binary_write param/test_paramdef_binary_write.c "param")
sf_add_test(souls_formats_test_paramdef_xml_read param/test_paramdef_xml_read.c "param")
sf_add_test(paramtdf_read param/test_paramtdf_read.c "param")
sf_add_test(souls_formats_test_paramtdf_write param/test_paramtdf_write.c "param")
sf_add_test(fmg_read param/test_fmg_read.c "param")
sf_add_test(fmg_write param/test_fmg_write.c "param")
sf_add_test(souls_formats_test_synthetic_roundtrip param/test_synthetic_roundtrip.c "param")
sf_add_test(paramdef_xml_e2e param/test_paramdef_xml_e2e.c "param")
sf_add_test(param_apply_paramdef_e2e param/test_param_apply_paramdef_e2e.c "param")
target_link_libraries(param_apply_paramdef_e2e PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(param_apply_paramdef_e2e PRIVATE
    SOULS_FORMATS_ROOT_DIR="${CMAKE_SOURCE_DIR}"
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")
target_compile_definitions(paramdef_xml_e2e PRIVATE
    SOULS_FORMATS_ROOT_DIR="${CMAKE_SOURCE_DIR}")
sf_add_test(fmg_e2e_er param/test_fmg_e2e_er.c "param")
target_link_libraries(fmg_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(fmg_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")
add_custom_target(souls_formats_test_fmg_e2e_er DEPENDS fmg_e2e_er)

#-----------------------------------------------------------------------------
# Phase 4 — Script
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_emevd_read script/test_emevd_read.c "script")
sf_add_test(souls_formats_test_emevd_write script/test_emevd_write.c "script")
sf_add_test(souls_formats_test_emevd_synthetic script/test_emevd_synthetic.c "script")
sf_add_test(souls_formats_test_emevd_mutation script/test_emevd_mutation.c "script")
sf_add_test(emevd_e2e_er script/test_emevd_e2e_er.c "script")
target_link_libraries(emevd_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(emevd_e2e_er PRIVATE
    SF_E2E_REPO_DIR=L"${CMAKE_SOURCE_DIR}"
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")
add_custom_target(souls_formats_test_emevd_e2e_er DEPENDS emevd_e2e_er)

sf_add_test(param_setters_e2e_er param/test_param_setters_e2e_er.c "param")
target_link_libraries(param_setters_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(param_setters_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
    SF_E2E_PARAMDEX_DIR="${CMAKE_SOURCE_DIR}/../../dev/paramdex/ER/Defs")
add_custom_target(souls_formats_test_param_setters_e2e_er DEPENDS param_setters_e2e_er)

sf_add_test(emevd_mutation_e2e_er script/test_emevd_mutation_e2e_er.c "script")
target_link_libraries(emevd_mutation_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(emevd_mutation_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")
add_custom_target(souls_formats_test_emevd_mutation_e2e_er DEPENDS emevd_mutation_e2e_er)

#-----------------------------------------------------------------------------
# Phase 5 — Map (MSB common skeleton)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_msb_common_skeleton
    map/test_msb_common_skeleton.c "map")
sf_add_test(msbs_root_smoke
    map/msbs/test_msbs_root_smoke.c "map")
sf_add_test(souls_formats_test_msbs_model_param_synthetic
    map/msbs/test_model_param_synthetic.c "map")
sf_add_test(souls_formats_test_msbs_synthetic_rt
    map/msbs/test_msbs_synthetic_rt.c "map")
sf_add_test(souls_formats_test_msbs_event_param_synthetic
    map/msbs/test_event_param_synthetic.c "map")
sf_add_test(msbs_point_param_synthetic
    map/msbs/test_point_param_synthetic.c "map")
sf_add_test(msbs_parts_param_synthetic
    map/msbs/test_parts_param_synthetic.c "map")
sf_add_test(msbs_route_param_synthetic
    map/msbs/test_route_param_synthetic.c "map")
sf_add_test(msbe_root_smoke
    map/msbe/test_msbe_root_smoke.c "map")
sf_add_test(souls_formats_test_msbe_model_param_synthetic
    map/msbe/test_model_param_synthetic.c "map")
sf_add_test(souls_formats_test_msbe_synthetic_rt
    map/msbe/test_msbe_synthetic_rt.c "map")
sf_add_test(souls_formats_test_msbe_event_param_synthetic
    map/msbe/test_event_param_synthetic.c "map")
sf_add_test(msbe_point_param_synthetic
    map/msbe/test_point_param_synthetic.c "map")
sf_add_test(msbe_parts_param_synthetic
    map/msbe/test_parts_param_synthetic.c "map")
sf_add_test(msbe_route_param_synthetic
    map/msbe/test_route_param_synthetic.c "map")
sf_add_test(msbvi_root_smoke
    map/msbvi/test_msbvi_root_smoke.c "map")
sf_add_test(souls_formats_test_msbvi_model_param_synthetic
    map/msbvi/test_model_param_synthetic.c "map")
sf_add_test(souls_formats_test_msbvi_synthetic_rt
    map/msbvi/test_msbvi_synthetic_rt.c "map")
sf_add_test(souls_formats_test_msbvi_event_param_synthetic
    map/msbvi/test_event_param_synthetic.c "map")
sf_add_test(msbvi_point_param_synthetic
    map/msbvi/test_point_param_synthetic.c "map")
sf_add_test(msbvi_parts_param_synthetic
    map/msbvi/test_parts_param_synthetic.c "map")
sf_add_test(msbvi_route_param_synthetic
    map/msbvi/test_route_param_synthetic.c "map")
sf_add_test(msbvi_layer_param_synthetic
    map/msbvi/test_layer_param_synthetic.c "map")

#-----------------------------------------------------------------------------
# Phase 6 — geometry / material (FLVER common helpers)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_flver_common
    geom/test_flver_common.c "geom")
sf_add_test(flver2_vertex
    geom/test_flver2_vertex.c "geom")
sf_add_test(souls_formats_test_flver2_top_level
    geom/test_flver2_top_level.c "geom")
sf_add_test(souls_formats_test_flver2_faceset
    geom/test_flver2_faceset.c "geom")
sf_add_test(souls_formats_test_flver2_synthetic
    geom/test_flver2_synthetic.c "geom")
sf_add_test(souls_formats_test_mtd_synthetic
    geom/test_mtd_synthetic.c "geom")
sf_add_test(souls_formats_test_matbin_synthetic
    geom/test_matbin_synthetic.c "geom")
sf_add_test(souls_formats_test_flver2_decode
    geom/test_flver2_decode.c "geom")
sf_add_test(souls_formats_test_flver0_synthetic
    geom/test_flver0_synthetic.c "legacy-flver")
sf_add_test(souls_formats_test_mdl4_synthetic
    geom/test_mdl4_synthetic.c "legacy-flver")
sf_add_test(souls_formats_test_mdl_synthetic
    geom/test_mdl_synthetic.c "legacy-flver")

sf_add_test(souls_formats_test_flver2_e2e_er
    geom/test_flver2_e2e_er.c "e2e_er")
target_compile_definitions(souls_formats_test_flver2_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

# T0.4 endian microbench
if(SF_BUILD_TESTS AND PROJECT_IS_TOP_LEVEL)
    add_executable(souls_formats_microbench_endian
        microbench/test_flver2_decode_endian.c)
    target_link_libraries(souls_formats_microbench_endian PRIVATE souls_formats_static)
    target_include_directories(souls_formats_microbench_endian PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(souls_formats_microbench_endian PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/microbench")
    sf_apply_warnings(souls_formats_microbench_endian)
    sf_apply_sanitizers(souls_formats_microbench_endian)
endif()

#-----------------------------------------------------------------------------
# Phase 5 — Sekiro e2e helper
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_sekiro_helper_smoke
    e2e/test_sekiro_helper_smoke.c "e2e_sekiro")
target_link_libraries(souls_formats_test_sekiro_helper_smoke PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_sekiro_helper_smoke PRIVATE
    SF_E2E_SEKIRO_DIR=L"C:/Games/Sekiro"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

#-----------------------------------------------------------------------------
# Phase 4 — pre-flight empirical probe (T0.1)
# Reads real ER data and writes findings to
# .sisyphus/evidence/phase4-pre-flight.md. No format parsing here.
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_emevd_format_probe script/test_emevd_format_probe.c "script")
target_link_libraries(souls_formats_test_emevd_format_probe PRIVATE souls_formats_e2e_helpers)
# Escape backslashes for both CMake and the C preprocessor so the macro
# value reaches the compiler as L"...\\Ubuntu\\..." verbatim.
target_compile_definitions(souls_formats_test_emevd_format_probe PRIVATE
    SF_E2E_REPO_DIR=L"${CMAKE_SOURCE_DIR}"
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

#-----------------------------------------------------------------------------
# Phase 5 — script + map
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_esd_bytecode script/test_esd_bytecode.c "script")

#-----------------------------------------------------------------------------
# Phase 5 — ESD + MSBE/MSBS real-game e2e tests (T36-T39)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_esd_e2e_er
    script/test_esd_e2e_er.c "e2e_er")
target_link_libraries(souls_formats_test_esd_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_esd_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_msbe_e2e_er
    map/msbe/test_msbe_e2e_er.c "e2e_er")
target_link_libraries(souls_formats_test_msbe_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_msbe_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_msbe_e2e_nr
    map/msbe/test_msbe_e2e_nr.c "e2e_nightreign")
target_link_libraries(souls_formats_test_msbe_e2e_nr PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_msbe_e2e_nr PRIVATE
    SF_E2E_NIGHTREIGN_DIR=L"C:/Games/ELDEN RING NIGHTREIGN/Game"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")

sf_add_test(souls_formats_test_msbs_e2e_sekiro
    map/msbs/test_msbs_e2e_sekiro.c "e2e_sekiro")
target_link_libraries(souls_formats_test_msbs_e2e_sekiro PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_msbs_e2e_sekiro PRIVATE
    SF_E2E_SEKIRO_DIR=L"C:/Games/Sekiro"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

#-----------------------------------------------------------------------------
# Phase 6 — MATBIN / MTD e2e (T27, T27b)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_matbin_e2e_er
    geom/test_matbin_e2e_er.c "e2e_er")
target_link_libraries(souls_formats_test_matbin_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_matbin_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_mtd_e2e_sekiro
    geom/test_mtd_e2e_sekiro.c "e2e_sekiro")
target_link_libraries(souls_formats_test_mtd_e2e_sekiro PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_mtd_e2e_sekiro PRIVATE
    SF_E2E_SEKIRO_DIR=L"C:/Games/Sekiro"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

# ── Probes (one-shot diagnostics, not part of regular test suite) ──────────
# Gated behind SF_BUILD_PROBES so casual builds and downstream consumers do
# not pay the compile cost. These targets reference local game-data paths
# and are intended only for local debugging.
if(SF_BUILD_PROBES AND PROJECT_IS_TOP_LEVEL)
    add_executable(probe_nightreign_msb ${CMAKE_SOURCE_DIR}/tests/probes/probe_nightreign_msb.c)
    target_link_libraries(probe_nightreign_msb PRIVATE souls_formats_e2e_helpers crypt32)
    target_compile_definitions(probe_nightreign_msb PRIVATE
        SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
        SF_E2E_OODLE_DIR=L"C:/Games/ELDEN RING NIGHTREIGN/Game")
    set_target_properties(probe_nightreign_msb PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/probes")

    add_executable(probe_matbin_paramtypes ${CMAKE_SOURCE_DIR}/tests/probes/probe_matbin_paramtypes.c)
    target_link_libraries(probe_matbin_paramtypes PRIVATE souls_formats_e2e_helpers)
    target_compile_definitions(probe_matbin_paramtypes PRIVATE
        SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
        SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")
    set_target_properties(probe_matbin_paramtypes PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/probes")
    include(${CMAKE_CURRENT_SOURCE_DIR}/probes/CMakeLists.txt)
endif()

#-----------------------------------------------------------------------------
# Phase 7 — animation / effects (synthetic round-trips)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_fxr3_xml
    anim/test_fxr3_xml.c "anim")

sf_add_test(souls_formats_test_fxr3_synthetic
    anim/test_fxr3_synthetic.c "anim")

# T24 — TAE e2e against ER c0000.anibnd.dcx (in Data3).
# Includes the AES + RSA crypto source files because the anibnd lives
# in Data3, whose RSA key is not embedded in sf_bhd5_open; the test
# therefore carries its own copies of all four Data0..Data3 PEM keys.
sf_add_test(souls_formats_test_tae_e2e_er
    anim/test_tae_e2e_er.c "e2e_er")
target_sources(souls_formats_test_tae_e2e_er PRIVATE
    ${CMAKE_SOURCE_DIR}/src/crypto/aes_cng.c
    ${CMAKE_SOURCE_DIR}/src/crypto/rsa_cng.c)
target_link_libraries(souls_formats_test_tae_e2e_er PRIVATE crypt32)
target_compile_definitions(souls_formats_test_tae_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

# T25 — FXR3 e2e against ER sfxbnd_commoneffects.ffxbnd.dcx (in Data0).
# Uses er_extract_from_data0 from the shared er_test_helper.
sf_add_test(souls_formats_test_fxr3_e2e_er
    anim/test_fxr3_e2e_er.c "e2e_er")
target_link_libraries(souls_formats_test_fxr3_e2e_er PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_fxr3_e2e_er PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

#-----------------------------------------------------------------------------
# Pre-refactor SHA-256 baseline — pins deterministic byte outputs across
# the api-realignment effort. See tests/golden/golden_hashes.h.
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_golden golden/capture_golden.c "golden")
target_include_directories(souls_formats_test_golden
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/golden)

sf_add_test(souls_formats_test_tae_synthetic anim/test_tae_synthetic.c "anim")

#-----------------------------------------------------------------------------
# TAE Template subsystem tests (tae-templates label)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_tae_template_xml
    anim/test_tae_template_xml.c "tae-templates")

sf_add_test(souls_formats_test_tae_template_apply
    anim/test_tae_template_apply.c "tae-templates")

sf_add_e2e_test(souls_formats_test_tae_legacy_ds1r
    anim/test_tae_legacy_ds1r.c "ds1r")
target_link_libraries(souls_formats_test_tae_legacy_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_tae_legacy_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")
set_tests_properties(souls_formats_test_tae_legacy_ds1r PROPERTIES
    LABELS "tae-templates;e2e;e2e_ds1r")

sf_add_e2e_test(souls_formats_test_tae_legacy_ds3
    anim/test_tae_legacy_ds3.c "ds3")
target_link_libraries(souls_formats_test_tae_legacy_ds3 PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_tae_legacy_ds3 PRIVATE
    SF_E2E_DS3_DIR=L"C:/Games/DARK SOULS III/Game"
    SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")
set_tests_properties(souls_formats_test_tae_legacy_ds3 PROPERTIES
    LABELS "tae-templates;e2e;e2e_ds3")

# T0.1 klib compile spike — remove after Wave 4 (T4.1 klib khash adoption in BHD5)
if(SF_BUILD_TESTS AND PROJECT_IS_TOP_LEVEL)
    add_executable(souls_formats_test_klib_spike spike/test_klib_compile.c)
    target_link_libraries(souls_formats_test_klib_spike PRIVATE klib)
    set_target_properties(souls_formats_test_klib_spike PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/spike")
    sf_apply_warnings(souls_formats_test_klib_spike)
endif()

#-----------------------------------------------------------------------------
# E2E label unification (T1.5)
#
# Every e2e test carries BOTH the 'e2e' umbrella label AND a per-game
# 'e2e_<game>' label, so ctest can filter all e2e at once via
#   ctest -L '^e2e$'
# or run a single game via
#   ctest -L 'e2e_er'
#
# This is a batch override of the LABELS property set by sf_add_test()
# above. New e2e tests should use the sf_add_e2e_test() helper instead.
#-----------------------------------------------------------------------------
set_tests_properties(
    souls_formats_test_er_helper_smoke
    souls_formats_test_bhd5_e2e_er
    souls_formats_test_bnd4_e2e_er
    souls_formats_test_bxf4_e2e_er
    souls_formats_test_tpf_e2e_er
    souls_formats_test_flver2_e2e_er
    souls_formats_test_esd_e2e_er
    souls_formats_test_msbe_e2e_er
    souls_formats_test_matbin_e2e_er
    souls_formats_test_tae_e2e_er
    souls_formats_test_fxr3_e2e_er
    paramdef_xml_e2e
    param_apply_paramdef_e2e
    fmg_e2e_er
    emevd_e2e_er
    param_setters_e2e_er
    emevd_mutation_e2e_er
    PROPERTIES LABELS "e2e;e2e_er")
set_tests_properties(
    souls_formats_test_ac6_helper_smoke
    souls_formats_test_msbvi_e2e_ac6
    PROPERTIES LABELS "e2e;e2e_ac6")
set_tests_properties(
    souls_formats_test_sekiro_helper_smoke
    souls_formats_test_msbs_e2e_sekiro
    souls_formats_test_mtd_e2e_sekiro
    PROPERTIES LABELS "e2e;e2e_sekiro")
set_tests_properties(
    souls_formats_test_ds1r_helper_smoke
    souls_formats_test_msb1_e2e_ds1r
    souls_formats_test_bnd3_e2e_ds1r
    souls_formats_test_param_e2e_ds1r
    souls_formats_test_fmg_e2e_ds1r
    souls_formats_test_emevd_e2e_ds1r
    souls_formats_test_flver2_e2e_ds1r
    souls_formats_test_mtd_e2e_ds1r
    PROPERTIES LABELS "e2e;e2e_ds1r")
set_tests_properties(
    souls_formats_test_ds2s_helper_smoke
    souls_formats_test_param_e2e_ds2s
    PROPERTIES LABELS "e2e;e2e_ds2s")
set_tests_properties(
    souls_formats_test_ds3_helper_smoke
    souls_formats_test_msb3_e2e_ds3
    souls_formats_test_emevd_e2e_ds3
    souls_formats_test_flver2_e2e_ds3
    souls_formats_test_fmg_e2e_ds3
    PROPERTIES LABELS "e2e;e2e_ds3")
#-----------------------------------------------------------------------------
# Legacy MSB variants — synthetic round-trip tests
#-----------------------------------------------------------------------------
sf_add_test(msb1_synthetic_rt    map/msb1/test_msb1_synthetic_rt.c    "legacy-msb")
sf_add_test(msb2_synthetic_rt    map/msb2/test_msb2_synthetic_rt.c    "legacy-msb")
sf_add_test(msb3_synthetic_rt    map/msb3/test_msb3_synthetic_rt.c    "legacy-msb")
sf_add_test(msbac4_synthetic_rt  map/msbac4/test_msbac4_synthetic_rt.c "legacy-msb")
sf_add_test(msbb_synthetic_rt    map/msbb/test_msbb_synthetic_rt.c    "legacy-msb")
sf_add_test(msbd_synthetic_rt    map/msbd/test_msbd_synthetic_rt.c    "legacy-msb")
sf_add_test(msbdr_synthetic_rt   map/msbdr/test_msbdr_synthetic_rt.c  "legacy-msb")
sf_add_test(msbfa_synthetic_rt   map/msbfa/test_msbfa_synthetic_rt.c  "legacy-msb")
sf_add_test(msbn_synthetic_rt    map/msbn/test_msbn_synthetic_rt.c    "legacy-msb")
sf_add_test(msbv_synthetic_rt    map/msbv/test_msbv_synthetic_rt.c    "legacy-msb")
sf_add_test(msbvd_synthetic_rt   map/msbvd/test_msbvd_synthetic_rt.c  "legacy-msb")

set_tests_properties(
    souls_formats_test_nightreign_helper_smoke
    souls_formats_test_msbe_e2e_nr
    PROPERTIES LABELS "e2e;e2e_nightreign")

#-----------------------------------------------------------------------------
# AC-specific formats (AcParts4, FSDATA, FSLIBLZS, MLB, PARAMDBP)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_acparts4_synthetic
    ac/test_acparts4_synthetic.c "ac")
sf_add_test(souls_formats_test_fsdata_synthetic
    ac/test_fsdata_synthetic.c "ac")
sf_add_test(souls_formats_test_fsliblzs_synthetic
    ac/test_fsliblzs_synthetic.c "ac")
sf_add_test(souls_formats_test_mlb_synthetic
    ac/test_mlb_synthetic.c "ac")
sf_add_test(souls_formats_test_paramdbp_synthetic
    ac/test_paramdbp_synthetic.c "ac")

#-----------------------------------------------------------------------------
# Misc formats (AIP, F2TR, CLM2, RMB, ...)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_aip_synthetic   misc/test_aip_synthetic.c   "misc")
sf_add_test(souls_formats_test_f2tr_synthetic  misc/test_f2tr_synthetic.c  "misc")
sf_add_test(souls_formats_test_clm2_synthetic  misc/test_clm2_synthetic.c  "misc")
sf_add_test(souls_formats_test_rmb_synthetic   misc/test_rmb_synthetic.c   "misc")
sf_add_test(souls_formats_test_grass_synthetic misc/test_grass_synthetic.c "misc")
sf_add_test(souls_formats_test_ccm_synthetic   misc/test_ccm_synthetic.c   "misc")
sf_add_test(souls_formats_test_edd_synthetic   misc/test_edd_synthetic.c   "misc")
sf_add_test(souls_formats_test_acb_synthetic   misc/test_acb_synthetic.c   "misc")
sf_add_test(souls_formats_test_smd4_synthetic  misc/test_smd4_synthetic.c  "misc")
sf_add_test(souls_formats_test_drb_synthetic   misc/test_drb_synthetic.c   "misc")
sf_add_test(souls_formats_test_legacy_formats_synthetic
    misc/test_legacy_formats_synthetic.c "misc")
sf_add_test(souls_formats_test_fmb_synthetic
    misc/test_fmb_synthetic.c "misc")
sf_add_test(souls_formats_test_ani_synthetic
    misc/test_ani_synthetic.c "effects")
sf_add_test(souls_formats_test_mqb_synthetic
    misc/test_mqb_synthetic.c "effects")
sf_add_test(souls_formats_test_ffxdlse_synthetic
    misc/test_ffxdlse_synthetic.c "effects")
sf_add_test(souls_formats_test_fxr1_synthetic
    misc/test_fxr1_synthetic.c "effects")
sf_add_test(souls_formats_test_nmb_synthetic
    misc/test_nmb_synthetic.c "effects")
sf_add_test(souls_formats_test_nsa_synthetic
    misc/test_nsa_synthetic.c "effects")

#-----------------------------------------------------------------------------
# Text-Script-Misc cluster (LUAGNL, LUAINFO, EMELD)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_luagnl_synthetic
    script/test_luagnl_synthetic.c "script")
sf_add_test(souls_formats_test_luainfo_synthetic
    script/test_luainfo_synthetic.c "script")
sf_add_test(souls_formats_test_emeld_synthetic
    script/test_emeld_synthetic.c "script")

#-----------------------------------------------------------------------------
# Post-v1 lighting formats (BTAB / BTL / GPARAM / PMDCL)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_pmdcl_synthetic lighting/test_pmdcl_synthetic.c "lighting")
target_link_libraries(souls_formats_test_pmdcl_synthetic PRIVATE souls_formats_static)

sf_add_test(souls_formats_test_btab_synthetic lighting/test_btab_synthetic.c "lighting")

sf_add_test(souls_formats_test_btl_synthetic lighting/test_btl_synthetic.c "lighting")

sf_add_test(souls_formats_test_gparam_synthetic lighting/test_gparam_synthetic.c "lighting")

# Post-v1 Wave 5 (T14) — multi-game e2e probe-skip tests for lighting formats.
# Each test tries Data0 paths across ER/AC6/NR/Sekiro and SKIPs cleanly if
# no lighting file is found. Wave-0 probe found 0 such files; these tests
# exist so future v2+ shard support can flip them green without scaffolding.
sf_add_test(souls_formats_test_pmdcl_e2e lighting/test_pmdcl_e2e.c "lighting")
target_link_libraries(souls_formats_test_pmdcl_e2e PRIVATE souls_formats_e2e_helpers)
set_tests_properties(souls_formats_test_pmdcl_e2e PROPERTIES LABELS "lighting;e2e")
target_compile_definitions(souls_formats_test_pmdcl_e2e PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_NIGHTREIGN_DIR=L"C:/Games/ELDEN RING NIGHTREIGN/Game"
    SF_E2E_AC6_DIR=L"C:/Games/ARMORED CORE VI FIRES OF RUBICON/Game"
    SF_E2E_SEKIRO_DIR=L"C:/Games/Sekiro"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_btab_e2e lighting/test_btab_e2e.c "lighting")
target_link_libraries(souls_formats_test_btab_e2e PRIVATE souls_formats_e2e_helpers)
set_tests_properties(souls_formats_test_btab_e2e PROPERTIES LABELS "lighting;e2e")
target_compile_definitions(souls_formats_test_btab_e2e PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_NIGHTREIGN_DIR=L"C:/Games/ELDEN RING NIGHTREIGN/Game"
    SF_E2E_AC6_DIR=L"C:/Games/ARMORED CORE VI FIRES OF RUBICON/Game"
    SF_E2E_SEKIRO_DIR=L"C:/Games/Sekiro"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_btl_e2e lighting/test_btl_e2e.c "lighting")
target_link_libraries(souls_formats_test_btl_e2e PRIVATE souls_formats_e2e_helpers)
set_tests_properties(souls_formats_test_btl_e2e PROPERTIES LABELS "lighting;e2e")
target_compile_definitions(souls_formats_test_btl_e2e PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_NIGHTREIGN_DIR=L"C:/Games/ELDEN RING NIGHTREIGN/Game"
    SF_E2E_AC6_DIR=L"C:/Games/ARMORED CORE VI FIRES OF RUBICON/Game"
    SF_E2E_SEKIRO_DIR=L"C:/Games/Sekiro"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

sf_add_test(souls_formats_test_gparam_e2e lighting/test_gparam_e2e.c "lighting")
target_link_libraries(souls_formats_test_gparam_e2e PRIVATE souls_formats_e2e_helpers)
set_tests_properties(souls_formats_test_gparam_e2e PROPERTIES LABELS "lighting;e2e")
target_compile_definitions(souls_formats_test_gparam_e2e PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_NIGHTREIGN_DIR=L"C:/Games/ELDEN RING NIGHTREIGN/Game"
    SF_E2E_AC6_DIR=L"C:/Games/ARMORED CORE VI FIRES OF RUBICON/Game"
    SF_E2E_SEKIRO_DIR=L"C:/Games/Sekiro"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")

#-----------------------------------------------------------------------------
# Navmesh formats (EDGE / NVM / MCG / MCP / NVA / NGP)
#-----------------------------------------------------------------------------
sf_add_test(souls_formats_test_edge_synthetic navmesh/test_edge_synthetic.c "navmesh")
sf_add_test(souls_formats_test_nvm_synthetic  navmesh/test_nvm_synthetic.c  "navmesh")
sf_add_test(souls_formats_test_mcg_synthetic  navmesh/test_mcg_synthetic.c  "navmesh")
sf_add_test(souls_formats_test_mcp_synthetic  navmesh/test_mcp_synthetic.c  "navmesh")
sf_add_test(souls_formats_test_nva_synthetic  navmesh/test_nva_synthetic.c  "navmesh")
sf_add_test(souls_formats_test_ngp_synthetic  navmesh/test_ngp_synthetic.c  "navmesh")

sf_add_test(souls_formats_test_mcg_e2e_ds1r navmesh/test_mcg_e2e_ds1r.c "navmesh")
target_link_libraries(souls_formats_test_mcg_e2e_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_mcg_e2e_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

sf_add_test(souls_formats_test_mcp_e2e_ds1r navmesh/test_mcp_e2e_ds1r.c "navmesh")
target_link_libraries(souls_formats_test_mcp_e2e_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_mcp_e2e_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

sf_add_test(souls_formats_test_nvm_e2e_ds1r navmesh/test_nvm_e2e_ds1r.c "navmesh")
target_link_libraries(souls_formats_test_nvm_e2e_ds1r PRIVATE souls_formats_e2e_helpers)
target_compile_definitions(souls_formats_test_nvm_e2e_ds1r PRIVATE
    SF_E2E_DS1R_DIR=L"C:/Games/DARK SOULS REMASTERED")

#-----------------------------------------------------------------------------
