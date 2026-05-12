# C-Mapping Policy

This document defines the rules for mapping the C# SoulsFormatsNEXT API to the C souls-formats-c API. Every mapping row in the format documentation must adhere to these policies.

### Properties
C# properties with `{ get; set; }` accessors are split into distinct `_get_` and `_set_` functions. Any side effects, such as `BinaryReaderEx.Position` triggering an underlying stream seek, must be documented inline in the API header.
* Example: `reader.Position` maps to `sf_binary_reader_get_position` and `sf_binary_reader_set_position`.

### Method overloads by parameter type
C does not support function overloading, so overloads are disambiguated by appending a type-specific suffix to the function name. Common suffixes include `_i32`, `_u64`, `_f32`, and `_str`.
* Example: `Foo(int)` becomes `sf_foo_i32`, while `Foo(string)` becomes `sf_foo_str`.

### Generic methods
Generic methods in C# are expanded into a set of explicitly typed functions in C. This typically applies to primitive types or common structures.
* Example: `ReadEnum<T>()` expands to `sf_read_enum_8`, `sf_read_enum_16`, `sf_read_enum_32`, and `sf_read_enum_64`.

### Default parameter values
Since C lacks default parameter values, all parameters must be explicitly provided by the caller. The upstream default value must be documented in the "Notes" column of the mapping table to ensure callers can replicate the original behavior.
* Example: `void Foo(int x = 10)` maps to `sf_foo(int x)`, with a note stating "Upstream default x=10".

### params arrays
C# `params` arrays are mapped to a pair of parameters: a `size_t count` followed by a `const T*` pointer to the array elements. The count parameter always precedes the pointer.
* Example: `void WriteBytes(params byte[] data)` maps to `sf_write_bytes(size_t count, const uint8_t* data)`.

### out / ref parameters
C# `out` and `ref` parameters are mapped to pointer types in C. An `out` parameter is treated as uninitialized on input and is set by the function on success, while a `ref` parameter must be initialized by the caller.
* Example: `bool TryRead(out int value)` maps to `sf_result_t sf_try_read(int* value)`.

### Static state
Global static state in C# is mapped to per-instance flags where possible to ensure thread safety. A separate `sf_*_set_<flag>_default()` function is provided to update the global default for new instances.
* Example: `BinaryReaderEx.IsFlexible` maps to a flag in `sf_binary_reader_t` and a global `sf_binary_reader_set_flexible_default` setter.

### null vs empty string
A `NULL` pointer for a string parameter indicates the value is absent or not provided. An empty string `""` indicates the value is present but contains no characters.
* Example: `sf_set_name(NULL)` means the name is not set; `sf_set_name("")` sets the name to an empty string.

### internal upstream APIs
Upstream APIs marked as `internal` are generally skipped and not exposed in the public C API. If an `internal` API is the only way to achieve necessary public behavior, it may be exposed as a public extension.
* Example: `internal void InternalInit()` is ignored unless it is required for the object to function correctly after creation.

### Throwing exceptions
C# exceptions are converted into `sf_result_t` error codes. The specific upstream exception type should be noted in the documentation to aid in debugging and error handling.
* Example: `NoOodleFoundException` maps to the `SF_ERR_OODLE_NOT_FOUND` result code.

### Encoding boundary
The library uses UTF-8 for all strings at the public API boundary. Internal conversions to UTF-16 or Shift-JIS are handled using Win32 APIs, and round-trip identity is verified for each supported encoding.
* Example: `sf_string_t` (a UTF-8 `char*`) is converted to `wchar_t*` before calling Windows filesystem APIs.

### Round-trip invariant
Every format module must satisfy the round-trip invariant: `read(write(x)) == x` and `write(read(b)) == b`. This ensures that the library can perfectly reproduce files and that no data is lost during the conversion process.
* Example: Loading an Elden Ring BND, modifying a file, and saving it must result in a valid BND that the game can still read.

### Status legend
The following symbols are used in mapping tables to indicate the implementation status of an API:
* `✓ aligned`: The C API matches the upstream C# API behavior and signature.
* `~ partial`: The C API implements a subset of the upstream functionality.
* `✗ deviation`: The C API intentionally differs from the upstream behavior.
* `+ extension`: The C API provides functionality not present in the upstream library.
* `未实现`: The API is planned but not yet implemented.
* `_skipped_`: The API is intentionally omitted from the C port.
61: 
62: ### Phase 3 Specific Policies
63: * RSA-bhd-decryption is integrated in our crypto layer; upstream BHD5 punts to caller. We support 4 v1 games via embedded PEM public keys.
64: * TPF Headerizer scope cap: PC platform only in v1; non-PC platforms return `SF_ERR_UNSUPPORTED_VERSION` (mirrors upstream NotImplementedException semantics).
65: * Round-trip semantic: synthetic fixtures byte-equal; real ER e2e content-equal (FromSoft hash table layouts are non-deterministic vs our writer).

### Phase 4 Adaptations
* **PARAMTDF Trim('"') mirror**: PARAMTDF parser uses naive `Trim('"')` matching upstream `PARAMTDF.cs:62-92`. No escape sequences, no BOM, no comments.
* **8→3 Apply fold**: `sf_param_apply_mode_t` folds 8 upstream `ApplyParamdef*` variants into 3 core modes. `RegulationVersioned*` variants deferred to v1.1.
* **Bit-packing literal mirror**: `paramdef_apply.c` bitstream helpers mirror `Row.cs:236-244` `(64 - bitSize - bitOffset)` shift pattern verbatim. No "beautification".

## MSB shared scaffolding extraction

The MSB entry-list scaffolding (offset-table read, count check, alloc loop, index backfill)
is extracted into `src/map/msb_common.c` helpers. The chosen technique is internal callback-table helpers.

### Rationale
C lacks the generic `Param<T>` class used in upstream C#. By extracting the shared scaffolding into common helpers that accept function pointers, we avoid duplicating ~50-100 LOC of boilerplate across 19 variant-specific MSB files while maintaining the polymorphic behavior of the original implementation.

### Upstream mapping
Upstream C# uses `Param<T>` generic class in `MSB.cs`. The C port uses internal callback functions (`msb_entry_write_fn` and `msb_list_cb_t`) to achieve the same polymorphism without C++ templates.

### LOC delta
Per T0.5 audit: ~50-100 LOC extractable per file across 19 MSB .c files. Total estimated reduction is ~1,100 LOC (26% of total MSB code).

## Error cleanup convention

All fallible functions use a single `goto cleanup` (or `goto fail`/`goto err`) label at the end of the function body. This is the established C error-handling pattern across ~812 sites in `src/`.

### Canonical example (from `src/archive/bnd4.c`)

```c
sf_result_t sf_bnd4_write(...) {
    sf_result_t r = SF_OK;
    bnd4_file_header_t *hdrs = sf_xalloc(b->alloc, ...);
    if (!hdrs) { r = SF_ERR_OOM; goto cleanup; }

    for (size_t i = 0; i < b->file_count; i++) {
        r = sfi_binder4_write_file_header(bw, ...);
        if (r != SF_OK) goto cleanup;
    }

    r = sf_binary_writer_fill_i64(bw, "HeadersEnd", ...);
    if (r != SF_OK) goto cleanup;

cleanup:
    sf_xfree(b->alloc, hdrs);
    return r;
}
```

### Rules
- One cleanup label per function (never multiple, except for nested resource acquisition where `cleanup_X` labels are used in reverse order).
- All allocations freed in the cleanup block regardless of success/failure.
- Do NOT extract a global macro — 812 sites is too many to abstract safely without risking freezing bugs into the abstraction layer (see T2.5 decision in Wave 2).
