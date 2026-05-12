# T0.2 — Reserve/Fill Mismatch Audit

## Verdict: GO (no bugs found)

Raw counts: 247 reserves, 238 fills (grep-based).
After accounting for dynamic-name patterns: all reserves are matched.

## Analysis

The grep-based count shows 247 reserves vs 238 fills = 9 apparent unmatched.
However, the grep pattern only matches **string-literal** name arguments.
Several patterns use **dynamic names** (variables or helper functions) that grep misses:

### Pattern 1: MSB NextList reserves (filled by parent msb*.c via snprintf)

The per-param-type files (`event_param.c`, `model_param.c`, `parts_param.c`, etc.) reserve
`"MsbsNextList<N>"`, `"MsbeNextList<N>"`, `"MsbviNextList<N>"` with literal names.
The fills happen in the parent `msbs.c`, `msbe.c`, `msbvi.c` via:
```c
snprintf(next_name, sizeof next_name, "MsbsNextList%d", reserve_id);
sf_binary_writer_fill_i64(w, next_name, offset);
```
These are **Legit** — the fill uses a dynamically constructed name that matches the reserve.

### Pattern 2: FXR3 offset reserves (filled via fill_pos() helper)

`fxr3.c` reserves `"StateMapOffset"`, `"StateOffset"`, `"TransitionOffset"`, etc.
These are filled by the `fill_pos()` static helper:
```c
static sf_result_t fill_pos(sf_binary_writer_t *bw, const char *name) {
    return sf_binary_writer_fill_i32(bw, name, sf_binary_writer_position(bw));
}
```
Called as `fill_pos(bw, "StateMapOffset")` etc. The grep for `fill_i32(bw, "StateMapOffset")`
misses these because the literal string is passed to `fill_pos`, not directly to `fill_i32`.
These are **Legit**.

### Pattern 3: TAE SkeletonName/SibName (filled via tae_write_optional_utf16)

`tae.c` reserves `"SkeletonName"` and `"SibName"` as varint.
These are filled by `tae_write_optional_utf16(bw, reserve_name, value)` which calls
`sf_binary_writer_fill_varint(bw, reserve_name, ...)` with the name as a parameter.
These are **Legit**.

### Pattern 4: FLVER2 dynamic labels (bbox_label, bone_label, etc.)

`flver2_mesh.c` uses dynamic label variables for per-mesh reserves.
The fills use the same variable. **Legit**.

### Pattern 5: flver_common.c name_buf

Dynamic name variable. **Legit**.

### Pattern 6: msbs/parts_param.c dynamic keys (unk1_key, unk2_key, etc.)

Dynamic key variables for per-subtype reserves. **Legit**.

## Classification Table (unmatched by grep)

| Site | Name | Type | Reserve file:line | Fill location | Classification | Action |
|------|------|------|-------------------|---------------|----------------|--------|
| msbs/event_param.c:424 | MsbsNextList1 | i64 | event_param.c:424 | msbs.c:111 (dynamic snprintf) | Legit | document-as-expected |
| msbs/model_param.c:231 | MsbsNextList0 | i64 | model_param.c:231 | msbs.c:111 (dynamic snprintf) | Legit | document-as-expected |
| msbs/parts_param.c:613 | MsbsNextList5 | i64 | parts_param.c:613 | msbs.c:111 (dynamic snprintf) | Legit | document-as-expected |
| msbs/point_param.c:508 | MsbsNextList2 | i64 | point_param.c:508 | msbs.c:111 (dynamic snprintf) | Legit | document-as-expected |
| msbs/route_param.c:136 | MsbsNextList3 | i64 | route_param.c:136 | msbs.c:111 (dynamic snprintf) | Legit | document-as-expected |
| msbe/event_param.c:181 | MsbeNextList1 | i64 | event_param.c:181 | msbe.c:105 (dynamic snprintf) | Legit | document-as-expected |
| msbe/model_param.c:166 | MsbeNextList0 | i64 | model_param.c:166 | msbe.c:105 (dynamic snprintf) | Legit | document-as-expected |
| msbe/parts_param.c:122 | MsbeNextList5 | i64 | parts_param.c:122 | msbe.c:105 (dynamic snprintf) | Legit | document-as-expected |
| msbe/point_param.c:155 | MsbeNextList2 | i64 | point_param.c:155 | msbe.c:105 (dynamic snprintf) | Legit | document-as-expected |
| msbe/route_param.c:97 | MsbeNextList3 | i64 | route_param.c:97 | msbe.c:105 (dynamic snprintf) | Legit | document-as-expected |
| msbvi/event_param.c:125 | MsbviNextList1 | i64 | event_param.c:125 | msbvi.c:85 (dynamic snprintf) | Legit | document-as-expected |
| msbvi/layer_param.c:71 | MsbviNextList4 | i64 | layer_param.c:71 | msbvi.c:85 (dynamic snprintf) | Legit | document-as-expected |
| msbvi/model_param.c:114 | MsbviNextList0 | i64 | model_param.c:114 | msbvi.c:85 (dynamic snprintf) | Legit | document-as-expected |
| msbvi/parts_param.c:96 | MsbviNextList5 | i64 | parts_param.c:96 | msbvi.c:85 (dynamic snprintf) | Legit | document-as-expected |
| msbvi/point_param.c:95 | MsbviNextList2 | i64 | point_param.c:95 | msbvi.c:85 (dynamic snprintf) | Legit | document-as-expected |
| msbvi/route_param.c:68 | MsbviNextList3 | i64 | route_param.c:68 | msbvi.c:85 (dynamic snprintf) | Legit | document-as-expected |
| fxr3.c:699 | StateMapStatesOffset | i32 | fxr3.c:699 | fxr3.c:962 via fill_pos() | Legit | document-as-expected |
| fxr3.c:919 | StateMapOffset | i32 | fxr3.c:919 | fxr3.c:958 via fill_pos() | Legit | document-as-expected |
| fxr3.c:921 | StateOffset | i32 | fxr3.c:921 | fxr3.c:961 via fill_pos() | Legit | document-as-expected |
| fxr3.c:924 | TransitionOffset | i32 | fxr3.c:924 | fxr3.c:965 via fill_pos() | Legit | document-as-expected |
| fxr3.c:926 | ContainerOffset | i32 | fxr3.c:926 | fxr3.c:975 via fill_pos() | Legit | document-as-expected |
| fxr3.c:928 | EffectOffset | i32 | fxr3.c:928 | fxr3.c:982 via fill_pos() | Legit | document-as-expected |
| fxr3.c:930 | ActionOffset | i32 | fxr3.c:930 | fxr3.c:997 via fill_pos() | Legit | document-as-expected |
| fxr3.c:932 | PropertyOffset | i32 | fxr3.c:932 | fxr3.c:1013 via fill_pos() | Legit | document-as-expected |
| fxr3.c:934 | ModifierOffset | i32 | fxr3.c:934 | fxr3.c:1023 via fill_pos() | Legit | document-as-expected |
| fxr3.c:936 | ConditionalPropertyOffset | i32 | fxr3.c:936 | fxr3.c:1033 via fill_pos() | Legit | document-as-expected |
| fxr3.c:938 | UnkFieldListOffset | i32 | fxr3.c:938 | fxr3.c:1043 via fill_pos() | Legit | document-as-expected |
| fxr3.c:940 | FieldOffset | i32 | fxr3.c:940 | fxr3.c:1053 via fill_pos() | Legit | document-as-expected |
| fxr3.c:945 | ReferenceOffset | i32 | fxr3.c:945 | fxr3.c:1089 via fill_pos() | Legit | document-as-expected |
| fxr3.c:948 | ExternalValueOffset | i32 | fxr3.c:948 | fxr3.c:1092 via fill_pos() | Legit | document-as-expected |
| fxr3.c:951 | UnkBloodEnablerOffset | i32 | fxr3.c:951 | fxr3.c:1095 via fill_pos() | Legit | document-as-expected |
| tae.c:832 | SkeletonName | varint | tae.c:832 | tae.c:849 via tae_write_optional_utf16() | Legit | document-as-expected |
| tae.c:835 | SibName | varint | tae.c:835 | tae.c:849 via tae_write_optional_utf16() | Legit | document-as-expected |
| flver2_mesh.c:143 | bbox_label (dynamic) | i32 | flver2_mesh.c:143 | flver2_mesh.c (same var) | Legit | document-as-expected |
| flver2_mesh.c:144 | bone_label (dynamic) | i32 | flver2_mesh.c:144 | flver2_mesh.c (same var) | Legit | document-as-expected |
| flver2_mesh.c:147 | face_label (dynamic) | i32 | flver2_mesh.c:147 | flver2_mesh.c (same var) | Legit | document-as-expected |
| flver2_mesh.c:150 | vb_label (dynamic) | i32 | flver2_mesh.c:150 | flver2_mesh.c (same var) | Legit | document-as-expected |
| flver_common.c:331 | name_buf (dynamic) | i32 | flver_common.c:331 | flver_common.c (same var) | Legit | document-as-expected |
| msbs/parts_param.c:557 | unk1_key (dynamic) | i64 | parts_param.c:557 | parts_param.c (same var) | Legit | document-as-expected |
| msbs/parts_param.c:558 | unk2_key (dynamic) | i64 | parts_param.c:558 | parts_param.c (same var) | Legit | document-as-expected |
| msbs/parts_param.c:561 | gparam_key (dynamic) | i64 | parts_param.c:561 | parts_param.c (same var) | Legit | document-as-expected |
| msbs/parts_param.c:562 | scene_key (dynamic) | i64 | parts_param.c:562 | parts_param.c (same var) | Legit | document-as-expected |
| msbs/parts_param.c:563 | unk7_key (dynamic) | i64 | parts_param.c:563 | parts_param.c (same var) | Legit | document-as-expected |
| msbvi/parts_param.c:82 | type_res (dynamic) | i64 | parts_param.c:82 | parts_param.c (same var) | Legit | document-as-expected |

## Downstream Impact

- **T1.8**: NO-OP — no bugs found. CHANGELOG note only.
- **T2.3**: GO — reserve/fill scaffold macro extraction is safe to proceed.
