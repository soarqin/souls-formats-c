# Phase 7 — Animation + Effects

> **Status**: ⏳ Optional in v1.0 / default in v1.1 ·
> **Estimate**: ~2 weeks · **Depends on**: Phase 3, Phase 6, mxml (Phase 4)

Strict upstream alignment policy applies — see [AGENTS.md](../../AGENTS.md) §5.x.

## Goal

Implement TAE (animation event timelines, both TAE3 and TAE4 layouts) and
FXR3 (the modern particle-effect format used since DS3). FXR3 needs both
binary and XML codec support since the editing workflow is XML-based.

This phase is **gated by `SF_ENABLE_PHASE7=ON`**. v1.0 ships with it OFF
by default; v1.1 flips it ON. Tests SKIP cleanly when the option is OFF.

---

## Deliverables

### 1. TAE (Time Act Editor)
Implement the animation event timeline format.

*   **Upstream references**:
    *   `SoulsFormats/Formats/TAE3.cs`
    *   `SoulsFormats/Formats/TAE3/TAE.Animation.cs`
    *   `SoulsFormats/Formats/TAE3/TAE.Event.cs`
    *   Mapping: [format-tae.md](../api-mapping/format-tae.md)
*   **API alignment checklist**:
    *   `sf_tae_t` must mirror `TAE` class (TAEFormat, Animations).
    *   `sf_tae_animation_t` must mirror `Animation` class (ID, Events, MiniHeader).
    *   `sf_tae_event_t` must mirror `Event` class (StartTime, EndTime, ID, Parameters).

### 2. FXR3 (Particle Effects)
Implement the modern particle effect format with binary and XML support.

*   **Upstream references**:
    *   `SoulsFormats/Formats/FXR3/FXR3.cs`
    *   `SoulsFormats/Formats/FXR3/FXR3.Xml.cs`
    *   Mapping: [format-fxr3.md](../api-mapping/format-fxr3.md)
*   **API alignment checklist**:
    *   `sf_fxr3_t` must mirror `FXR3` class (Version, States, Effects).
    *   Implement `sf_fxr3_from_xml` and `sf_fxr3_to_xml` matching upstream `FXR3EnhancedSerialization` logic using `mxml`.

---

## File structure

```
include/souls_formats/
├── sf_tae.h
└── sf_fxr3.h
src/effects/
├── tae.c
├── fxr3.c
├── fxr3_xml_read.c       ← mxml DOM → fxr3
└── fxr3_xml_write.c      ← fxr3 → mxml DOM → string
tests/anim/
├── test_tae_synthetic.c
├── test_fxr3_synthetic.c
├── test_tae_e2e_er.c
└── test_fxr3_e2e_er.c
```

CMake gating:

```cmake
option(SF_ENABLE_PHASE7 "Enable Phase 7 anim/effect formats" OFF)

if(SF_ENABLE_PHASE7)
    list(APPEND SF_PUBLIC_HEADERS
        include/souls_formats/sf_tae.h
        include/souls_formats/sf_fxr3.h)
    list(APPEND SF_SOURCES
        src/effects/tae.c
        src/effects/fxr3.c
        src/effects/fxr3_xml_read.c
        src/effects/fxr3_xml_write.c)
endif()
```

---

## Public API sketch

```c
/* sf_tae.h */
typedef struct sf_tae sf_tae_t;
SF_API sf_result_t sf_tae_read_from_memory(sf_tae_t **out,
                                           const void *bytes, size_t size,
                                           const sf_allocator_t *a);
SF_API size_t sf_tae_animation_count(const sf_tae_t *t);
SF_API const sf_tae_anim_t *sf_tae_animation(const sf_tae_t *t, size_t i);
SF_API void   sf_tae_destroy(sf_tae_t *t);

/* sf_fxr3.h */
typedef struct sf_fxr3 sf_fxr3_t;
SF_API sf_result_t sf_fxr3_read_from_memory(sf_fxr3_t **out,
                                            const void *bytes, size_t size,
                                            const sf_allocator_t *a);
SF_API sf_result_t sf_fxr3_read_xml_from_memory(sf_fxr3_t **out,
                                                const char *xml, size_t size,
                                                const sf_allocator_t *a);
SF_API sf_result_t sf_fxr3_write_xml_to_memory (const sf_fxr3_t *f,
                                                char **out_xml,
                                                size_t *out_size,
                                                const sf_allocator_t *a);
SF_API size_t sf_fxr3_node_count(const sf_fxr3_t *f);
SF_API void   sf_fxr3_destroy(sf_fxr3_t *f);
```

---

## Implementation notes

* **TAE3 vs TAE4**: TAE3 is used by DS3 / Sekiro / ER, TAE4 is the AC6
  variant. Detect via header version + format byte and route to a per-
  variant parser. Most fields are identical; only the event encoding
  changes.
* **FXR3 binary** is a deeply-nested node tree with ID-tagged fields. The
  XML form mirrors this 1:1. mxml's DOM API is sufficient; no SAX needed.
* **FXR3 XML round-trip is byte-equal up to whitespace**. Tests should
  compare the parsed in-memory representation, not raw XML strings.

---

## QA scenarios

Tools: `cmake / ninja / ctest / mxml / ER copy`.

```bash
cmake -B build-mingw -DSF_ENABLE_PHASE7=ON ...
cmake --build build-mingw --target souls_formats_test_anim
ctest --test-dir build-mingw -L anim --output-on-failure
```

### Synthetic fixtures
* `test_tae_synthetic` — 1 anim × 1 event, byte-equal round-trip.
* `test_fxr3_synthetic` — minimal FXR3 tree (1 root node + 1 leaf field),
  byte-equal binary round-trip; XML write → re-read → field-equal.

### ER e2e
* `test_tae_e2e_er` — `er_extract_from_data0("/chr/c0000.anibnd.dcx")`
  → BND4 → any `.tae`, anim_count > 0.
* `test_fxr3_e2e_er` — `er_extract_from_data0("/sfx/sfxbnd_commoneffects.ffxbnd.dcx")`
  → BND4 → any `.fxr`, node_count > 0.

### Mapping Coverage Check
* [ ] Verify all `未实现` rows in `format-tae.md` are addressed.
* [ ] Verify all `未实现` rows in `format-fxr3.md` are addressed.

---

## Risks

| Risk | Mitigation |
|---|---|
| FXR3 node ID space is huge (hundreds of node types); not all are documented | Read upstream's `XmlNodeExtensions.cs` for the registry; treat unknown node IDs as opaque blobs (binary round-trip, no XML decode) |
| mxml memory-management hooks don't take an opaque user pointer | Use mxml's default malloc; no allocator override for v1.1 |
| TAE event parameter blobs are similar to EMEVD: typed but documented per-game | Keep params opaque bytes for v1.1; parametric typed access deferred to v1.2 |

---

## Exit criteria

- [ ] All deliverables checked off above.
- [ ] `ctest -L anim` green when `SF_ENABLE_PHASE7=ON`.
- [ ] `PLAN.md` Phase 7 boxes ticked.
- [ ] Decision recorded: ship in v1.0 or defer to v1.1 (record in
      [`PLAN.md` §2.3](../../.sisyphus/plans/PLAN.md)).

When green, v1 is feature-complete. Plan v2 via [post-v1.md](post-v1.md).
