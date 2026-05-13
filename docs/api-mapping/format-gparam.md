# format-gparam.md — API Mapping

> Upstream: `SoulsFormats/Formats/GPARAM.cs`
> Phase: v0.5.0 (Post-v1 Lighting)
> Applicable games: DS2+ (V2), BB+ (V3), Sekiro+ (V5), AC6+ (V6)

## Overview

A graphics config file used since DS2. Both `.fltparam` (Sekiro pre-V6) and `.gparam` (V5+ ER/NR/AC6) are handled transparently.
GPARAM was previously misclassified as v2 in legacy.md; corrected in 0.5.0.

## Mapping Table

| Upstream Symbol | C Symbol | Type | Status | Phase | Notes |
|---|---|---|---|---|---|
| `GPARAM` | `sf_gparam_t` | Opaque | `✓ aligned` | 0.5.0 | Root container for .gparam files |
| `GPARAM.Read()` | `sf_gparam_read_from_memory` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.Write()` | `sf_gparam_write_to_buffer` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.Version` | `sf_gparam_get_version` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.GparamVersion` | `sf_gparam_version_t` | Enum | `✓ aligned` | 0.5.0 | |
| `GparamVersion.V2` | `SF_GPARAM_VERSION_V2` | Enum Value | `✗ deviation` | 0.5.0 | V2 unsupported (returns SF_ERR_UNSUPPORTED_VERSION) |
| `GparamVersion.V3` | `SF_GPARAM_VERSION_V3` | Enum Value | `✓ aligned` | 0.5.0 | |
| `GparamVersion.V5` | `SF_GPARAM_VERSION_V5` | Enum Value | `✓ aligned` | 0.5.0 | |
| `GparamVersion.V6` | `SF_GPARAM_VERSION_V6` | Enum Value | `✓ aligned` | 0.5.0 | |
| `GPARAM.Unk0D` | `sf_gparam_get_unk0d` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.Count14` | `sf_gparam_get_count14` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.Params` | `sf_gparam_param_count` / `sf_gparam_get_param` | Function | `✓ aligned` | 0.5.0 | List accessors |
| `GPARAM.Data30` | `sf_gparam_get_data30` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.UnkParamExtras` | `sf_gparam_unk_param_extra_count` / `sf_gparam_get_unk_param_extra` | Function | `✓ aligned` | 0.5.0 | List accessors |
| `GPARAM.Unk40` | `sf_gparam_get_unk40` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.Unk50` | `sf_gparam_get_unk50` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.Param` | `sf_gparam_param_t` | Opaque | `✓ aligned` | 0.5.0 | Individual param entry |
| `GPARAM.Param.Key` | `sf_gparam_param_get_key` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.Param.Name` | `sf_gparam_param_get_name` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.Param.Fields` | `sf_gparam_param_field_count` / `sf_gparam_param_get_field` | Function | `✓ aligned` | 0.5.0 | List accessors |
| `GPARAM.Param.Comments` | `sf_gparam_param_comment_count` / `sf_gparam_param_get_comment` | Function | `✓ aligned` | 0.5.0 | List accessors |
| `GPARAM.FieldType` | `sf_gparam_field_type_t` | Enum | `✓ aligned` | 0.5.0 | |
| `FieldType.Sbyte` | `SF_GPARAM_FIELD_TYPE_SBYTE` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Short` | `SF_GPARAM_FIELD_TYPE_SHORT` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Int` | `SF_GPARAM_FIELD_TYPE_INT` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Long` | `SF_GPARAM_FIELD_TYPE_LONG` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Byte` | `SF_GPARAM_FIELD_TYPE_BYTE` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Ushort` | `SF_GPARAM_FIELD_TYPE_USHORT` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Uint` | `SF_GPARAM_FIELD_TYPE_UINT` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Ulong` | `SF_GPARAM_FIELD_TYPE_ULONG` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Float` | `SF_GPARAM_FIELD_TYPE_FLOAT` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Double` | `SF_GPARAM_FIELD_TYPE_DOUBLE` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Bool` | `SF_GPARAM_FIELD_TYPE_BOOL` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Vec2` | `SF_GPARAM_FIELD_TYPE_VEC2` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Vec3` | `SF_GPARAM_FIELD_TYPE_VEC3` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Vec4` | `SF_GPARAM_FIELD_TYPE_VEC4` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.Color` | `SF_GPARAM_FIELD_TYPE_COLOR` | Enum Value | `✓ aligned` | 0.5.0 | |
| `FieldType.String` | `SF_GPARAM_FIELD_TYPE_STRING` | Enum Value | `✓ aligned` | 0.5.0 | |
| `GPARAM.IField` | `sf_gparam_field_t` | Opaque | `✓ aligned` | 0.5.0 | Individual field entry |
| `GPARAM.IField.Key` | `sf_gparam_field_get_key` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.IField.Name` | `sf_gparam_field_get_name` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.IField.Values` | `sf_gparam_field_value_count` / `sf_gparam_field_get_value` | Function | `✓ aligned` | 0.5.0 | List accessors |
| `GPARAM.FieldValue<T>` | `sf_gparam_value_t` | POD | `+ extension` | 0.5.0 | Tagged-union value POD (cite extensions.md) |
| `FieldValue<T>.Id` | `sf_gparam_value_t.id` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<T>.Unk04` | `sf_gparam_value_t.unk04` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<sbyte>.Value` | `sf_gparam_value_t.v.as_sbyte` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<short>.Value` | `sf_gparam_value_t.v.as_short` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<int>.Value` | `sf_gparam_value_t.v.as_int` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<long>.Value` | `sf_gparam_value_t.v.as_long` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<byte>.Value` | `sf_gparam_value_t.v.as_byte` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<ushort>.Value` | `sf_gparam_value_t.v.as_ushort` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<uint>.Value` | `sf_gparam_value_t.v.as_uint` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<ulong>.Value` | `sf_gparam_value_t.v.as_ulong` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<float>.Value` | `sf_gparam_value_t.v.as_float` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<double>.Value` | `sf_gparam_value_t.v.as_double` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<bool>.Value` | `sf_gparam_value_t.v.as_bool` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<Vector2>.Value` | `sf_gparam_value_t.v.as_vec2` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<Vector3>.Value` | `sf_gparam_value_t.v.as_vec3` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<Vector4>.Value` | `sf_gparam_value_t.v.as_vec4` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<Color>.Value` | `sf_gparam_value_t.v.as_color` | Field | `✓ aligned` | 0.5.0 | |
| `FieldValue<string>.Value` | `sf_gparam_value_t.v.as_string` | Field | `✓ aligned` | 0.5.0 | |
| `GPARAM.UnkParamExtra` | `sf_gparam_unk_param_extra_t` | Opaque | `✓ aligned` | 0.5.0 | |
| `GPARAM.UnkParamExtra.Unk00` | `sf_gparam_unk_param_extra_get_unk00` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.UnkParamExtra.Ids` | `sf_gparam_unk_param_extra_id_count` / `sf_gparam_unk_param_extra_get_id` | Function | `✓ aligned` | 0.5.0 | |
| `GPARAM.UnkParamExtra.Unk0c` | `sf_gparam_unk_param_extra_get_unk0c` | Function | `✓ aligned` | 0.5.0 | |
| (Internal) | `sf_gparam_destroy` | Function | `✗ deviation` | 0.5.0 | Standard C-style destructor |

## Value POD Union Members

The `sf_gparam_value_t` tagged-union (v) maps to the generic `FieldValue<T>.Value` property in C#.

| Upstream Type | C Union Member | Status | Notes |
|---|---|---|---|
| `FieldValue<sbyte>.Value` | `v.as_sbyte` | `✓ aligned` | |
| `FieldValue<short>.Value` | `v.as_short` | `✓ aligned` | |
| `FieldValue<int>.Value` | `v.as_int` | `✓ aligned` | |
| `FieldValue<long>.Value` | `v.as_long` | `✓ aligned` | |
| `FieldValue<byte>.Value` | `v.as_byte` | `✓ aligned` | |
| `FieldValue<ushort>.Value` | `v.as_ushort` | `✓ aligned` | |
| `FieldValue<uint>.Value` | `v.as_uint` | `✓ aligned` | |
| `FieldValue<ulong>.Value` | `v.as_ulong` | `✓ aligned` | |
| `FieldValue<float>.Value` | `v.as_float` | `✓ aligned` | |
| `FieldValue<double>.Value` | `v.as_double` | `✓ aligned` | |
| `FieldValue<bool>.Value` | `v.as_bool` | `✓ aligned` | |
| `FieldValue<Vector2>.Value` | `v.as_vec2` | `✓ aligned` | |
| `FieldValue<Vector3>.Value` | `v.as_vec3` | `✓ aligned` | |
| `FieldValue<Vector4>.Value` | `v.as_vec4` | `✓ aligned` | |
| `FieldValue<Color>.Value` | `v.as_color` | `✓ aligned` | |
| `FieldValue<string>.Value` | `v.as_string` | `✓ aligned` | |
