# Phase 4 — Param + Text

> **Status**: ⏳ Pending · **Estimate**: ~1.5 weeks · **Depends on**: Phase 2, Phase 3

Strict upstream alignment policy applies — see [AGENTS.md](../../AGENTS.md) §5.x.

## Goal

Implement PARAM (parameter tables), PARAMDEF (schemas, both binary and
Paramdex-style XML), PARAMTDF (enum friendly names), and FMG (localized
strings). After this phase, the library can decrypt `regulation.bin`,
parse the inner BND4, apply Paramdex schemas to specific .param entries,
and read in-game text from MSGBND archives.

XML reader for PARAMDEF lands here; XML writer is **deferred to v1.1**.

---

## Deliverables

### 1. PARAM (Parameter Tables)
Implement the core parameter table format used for game data.

*   **Upstream references**:
    *   `SoulsFormats/Formats/PARAM/PARAM/PARAM.cs`
    *   `SoulsFormats/Formats/PARAM/PARAM/Row.cs`
    *   `SoulsFormats/Formats/PARAM/PARAM/Cell.cs`
    *   Mapping: [format-param.md](../api-mapping/format-param.md)
*   **API alignment checklist**:
    *   `sf_param_t` must mirror `PARAM` class (BigEndian, Format2D, Format2E, ParamType).
    *   `sf_param_row_t` must mirror `Row` class (ID, Name, Cells).
    *   `sf_param_cell_t` must mirror `Cell` class (Value).
    *   Implement `sf_param_apply_paramdef` and `sf_param_apply_paramdef_carefully` matching upstream logic.

### 2. PARAMDEF (Schemas)
Implement binary and XML schema definitions for parameters.

*   **Upstream references**:
    *   `SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs`
    *   `SoulsFormats/Formats/PARAM/PARAMDEF/Field.cs`
    *   `SoulsFormats/Formats/PARAM/PARAMDEF/XmlSerializer.cs`
    *   Mapping: [format-paramdef.md](../api-mapping/format-paramdef.md)
*   **API alignment checklist**:
    *   `sf_paramdef_t` must mirror `PARAMDEF` class (DataVersion, ParamType, Unicode, FormatVersion).
    *   `sf_paramdef_field_t` must mirror `Field` class (DisplayName, InternalName, DisplayType, BitSize).
    *   Implement `sf_paramdef_xml_deserialize_from_path` using `mxml` to match upstream `XmlDeserialize`.
    *   **Note**: XML writing (`XmlSerialize`) is deferred to v1.1.

### 3. PARAMTDF (Enum Names)
Implement friendly names for parameter enums.

*   **Upstream references**:
    *   `SoulsFormats/Formats/PARAM/PARAMTDF.cs`
    *   Mapping: [format-paramtdf.md](../api-mapping/format-paramtdf.md)
*   **API alignment checklist**:
    *   `sf_paramtdf_t` must mirror `PARAMTDF` class (Name, Type, Entries).
    *   `sf_paramtdf_entry_t` must mirror `Entry` class (Name, Value).

### 4. FMG (Localized Text)
Implement the string table format used for all in-game text.

*   **Upstream references**:
    *   `SoulsFormats/Formats/FMG.cs`
    *   Mapping: [format-fmg.md](../api-mapping/format-fmg.md)
*   **API alignment checklist**:
    *   `sf_fmg_t` must mirror `FMG` class (Version, BigEndian, Unicode, Md5).
    *   `sf_fmg_entry_t` must mirror `Entry` class (ID, Text).

---

## File structure

```
include/souls_formats/
├── sf_param.h
├── sf_paramdef.h
├── sf_paramtdf.h
└── sf_fmg.h
src/param/
├── param.c
├── paramdef.c            ← binary read + write
├── paramdef_xml_read.c   ← mxml DOM → sf_paramdef_t
├── paramdef_apply.c      ← careful + unconditional apply paths
└── paramtdf.c
src/text/
└── fmg.c
tests/param/
├── test_param_synthetic.c
├── test_paramdef_binary.c
├── test_paramdef_xml.c
├── test_fmg_synthetic.c
├── test_paramdef_xml_e2e.c
├── test_param_apply_paramdef_e2e.c
└── test_fmg_e2e_er.c
examples/
└── sf_param_dump.c
```

---

## Public API sketch

```c
/* sf_paramdef.h */
typedef struct sf_paramdef sf_paramdef_t;

SF_API sf_result_t sf_paramdef_read_from_memory(sf_paramdef_t **out,
                                                const void *bytes, size_t size,
                                                const sf_allocator_t *a);
SF_API sf_result_t sf_paramdef_read_xml_from_memory(sf_paramdef_t **out,
                                                    const char *xml, size_t size,
                                                    const sf_allocator_t *a);
SF_API sf_result_t sf_paramdef_read_xml_from_path(sf_paramdef_t **out,
                                                  const wchar_t *path,
                                                  const sf_allocator_t *a);
SF_API const char *sf_paramdef_param_type   (const sf_paramdef_t *d);
SF_API uint16_t    sf_paramdef_data_version (const sf_paramdef_t *d);
SF_API size_t      sf_paramdef_field_count  (const sf_paramdef_t *d);
SF_API const sf_paramdef_field_t *sf_paramdef_field(const sf_paramdef_t *d, size_t i);
SF_API void        sf_paramdef_destroy      (sf_paramdef_t *d);

/* sf_param.h */
typedef struct sf_param sf_param_t;

SF_API sf_result_t sf_param_read_from_memory(sf_param_t **out,
                                             const void *bytes, size_t size,
                                             const sf_allocator_t *a);
SF_API sf_result_t sf_param_apply_paramdef  (sf_param_t *p,
                                             const sf_paramdef_t *const *defs,
                                             size_t def_count,
                                             bool careful_mode);
SF_API size_t            sf_param_row_count(const sf_param_t *p);
SF_API const sf_param_row_t *sf_param_row(const sf_param_t *p, size_t i);
SF_API const sf_param_row_t *sf_param_find_row_by_id(const sf_param_t *p, int32_t id);
SF_API sf_result_t       sf_param_cell_value(const sf_param_row_t *r,
                                             const char *cell_name,
                                             sf_param_cell_value_t *out);
```

---

## Implementation notes

* **PARAM auto-detect**: header has a `BigEndian` flag and `DataVersion`,
  but the row size is *not* in the header — you compute it from the
  applied PARAMDEF. Hence `careful_mode` checks param type + data version
  + computed row size all match before applying.
* **Bit-packed cells**: PARAMDEF can pack multiple sub-byte fields into
  one byte (e.g., 1-bit booleans + 7-bit padding). The reader must
  produce the per-cell view from a bitstream, not byte-aligned reads.
* **Paramdex XML** is straightforward DOM: top-level `<PARAMDEF>` with
  `<ParamType>`, `<Index>`, `<DataVersion>`, `<BigEndian>`, `<Unicode>`,
  `<FormatVersion>`, then a `<Fields>` list. mxml's `mxmlFindElement` +
  `mxmlGetText` cover the entire schema.
* **regulation.bin pipeline** (Phase 2 already gives you the AES step):
  1. Read `/mnt/c/Games/ELDEN RING/Game/regulation.bin`.
  2. AES-256-CBC decrypt with embedded ER key (Phase 2 helper).
  3. Inner bytes form a BND4 archive (Phase 3 reader).
  4. Locate entry by name (e.g., `param/GameParam/SpEffectParam.param`).
  5. Parse as PARAM with `sf_param_read_from_memory`.
  6. Apply `SpEffect.xml` PARAMDEF.
* **FMG MD5 prefix** is optional and only present in some non-Souls
  FromSoft titles; check the magic + version to decide whether to read
  it.

---

## QA scenarios

Tools: `cmake / ninja / ctest / mxml / Paramdex / ER copy`.

```bash
cmake --build build-mingw --target souls_formats_test_param
ctest --test-dir build-mingw -L param --output-on-failure
```

### Synthetic fixtures
* `test_param_synthetic` — 3 rows (id 100/200/300) × 5 fields (u8/u16/u32/f32/fixstr16), byte-equal round-trip.
* `test_paramdef_binary` — 10-field ER-style PARAMDEF with mixed bit-aligned widths, byte-equal round-trip.
* `test_fmg_synthetic` — 5 strings including 日文 `エルデンリング` and 中文 `黑暗之魂`, byte-equal.

### ER e2e (Paramdex + regulation)
* `test_paramdef_xml_e2e` — load `/home/soar/dev/paramdex/ER/Defs/SpEffect.xml`,
  assert `ParamType == "SP_EFFECT_PARAM_ST"`, `Index == 86`,
  `DataVersion == 4`, `Unicode == true`, field count ≥ 100.
* `test_param_apply_paramdef_e2e` — full chain:
  1. Read `regulation.bin`.
  2. AES-256-CBC decrypt.
  3. Parse as BND4.
  4. Find entry `param/GameParam/SpEffectParam.param`.
  5. Parse as PARAM.
  6. Apply `SpEffect.xml` (via `sf_param_apply_paramdef(careful_mode=true)`).
  7. Assert row count ≥ 100, ParamType matches.
* `test_fmg_e2e_er` — `er_extract_from_data0("/msg/engus/item.msgbnd.dcx")`
  → BND4 → `ItemName.fmg` → query well-known item id (e.g., 1030000
  "Dagger" / "短剑") and assert non-empty string.

### Mapping Coverage Check
* [ ] Verify all `未实现` rows in `format-param.md` are addressed.
* [ ] Verify all `未实现` rows in `format-paramdef.md` are addressed.
* [ ] Verify all `未实现` rows in `format-paramtdf.md` are addressed.
* [ ] Verify all `未实现` rows in `format-fmg.md` are addressed.

---

## Risks

| Risk | Mitigation |
|---|---|
| Paramdex defs lag game patches; new fields appear in regulation but not the XML | Tolerant apply: extra trailing bytes in PARAM row → logged warning, not error |
| PARAMDEF XML drift between Paramdex revisions | Pin the Paramdex commit SHA in the test fixture documentation; treat schema mismatch as test SKIP |
| Bitstream cell pack ordering can be little-endian-bit or big-endian-bit | Match upstream: little-endian bit order within a byte; encode in dedicated `paramdef_apply.c` helpers |
| FMG entry IDs are sometimes reused with empty strings as "deleted" | Treat empty string as valid (matches upstream); do not throw |

---

## Exit criteria

- [ ] All deliverables checked off above (XML write deferred is fine).
- [ ] `ctest -L param` green on dev machine.
- [ ] `examples/sf_param_dump.c` runs end-to-end against
      `regulation.bin` + `SpEffect.xml`, produces TSV.
- [ ] `PLAN.md` Phase 4 boxes ticked.

When green, proceed to [Phase 5](phase-5-script-map.md).
