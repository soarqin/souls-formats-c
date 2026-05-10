# Math Types — API Mapping

**Upstream types**: `System.Numerics.Vector2/3/4`, `System.Numerics.Quaternion`, `System.Numerics.Matrix4x4`, `System.Drawing.Color`
**Pinned commit**: `9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a`

These are .NET BCL types, not SoulsFormatsNEXT types. They are used as field types throughout the upstream format classes.

| Upstream type | Upstream loc | Kind | Our type | Status | Notes |
|---|---|---|---|---|---|
| `System.Numerics.Vector2` | BCL | struct | `sf_vec2_t` | `✓ aligned` | `float x, y` — 8 bytes, matches .NET layout |
| `System.Numerics.Vector3` | BCL | struct | `sf_vec3_t` | `✓ aligned` | `float x, y, z` — 12 bytes |
| `System.Numerics.Vector4` | BCL | struct | `sf_vec4_t` | `✓ aligned` | `float x, y, z, w` — 16 bytes |
| `System.Numerics.Quaternion` | BCL | struct | `sf_quat_t` | `✓ aligned` | `float x, y, z, w` — 16 bytes, XYZW order |
| `System.Numerics.Matrix4x4` | BCL | struct | `sf_mat4_t` | `✓ aligned` | Row-major 4×4 float — 64 bytes, m11..m44 |
| `System.Drawing.Color` | BCL | struct | `sf_color_t` | `✓ aligned` | `uint8_t a, r, g, b` — 4 bytes, ARGB order |

## Layout verification

`_Static_assert` guards in `include/souls_formats/sf_math.h` verify sizeof for each type at compile time.
