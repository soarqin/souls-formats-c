# format-btl.md — API Mapping

> Upstream: `SoulsFormats/Formats/BTL.cs`
> Phase: v0.5.0 (Post-v1 Lighting)
> Applicable games: BB, DS3, Sekiro

## Overview

Point light sources in a map, used in BB, DS3, and Sekiro.

## Mapping Table

| Upstream Symbol | C Symbol | Type | Status | Phase | Notes |
|---|---|---|---|---|---|
| `BTL` | `sf_btl_t` | Opaque | `✓ aligned` | 0.5.0 | Root container for .btl files |
| `BTL.Read()` | `sf_btl_read_from_memory` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Write()` | `sf_btl_write_to_buffer` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Version` | `sf_btl_version` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Lights` | `sf_btl_light_count` / `sf_btl_get_light` | Function | `✓ aligned` | 0.5.0 | List accessors |
| `BTL.LightType` | `sf_btl_light_type_t` | Enum | `✓ aligned` | 0.5.0 | |
| `BTL.Light` | `sf_btl_light_t` | Opaque | `✓ aligned` | 0.5.0 | Individual light entry |
| `BTL.Light.Unk00` | `sf_btl_light_unk00` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Name` | `sf_btl_light_name` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Type` | `sf_btl_light_type` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk1C` | `sf_btl_light_unk1c` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.DiffuseColor` | `sf_btl_light_diffuse_color` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.DiffusePower` | `sf_btl_light_diffuse_power` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.SpecularColor` | `sf_btl_light_specular_color` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.CastShadows` | `sf_btl_light_cast_shadows` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.SpecularPower` | `sf_btl_light_specular_power` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.ConeAngle` | `sf_btl_light_cone_angle` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk30` | `sf_btl_light_unk30` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk34` | `sf_btl_light_unk34` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Position` | `sf_btl_light_position` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Rotation` | `sf_btl_light_rotation` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk50` | `sf_btl_light_unk50` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk54` | `sf_btl_light_unk54` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Radius` | `sf_btl_light_radius` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk5C` | `sf_btl_light_unk5c` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk64` | `sf_btl_light_unk64` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk68` | `sf_btl_light_unk68` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.ShadowColor` | `sf_btl_light_shadow_color` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk70` | `sf_btl_light_unk70` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.FlickerIntervalMin` | `sf_btl_light_flicker_interval_min` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.FlickerIntervalMax` | `sf_btl_light_flicker_interval_max` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.FlickerBrightnessMult` | `sf_btl_light_flicker_brightness_mult` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk80` | `sf_btl_light_unk80` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk84` | `sf_btl_light_unk84` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk88` | `sf_btl_light_unk88` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk90` | `sf_btl_light_unk90` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Unk98` | `sf_btl_light_unk98` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.NearClip` | `sf_btl_light_near_clip` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkA0` | `sf_btl_light_unk_a0` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Sharpness` | `sf_btl_light_sharpness` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkAC` | `sf_btl_light_unk_ac` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.Width` | `sf_btl_light_width` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkBC` | `sf_btl_light_unk_bc` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkC0` | `sf_btl_light_unk_c0` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkC4` | `sf_btl_light_unk_c4` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkC8` | `sf_btl_light_unk_c8` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkCC` | `sf_btl_light_unk_cc` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkD0` | `sf_btl_light_unk_d0` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkD4` | `sf_btl_light_unk_d4` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkD8` | `sf_btl_light_unk_d8` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkDC` | `sf_btl_light_unk_dc` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkE0` | `sf_btl_light_unk_e0` | Function | `✓ aligned` | 0.5.0 | |
| `BTL.Light.UnkE4` | `sf_btl_light_unk_e4` | Function | `✓ aligned` | 0.5.0 | |
| (Internal) | `sf_btl_destroy` | Function | `✗ deviation` | 0.5.0 | Standard C-style destructor |
| (Internal) | `sf_btl_name_pool` | Extension | `+ extension` | 0.5.0 | Bulk allocation for strings |
