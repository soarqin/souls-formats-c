# API Extensions

This document tracks symbols and features in `souls-formats-c` that have no direct counterpart in the upstream `SoulsFormatsNEXT` library. These extensions are typically added for C-idiomatic memory management, performance optimizations, or platform-specific requirements.

| Symbol | Module | Rationale | Lifecycle | Source loc |
| :--- | :--- | :--- | :--- | :--- |
| `sf_dcx_unwrap` | `sf_dcx.h` | Convenience to probe+decompress in one call | stable | `include/souls_formats/sf_dcx.h` |
| `sf_ostream_detach_buffer` | `sf_io.h` | Lets caller steal the write buffer without copying | stable | `include/souls_formats/sf_io.h` |
| `sf_oodle_set_search_path` | `sf_oodle.h` | Override DLL search path (no upstream equivalent) | stable | `include/souls_formats/sf_oodle.h` |
| `sf_oodle_load` | `sf_oodle.h` | Explicit load trigger (no upstream equivalent) | stable | `include/souls_formats/sf_oodle.h` |
| `sf_oodle_unload` | `sf_oodle.h` | Explicit unload (no upstream equivalent) | stable | `include/souls_formats/sf_oodle.h` |
| `sf_oodle_version` | `sf_oodle.h` | Query currently loaded Oodle DLL major version | stable | `include/souls_formats/sf_oodle.h` |
| `sf_istream_t + sf_ostream_t dual-layer stream` | `sf_io.h` | Layered abstraction over Win32 handles + memory; upstream BinaryReaderEx wraps a .NET Stream directly | stable | `include/souls_formats/sf_io.h` |
