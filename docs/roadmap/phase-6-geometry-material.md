# Phase 6 — Geometry + Material

> **Status**: ✅ Done (2026-05-12) · **Tests**: 15/15 PASS · **Depends on**: Phase 3

Strict upstream alignment policy applies — see [AGENTS.md](../../AGENTS.md) §5.x.

## Goal

Implement FLVER2 (the modern FromSoft mesh format used by Sekiro / ER /
Nightreign / AC6), MTD (Sekiro-era material defs), and MATBIN (ER / AC6
material defs). FLVER2 is the largest single undertaking in v1 because
its vertex layouts vary widely between models, materials, and games.

All deliverables are complete and verified against synthetic fixtures and
real game data from Elden Ring and Sekiro.

---

## Deliverables

### 1. FLVER Common ✅
Implement shared utilities for FLVER formats.

*   **Upstream references**:
    *   `SoulsFormats/Formats/FLVER/FLVER.cs`
    *   Mapping: [format-flver-common.md](../api-mapping/format-flver-common.md)
*   **API alignment checklist**:
    *   [x] `sf_flver_dummy_t` must mirror `FLVER.Dummy`.
    *   [x] `sf_flver_node_t` must mirror `FLVER.Node`.
    *   [x] Implement half-float and 11_11_10 normal packing helpers matching upstream static methods.

### 2. FLVER2 (Modern Mesh) ✅
Implement the modern mesh format with per-game vertex layout dispatch.

*   **Upstream references**:
    *   `SoulsFormats/Formats/FLVER/FLVER2/FLVER2.cs`
    *   `SoulsFormats/Formats/FLVER/FLVER2/FLVER2.BufferLayout.cs`
    *   `SoulsFormats/Formats/FLVER/FLVER2/FLVER2.LayoutMember.cs`
    *   Mapping: [format-flver2.md](../api-mapping/format-flver2.md)
*   **API alignment checklist**:
    *   [x] `sf_flver2_t` must mirror `FLVER2` class (Header, GXList, Materials, Meshes, SkeletonSet).
    *   [x] `sf_flver2_buffer_layout_t` must mirror `BufferLayout` and its `LayoutMember` list.
    *   [x] **Vertex Layout Dispatch**: Implement a registry matching the `LayoutType` and `Semantic` logic in `FLVER2.LayoutMember.cs`. Handle game-specific differences (e.g., AC6-specific `UShort4` normals, ER `uvFactor` logic).

### 3. MTD (Sekiro Materials) ✅
Implement the material definition format used in Sekiro and earlier titles.

*   **Upstream references**:
    *   `SoulsFormats/Formats/MTD.cs`
    *   Mapping: [format-mtd.md](../api-mapping/format-mtd.md)
*   **API alignment checklist**:
    *   [x] `sf_mtd_t` must mirror `MTD` class (ShaderPath, Description, Params, Textures).
    *   [x] Handle `Extended` texture info used in Sekiro.

### 4. MATBIN (ER / AC6 Materials) ✅
Implement the modern material definition format.

*   **Upstream references**:
    *   `SoulsFormats/Formats/MATBIN.cs`
    *   Mapping: [format-matbin.md](../api-mapping/format-matbin.md)
*   **API alignment checklist**:
    *   [x] `sf_matbin_t` must mirror `MATBIN` class (ShaderPath, SourcePath, Params, Samplers).
    *   [x] Match `ParamType` enum and value union exactly.

---

## File structure

```
include/souls_formats/
├── sf_flver.h
├── sf_flver2.h
├── sf_mtd.h
└── sf_matbin.h
src/geom/
├── flver_common.c        ← shared half-float, 11_11_10, normal packing
├── flver2.c              ← top-level + mesh / bone / material parsing
├── flver2_vertex.c       ← THE vertex layout registry
├── mtd.c
└── matbin.c
tests/geom/
├── test_flver2_synthetic.c
├── test_mtd_synthetic.c
├── test_matbin_synthetic.c
├── test_flver2_e2e_er.c
└── test_matbin_e2e_er.c
```

---

## Public API sketch

```c
/* sf_flver.h */
typedef enum sf_flver_layout_type {
    SF_FLVER_LAYOUT_FLOAT2 = 0x01,
    SF_FLVER_LAYOUT_FLOAT3 = 0x02,
    SF_FLVER_LAYOUT_FLOAT4 = 0x03,
    SF_FLVER_LAYOUT_BYTE4A = 0x10,
    SF_FLVER_LAYOUT_BYTE4B = 0x11,
    SF_FLVER_LAYOUT_SHORT2_TO_FLOAT2 = 0x12,
    /* … full set per upstream LayoutMember.LayoutType … */
} sf_flver_layout_type_t;

typedef enum sf_flver_layout_semantic {
    SF_FLVER_SEM_POSITION = 0,
    SF_FLVER_SEM_BONE_WEIGHTS,
    SF_FLVER_SEM_BONE_INDICES,
    SF_FLVER_SEM_NORMAL,
    SF_FLVER_SEM_UV,
    SF_FLVER_SEM_TANGENT,
    SF_FLVER_SEM_BITANGENT,
    SF_FLVER_SEM_VERTEX_COLOR,
} sf_flver_layout_semantic_t;

float sf_half_to_float(uint16_t half);
uint16_t sf_float_to_half(float f);

/* sf_flver2.h */
typedef struct sf_flver2 sf_flver2_t;

SF_API sf_result_t sf_flver2_read_from_memory(sf_flver2_t **out,
                                              const void *bytes, size_t size,
                                              const sf_allocator_t *a);
SF_API size_t sf_flver2_mesh_count    (const sf_flver2_t *f);
SF_API size_t sf_flver2_bone_count    (const sf_flver2_t *f);
SF_API size_t sf_flver2_material_count(const sf_flver2_t *f);
SF_API const sf_flver2_mesh_t *sf_flver2_mesh(const sf_flver2_t *f, size_t i);
SF_API void   sf_flver2_destroy(sf_flver2_t *f);

/* Optional decode helper: lay out vertex attributes into typed arrays. */
typedef struct sf_flver2_decoded_mesh {
    uint32_t   vertex_count;
    sf_vec3_t *positions;     /* always present */
    sf_vec3_t *normals;       /* NULL if absent */
    sf_vec2_t *uvs[8];        /* up to 8 channels; NULL where absent */
    uint8_t   *bone_indices;  /* 4 per vertex if skinned */
    float     *bone_weights;  /* 4 per vertex if skinned */
    uint32_t  *indices;
    uint32_t   index_count;
} sf_flver2_decoded_mesh_t;

SF_API sf_result_t sf_flver2_decode_mesh(const sf_flver2_t *f, size_t mesh_index,
                                         sf_flver2_decoded_mesh_t *out,
                                         const sf_allocator_t *a);
SF_API void sf_flver2_decoded_mesh_free(sf_flver2_decoded_mesh_t *m,
                                        const sf_allocator_t *a);
```

---

## Implementation notes

* **Vertex layout registry** is the single highest-risk piece. Build a
  table indexed by `(layout_type, semantic, index)` and emit a per-element
  decoder function pointer. New unknown layouts must NOT crash; instead
  return `SF_ERR_UNSUPPORTED_VERSION` and log the layout id so the
  registry can be extended later.
* **Half-float conversion** must be IEEE 754-correct (handle subnormal,
  inf, NaN). Use `_cvtss_sh` / `_cvtsh_ss` if available; else the bit-twiddling
  reference impl from upstream.
* **11_11_10 normals** decode to signed [-1, 1] floats. Phase 1 already
  delivered `sf_binary_reader_read_vec3_11_11_10` for this.
* **Bone weights** can be float, byte, or short — pick the decoder via
  layout type.
* **MATBIN params** carry typed values (bool / int / float / vec / string).
  Match upstream's `MATBIN.Param.ParamType` enum exactly.
* **Round-trip caveat**: FLVER2 vertex *bytes* round-trip is the goal,
  not high-level field equality. Some packed formats lose precision (e.g.,
  byte normals); we round-trip the bytes verbatim.

---

## QA scenarios

Tools: `cmake / ninja / ctest / WSL interop / ER copy / Oodle DLL`.

```bash
cmake --build build-mingw --target souls_formats_test_geom
ctest --test-dir build-mingw -L geom --output-on-failure
```

### Synthetic fixtures
* `test_flver2_synthetic` — 1 mesh × 1 material × 8 vertices × 12 indices
  (a unit cube), byte-equal round-trip.
* `test_mtd_synthetic` — 3 params × 2 samplers, byte-equal.
* `test_matbin_synthetic` — 5 params × 3 samplers, byte-equal.

### ER e2e
* `test_flver2_e2e_er` — `er_extract_from_data0("/chr/c0000.chrbnd.dcx")`
  → BND4 → entry `c0000.flver` → parse. Assert mesh_count > 0,
  bone_count > 0, material_count > 0, **all vertex layout types in the
  built-in registry** (no `SF_ERR_UNSUPPORTED_VERSION`).
* `test_matbin_e2e_er` — `er_extract_from_data0("/material/allmaterial.matbinbnd.dcx")`
  → BND4 → any `.matbin` → shader name non-empty (matches `ER_*.spx`
  pattern), params non-empty.

### Mapping Coverage Check
* [x] Verify all `未实现` rows in `format-flver-common.md` are addressed.
* [x] Verify all `未实现` rows in `format-flver2.md` are addressed.
* [x] Verify all `未实现` rows in `format-mtd.md` and `format-matbin.md` are addressed.

---

## Risks

| Risk | Mitigation |
|---|---|
| Vertex layout zoo: hundreds of permutations across games | Build registry as data, not code; layout decoders are small functions registered at init |
| FLVER2 has both v1 and v2 bone hierarchy variations across games | Detect via header version; share parser, branch on hierarchy reader |
| MATBIN field types include strings whose encoding differs (UTF-8 vs Shift-JIS) | Match upstream verbatim; document in the implementation comments |
| Round-trip bytes differ on platforms with different float representation | We're x86_64-only; IEEE 754 is universal — but watch for `-ffast-math` (we don't enable it) |

---

## Exit criteria

- [x] All deliverables checked off above.
- [x] `ctest -L geom` green on dev machine.
- [x] No `SF_ERR_UNSUPPORTED_VERSION` for `c0000.flver` and the first
      five `.matbin` files extracted from `allmaterial.matbinbnd.dcx`.
- [x] `PLAN.md` Phase 6 boxes ticked.

When green, the v1 core is complete. Decide whether to ship Phase 7 with
v1.0 or defer to v1.1 (see [Phase 7](phase-7-animation-effects.md)).
