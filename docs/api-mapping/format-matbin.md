# MATBIN API Mapping

| Upstream Class/Member | C Type/Symbol | Status | Phase | Notes |
|:---|:---|:---|:---|:---|
| `MATBIN` | `sf_matbin_t` | 未实现 | 6 | Opaque pointer |
| `ShaderPath` | `shader_path` | 未实现 | 6 | UTF-8 string |
| `SourcePath` | `source_path` | 未实现 | 6 | UTF-8 string |
| `Key` | `key` | 未实现 | 6 | |
| `Params` | `params` | 未实现 | 6 | Array of `sf_matbin_param_t` |
| `Samplers` | `samplers` | 未实现 | 6 | Array of `sf_matbin_sampler_t` |
| `Param` | `sf_matbin_param_t` | 未实现 | 6 | |
| `Param.Name` | `name` | 未实现 | 6 | |
| `Param.Value` | `value` | 未实现 | 6 | Union of supported types |
| `Param.Key` | `key` | 未实现 | 6 | |
| `Param.Type` | `type` | 未实现 | 6 | `sf_matbin_param_type_t` |
| `ParamType` | `sf_matbin_param_type_t` | 未实现 | 6 | Enum |
| `Sampler` | `sf_matbin_sampler_t` | 未实现 | 6 | |
| `Sampler.Type` | `type` | 未实现 | 6 | |
| `Sampler.Path` | `path` | 未实现 | 6 | |
| `Sampler.Key` | `key` | 未实现 | 6 | |
| `Sampler.Unk14` | `unk14` | 未实现 | 6 | `sf_vec2_t` |

## Game Applicability
- **Elden Ring:** Primary material format.
- **Armored Core VI:** Primary material format.
- **Nightreign:** Primary material format.
- **Sekiro and earlier:** Uses MTD instead.
