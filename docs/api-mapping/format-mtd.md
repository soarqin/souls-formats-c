# MTD API Mapping

| Upstream Class/Member | C Type/Symbol | Status | Phase | Notes |
|:---|:---|:---|:---|:---|
| `MTD` | `sf_mtd_t` | 已实现 | 6 | Opaque pointer |
| `ShaderPath` | `sf_mtd_shader_path` | 已实现 | 6 | UTF-8 string |
| `Description` | `sf_mtd_description` | 已实现 | 6 | UTF-8 string |
| `Params` | `sf_mtd_param_count` / `sf_mtd_param` | 已实现 | 6 | Array of `sf_mtd_param_t` |
| `Textures` | `sf_mtd_texture_count` / `sf_mtd_texture` | 已实现 | 6 | Array of `sf_mtd_texture_t` |
| `Param` | `sf_mtd_param_t` | 已实现 | 6 | |
| `Param.Name` | `sf_mtd_param_name` | 已实现 | 6 | |
| `Param.Type` | `sf_mtd_param_type` | 已实现 | 6 | `sf_mtd_param_type_t` |
| `Param.Value` | `sf_mtd_param_value_*` | 已实现 | 6 | Typed accessors |
| `ParamType` | `sf_mtd_param_type_t` | 已实现 | 6 | Enum |
| `Texture` | `sf_mtd_texture_t` | 已实现 | 6 | |
| `Texture.Type` | `sf_mtd_texture_type` | 已实现 | 6 | |
| `Texture.Extended" | "sf_mtd_texture_has_extended" | 已实现 | 6 | |
| `Texture.UVNumber` | `sf_mtd_texture_uv_number` | 已实现 | 6 | |
| `Texture.ShaderDataIndex` | `sf_mtd_texture_shader_data_index` | 已实现 | 6 | |
| `Texture.Path` | `sf_mtd_texture_path` | 已实现 | 6 | |
| `Texture.UnkFloats` | `sf_mtd_texture_unk_float_*` | 已实现 | 6 | |
| `BlendMode` | `sf_mtd_blend_mode_t` | 已实现 | 6 | Enum |
| `LightingType` | `sf_mtd_lighting_type_t` | 已实现 | 6 | Enum |

## Game Applicability
- **Sekiro:** Uses MTD with `Extended` texture info.
- **Dark Souls 3 / Bloodborne:** Uses MTD (standard).
- **Elden Ring / AC6:** Replaced by MATBIN.
