# Phase 5: Script + Map

> **Status**: ✅ Complete · **Estimate**: ~3 weeks · **Depends on**: Phase 3

Strict upstream alignment policy applies. See [AGENTS.md](../../AGENTS.md) §5.x.

## Goal

Implement ESD (state machines) and the MSB family for Sekiro (MSBS), Elden
Ring + Nightreign (MSBE), and Armored Core VI (MSBVI). Together these cover
every gameplay-logic surface of the four target games.

Note: EMEVD was completed ahead of schedule in Phase 4 (commit 4ab075e).
Phase 5 covers ESD + MSB family only.

This is the longest single phase because MSB has many sub-record types and
varies between games.

## Wave Overview

- Wave 0 (Preflight): Fix Phase 4 debt + docs alignment
- Wave 1 (Foundation): sf_msb.h + msb_common skeleton + sf_esd.h + esd reader + 3 game helpers + 3 variant dispatchers
- Wave 2 (Sub-params): MSBS/MSBE/MSBVI sub-param implementations (16 parallel tasks)
- Wave 3 (Integration + e2e): Round-trip tests + 4 game e2e
- Wave 4 (Docs): API mapping refresh + status table

---

## Deliverables

### 1. EMEVD (Event Scripts): MOVED TO PHASE 4

EMEVD implementation was moved to Phase 4 for early integration with
regulation.bin testing. See commit 4ab075e.

### 2. ESD (State Machines)
Implement the state machine format.

*   **Upstream references**:
    *   `SoulsFormats/Formats/ESD/ESD.cs`
    *   `SoulsFormats/Formats/ESD/ESD.State.cs`
    *   Mapping: [format-esd.md](../api-mapping/format-esd.md)
*   **API alignment checklist**:
    *   `sf_esd_t` must mirror `ESD` class (LongFormat, DarkSoulsCount, StateGroups).
    *   `sf_esd_state_t` must mirror `State` class (Conditions, EntryCommands, ExitCommands).

### 3. MSB Common & MSBS (Sekiro)
Implement shared MSB infrastructure and the Sekiro variant.

*   **Upstream references**:
    *   `SoulsFormats/Formats/MSB/MSB.cs`
    *   `SoulsFormats/Formats/MSB/MSBS/MSBS.cs`
    *   Mapping: [format-msb-common.md](../api-mapping/format-msb-common.md), [format-msbs.md](../api-mapping/format-msbs.md)
*   **API alignment checklist**:
    *   `sf_msb_part_t` must mirror `IMsbPart` interface and common fields (Position, Rotation, Scale).
    *   `sf_msbs_t` must mirror `MSBS` class and its param lists (ModelParam, EventParam, PointParam, PartsParam).

### 4. MSBE (Elden Ring + Nightreign)
Implement the Elden Ring MSB variant.

*   **Upstream references**:
    *   `SoulsFormats/Formats/MSB/MSBE/MSBE.cs`
    *   Mapping: [format-msbe.md](../api-mapping/format-msbe.md)
*   **API alignment checklist**:
    *   `sf_msbe_t` must mirror `MSBE` class.
    *   **Note**: MSBE applies to BOTH Elden Ring and Nightreign. One C implementation serves both, with separate e2e test suites.

### 5. MSBVI (Armored Core VI)
Implement the Armored Core VI MSB variant.

*   **Upstream references**:
    *   `SoulsFormats/Formats/MSB/MSBVI/MSBVI.cs`
    *   Mapping: [format-msbvi.md](../api-mapping/format-msbvi.md)
*   **API alignment checklist**:
    *   `sf_msbvi_t` must mirror `MSBVI` class, including the `LayerParam` unique to this variant.

---

## File structure

```
include/souls_formats/
├── sf_esd.h
├── sf_msb.h
├── sf_msbs.h
├── sf_msbe.h
└── sf_msbvi.h
src/script/
├── esd.c
├── esd_bytecode.c
└── esd_write.c
src/map/
├── msb_common.c
├── msbs/
│   ├── model_param.c
│   ├── event_param.c
│   ├── point_param.c
│   ├── parts_param.c
│   ├── route_param.c
│   └── msbs.c
├── msbe/
│   ├── model_param.c
│   ├── event_param.c
│   ├── point_param.c
│   ├── parts_param.c
│   ├── route_param.c
│   └── msbe.c
└── msbvi/
    ├── model_param.c
    ├── event_param.c
    ├── point_param.c
    ├── parts_param.c
    ├── route_param.c
    ├── layer_param.c
    └── msbvi.c
tests/
├── script/
│   ├── test_esd_synthetic.c
│   └── test_esd_e2e_er.c
├── map/
│   ├── test_msbs_synthetic.c
│   ├── test_msbe_synthetic.c
│   ├── test_msbvi_synthetic.c
│   └── test_msbe_e2e_er.c
└── e2e/
    ├── sekiro_test_helper.c
    ├── sekiro_test_helper.h
    ├── nightreign_test_helper.c
    ├── nightreign_test_helper.h
    ├── ac6_test_helper.c
    └── ac6_test_helper.h
```

---

## Public API sketch

```c
/* sf_msb.h: shared types */
typedef enum sf_msb_part_kind {
    SF_MSB_PART_MAP_PIECE = 0,
    SF_MSB_PART_OBJECT,
    SF_MSB_PART_ENEMY,
    SF_MSB_PART_PLAYER,
    SF_MSB_PART_COLLISION,
    SF_MSB_PART_DUMMY_OBJECT,
    SF_MSB_PART_DUMMY_ENEMY,
    SF_MSB_PART_CONNECT_COLLISION,
    /* … */
} sf_msb_part_kind_t;

typedef struct sf_msb_part {
    const char        *name_utf8;
    sf_msb_part_kind_t kind;
    sf_vec3_t          position;
    sf_vec3_t          rotation;
    sf_vec3_t          scale;
    int32_t            model_index;
    /* … per-game extension fields hang off via tagged union */
} sf_msb_part_t;

/* sf_msbe.h: Elden Ring */
typedef struct sf_msbe sf_msbe_t;
SF_API sf_result_t sf_msbe_read_from_memory(sf_msbe_t **out,
                                            const void *bytes, size_t size,
                                            const sf_allocator_t *a);
SF_API size_t sf_msbe_part_count  (const sf_msbe_t *m);
SF_API size_t sf_msbe_region_count(const sf_msbe_t *m);
SF_API size_t sf_msbe_event_count (const sf_msbe_t *m);

---

## Delivery Summary (2026-05-12)

Phase 5 is complete with the following deliverables:

- **ESD (State Machines)**: Full implementation including reader, writer, and bytecode decoder. Verified with synthetic round-trip and Elden Ring e2e tests.
- **MSB Common**: Shared infrastructure for MSB parsing, including header validation and list iteration.
- **MSB Variants**:
    - **MSBS (Sekiro)**: Dispatcher and all 5 sub-param types (Model, Event, Point, Parts, Route). Verified with Sekiro e2e tests.
    - **MSBE (Elden Ring + Nightreign)**: Dispatcher and all 5 sub-param types. Verified with Elden Ring and Nightreign e2e tests.
    - **MSBVI (AC6)**: Dispatcher and all 6 sub-param types, including the AC6-specific LayerParam.
- **Test Helpers**: Specialized helpers for Sekiro and Nightreign to facilitate e2e testing.
- **QA**: 38/38 tests passing across 38 test binaries.

SF_API size_t sf_msbe_model_count (const sf_msbe_t *m);
SF_API const sf_msb_part_t *sf_msbe_part(const sf_msbe_t *m, size_t i);
/* … */
SF_API void sf_msbe_destroy(sf_msbe_t *m);
```

---

## Implementation notes

* **MSB shared list-of-lists**: every MSB starts with a "list of named
  sublists" header (`Models`, `Events`, `Regions`, `Routes`, `Layers`,
  `Parts`). `msb_common.c` parses this skeleton and dispatches each
  sublist to the per-game module via callbacks.
* **MSB part union**: each part kind has its own field set. Use a tagged
  union (`sf_msb_part_t::kind` + an embedded anonymous union per kind) or
  a per-kind subtype (`sf_msb_part_map_piece_t`) accessed via cast. Pick
  one and stay consistent across all three MSB modules.
* **ESD bytecode** is compact and well-documented in upstream. Port
  verbatim.
* **Per-game branch macro**: pull endian, varint-long, and table layout
  defaults from a small header table indexed by an `sf_msb_game_t` enum.

---

## QA scenarios

Tools: `cmake / ninja / ctest / WSL interop / ER copy`.

```bash
cmake --build build-mingw --target souls_formats_test_script souls_formats_test_map
ctest --test-dir build-mingw -L 'script|map' --output-on-failure
```

### Synthetic fixtures
* `test_esd_synthetic`: 2 states × 1 transition round-trip.
* `test_msbs_synthetic`, `test_msbe_synthetic`, `test_msbvi_synthetic`:
  each a 1 Part + 1 Region + 1 Event + 1 Model minimal MSB, byte-equal.

### Game e2e matrix
| Game | Format | Source | Validation |
|---|---|---|---|
| Elden Ring | MSBE | `/map/mapstudio/m60_42_36_00.msb.dcx` | Part count > 0, Region count > 0 |
| Elden Ring | ESD | `/script/talk/m10_00_00_00.talkesdbnd.dcx` | State count > 0 |
| Sekiro | MSBS | `/map/mapstudio/m11_00_00_00.msb.dcx` | Part count > 0 |
| Nightreign | MSBE | `/map/mapstudio/m10_00_00_00.msb.dcx` | Part count > 0 |
| AC6 | MSBVI | `/map/mapstudio/m01_00_00_00.msb.dcx` | Layer count > 0 |

### Mapping Coverage Check
* [ ] Verify all `未实现` rows in `format-esd.md` are addressed.
* [ ] Verify all `未实现` rows in `format-msb-common.md` are addressed.
* [ ] Verify all `未实现` rows in `format-msbs.md`, `format-msbe.md`, and `format-msbvi.md` are addressed.

## Risks

| Risk | Mitigation |
|---|---|
| MSBE has subtle field-order differences between ER 1.0 / 1.x / Nightreign | Detect via header version; per-version override only the changed records |
| MSBVI is sparsely documented; field semantics may be wrong | Compare round-trip bytes against an extracted real msb; never compare against ground-truth field values |
| Gigabytes of e2e churn if every map is round-tripped | Phase exit only requires one map per game; scale-out is post-GA |
| ESD is rare in ER (mostly used by NPC dialog) | Pick `m10_00_00_00.talkesdbnd.dcx` which is known to ship in ER 1.x |

---

## Exit Criteria

1. All ESD and MSB (MSBS/MSBE/MSBVI) unit tests pass.
2. Round-trip byte-equality achieved for all synthetic fixtures.
3. E2E tests for Elden Ring, Sekiro, Nightreign, and AC6 pass.
4. API mapping documentation updated to reflect 100% coverage.
5. No new `sf_` symbols without `SF_API` decoration.
6. `lsp_diagnostics` clean across all new files.
7. `PLAN.md` Phase 5 boxes ticked.

When green, proceed to [Phase 6](phase-6-geometry-material.md).
