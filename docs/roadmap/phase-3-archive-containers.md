# Phase 3 — Archive Containers

> **Status**: ⏳ Pending · **Estimate**: ~2 weeks · **Depends on**: Phase 2

Strict upstream alignment policy applies — see [AGENTS.md](../../AGENTS.md) §5.x.

## Goal

Implement BND3, BND4, BXF3, BXF4, BHD5, TPF, and ENFL — the entire
archive-container layer that every other format module sits inside. By
the end, the library can open Elden Ring's `Data0.bhd/bdt` and extract
arbitrary entries by path, fully unwrapping AES + DCX along the way.

This phase delivers the **`er_extract_from_data0()`** test helper — every
downstream phase's e2e tests depend on it because Elden Ring ships zero
loose `.dcx` files.

---

## Deliverables

### 1. Binder Common & BND3
Implement the shared binder logic and the legacy BND3 format used in older titles and some Sekiro/ER assets.

*   **Upstream references**:
    *   `SoulsFormats/Formats/Binder/Binder.cs`
    *   `SoulsFormats/Formats/Binder/BinderFile.cs`
    *   `SoulsFormats/Formats/Binder/BND3/BND3.cs`
    *   Mapping: [format-binder-common.md](../api-mapping/format-binder-common.md), [format-bnd3.md](../api-mapping/format-bnd3.md)
*   **API alignment checklist**:
    *   `sf_binder_file_t` must map to `BinderFile` (id, name, data, size, flags).
    *   `sf_bnd3_t` must mirror `BND3` class properties (Version, Format, BigEndian, BitBigEndian).
    *   Implement `sf_bnd3_read` and `sf_bnd3_write` matching upstream `Read`/`Write` logic.

### 2. BND4
Implement the modern BND4 format used in DS3, Sekiro, ER, and AC6.

*   **Upstream references**:
    *   `SoulsFormats/Formats/Binder/BND4/BND4.cs`
    *   `SoulsFormats/Formats/Binder/BinderHashTable.cs`
    *   Mapping: [format-bnd4.md](../api-mapping/format-bnd4.md)
*   **API alignment checklist**:
    *   `sf_bnd4_t` must mirror `BND4` class properties (Unicode, Extended, Hash Table).
    *   `sf_bnd4_find_by_path_hash` must use `BinderHashTable` logic and `sf_path_hash`.
    *   Handle UTF-16 entry names for ER/AC6 as per `Unicode` flag.

### 3. BXF3 & BXF4 (Split Archives)
Implement the split header (.bhd) and data (.bdt) archives.

*   **Upstream references**:
    *   `SoulsFormats/Formats/Binder/BXF3/BXF3.cs`
    *   `SoulsFormats/Formats/Binder/BXF4/BXF4.cs`
    *   Mapping: [format-bxf3.md](../api-mapping/format-bxf3.md), [format-bxf4.md](../api-mapping/format-bxf4.md)
*   **API alignment checklist**:
    *   `sf_bxf3_t` and `sf_bxf4_t` must mirror their respective upstream classes.
    *   Implement split-file reading logic matching `BXF3.Read` and `BXF4.Read`.

### 4. BHD5 (Master Archive)
Implement the encrypted master archive format used for `Data0.bhd`.

*   **Upstream references**:
    *   `SoulsFormats/Formats/BHD5.cs`
    *   Mapping: [format-bhd5.md](../api-mapping/format-bhd5.md)
*   **API alignment checklist**:
    *   `sf_bhd5_t` must mirror `BHD5` class (Buckets, Salt, Game enum).
    *   Implement AES-128-ECB range decryption matching upstream `AESKey` and `Range` logic.
    *   Store 32-byte salted SHA hash blobs verbatim as per `SHAHash`.

### 5. TPF & ENFL
Implement texture containers and preload lists.

*   **Upstream references**:
    *   `SoulsFormats/Formats/TPF/TPF.cs`
    *   `SoulsFormats/Formats/ENFL.cs`
    *   Mapping: [format-tpf.md](../api-mapping/format-tpf.md), [format-enfl.md](../api-mapping/format-enfl.md)
*   **API alignment checklist**:
    *   `sf_tpf_t` must mirror `TPF` class (Platform, Encoding, Textures).
    *   `sf_enfl_t` must mirror `ENFL` class (Struct1s, Struct2s, Strings).

### 6. ER Test Helper
Implement `er_extract_from_data0` for downstream e2e tests.

*   **Note**: This helper must use `sf_get_decompressed_reader` (from `sf_io.h`) to handle DCX-wrapped entries extracted from BHD5.

---

## File structure

```
include/souls_formats/
├── sf_binder.h           ← shared types for all BND/BXF
├── sf_bnd3.h
├── sf_bnd4.h
├── sf_bxf3.h
├── sf_bxf4.h
├── sf_bhd5.h
├── sf_tpf.h
└── sf_enfl.h
src/archive/
├── binder_common.c       ← shared parsing for entry lists
├── bnd3.c
├── bnd4.c
├── bxf3.c
├── bxf4.c
├── bhd5.c
├── bhd5_keys.c           ← const arrays of AES-128-ECB keys per game
├── tpf.c
└── enfl.c
tests/
├── archive/
│   ├── test_bnd3_synthetic.c
│   ├── test_bnd4_synthetic.c
│   ├── test_bxf3_synthetic.c
│   ├── test_bxf4_synthetic.c
│   ├── test_bhd5_synthetic.c
│   ├── test_tpf_synthetic.c
│   └── test_enfl_synthetic.c
└── e2e/
    ├── er_test_helper.h
    ├── er_test_helper.c
    ├── test_bhd5_e2e_er.c   ← THE foundational e2e
    ├── test_bnd4_e2e_er.c
    ├── test_bxf4_e2e_er.c
    └── test_tpf_e2e_er.c
examples/
└── sf_bnd_extract.c
```

---

## Public API sketch

```c
/* sf_binder.h */
typedef struct sf_binder_file {
    int64_t        id;            /* may be -1 if BND has no ID column */
    const char    *name_utf8;     /* heap-owned by the binder; do not free */
    const uint8_t *data;           /* heap-owned by the binder */
    size_t         size;           /* uncompressed size */
    uint32_t       flags;          /* BND-format-specific flag bits */
    sf_dcx_type_t  compression;    /* per-entry DCX type, or SF_DCX_TYPE_NONE */
} sf_binder_file_t;

/* sf_bnd4.h */
typedef struct sf_bnd4 sf_bnd4_t;

SF_API sf_result_t sf_bnd4_read_from_path(sf_bnd4_t **out, const wchar_t *path,
                                          const sf_allocator_t *a);
SF_API sf_result_t sf_bnd4_read_from_memory(sf_bnd4_t **out,
                                            const void *bytes, size_t size,
                                            const sf_allocator_t *a);

SF_API sf_result_t sf_bnd4_write_to_path  (const sf_bnd4_t *b, const wchar_t *path);
SF_API sf_result_t sf_bnd4_write_to_memory(const sf_bnd4_t *b,
                                           void **out, size_t *out_size);

SF_API void   sf_bnd4_destroy   (sf_bnd4_t *b);
SF_API size_t sf_bnd4_file_count(const sf_bnd4_t *b);
SF_API const sf_binder_file_t *sf_bnd4_get_file(const sf_bnd4_t *b, size_t index);
SF_API const sf_binder_file_t *sf_bnd4_find_by_path_hash(const sf_bnd4_t *b,
                                                         uint32_t path_hash);
SF_API sf_result_t sf_bnd4_add_file(sf_bnd4_t *b,
                                    int64_t id, const char *name_utf8,
                                    const void *data, size_t size,
                                    uint32_t flags);

/* sf_bhd5.h — split archive */
typedef struct sf_bhd5 sf_bhd5_t;
SF_API sf_result_t sf_bhd5_open(sf_bhd5_t **out,
                                const wchar_t *bhd_path,
                                const wchar_t *bdt_path,
                                const sf_allocator_t *a);
SF_API sf_result_t sf_bhd5_extract_by_hash(const sf_bhd5_t *b,
                                           uint32_t path_hash,
                                           void **out, size_t *out_size);
SF_API sf_result_t sf_bhd5_extract_by_path(const sf_bhd5_t *b,
                                           const char *utf8_path,
                                           void **out, size_t *out_size);
SF_API void        sf_bhd5_close(sf_bhd5_t *b);
```

---

## Implementation notes

* **`er_test_helper`** must be a singleton: opening Data0.bhd/bdt is
  expensive (BHD parse + hash table build over 1 M+ entries). Cache the
  open `sf_bhd5_t *` in a process-local static and reuse across all
  downstream tests in the same `ctest` run.
* **BHD5 + Data0.bdt size**: Data0.bdt is ~10.9 GiB — never copy it into
  memory. Always use the file-backed `sf_istream_t` and seek + read the
  exact range we need.
* **AES range decryption** in BHD5 is per-bucket: the BHD5 file lists
  encrypted byte ranges with associated AES keys. Decrypt only those
  ranges, leaving the rest passthrough.
* **Salted SHA hash blob** in BHD5: 32 bytes per entry, used by FromSoft
  for tamper detection. We **read and store** these bytes verbatim, but
  we do **not** compute them on write — round-trip preserves the exact
  bytes the input had. Matches upstream.
* **BND4 unicode names**: ER and AC6 archives use UTF-16 entry names. The
  reader returns UTF-8 (via `sf_utf16le_to_utf8`). On write, convert UTF-8
  back to UTF-16 LE.
* **Per-entry DCX**: an entry inside a BND can itself be DCX-wrapped. The
  binder reader auto-detects via `sf_dcx_sniff` and either decompresses
  inline or stores `compression = SF_DCX_TYPE_NONE` on plain entries.
* **DCX type preservation**: when re-writing a binder that contained DCX
  entries, restore the same DCX type. Per-entry compression metadata is
  baked into the binder's per-entry flag bits.
* **TPF platform enum** matches upstream; do not parse DDS pixels here —
  later phases or consumer code do that.

---

## QA scenarios

Tools: `cmake / ninja / ctest / WSL interop / ER copy / Oodle DLL`.

```bash
cmake --build build-mingw --target souls_formats_test_archive
ctest --test-dir build-mingw -L archive --output-on-failure
```

### Synthetic fixtures (always run)
* `test_bnd3_synthetic` — 3 entries (id 100/200/300, names `a.txt`/`b.bin`/`c.dat`) BND3 v1.0, byte-equal round-trip.
* `test_bnd4_synthetic` — 3 entries with unicode name `日本.bin`, 64-bit IDs, hash table — byte-equal.
* `test_bxf3_synthetic`, `test_bxf4_synthetic` — split header + body, both files round-trip.
* `test_bhd5_synthetic` — 1 bucket × 2 files, one entry has a 256-byte AES-128-ECB encrypted range, byte-equal.
* `test_tpf_synthetic` — 2 minimal 8×8 BC1 DDS payloads.
* `test_enfl_synthetic` — 5 entries with zlib-compressed payload.

### ER e2e (the keystone)
> ER ships everything inside `Data0-3.bhd/bdt` + `DLC.bhd/bdt`. **There
> are no loose `.dcx` files.** All e2e must extract through BHD5.

* `test_bhd5_e2e_er` — **the keystone test** that exercises the full chain:
  1. Open `/mnt/c/Games/ELDEN RING/Game/Data0.bhd` (~1 MB) + `Data0.bdt` (~10.9 GB).
  2. Decrypt BHD5 ranges with the embedded ER AES-128-ECB key.
  3. List bucket count > 0 and total file count > 1000.
  4. `sf_path_hash("/chr/c0000.chrbnd.dcx")` → look up in BHD5 → entry found.
  5. Read entry bytes from BDT, `sf_dcx_sniff` → `SF_DCX_TYPE_DCX_KRAK`.
  6. Pre-set `sf_oodle_set_search_path(L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")`,
     decompress via `sf_dcx_decompress`, decompressed bytes start with `BND4` magic.
* `test_bnd4_e2e_er` — built on `er_extract_from_data0("/chr/c0000.chrbnd.dcx")`,
  parses the resulting BND4, asserts entry count ≥ 5, finds `c0000.flver` with size > 100 KB.
* `test_bxf4_e2e_er` — extract any `.tpfbhd` + `.tpfbdt` pair from Data0
  (path like `/parts/wp_a_0010.tpfbhd`), parse as BXF4, file count > 0.
  If ER doesn't expose loose tpfbhd pairs in this version, SKIP with logged note.
* `test_tpf_e2e_er` — pull a `.tpf` out of any extracted BND4, DDS magic
  `'DDS '`.

### Mapping Coverage Check
* [ ] Verify all `未实现` rows in `format-binder-common.md` are addressed.
* [ ] Verify all `未实现` rows in `format-bnd3.md` and `format-bnd4.md` are addressed.
* [ ] Verify all `未实现` rows in `format-bxf3.md` and `format-bxf4.md` are addressed.
* [ ] Verify all `未实现` rows in `format-bhd5.md` are addressed.
* [ ] Verify all `未实现` rows in `format-tpf.md` and `format-enfl.md` are addressed.

---

## Risks

| Risk | Mitigation |
|---|---|
| BHD5 AES key for ER varies between patches | Treat the embedded constant as the "shipped 1.x" key; if 2.x ships with a new key, add a key-list and try in order |
| BND4 hash-table layout has subtle differences across DS3/Sekiro/ER/AC6 | Detect via header version + flags, branch to per-game parsers |
| Inline-compressed BND entries make round-trip non-trivial | Cache per-entry compression metadata at parse time, replay on write |
| `sf_bhd5_t` lifetime — Data0.bdt must stay open for the whole test process | Singleton, opened lazily, closed in `atexit`-style shutdown |

---

## Exit criteria

- [ ] All deliverables checked off above.
- [ ] `ctest -L archive` all green on the dev machine, including
      `test_bhd5_e2e_er` walking the full BHD5 → AES → DCX → KRAK → Oodle chain.
- [ ] `er_extract_from_data0` exposed in `tests/e2e/er_test_helper.h`,
      ready for downstream phases to call.
- [ ] `examples/sf_bnd_extract.c` builds, runs, and dumps a real ER chrbnd
      to disk on the dev machine.
- [ ] `PLAN.md` Phase 3 boxes ticked.

When green, proceed to [Phase 4](phase-4-param-text.md).
