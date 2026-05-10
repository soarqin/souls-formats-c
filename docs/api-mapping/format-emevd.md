# EMEVD Mapping

| Upstream File | Upstream Class | C Type | Status |
| :--- | :--- | :--- | :--- |
| `EMEVD.cs` | `SoulsFormats.EMEVD` | `sf_emevd_t` | ✓ aligned |
| `Event.cs` | `SoulsFormats.EMEVD.Event` | `sf_emevd_event_t` | ✓ aligned |
| `Instruction.cs` | `SoulsFormats.EMEVD.Instruction` | `sf_emevd_instruction_t` | ✓ aligned |
| `Parameter.cs` | `SoulsFormats.EMEVD.Parameter` | `sf_emevd_parameter_t` | ✓ aligned |

## Member Mapping

| Upstream Member | C API Symbol | Status | Phase | Notes |
| :--- | :--- | :--- | :--- | :--- |
| `EMEVD.Format` | `sf_emevd_get_format` / `sf_emevd_set_format` | ✓ aligned | 4 | Game format enum |
| `EMEVD.Events` | `sf_emevd_get_event_count` / `sf_emevd_get_event` | ✓ aligned | 4 | List of events |
| `EMEVD.LinkedFileOffsets` | `sf_emevd_get_linked_file_count` / `sf_emevd_get_linked_file_offset` | ✓ aligned | 4 | BB/DS3 linked files |
| `EMEVD.StringData` | `sf_emevd_get_string_data` | ✓ aligned | 4 | Raw string block |
| `Event.ID` | `sf_emevd_event_get_id` | ✓ aligned | 4 | Event ID |
| `Event.Instructions` | `sf_emevd_event_get_instruction_count` / `sf_emevd_event_get_instruction` | ✓ aligned | 4 | List of instructions |
| `Event.Parameters` | `sf_emevd_event_get_parameter_count` / `sf_emevd_event_get_parameter` | ✓ aligned | 4 | List of parameters |
| `Event.RestBehavior` | `sf_emevd_event_get_rest_behavior` | ✓ aligned | 4 | Rest behavior enum |
| `Instruction.Bank` | `sf_emevd_instruction_get_bank` | ✓ aligned | 4 | Instruction bank |
| `Instruction.ID` | `sf_emevd_instruction_get_id` | ✓ aligned | 4 | Instruction ID |
| `Instruction.ArgData` | `sf_emevd_instruction_get_arg_data` | ✓ aligned | 4 | Raw argument bytes |
| `Instruction.Layer` | `sf_emevd_instruction_get_layer` | ✓ aligned | 4 | Optional layer mask |
| `Parameter.InstructionIndex` | `sf_emevd_parameter_get_instruction_index` | ✓ aligned | 4 | Target instruction index |
| `Parameter.TargetStartByte` | `sf_emevd_parameter_get_target_start_byte` | ✓ aligned | 4 | Target arg offset |
| `Parameter.SourceStartByte` | `sf_emevd_parameter_get_source_start_byte` | ✓ aligned | 4 | Source param offset |
| `Parameter.ByteCount` | `sf_emevd_parameter_get_byte_count` | ✓ aligned | 4 | Bytes to copy |
| `Instruction.PackArgs` | `sf_emevd_instruction_pack_args` | _skipped_ | 4 | C# specific reflection |
| `Instruction.UnpackArgs` | `sf_emevd_instruction_unpack_args` | _skipped_ | 4 | C# specific reflection |
| `EMEDF.JsonLoad` | `sf_emedf_json_load` | _skipped_ | 4 | Out of scope |

## Game-specific Notes

- **DS1/DS2**: 32-bit, little-endian (PC) or big-endian (Console).
- **Bloodborne/SotFS**: 64-bit, little-endian.
- **DS3**: 64-bit, little-endian, version 0xCD, unk06=true.
- **Sekiro**: 64-bit, little-endian, version 0xCD, unk06=true, unk07=true.
- **Elden Ring/AC6/Nightreign**: ER/AC6/Nightreign probe: Wave 0 evidence unavailable (BHD5 parse issue); defaulting to Sekiro alias.
- **Format Aliases**: `SF_EMEVD_FORMAT_ELDEN_RING/ARMORED_CORE_VI/NIGHTREIGN` are aliases for `SF_EMEVD_FORMAT_SEKIRO`.
