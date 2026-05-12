# T15-T19 FXR3 binary read/write evidence

- Changed: `src/effects/fxr3.c`
- Implemented: full DS3/Sekiro FXR3 read path and writer path.
- Verification:
  - `lsp_diagnostics /home/soar/src/souls-formats-c/src/effects/fxr3.c` → no diagnostics
  - `cmake --build build-on --target souls_formats_static` → PASS
  - `nm build-on/libsouls_formats.a | grep sf_fxr3_read_from_memory` → exported
  - `nm build-on/libsouls_formats.a | grep sf_fxr3_write_to_memory` → exported
