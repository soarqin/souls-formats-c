# Cluster Plan: AC Specific (T6.9)

## TL;DR

Implement formats specific to the Armored Core series (excluding AC6 which is in v1). This includes the extensive AcParts system for AC4/ACFA, MLB resource containers, FSDATA manifests, and FSLIBLZS archives.

## Upstream formats covered

- `SoulsFormats/Formats/AcParts/AC4/AcParts4.cs`
- `SoulsFormats/Formats/AcParts/AC4/Component/BoosterComponent.cs`
- `SoulsFormats/Formats/AcParts/AC4/Component/DefenseComponent.cs`
- `SoulsFormats/Formats/AcParts/AC4/Component/FrameComponent.cs`
- `SoulsFormats/Formats/AcParts/AC4/Component/PAComponent.cs`
- `SoulsFormats/Formats/AcParts/AC4/Component/PartComponent.cs`
- `SoulsFormats/Formats/AcParts/AC4/Component/RadarComponent.cs`
- `SoulsFormats/Formats/AcParts/AC4/Component/StabilizerComponent.cs`
- `SoulsFormats/Formats/AcParts/AC4/Component/WeaponBoosterComponent.cs`
- `SoulsFormats/Formats/AcParts/AC4/Component/WeaponComponent.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/Arm.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/ArmStabilizer.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/ArmUnit.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/BackBooster.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/BackUnit.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/Core.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/CoreLowerSideStabilizer.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/CoreUpperSideStabilizer.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/FCS.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/Generator.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/Head.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/HeadSideStabilizer.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/HeadTopStabilizer.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/IBooster.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/IFrame.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/IPart.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/IStabilizer.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/IWeapon.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/Leg.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/LegBackStabilizer.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/LegLowerStabilizer.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/LegMiddleStabilizer.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/LegUpperStabilizer.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/MainBooster.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/OveredBooster.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/ShoulderUnit.cs`
- `SoulsFormats/Formats/AcParts/AC4/Part/SideBooster.cs`
- `SoulsFormats/Formats/AcParts/AC4/Types/DispType.cs`
- `SoulsFormats/Formats/FSDATA.cs`
- `SoulsFormats/Formats/FSLIBLZS.cs`
- `SoulsFormats/Formats/MLB/IMLB.cs`
- `SoulsFormats/Formats/MLB/IMlbResource.cs`
- `SoulsFormats/Formats/MLB/MLB_AC4.cs`
- `SoulsFormats/Formats/MLB/MLB_AC5.cs`
- `SoulsFormats/Formats/Other/AC3SL/BND.cs`
- `SoulsFormats/Formats/Other/AC4/ANC.cs`
- `SoulsFormats/Formats/Other/AC4/AcAttachInfo.cs`
- `SoulsFormats/Formats/Other/AC4/AcColorSet4.cs`
- `SoulsFormats/Formats/Other/AC4/AcConflictInfo.cs`
- `SoulsFormats/Formats/Other/AC4/AcPartCategory.cs`
- `SoulsFormats/Formats/Other/AC4/DBSUB.cs`
- `SoulsFormats/Formats/Other/ACE3/BND.cs`
- `SoulsFormats/Formats/PARAM/PARAMDBP/DBPPARAM.cs`
- `SoulsFormats/Formats/PARAM/PARAMDBP/PARAMDBP.cs`
- `SoulsFormats/Formats/PARAM/PARAMDBP/TxtSerializer.cs`
- `SoulsFormats/Formats/PARAM/PARAMDBP/XmlSerializer.cs`
- `SoulsFormats/Formats/PARAM/ParamDbpUtil.cs`

## Must Have

- Full support for AcParts4 and its various components/parts.
- Read/write support for MLB (AC4/AC5 variants).
- Support for FSDATA and FSLIBLZS (legacy AC filesystem formats).
- Support for PARAMDBP (AC-specific parameter database).

## Must NOT Have

- AC6-specific files — these are already in v1.
- Game-specific logic for part stats or assembly validation.

## Dependencies on prior clusters

- Phase 1 (Core IO): `sf_binary_reader_t`, `sf_binary_writer_t`.
- Phase 4 (Param): Shared PARAM logic if applicable.

## Acceptance criteria

- All formats pass the validator:
```bash
bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-ac-specific.md
```
- Build succeeds with new modules:
```bash
cmake --build build-mingw
```
- New tests pass:
```bash
ctest --test-dir build-mingw -L ac
```

## STRICT UPSTREAM REFERENCE

| Format | Upstream Path |
|--------|---------------|
| AcParts4 | `SoulsFormats/Formats/AcParts/AC4/AcParts4.cs` |
| FSDATA | `SoulsFormats/Formats/FSDATA.cs` |
| FSLIBLZS | `SoulsFormats/Formats/FSLIBLZS.cs` |
| MLB | `SoulsFormats/Formats/MLB/MLB_AC4.cs` |
| PARAMDBP | `SoulsFormats/Formats/PARAM/PARAMDBP/PARAMDBP.cs` |

## Estimated effort

- 4 days (Medium complexity due to the sheer number of AcParts components).

## Risk

- Medium. The AcParts system is very broad but mostly consists of simple POD structures.
