# MATBIN API Mapping

| Upstream Class/Member | C Type/Symbol | Status | Phase | Notes |
|:---|:---|:---|:---|:---|
| `MATBIN` | `sf_matbin_t` | 已实现 | 6 | Opaque pointer |
| `ShaderPath` | `sf_matbin_shader_path` | 已实现 | 6 | UTF-8 string |
| `SourcePath` | `sf_matbin_source_path` | 已实现 | 6 | UTF-8 string |
| `Key` | `sf_matbin_key` | 已实现 | 6 | |
| `Params` | `sf_matbin_param_count` / `sf_matbin_param` | 已实现 | 6 | Array of `sf_matbin_param_t` |
| `Samplers` | `sf_matbin_sampler_count` / `sf_matbin_sampler` | 已实现 | 6 | Array of `sf_matbin_sampler_t` |
| `Param` | `sf_matbin_param_t` | 已实现 | 6 | |
| `Param.Name` | `sf_matbin_param_name` | 已实现 | 6 | |
| `Param.Value` | `sf_matbin_param_value_*` | 已实现 | 6 | Typed accessors |
| `Param.Key` | `sf_matbin_param_key` | 已实现 | 6 | |
| `Param.Type" | "sf_matbin_param_type" | 已实现 | 6 | `sf_matbin_param_type_t` |
| `ParamType` | `sf_matbin_param_type_t` | 已实现 | 6 | Enum |
| `Sampler` | `sf_matbin_sampler_t` | 已实现 | 6 | |
| `Sampler.Type` | `sf_matbin_sampler_type` | 已实现 | 6 | |
| `Sampler.Path` | `sf_matbin_sampler_path` | 已实现 | 6 | |
| `Sampler.Key` | `sf_matbin_sampler_key` | 已实现 | 6 | |
| `Sampler.Unk14` | `sf_matbin_sampler_unk14` | 已实现 | 6 | `sf_vec2_t` |

## Game Applicability
- **Elden Ring:** Primary material format.
- **Armored Core VI:** Primary material format.
- **Nightreign:** Primary material format.
- **Sekiro and earlier:** Uses MTD instead.
