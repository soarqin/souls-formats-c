# TAE (Time Act Editor) Mapping

**v1.1 scope**：仅 SDT 格式（version 0x1000D，covers Sekiro + Elden Ring）；其他 TAEFormat 路径 `_skipped_`。**Template 子系统全部 `_skipped_`**（v1.2 推迟，参见 extensions.md）。

| Upstream Class | Upstream File |
| :--- | :--- |
| `SoulsFormats.TAE` | `Formats/TAE/TAE.cs` |
| `SoulsFormats.TAE.Animation` | `Formats/TAE/Animation.cs` |
| `SoulsFormats.TAE.Event` | `Formats/TAE/Event.cs` |
| `SoulsFormats.TAE.EventGroup` | `Formats/TAE/EventGroup.cs` |
| `SoulsFormats.TAE.Template` | `Formats/TAE/Template.cs` |

## API Mapping

| Upstream Symbol | C Symbol | Type | Status | Phase | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `TAE` | `sf_tae_t` | Opaque | 未实现 | 7 | Root container for .tae files |
| `TAE.TAEFormat` | `sf_tae_format_t` | Enum | 未实现 | 7 | Game-specific format version |
| `TAE.Animation` | `sf_tae_animation_t` | Opaque | 未实现 | 7 | Individual animation entry |
| `TAE.Animation.AnimMiniHeader` | `sf_tae_anim_mini_header_t` | Opaque | 未实现 | 7 | Abstract base for mini-headers |
| `TAE.Animation.AnimMiniHeader.Standard` | `sf_tae_anim_mini_header_standard_t` | Opaque | 未实现 | 7 | Standard mini-header |
| `TAE.Animation.AnimMiniHeader.ImportOtherAnim` | `sf_tae_anim_mini_header_import_t` | Opaque | 未实现 | 7 | Import-based mini-header |
| `TAE.Event` | `sf_tae_event_t` | Opaque | 未实现 | 7 | Timed action or effect |
| `TAE.Event.ParameterContainer` | `sf_tae_event_params_t` | Opaque | 未实现 | 7 | Event parameter values (opaque bytes, no typed decode) |
| `TAE.EventGroup` | `sf_tae_event_group_t` | Opaque | 未实现 | 7 | Grouping of events |
| `TAE.Template` | `sf_tae_template_t` | Opaque | _skipped_ | 7 | v1.2 推迟，参见 extensions.md |
| `TAE.Template.BankTemplate` | `sf_tae_bank_template_t` | Opaque | _skipped_ | 7 | v1.2 推迟 |
| `TAE.Template.EventTemplate` | `sf_tae_event_template_t` | Opaque | _skipped_ | 7 | v1.2 推迟 |
| `TAE.Template.ParameterTemplate` | `sf_tae_param_template_t` | Opaque | _skipped_ | 7 | v1.2 推迟 |
| `TAE.Template.ParamType` | `sf_tae_param_type_t` | Enum | _skipped_ | 7 | v1.2 推迟 |
