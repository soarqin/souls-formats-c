# Phase 5 — Script + Map

> **Status**: ⏳ Pending · **Estimate**: ~2.5 weeks · **Depends on**: Phase 3

Strict upstream alignment policy applies — see [AGENTS.md](../../AGENTS.md) §5.x.

## Goal

Implement EMEVD (event scripts), ESD (state machines), and the MSB family
for Sekiro (MSBS), Elden Ring + Nightreign (MSBE), and Armored Core VI
(MSBVI). Together these cover every gameplay-logic surface of the four
target games.

This is the longest single phase because MSB has many sub-record types
and varies between games.

---

## Deliverables

### 1. EMEVD (Event Scripts)
Implement the event-script bytecode format.

*   **Upstream references**:
    *   `SoulsFormats/Formats/EMEVD/EMEVD.cs`
    *   `SoulsFormats/Formats/EMEVD/EMEVD.Event.cs`
    *   `SoulsFormats/Formats/EMEVD/EMEVD.Instruction.cs`
    *   Mapping: [format-emevd.md](../api-mapping/format-emevd.md)
*   **API alignment checklist**:
    *   `sf_emevd_t` must mirror `EMEVD` class (Format, Events, LinkedFileOffsets).
    *   `sf_emevd_event_t` must mirror `Event` class (ID, Instructions, Parameters).
    *   `sf_emevd_instruction_t` must mirror `Instruction` class (Bank, ID, ArgData).

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
├── sf_emevd.h
├── sf_esd.h
├── sf_msb.h              ← shared types
├── sf_msbs.h
├── sf_msbe.h
└── sf_msbvi.h
src/script/
├── emevd.c
└── esd.c
src/map/
├── msb_common.c          ← shared entry-list / list-of-list parser
├── msbs.c
├── msbe.c
└── msbvi.c
tests/
├── script/
│   ├── test_emevd_synthetic.c
│   ├── test_esd_synthetic.c
│   ├── test_emevd_e2e_er.c
│   └── test_esd_e2e_er.c
└── map/
    ├── test_msbs_synthetic.c
    ├── test_msbe_synthetic.c
    ├── test_msbvi_synthetic.c
    └── test_msbe_e2e_er.c
```

---

## Public API sketch

```c
/* sf_msb.h — shared types */
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

/* sf_msbe.h — Elden Ring */
typedef struct sf_msbe sf_msbe_t;
SF_API sf_result_t sf_msbe_read_from_memory(sf_msbe_t **out,
                                            const void *bytes, size_t size,
                                            const sf_allocator_t *a);
SF_API size_t sf_msbe_part_count  (const sf_msbe_t *m);
SF_API size_t sf_msbe_region_count(const sf_msbe_t *m);
SF_API size_t sf_msbe_event_count (const sf_msbe_t *m);
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
* **EMEVD instruction encoding** has per-instruction parameter blobs
  whose length depends on a separate "instruction definition" table that
  ships *with* the EMEVD file. The parameter blob bytes are kept opaque
  for v1 — we read them as raw bytes and let consumers interpret. This
  matches upstream's `Instruction.Bytes` field.
* **ESD bytecode** is compact and well-documented in upstream — port
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
* `test_emevd_synthetic` — 1 event × 1 instruction round-trip.
* `test_esd_synthetic` — 2 states × 1 transition round-trip.
* `test_msbs_synthetic`, `test_msbe_synthetic`, `test_msbvi_synthetic` —
  each a 1 Part + 1 Region + 1 Event + 1 Model minimal MSB, byte-equal.

### ER e2e (every test goes through `er_extract_from_data0`)
* `test_emevd_e2e_er` — `er_extract_from_data0("/event/m60_42_36_00.emevd.dcx")`
  (Limgrave central event file), parse, event count > 50, well-known
  event id (e.g., 4234) findable.
* `test_msbe_e2e_er` — `er_extract_from_data0("/map/mapstudio/m60_42_36_00.msb.dcx")`,
  parse, Part count > 0, Region count > 0, Event count > 0, any Part's
  bounding box non-zero.
* `test_esd_e2e_er` — `er_extract_from_data0("/script/talk/m10_00_00_00.talkesdbnd.dcx")`,
  open BND4, find any `.esd`, state count > 0.

### Mapping Coverage Check
* [ ] Verify all `未实现` rows in `format-emevd.md` are addressed.
* [ ] Verify all `未实现` rows in `format-esd.md` are addressed.
* [ ] Verify all `未实现` rows in `format-msb-common.md` are addressed.
* [ ] Verify all `未实现` rows in `format-msbs.md`, `format-msbe.md`, and `format-msbvi.md` are addressed.

---

## Risks

| Risk | Mitigation |
|---|---|
| MSBE has subtle field-order differences between ER 1.0 / 1.x / Nightreign | Detect via header version; per-version override only the changed records |
| MSBVI is sparsely documented; field semantics may be wrong | Compare round-trip bytes against an extracted real msb; never compare against ground-truth field values |
| Gigabytes of e2e churn if every map is round-tripped | Phase exit only requires one map per game; scale-out is post-GA |
| ESD is rare in ER (mostly used by NPC dialog) | Pick `m10_00_00_00.talkesdbnd.dcx` which is known to ship in ER 1.x |

---

## Exit criteria

- [ ] All deliverables checked off above.
- [ ] `ctest -L 'script|map'` green on dev machine for ER subset; Sekiro / AC6 / Nightreign tests SKIP cleanly.
- [ ] `PLAN.md` Phase 5 boxes ticked.

When green, proceed to [Phase 6](phase-6-geometry-material.md).
