# T19 FXR3 structural writer fix evidence

- Fixed `sf_fxr3_write_to_memory` to emit structure instead of copying `raw_bytes`.
- Verification:
  - `lsp_diagnostics /home/soar/src/souls-formats-c/src/effects/fxr3.c` → no diagnostics
  - `cmake --build build-on --target souls_formats_static` → PASS
  - `/tmp/opencode/fxr3_min.exe` synthetic DS3 minimal fixture read → write → memcmp → PASS
