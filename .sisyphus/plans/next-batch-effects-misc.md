# Cluster Plan: Effects Misc (T6.8)

## TL;DR

Implement miscellaneous effect and animation formats, including legacy FXR1 (DS1), FFXDLSE (DS2), legacy ANI containers, MQB cutscene/movie metadata, and the extensive Morpheme animation system (NMB, NSA, and Bundle components).

## Upstream formats covered

- `SoulsFormats/Formats/ANI.cs`
- `SoulsFormats/Formats/FFXDLSE/Action.cs`
- `SoulsFormats/Formats/FFXDLSE/Evaluatable.cs`
- `SoulsFormats/Formats/FFXDLSE/FFXDLSE.cs`
- `SoulsFormats/Formats/FFXDLSE/FXEffect.cs`
- `SoulsFormats/Formats/FFXDLSE/Param.cs`
- `SoulsFormats/Formats/FFXDLSE/ParamList.cs`
- `SoulsFormats/Formats/FFXDLSE/Primitive.cs`
- `SoulsFormats/Formats/FFXDLSE/ResourceSet.cs`
- `SoulsFormats/Formats/FFXDLSE/State.cs`
- `SoulsFormats/Formats/FFXDLSE/StateMap.cs`
- `SoulsFormats/Formats/FFXDLSE/Trigger.cs`
- `SoulsFormats/Formats/FXR1/DS1RExtraParams.cs`
- `SoulsFormats/Formats/FXR1/FXAction.cs`
- `SoulsFormats/Formats/FXR1/FXActionData.cs`
- `SoulsFormats/Formats/FXR1/FXContainer.cs`
- `SoulsFormats/Formats/FXR1/FXField.cs`
- `SoulsFormats/Formats/FXR1/FXModifier.cs`
- `SoulsFormats/Formats/FXR1/FXNode.cs`
- `SoulsFormats/Formats/FXR1/FXNodePointer.cs`
- `SoulsFormats/Formats/FXR1/FXR1.cs`
- `SoulsFormats/Formats/FXR1/FXState.cs`
- `SoulsFormats/Formats/FXR1/FXTransition.cs`
- `SoulsFormats/Formats/FXR1/FxrEnvironment.cs`
- `SoulsFormats/Formats/FXR1/Ticks.cs`
- `SoulsFormats/Formats/FXR1/XIDable.cs`
- `SoulsFormats/Formats/MQB/Cut.cs`
- `SoulsFormats/Formats/MQB/Event.cs`
- `SoulsFormats/Formats/MQB/MQB.cs`
- `SoulsFormats/Formats/MQB/Parameter.cs`
- `SoulsFormats/Formats/MQB/Resource.cs`
- `SoulsFormats/Formats/MQB/Timeline.cs`
- `SoulsFormats/Formats/MQB/Transform.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/AnimToRigTableMap.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Event.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/EventTrack.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/FileNameLookupTable.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/LookupTable.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeBundleEnums.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeBundleGUID.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeBundleGeneric.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeBundle_Base.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeFileHeader.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeSizeAlignFormatting.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/MessageDef.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/MorphemeNodeDef.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeAttrib/NodeAttribBase.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeAttrib/NodeAttribBool.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeAttrib/NodeAttribSourceAnim.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeAttrib/NodeAttribSourceEventTrack.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeAttrib/NodeAttribUnknown.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeDataSet.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeDef.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/SmStateList.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/NetworkBundle.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/Rig.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/RigToAnimEntryMap.cs`
- `SoulsFormats/Formats/Morpheme/MorphemeBundle/RigToAnimMap.cs`
- `SoulsFormats/Formats/Morpheme/NMB.cs`
- `SoulsFormats/Formats/Morpheme/NSA/DequantizationFactor.cs`
- `SoulsFormats/Formats/Morpheme/NSA/DequantizationInfo.cs`
- `SoulsFormats/Formats/Morpheme/NSA/DynamicSegment.cs`
- `SoulsFormats/Formats/Morpheme/NSA/NSA.cs`
- `SoulsFormats/Formats/Morpheme/NSA/NSAHeader.cs`
- `SoulsFormats/Formats/Morpheme/NSA/NSAVec3.cs`
- `SoulsFormats/Formats/Morpheme/NSA/RootMotionSegment.cs`
- `SoulsFormats/Formats/Morpheme/NSA/RotationData.cs`
- `SoulsFormats/Formats/Morpheme/NSA/StaticSegment.cs`
- `SoulsFormats/Formats/Morpheme/NSA/TranslationData.cs`

## Must Have

- Full read/write support for FXR1 (DS1/DS1R) and FFXDLSE (DS2).
- Implementation of the Morpheme animation system (NMB/NSA) including bundle parsing.
- Support for MQB cutscene metadata.
- Support for legacy ANI containers.

## Must NOT Have

- FXR3 — already implemented in v1.
- Real-time animation blending or IK solvers.

## Dependencies on prior clusters

- Phase 1 (Core IO): `sf_binary_reader_t`, `sf_binary_writer_t`.
- Phase 7 (Animation): Shared TAE logic if applicable.

## Acceptance criteria

- All formats pass the validator:
```bash
bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-effects-misc.md
```
- Build succeeds with new modules:
```bash
cmake --build build-mingw
```
- New tests pass:
```bash
ctest --test-dir build-mingw -L effects
```

## STRICT UPSTREAM REFERENCE

| Format | Upstream Path |
|--------|---------------|
| ANI | `SoulsFormats/Formats/ANI.cs` |
| FFXDLSE | `SoulsFormats/Formats/FFXDLSE/FFXDLSE.cs` |
| FXR1 | `SoulsFormats/Formats/FXR1/FXR1.cs` |
| MQB | `SoulsFormats/Formats/MQB/MQB.cs` |
| Morpheme NMB | `SoulsFormats/Formats/Morpheme/NMB.cs` |
| Morpheme NSA | `SoulsFormats/Formats/Morpheme/NSA/NSA.cs` |

## Estimated effort

- 5 days (High complexity due to Morpheme and FXR1/FFXDLSE node graphs).

## Risk

- High. Morpheme is a complex third-party system with many nested structures. FXR1/FFXDLSE are also highly complex node-based formats.
