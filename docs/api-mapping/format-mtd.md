# MTD API Mapping

| Upstream Class/Member | C Type/Symbol | Status | Phase | Notes |
|:---|:---|:---|:---|:---|
| `MTD` | `sf_mtd_t` | 未实现 | 6 | Opaque pointer |
| `ShaderPath` | `shader_path` | 未实现 | 6 | UTF-8 string |
| `Description` | `description` | 未实现 | 6 | UTF-8 string |
| `Params` | `params` | 未实现 | 6 | Array of `sf_mtd_param_t` |
| `Textures` | `textures` | 未实现 | 6 | Array of `sf_mtd_texture_t` |
| `Param` | `sf_mtd_param_t` | 未实现 | 6 | |
| `Param.Name` | `name` | 未实现 | 6 | |
| `Param.Type` | `type` | 未实现 | 6 | `sf_mtd_param_type_t` |
| `Param.Value` | `value` | 未实现 | 6 | Union of supported types |
| `ParamType` | `sf_mtd_param_type_t` | 未实现 | 6 | Enum |
| `Texture` | `sf_mtd_texture_t` | 未实现 | 6 | |
| `Texture.Type` | `type` | 未实现 | 6 | |
| `Texture.Extended` | `extended` | 未实现 | 6 | |
| `Texture.UVNumber` | `uv_number` | 未实现 | 6 | |
| `Texture.ShaderDataIndex` | `shader_data_index` | 未实现 | 6 | |
| `Texture.Path` | `path` | 未实现 | 6 | |
| `Texture.UnkFloats` | `unk_floats` | 未实现 | 6 | |
| `BlendMode` | `sf_mtd_blend_mode_t` | 未实现 | 6 | Enum |
| `LightingType` | `sf_mtd_lighting_type_t` | 未实现 | 6 | Enum |

## Game Applicability
- **Sekiro:** Uses MTD with `Extended` texture info.
- **Dark Souls 3 / Bloodborne:** Uses MTD (standard).
- **Elden Ring / AC6:** Replaced by MATBIN.
