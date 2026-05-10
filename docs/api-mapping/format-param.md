# PARAM API Mapping

Upstream references:
- `SoulsFormats/Formats/PARAM/PARAM/PARAM.cs`
- `SoulsFormats/Formats/PARAM/PARAM/Row.cs`
- `SoulsFormats/Formats/PARAM/PARAM/Cell.cs`
- `SoulsFormats/Formats/PARAM/ParamUtil.cs`

| Upstream signature | Upstream loc | Kind | Our API (or 未实现) | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public partial class PARAM` | `PARAM.cs:10` | class | `sf_param_t` | 未实现 | Phase 4 |
| `public bool BigEndian { get; set; }` | `PARAM.cs:15` | property | `sf_param_get_big_endian` / `sf_param_set_big_endian` | 未实现 | Phase 4 |
| `public FormatFlags1 Format2D { get; set; }` | `PARAM.cs:20` | property | `sf_param_get_format2d` / `sf_param_set_format2d` | 未实现 | Phase 4 |
| `public FormatFlags2 Format2E { get; set; }` | `PARAM.cs:25` | property | `sf_param_get_format2e` / `sf_param_set_format2e` | 未实现 | Phase 4 |
| `public byte ParamdefFormatVersion { get; set; }` | `PARAM.cs:30` | property | `sf_param_get_paramdef_format_version` | 未实现 | Phase 4 |
| `public short ParamdefDataVersion { get; set; }` | `PARAM.cs:40` | property | `sf_param_get_paramdef_data_version` | 未实现 | Phase 4 |
| `public string ParamType { get; set; }` | `PARAM.cs:45` | property | `sf_param_get_param_type` | 未实现 | Phase 4 |
| `public List<Row> Rows { get; set; }` | `PARAM.cs:55` | property | `sf_param_get_rows` | 未实现 | Phase 4 |
| `public void ApplyParamdef(PARAMDEF paramdef)` | `PARAM.cs:309` | method | `sf_param_apply_paramdef` | 未实现 | Phase 4 |
| `public bool ApplyParamdefCarefully(PARAMDEF paramdef)` | `PARAM.cs:320` | method | `sf_param_apply_paramdef_carefully` | 未实现 | Phase 4 |
| `public bool ApplyParamdefCarefully(IEnumerable<PARAMDEF> paramdefs)` | `PARAM.cs:334` | method | `sf_param_apply_paramdef_carefully_multi` | 未实现 | Phase 4 |
| `public bool ApplyParamdefSomewhatCarefully(PARAMDEF paramdef)` | `PARAM.cs:347` | method | `sf_param_apply_paramdef_somewhat_carefully` | 未实现 | Phase 4 |
| `public bool ApplyParamdefSomewhatCarefully(IEnumerable<PARAMDEF> paramdefs)` | `PARAM.cs:362` | method | `sf_param_apply_paramdef_somewhat_carefully_multi` | 未实现 | Phase 4 |
| `public void ApplyRegulationVersionedParamdef(PARAMDEF paramdef, ulong version)` | `PARAM.cs:378` | method | `sf_param_apply_regulation_versioned_paramdef` | 未实现 | Phase 4 |
| `public bool ApplyRegulationVersionedParamdefSomewhatCarefully(PARAMDEF paramdef, ulong version)` | `PARAM.cs:390` | method | `sf_param_apply_regulation_versioned_paramdef_somewhat_carefully` | 未实现 | Phase 4 |
| `public bool ApplyRegulationVersionedParamdefSomewhatCarefully(IEnumerable<PARAMDEF> paramdefs, ulong version)` | `PARAM.cs:409` | method | `sf_param_apply_regulation_versioned_paramdef_somewhat_carefully_multi` | 未实现 | Phase 4 |
| `public bool ApplyRegulationVersionedParamdefCarefully(PARAMDEF paramdef, ulong version)` | `PARAM.cs:426` | method | `sf_param_apply_regulation_versioned_paramdef_carefully` | 未实现 | Phase 4 |
| `public bool ApplyRegulationVersionedParamdefCarefully(IEnumerable<PARAMDEF> paramdefs, ulong version)` | `PARAM.cs:440` | method | `sf_param_apply_regulation_versioned_paramdef_carefully_multi` | 未实现 | Phase 4 |
| `public class Row` | `Row.cs:13` | class | `sf_param_row_t` | 未实现 | Phase 4 |
| `public int ID { get; set; }` | `Row.cs:23` | property | `sf_param_row_get_id` | 未实现 | Phase 4 |
| `public string Name { get; set; }` | `Row.cs:28` | property | `sf_param_row_get_name` | 未实现 | Phase 4 |
| `public IReadOnlyList<Cell> Cells { get; private set; }` | `Row.cs:33` | property | `sf_param_row_get_cells` | 未实现 | Phase 4 |
| `public class Cell` | `Cell.cs:10` | class | `sf_param_cell_t` | 未实现 | Phase 4 |
| `public object Value { get; set; }` | `Cell.cs:20` | property | `sf_param_cell_get_value` / `sf_param_cell_set_value` | 未实现 | Phase 4 |
| `public enum FormatFlags1` | `PARAM.cs:467` | enum | `sf_param_format_flags1_t` | 未实现 | Phase 4 |
| `public enum FormatFlags2` | `PARAM.cs:519` | enum | `sf_param_format_flags2_t` | 未实现 | Phase 4 |
