# format-btab.md — API Mapping

> Upstream: `SoulsFormats/Formats/BTAB.cs`
> Phase: v0.5.0 (Post-v1 Lighting)
> Applicable games: DS2+

## Overview

A lightmap atlasing config file introduced in DS2.

## Mapping Table

| Upstream Symbol | C Symbol | Type | Status | Phase | Notes |
|---|---|---|---|---|---|
| `BTAB` | `sf_btab_t` | Opaque | `✓ aligned` | 0.5.0 | Root container for .btab files |
| `BTAB.Read()` | `sf_btab_read_from_memory` | Function | `✓ aligned` | 0.5.0 | |
| `BTAB.Write()` | `sf_btab_write_to_buffer` | Function | `✓ aligned` | 0.5.0 | |
| `BTAB.BigEndian` | `sf_btab_is_big_endian` | Function | `✗ deviation` | 0.5.0 | BigEndian=true refused (SF_ERR_UNSUPPORTED_VERSION) |
| `BTAB.LongFormat` | `sf_btab_is_long_format` | Function | `✓ aligned` | 0.5.0 | |
| `BTAB.Entries` | `sf_btab_entry_count` / `sf_btab_get_entry` | Function | `✓ aligned` | 0.5.0 | List accessors |
| `BTAB.Entry` | `sf_btab_entry_t` | Opaque | `✓ aligned` | 0.5.0 | Individual entry |
| `BTAB.Entry.PartName` | `sf_btab_entry_part_name` | Function | `✓ aligned` | 0.5.0 | |
| `BTAB.Entry.MaterialName` | `sf_btab_entry_material_name` | Function | `✓ aligned` | 0.5.0 | |
| `BTAB.Entry.AtlasID` | `sf_btab_entry_atlas_id` | Function | `✓ aligned` | 0.5.0 | |
| `BTAB.Entry.UVOffset` | `sf_btab_entry_uv_offset` | Function | `✓ aligned` | 0.5.0 | |
| `BTAB.Entry.UVScale` | `sf_btab_entry_uv_scale` | Function | `✓ aligned` | 0.5.0 | |
| (Internal) | `sf_btab_destroy` | Function | `✗ deviation` | 0.5.0 | Standard C-style destructor |
| (Internal) | `sf_btab_name_pool` | Extension | `+ extension` | 0.5.0 | Bulk allocation for strings |
