# FXR3 (Rainbow Stone FXR) Mapping

v1.1 implements FXR3 binary + XML round-trip for DS3 (version 4) and Sekiro (version 5). ER/AC6/Nightreign use Sekiro version. UnkAc6 InterpolationType (7) accepted.

| Upstream Class | Upstream File |
| :--- | :--- |
| `SoulsFormats.FXR3` | `Formats/FXR3.cs` |

## API Mapping

| Upstream Symbol | C Symbol | Type | Status | Phase | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `FXR3` | `sf_fxr3_t` | Opaque | ✓ aligned | 7 | Root container for .fxr files |
| `FXR3.FXRVersion` | `sf_fxr3_version_t` | Enum | ✓ aligned | 7 | DS3 or Sekiro version |
| `FXR3.StateMap` | `sf_fxr3_state_map_t` | Opaque | ✓ aligned | 7 | Map of states |
| `FXR3.State` | `sf_fxr3_state_t` | Opaque | ✓ aligned | 7 | Individual state |
| `FXR3.StateCondition` | `sf_fxr3_state_condition_t` | Opaque | ✓ aligned | 7 | Transition condition |
| `FXR3.StateCondition.ConditionOperand` | `sf_fxr3_operand_t` | Opaque | ✓ aligned | 7 | Abstract operand |
| `FXR3.StateCondition.ConditionOperator` | `sf_fxr3_operator_t` | Opaque | ✓ aligned | 7 | Comparison operator |
| `FXR3.Container` | `sf_fxr3_container_t` | Opaque | ✓ aligned | 7 | Effect container |
| `FXR3.Effect` | `sf_fxr3_effect_t` | Opaque | ✓ aligned | 7 | Individual effect |
| `FXR3.Action` | `sf_fxr3_action_t` | Opaque | ✓ aligned | 7 | Action within effect |
| `FXR3.Field` | `sf_fxr3_field_t` | Opaque | ✓ aligned | 7 | Abstract data field |
| `FXR3.Property` | `sf_fxr3_property_t` | Opaque | ✓ aligned | 7 | Effect property |
| `FXR3.PropertyModifier` | `sf_fxr3_property_modifier_t` | Opaque | ✓ aligned | 7 | Property modifier |
| `FXR3.UnkFieldList` | `sf_fxr3_unk_field_list_t` | Opaque | ~ partial | 7 | Unknown field collection (opaque, not fully decoded) |
| `FXR3EnhancedSerialization` | `sf_fxr3_xml_serialize` | Function | + extension | 7 | XML serialization surface |
| `FXR3EnhancedSerialization.XMLToFXR3` | `sf_fxr3_from_xml` | Function | ✓ aligned | 7 | Deserialize from XML |
| `FXR3EnhancedSerialization.FXR3ToXML` | `sf_fxr3_to_xml` | Function | ✓ aligned | 7 | Serialize to XML |
