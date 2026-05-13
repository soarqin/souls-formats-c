# format-pmdcl.md — API Mapping

> Upstream: `SoulsFormats/Formats/PMDCL.cs`
> Phase: v0.5.0 (Post-v1 Lighting)
> Applicable games: DS3

## Overview

Defines static decals in DS3 maps.

## Mapping Table

| Upstream Symbol | C Symbol | Type | Status | Phase | Notes |
|---|---|---|---|---|---|
| `PMDCL` | `sf_pmdcl_t` | Opaque | `✓ aligned` | 0.5.0 | Root container for .pmdcl files |
| `PMDCL.Read()` | `sf_pmdcl_read_from_memory` | Function | `✓ aligned` | 0.5.0 | |
| `PMDCL.Write()` | `sf_pmdcl_write_to_buffer` | Function | `✓ aligned` | 0.5.0 | |
| `PMDCL.Decals` | `sf_pmdcl_decal_count` / `sf_pmdcl_get_decal` | Function | `✓ aligned` | 0.5.0 | List accessors |
| `PMDCL.Decal` | `sf_pmdcl_decal_t` | Opaque | `✓ aligned` | 0.5.0 | Individual decal entry |
| `PMDCL.Decal.XAngles` | `sf_pmdcl_decal_x_angles` | Function | `✓ aligned` | 0.5.0 | |
| `PMDCL.Decal.YAngles` | `sf_pmdcl_decal_y_angles` | Function | `✓ aligned` | 0.5.0 | |
| `PMDCL.Decal.ZAngles` | `sf_pmdcl_decal_z_angles` | Function | `✓ aligned` | 0.5.0 | |
| `PMDCL.Decal.Position` | `sf_pmdcl_decal_position` | Function | `✓ aligned` | 0.5.0 | |
| `PMDCL.Decal.Unk3C` | `sf_pmdcl_decal_unk3c` | Function | `✓ aligned` | 0.5.0 | |
| `PMDCL.Decal.DecalParamID` | `sf_pmdcl_decal_param_id` | Function | `✓ aligned` | 0.5.0 | |
| `PMDCL.Decal.Size1` | `sf_pmdcl_decal_size1` | Function | `✓ aligned` | 0.5.0 | |
| `PMDCL.Decal.Size2` | `sf_pmdcl_decal_size2` | Function | `✓ aligned` | 0.5.0 | |
| (Internal) | `sf_pmdcl_destroy` | Function | `✗ deviation` | 0.5.0 | Standard C-style destructor |
