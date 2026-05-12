# Cluster Plan: Uncategorized Deferred (T6.10)

## TL;DR

Implement all remaining formats and utilities from the upstream inventory that were not covered by previous clusters. This includes UI layouts (DRB), various deferred formats (ACB, CCM, RMB, GRASS, F2TR, EDD, AIP, SMD4, CLM2), legacy console formats (Other/*), and ancillary C# utilities.

## Upstream formats covered

- `SoulsFormats/Formats/ACB.cs`
- `SoulsFormats/Formats/AIP.cs`
- `SoulsFormats/Formats/CCM.cs`
- `SoulsFormats/Formats/CLM2.cs`
- `SoulsFormats/Formats/DRB/Anik.cs`
- `SoulsFormats/Formats/DRB/Anim.cs`
- `SoulsFormats/Formats/DRB/Anio.cs`
- `SoulsFormats/Formats/DRB/Control.cs`
- `SoulsFormats/Formats/DRB/DRB.cs`
- `SoulsFormats/Formats/DRB/Dlg.cs`
- `SoulsFormats/Formats/DRB/Dlgo.cs`
- `SoulsFormats/Formats/DRB/Scdk.cs`
- `SoulsFormats/Formats/DRB/Scdl.cs`
- `SoulsFormats/Formats/DRB/Scdo.cs`
- `SoulsFormats/Formats/DRB/Shape.cs`
- `SoulsFormats/Formats/DRB/Texture.cs`
- `SoulsFormats/Formats/EDD.cs`
- `SoulsFormats/Formats/F2TR.cs`
- `SoulsFormats/Formats/GRASS.cs`
- `SoulsFormats/Formats/Other/Dreamcast/MGF.cs`
- `SoulsFormats/Formats/Other/KF4/CHR.cs`
- `SoulsFormats/Formats/Other/KF4/DAT.cs`
- `SoulsFormats/Formats/Other/KF4/MAP.cs`
- `SoulsFormats/Formats/Other/KF4/OM2.cs`
- `SoulsFormats/Formats/Other/Kuon/BND.cs`
- `SoulsFormats/Formats/Other/Kuon/DVDBND.cs`
- `SoulsFormats/Formats/Other/LDMU.cs`
- `SoulsFormats/Formats/Other/MWC/DEV.cs`
- `SoulsFormats/Formats/Other/MWC/MDAT.cs`
- `SoulsFormats/Formats/Other/MWC/MMD.cs`
- `SoulsFormats/Formats/Other/MWC/OTR.cs`
- `SoulsFormats/Formats/Other/MWC/SMD.cs`
- `SoulsFormats/Formats/Other/MWC/TDAT.cs`
- `SoulsFormats/Formats/Other/Murakumo/DDL.cs`
- `SoulsFormats/Formats/Other/Otogi2/DAT.cs`
- `SoulsFormats/Formats/Other/SOM/MDO.cs`
- `SoulsFormats/Formats/Other/Zero3.cs`
- `SoulsFormats/Formats/PARAM/Deprecated/Enum.cs`
- `SoulsFormats/Formats/PARAM/Deprecated/Layout.cs`
- `SoulsFormats/Formats/RMB.cs`
- `SoulsFormats/Formats/SMD4/Mesh.cs`
- `SoulsFormats/Formats/SMD4/Node.cs`
- `SoulsFormats/Formats/SMD4/SMD4.cs`
- `SoulsFormats/Formats/SMD4/Unk10.cs`
- `SoulsFormats/Formats/SMD4/Vertex.cs`
- `SoulsFormats/Formats/SMD4/VertexBoneIndices.cs`
- `SoulsFormats/Formats/SMD4/VertexBoneWeights.cs`
- `SoulsFormats/Formats/TPF/SecretHeaderizer.cs`
- `SoulsFormats/Utilities/Attributes/HideProperty.cs`
- `SoulsFormats/Utilities/Attributes/NoRenderGroupInheritance.cs`
- `SoulsFormats/Utilities/Attributes/RotationRadians.cs`
- `SoulsFormats/Utilities/Attributes/RotationXZY.cs`
- `SoulsFormats/Utilities/Attributes/SupportsAlphaAttribute.cs`
- `SoulsFormats/Utilities/BitConverterHelper.cs`
- `SoulsFormats/Utilities/Collections/ListExtensions.cs`
- `SoulsFormats/Utilities/EndianHelper.cs`
- `SoulsFormats/Utilities/Exceptions/NoOodleException.cs`
- `SoulsFormats/Utilities/Formats/ISoulsFile.cs`
- `SoulsFormats/Utilities/Formats/SoulsFile.cs`
- `SoulsFormats/Utilities/Guessing/ExtensionGuesser.cs`
- `SoulsFormats/Utilities/HexHelper.cs`
- `SoulsFormats/Utilities/NativeLibrary.cs`
- `SoulsFormats/Utilities/Xml/XmlNodeExtensions.cs`
- `SoulsFormats/Utilities/Xml/XmlWriterExtensions.cs`

## Must Have

- Full support for DRB (UI layout) and its nested objects.
- Support for all remaining formats listed in the inventory.
- Implementation of necessary utility classes to match upstream functionality.
- Support for legacy console formats (King's Field, Kuon, Metal Wolf Chaos, etc.).

## Must NOT Have

- Formats already covered in clusters T6.1 through T6.9.
- Formats already implemented in v1.

## Dependencies on prior clusters

- All previous clusters (T6.1-T6.9) must be complete to ensure no overlap.

## Acceptance criteria

- All formats pass the validator:
```bash
bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-uncategorized-deferred.md
```
- Build succeeds with new modules:
```bash
cmake --build build-mingw
```
- New tests pass:
```bash
ctest --test-dir build-mingw -L misc
```

## STRICT UPSTREAM REFERENCE

| Format | Upstream Path |
|--------|---------------|
| DRB | `SoulsFormats/Formats/DRB/DRB.cs` |
| ACB | `SoulsFormats/Formats/ACB.cs` |
| AIP | `SoulsFormats/Formats/AIP.cs` |
| CCM | `SoulsFormats/Formats/CCM.cs` |
| CLM2 | `SoulsFormats/Formats/CLM2.cs` |
| EDD | `SoulsFormats/Formats/EDD.cs` |
| F2TR | `SoulsFormats/Formats/F2TR.cs` |
| GRASS | `SoulsFormats/Formats/GRASS.cs` |
| RMB | `SoulsFormats/Formats/RMB.cs` |
| SMD4 | `SoulsFormats/Formats/SMD4/SMD4.cs` |

## Estimated effort

- 5 days (High complexity due to the variety and number of formats).

## Risk

- High. This is a "catch-all" cluster with many obscure and legacy formats that may have undocumented quirks.
