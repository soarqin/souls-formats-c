# One-shot empirical probes. These targets are buildable on demand, but they are
# not registered as ctest tests because they require locally-installed game data.

add_executable(probe_flver2_layouts
    ${CMAKE_SOURCE_DIR}/tests/probes/probe_flver2_layouts.c
    ${CMAKE_SOURCE_DIR}/src/crypto/aes_cng.c
    ${CMAKE_SOURCE_DIR}/src/crypto/rsa_cng.c)
target_link_libraries(probe_flver2_layouts PRIVATE souls_formats_static bcrypt crypt32)
target_include_directories(probe_flver2_layouts PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_compile_definitions(probe_flver2_layouts PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")
set_target_properties(probe_flver2_layouts PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/probes")
sf_apply_warnings(probe_flver2_layouts)
sf_apply_sanitizers(probe_flver2_layouts)

add_executable(probe_fxr3_format
    ${CMAKE_SOURCE_DIR}/tests/probes/probe_fxr3_format.c
    ${CMAKE_SOURCE_DIR}/tests/e2e/er_test_helper.c)
target_link_libraries(probe_fxr3_format PRIVATE souls_formats_static bcrypt)
target_include_directories(probe_fxr3_format PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_compile_definitions(probe_fxr3_format PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")
set_target_properties(probe_fxr3_format PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/probes")
sf_apply_warnings(probe_fxr3_format)
sf_apply_sanitizers(probe_fxr3_format)

add_executable(probe_tae_format
    ${CMAKE_SOURCE_DIR}/tests/probes/probe_tae_format.c
    ${CMAKE_SOURCE_DIR}/src/crypto/aes_cng.c
    ${CMAKE_SOURCE_DIR}/src/crypto/rsa_cng.c)
target_link_libraries(probe_tae_format PRIVATE souls_formats_static bcrypt crypt32)
target_include_directories(probe_tae_format PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_compile_definitions(probe_tae_format PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")
set_target_properties(probe_tae_format PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/probes")
sf_apply_warnings(probe_tae_format)
sf_apply_sanitizers(probe_tae_format)

add_executable(probe_sha256_phase7
    ${CMAKE_SOURCE_DIR}/tests/probes/probe_sha256_phase7.c)
target_link_libraries(probe_sha256_phase7 PRIVATE souls_formats_static bcrypt crypt32)
target_include_directories(probe_sha256_phase7 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_compile_definitions(probe_sha256_phase7 PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")
set_target_properties(probe_sha256_phase7 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/probes")
sf_apply_warnings(probe_sha256_phase7)
sf_apply_sanitizers(probe_sha256_phase7)

add_executable(probe_lighting_files
    ${CMAKE_SOURCE_DIR}/tests/probes/probe_lighting_files.c
    ${CMAKE_SOURCE_DIR}/src/crypto/aes_cng.c
    ${CMAKE_SOURCE_DIR}/src/crypto/rsa_cng.c
    ${CMAKE_SOURCE_DIR}/tests/e2e/er_test_helper.c
    ${CMAKE_SOURCE_DIR}/tests/e2e/sekiro_test_helper.c
    ${CMAKE_SOURCE_DIR}/tests/e2e/nightreign_test_helper.c
    ${CMAKE_SOURCE_DIR}/tests/e2e/ac6_test_helper.c)
target_link_libraries(probe_lighting_files PRIVATE souls_formats_static bcrypt crypt32)
target_include_directories(probe_lighting_files PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/e2e)
target_compile_definitions(probe_lighting_files PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_SEKIRO_DIR=L"C:/Games/Sekiro"
    SF_E2E_NIGHTREIGN_DIR=L"C:/Games/ELDEN RING NIGHTREIGN/Game"
    SF_E2E_AC6_DIR=L"C:/Games/ARMORED CORE VI FIRES OF RUBICON/Game"
    SF_E2E_OODLE_DIR=L"\\\\\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle")
set_target_properties(probe_lighting_files PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/probes")
sf_apply_warnings(probe_lighting_files)
sf_apply_sanitizers(probe_lighting_files)
