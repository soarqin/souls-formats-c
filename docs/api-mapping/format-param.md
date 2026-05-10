# PARAM API Mapping

Upstream references:
- `SoulsFormats/Formats/PARAM/PARAM/PARAM.cs`
- `SoulsFormats/Formats/PARAM/PARAM/Row.cs`
- `SoulsFormats/Formats/PARAM/PARAM/Cell.cs`
- `SoulsFormats/Formats/PARAM/ParamUtil.cs`

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public partial class PARAM` | `PARAM.cs:10` | class | `sf_param_t` | ✓ aligned | Phase 4 |
| `public bool BigEndian { get; set; }` | `PARAM.cs:15` | property | `sf_param_get_big_endian` / `sf_param_set_big_endian` | ✓ aligned | Phase 4 |
| `public FormatFlags1 Format2D { get; set; }` | `PARAM.cs:20` | property | `sf_param_get_format2d` / `sf_param_set_format2d` | ✓ aligned | Phase 4 |
| `public FormatFlags2 Format2E { get; set; }` | `PARAM.cs:25` | property | `sf_param_get_format2e` / `sf_param_set_format2e` | ✓ aligned | Phase 4 |
| `public byte ParamdefFormatVersion { get; set; }` | `PARAM.cs:30` | property | `sf_param_get_paramdef_format_version` | ✓ aligned | Phase 4 |
| `public short ParamdefDataVersion { get; set; }` | `PARAM.cs:40` | property | `sf_param_get_paramdef_data_version` | ✓ aligned | Phase 4 |
| `public string ParamType { get; set; }` | `PARAM.cs:45` | property | `sf_param_get_param_type` | ✓ aligned | Phase 4 |
| `public List<Row> Rows { get; set; }` | `PARAM.cs:55` | property | `sf_param_get_rows` | ✓ aligned | Phase 4 |
| `public void ApplyParamdef(PARAMDEF paramdef)` | `PARAM.cs:309` | method | `sf_param_apply_paramdef` | ✓ aligned | Phase 4 |
| `public bool ApplyParamdefCarefully(PARAMDEF paramdef)` | `PARAM.cs:320` | method | `sf_param_apply_paramdef` | + extension | Folds into `sf_param_apply_mode_t` |
| `public bool ApplyParamdefCarefully(IEnumerable<PARAMDEF> paramdefs)` | `PARAM.cs:334` | method | `sf_param_apply_paramdef` | + extension | Folds into `sf_param_apply_mode_t` |
| `public bool ApplyParamdefSomewhatCarefully(PARAMDEF paramdef)` | `PARAM.cs:347` | method | `sf_param_apply_paramdef` | + extension | Folds into `sf_param_apply_mode_t` |
| `public bool ApplyParamdefSomewhatCarefully(IEnumerable<PARAMDEF> paramdefs)` | `PARAM.cs:362` | method | `sf_param_apply_paramdef` | + extension | Folds into `sf_param_apply_mode_t` |
| `public void ApplyRegulationVersionedParamdef(PARAMDEF paramdef, ulong version)` | `PARAM.cs:378` | method | `sf_param_apply_regulation_versioned_paramdef` | _skipped_ | Deferred to v1.1 |
| `public bool ApplyRegulationVersionedParamdefSomewhatCarefully(PARAMDEF paramdef, ulong version)` | `PARAM.cs:390` | method | `sf_param_apply_regulation_versioned_paramdef_somewhat_carefully` | _skipped_ | Deferred to v1.1 |
| `public bool ApplyRegulationVersionedParamdefSomewhatCarefully(IEnumerable<PARAMDEF> paramdefs, ulong version)` | `PARAM.cs:409` | method | `sf_param_apply_regulation_versioned_paramdef_somewhat_carefully_multi` | _skipped_ | Deferred to v1.1 |
| `public bool ApplyRegulationVersionedParamdefCarefully(PARAMDEF paramdef, ulong version)` | `PARAM.cs:426` | method | `sf_param_apply_regulation_versioned_paramdef_carefully` | _skipped_ | Deferred to v1.1 |
| `public bool ApplyRegulationVersionedParamdefCarefully(IEnumerable<PARAMDEF> paramdefs, ulong version)` | `PARAM.cs:440` | method | `sf_param_apply_regulation_versioned_paramdef_carefully_multi` | _skipped_ | Deferred to v1.1 |
| `public class Row` | `Row.cs:13` | class | `sf_param_row_t` | ✓ aligned | Phase 4 |
| `public int ID { get; set; }` | `Row.cs:23` | property | `sf_param_row_get_id` | ✓ aligned | Phase 4 |
| `public string Name { get; set; }` | `Row.cs:28` | property | `sf_param_row_get_name` | ✓ aligned | Phase 4 |
| `public IReadOnlyList<Cell> Cells { get; private set; }` | `Row.cs:33` | property | `sf_param_row_get_cells` | ✓ aligned | Phase 4 |
| `public class Cell` | `Cell.cs:10` | class | `sf_param_cell_t` | ✓ aligned | Phase 4 |
| `public object Value { get; set; }` | `Cell.cs:20` | property | `sf_param_cell_get_value` / `sf_param_cell_set_value` | ✓ aligned | Phase 4 |
| `public enum FormatFlags1` | `PARAM.cs:467` | enum | `sf_param_format_flags1_t` | ✓ aligned | Phase 4 |
| `public enum FormatFlags2` | `PARAM.cs:519` | enum | `sf_param_format_flags2_t` | ✓ aligned | Phase 4 |
| (13 typed getters) | Row.cs:40-150 | methods | `sf_param_row_get_u8/i8/u16/i16/u32/i32/f32/str/etc` | ✓ aligned | Phase 4 |
