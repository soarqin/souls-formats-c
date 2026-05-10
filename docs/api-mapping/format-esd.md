# ESD Mapping

| Upstream File | Upstream Class | C Type | Status |
| :--- | :--- | :--- | :--- |
| `ESD.cs` | `SoulsFormats.ESD` | `sf_esd_t` | 未实现 |
| `ESD.cs` | `SoulsFormats.ESD.State` | `sf_esd_state_t` | 未实现 |
| `ESD.cs` | `SoulsFormats.ESD.Condition` | `sf_esd_condition_t` | 未实现 |
| `ESD.cs` | `SoulsFormats.ESD.CommandCall` | `sf_esd_command_call_t` | 未实现 |

## Member Mapping

| Upstream Member | C API Symbol | Status | Phase | Notes |
| :--- | :--- | :--- | :--- | :--- |
| `ESD.LongFormat` | `sf_esd_is_long_format` | 未实现 | 5 | 64-bit vs 32-bit |
| `ESD.DarkSoulsCount` | `sf_esd_get_dark_souls_count` | 未实现 | 5 | Format version (1, 2, or 3) |
| `ESD.Name` | `sf_esd_get_name` | 未实现 | 5 | Optional file name/desc |
| `ESD.StateGroups` | `sf_esd_get_state_group_count` / `sf_esd_get_state_group` | 未实现 | 5 | Dictionary of state groups |
| `State.Conditions` | `sf_esd_state_get_condition_count` / `sf_esd_state_get_condition` | 未实现 | 5 | Transitions |
| `State.EntryCommands` | `sf_esd_state_get_entry_command_count` / `sf_esd_state_get_entry_command` | 未实现 | 5 | On enter |
| `State.ExitCommands` | `sf_esd_state_get_exit_command_count` / `sf_esd_state_get_exit_command` | 未实现 | 5 | On exit |
| `State.WhileCommands` | `sf_esd_state_get_while_command_count` / `sf_esd_state_get_while_command` | 未实现 | 5 | While in state |
| `Condition.TargetState` | `sf_esd_condition_get_target_state` | 未实现 | 5 | Next state ID |
| `Condition.PassCommands` | `sf_esd_condition_get_pass_command_count` / `sf_esd_condition_get_pass_command` | 未实现 | 5 | On transition |
| `Condition.Subconditions` | `sf_esd_condition_get_subcondition_count` / `sf_esd_condition_get_subcondition` | 未实现 | 5 | Nested conditions |
| `Condition.Evaluator` | `sf_esd_condition_get_evaluator` | 未实现 | 5 | Bytecode for condition |
| `CommandCall.CommandBank` | `sf_esd_command_call_get_bank` | 未实现 | 5 | Command bank (1, 5, 6, 7) |
| `CommandCall.CommandID` | `sf_esd_command_call_get_id` | 未实现 | 5 | Command ID |
| `CommandCall.Arguments` | `sf_esd_command_call_get_argument_count` / `sf_esd_command_call_get_argument` | 未实现 | 5 | Bytecode arguments |
