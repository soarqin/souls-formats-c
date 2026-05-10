# EMEVD Mapping

| Upstream File | Upstream Class | C Type | Status |
| :--- | :--- | :--- | :--- |
| `EMEVD.cs` | `SoulsFormats.EMEVD` | `sf_emevd_t` | 未实现 |
| `Event.cs` | `SoulsFormats.EMEVD.Event` | `sf_emevd_event_t` | 未实现 |
| `Instruction.cs` | `SoulsFormats.EMEVD.Instruction` | `sf_emevd_instruction_t` | 未实现 |
| `Parameter.cs` | `SoulsFormats.EMEVD.Parameter` | `sf_emevd_parameter_t` | 未实现 |

## Member Mapping

| Upstream Member | C API Symbol | Status | Phase | Notes |
| :--- | :--- | :--- | :--- | :--- |
| `EMEVD.Format` | `sf_emevd_get_format` / `sf_emevd_set_format` | 未实现 | 5 | Game format enum |
| `EMEVD.Events` | `sf_emevd_get_event_count` / `sf_emevd_get_event` | 未实现 | 5 | List of events |
| `EMEVD.LinkedFileOffsets` | `sf_emevd_get_linked_file_count` / `sf_emevd_get_linked_file_offset` | 未实现 | 5 | BB/DS3 linked files |
| `EMEVD.StringData` | `sf_emevd_get_string_data` | 未实现 | 5 | Raw string block |
| `Event.ID` | `sf_emevd_event_get_id` | 未实现 | 5 | Event ID |
| `Event.Instructions` | `sf_emevd_event_get_instruction_count` / `sf_emevd_event_get_instruction` | 未实现 | 5 | List of instructions |
| `Event.Parameters` | `sf_emevd_event_get_parameter_count` / `sf_emevd_event_get_parameter` | 未实现 | 5 | List of parameters |
| `Event.RestBehavior` | `sf_emevd_event_get_rest_behavior` | 未实现 | 5 | Rest behavior enum |
| `Instruction.Bank` | `sf_emevd_instruction_get_bank` | 未实现 | 5 | Instruction bank |
| `Instruction.ID` | `sf_emevd_instruction_get_id` | 未实现 | 5 | Instruction ID |
| `Instruction.ArgData` | `sf_emevd_instruction_get_arg_data` | 未实现 | 5 | Raw argument bytes |
| `Instruction.Layer` | `sf_emevd_instruction_get_layer` | 未实现 | 5 | Optional layer mask |
| `Parameter.InstructionIndex` | `sf_emevd_parameter_get_instruction_index` | 未实现 | 5 | Target instruction index |
| `Parameter.TargetStartByte` | `sf_emevd_parameter_get_target_start_byte` | 未实现 | 5 | Target arg offset |
| `Parameter.SourceStartByte` | `sf_emevd_parameter_get_source_start_byte` | 未实现 | 5 | Source param offset |
| `Parameter.ByteCount` | `sf_emevd_parameter_get_byte_count` | 未实现 | 5 | Bytes to copy |

## Game-specific Notes

- **DS1/DS2**: 32-bit, little-endian (PC) or big-endian (Console).
- **Bloodborne/SotFS**: 64-bit, little-endian.
- **DS3**: 64-bit, little-endian, version 0xCD, unk06=true.
- **Sekiro**: 64-bit, little-endian, version 0xCD, unk06=true, unk07=true.
- **Elden Ring/AC6**: Likely follows Sekiro or newer (upstream `Game` enum only goes up to `Sekiro`).
