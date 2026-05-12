# Upstream Inventory (SoulsFormatsNEXT → souls-formats-c)

> **Source of truth** for T6.0 (Wave 6 of refactor-and-gap-analysis plan).
> Enumerates every `.cs` file under upstream `SoulsFormats/` and assigns
> each NOT-IMPLEMENTED file to one of 10 clusters consumed by T6.1–T6.10.

## Methodology

1. `find /home/soar/src/SoulsFormatsNEXT/SoulsFormats -name '*.cs'` → **413 files**
   (`Formats/` = 380, `Utilities/` = 33).
2. Cross-referenced against our `docs/api-mapping/format-*.md` and `util-*.md` rows
   plus actual `src/` source tree to determine state.
3. Upstream-root listing (`ls /home/soar/src/SoulsFormatsNEXT/SoulsFormats/`)
   confirms there is **no top-level `Other/` directory** — only
   `Formats/`, `Utilities/`, and `SoulsFormats.csproj`. A nested
   `Formats/Other/` subdirectory holds 38 console/legacy files.
4. v1 in-scope (per `AGENTS.md`): Sekiro + Elden Ring + Nightreign + AC6.
   Older titles deferred to v2+.

## States

| State | Meaning |
|-------|---------|
| `IMPLEMENTED` | Format module exists in `src/` and matches upstream API surface for v1-target games. |
| `PARTIAL` | Format module exists but a non-trivial subset of upstream surface is missing. Cluster field points to where the remaining work belongs. |
| `NOT-IMPLEMENTED` | No corresponding module in `src/`. Mandatory `cluster:` field. |

## Cluster legend

Ten clusters, fixed during T6.0 interview (see refactor-and-gap-analysis plan §Wave 6):

| Cluster | Covers |
|---------|--------|
| `legacy-binder` | BND, BND2 |
| `legacy-msb` | MSB1/2/3/AC4/B/D/DR/FA/N/V/VD (10 legacy MSB variants) |
| `legacy-flver` | FLVER0, FLVER2 PS3 Edge buffers, MDL/MDL0/MDL4 + EdgeGeom helper |
| `tae-templates` | TAE template subsystem + non-SDT TAE variants |
| `lighting` | BTAB, BTL, BTPB, GPARAM, PMDCL |
| `navmesh` | NVA, NVM, NGP, MCG, MCP, EDGE |
| `text-script-misc` | LUAGNL, LUAINFO, EMELD, FMB |
| `effects-misc` | FXR1, FFXDLSE, ANI, MQB, Morpheme (NMB + NSA + Bundle) |
| `ac-specific` | AcParts, MLB, FSDATA, FSLIBLZS, PARAMDBP, Other/AC* |
| `uncategorized-deferred` | DRB, ACB, CCM, RMB, GRASS, F2TR, EDD, AIP, SMD4, CLM2, Deprecated PARAM, all `Other/` console formats, and ancillary C#-only utilities |

## Summary counts

- Total upstream `.cs` files: **413** (target: 413 — exact match, 0 % deviation)
- `IMPLEMENTED`: **103**
- `PARTIAL`: **1**
- `NOT-IMPLEMENTED`: **309**

Cluster breakdown:

| Cluster | Count |
|---------|-------|
| `legacy-binder` | 5 |
| `legacy-msb` | 75 |
| `legacy-flver` | 23 |
| `tae-templates` | 2 |
| `lighting` | 5 |
| `navmesh` | 6 |
| `text-script-misc` | 4 |
| `effects-misc` | 69 |
| `ac-specific` | 57 |
| `uncategorized-deferred` | 64 |
| **Total clusters** | **310** |

> Note: `tae-templates` count includes the single `PARTIAL` row (TAE.cs).
> Adding `IMPLEMENTED` (103) + `tae-templates partial` already-counted-once + remaining `NOT-IMPLEMENTED` = 413.

## Cross-cluster validator integration

This file is read by `tests/cluster-plan-validator.sh` (created by T6.0).
Every T6.1-T6.10 cluster plan must cite ≥ 80 % of the `.cs` files mapped
here under its cluster name. The validator awk script greps for `^path:` /
`^cluster: <name>` lines, so the entry format below must remain stable.

Each inventory entry is a five-line block with **column-0** prefixes (the
example here is indented with two spaces so it does not contaminate the
validator's path/cluster scan):

  ↪ `path: SoulsFormats/Formats/SomeFormat.cs`
  ↪ `state: NOT-IMPLEMENTED`
  ↪ `cluster: legacy-binder`
  ↪ `description: ...`
  ↪ `---`

`IMPLEMENTED` entries deliberately omit the `cluster:` line so they are not
counted as required citations for any cluster plan.

---

## Inventory

Entries ordered by upstream path. Total = 413.

path: SoulsFormats/Formats/ACB.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: ACB asset container (deferred)
---
path: SoulsFormats/Formats/AIP.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: AI parameter data (deferred)
---
path: SoulsFormats/Formats/ANI.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Legacy animation container
---
path: SoulsFormats/Formats/AcParts/AC4/AcParts4.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 AcParts4 root
---
path: SoulsFormats/Formats/AcParts/AC4/Component/BoosterComponent.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 booster component
---
path: SoulsFormats/Formats/AcParts/AC4/Component/DefenseComponent.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 defense component
---
path: SoulsFormats/Formats/AcParts/AC4/Component/FrameComponent.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 frame component
---
path: SoulsFormats/Formats/AcParts/AC4/Component/PAComponent.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 PA component
---
path: SoulsFormats/Formats/AcParts/AC4/Component/PartComponent.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part component
---
path: SoulsFormats/Formats/AcParts/AC4/Component/RadarComponent.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 radar component
---
path: SoulsFormats/Formats/AcParts/AC4/Component/StabilizerComponent.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 stabilizer component
---
path: SoulsFormats/Formats/AcParts/AC4/Component/WeaponBoosterComponent.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 weapon booster component
---
path: SoulsFormats/Formats/AcParts/AC4/Component/WeaponComponent.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 weapon component
---
path: SoulsFormats/Formats/AcParts/AC4/Part/Arm.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: Arm
---
path: SoulsFormats/Formats/AcParts/AC4/Part/ArmStabilizer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: ArmStabilizer
---
path: SoulsFormats/Formats/AcParts/AC4/Part/ArmUnit.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: ArmUnit
---
path: SoulsFormats/Formats/AcParts/AC4/Part/BackBooster.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: BackBooster
---
path: SoulsFormats/Formats/AcParts/AC4/Part/BackUnit.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: BackUnit
---
path: SoulsFormats/Formats/AcParts/AC4/Part/Core.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: Core
---
path: SoulsFormats/Formats/AcParts/AC4/Part/CoreLowerSideStabilizer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: CoreLowerSideStabilizer
---
path: SoulsFormats/Formats/AcParts/AC4/Part/CoreUpperSideStabilizer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: CoreUpperSideStabilizer
---
path: SoulsFormats/Formats/AcParts/AC4/Part/FCS.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: FCS
---
path: SoulsFormats/Formats/AcParts/AC4/Part/Generator.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: Generator
---
path: SoulsFormats/Formats/AcParts/AC4/Part/Head.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: Head
---
path: SoulsFormats/Formats/AcParts/AC4/Part/HeadSideStabilizer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: HeadSideStabilizer
---
path: SoulsFormats/Formats/AcParts/AC4/Part/HeadTopStabilizer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: HeadTopStabilizer
---
path: SoulsFormats/Formats/AcParts/AC4/Part/IBooster.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part interface: IBooster
---
path: SoulsFormats/Formats/AcParts/AC4/Part/IFrame.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part interface: IFrame
---
path: SoulsFormats/Formats/AcParts/AC4/Part/IPart.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part interface: IPart
---
path: SoulsFormats/Formats/AcParts/AC4/Part/IStabilizer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part interface: IStabilizer
---
path: SoulsFormats/Formats/AcParts/AC4/Part/IWeapon.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part interface: IWeapon
---
path: SoulsFormats/Formats/AcParts/AC4/Part/Leg.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: Leg
---
path: SoulsFormats/Formats/AcParts/AC4/Part/LegBackStabilizer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: LegBackStabilizer
---
path: SoulsFormats/Formats/AcParts/AC4/Part/LegLowerStabilizer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: LegLowerStabilizer
---
path: SoulsFormats/Formats/AcParts/AC4/Part/LegMiddleStabilizer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: LegMiddleStabilizer
---
path: SoulsFormats/Formats/AcParts/AC4/Part/LegUpperStabilizer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: LegUpperStabilizer
---
path: SoulsFormats/Formats/AcParts/AC4/Part/MainBooster.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: MainBooster
---
path: SoulsFormats/Formats/AcParts/AC4/Part/OveredBooster.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: OveredBooster
---
path: SoulsFormats/Formats/AcParts/AC4/Part/ShoulderUnit.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: ShoulderUnit
---
path: SoulsFormats/Formats/AcParts/AC4/Part/SideBooster.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part: SideBooster
---
path: SoulsFormats/Formats/AcParts/AC4/Types/DispType.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 display type enum
---
path: SoulsFormats/Formats/BHD5.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/BTAB.cs
state: NOT-IMPLEMENTED
cluster: lighting
description: Bake atlas table (lightmap atlas indirection)
---
path: SoulsFormats/Formats/BTL.cs
state: NOT-IMPLEMENTED
cluster: lighting
description: Map light placement (point/spot lights)
---
path: SoulsFormats/Formats/BTPB.cs
state: NOT-IMPLEMENTED
cluster: lighting
description: Baked lighting prebake buffer
---
path: SoulsFormats/Formats/Binder/BND/BND.cs
state: NOT-IMPLEMENTED
cluster: legacy-binder
description: Legacy BND binder root (DeS/DS1/DS2/BB/AC)
---
path: SoulsFormats/Formats/Binder/BND2/BND2.cs
state: NOT-IMPLEMENTED
cluster: legacy-binder
description: Legacy BND2 binder root
---
path: SoulsFormats/Formats/Binder/BND2/BND2FileHeader.cs
state: NOT-IMPLEMENTED
cluster: legacy-binder
description: BND2 file header struct
---
path: SoulsFormats/Formats/Binder/BND2/BND2Reader.cs
state: NOT-IMPLEMENTED
cluster: legacy-binder
description: BND2 streaming reader
---
path: SoulsFormats/Formats/Binder/BND2/IBND2.cs
state: NOT-IMPLEMENTED
cluster: legacy-binder
description: BND2 interface
---
path: SoulsFormats/Formats/Binder/BND3/BND3.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BND3/BND3Reader.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BND3/IBND3.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BND4/BND4.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BND4/BND4Reader.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BND4/IBND4.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BXF3/BXF3.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BXF3/BXF3Reader.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BXF3/IBXF3.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BXF4/BXF4.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BXF4/BXF4Reader.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BXF4/IBXF4.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/Binder.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BinderFile.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BinderFileHeader.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BinderHashTable.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/BinderReader.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Binder/IBinder.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/CCM.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Character menu config (deferred)
---
path: SoulsFormats/Formats/CLM2.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: CLM2 collision-mesh v2 (deferred)
---
path: SoulsFormats/Formats/DCX.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/DRB/Anik.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB animation keyframe
---
path: SoulsFormats/Formats/DRB/Anim.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB animation
---
path: SoulsFormats/Formats/DRB/Anio.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB animation object
---
path: SoulsFormats/Formats/DRB/Control.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB UI control
---
path: SoulsFormats/Formats/DRB/DRB.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB UI layout root
---
path: SoulsFormats/Formats/DRB/Dlg.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB dialog
---
path: SoulsFormats/Formats/DRB/Dlgo.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB dialog object
---
path: SoulsFormats/Formats/DRB/Scdk.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB Scdk
---
path: SoulsFormats/Formats/DRB/Scdl.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB Scdl
---
path: SoulsFormats/Formats/DRB/Scdo.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB Scdo
---
path: SoulsFormats/Formats/DRB/Shape.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB shape
---
path: SoulsFormats/Formats/DRB/Texture.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: DRB texture
---
path: SoulsFormats/Formats/EDD.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: EDD (deferred)
---
path: SoulsFormats/Formats/EDGE.cs
state: NOT-IMPLEMENTED
cluster: navmesh
description: Navmesh edge data
---
path: SoulsFormats/Formats/EMELD.cs
state: NOT-IMPLEMENTED
cluster: text-script-misc
description: Event message localization data
---
path: SoulsFormats/Formats/EMEVD/EMEVD.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/EMEVD/Event.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/EMEVD/Instruction.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/EMEVD/Layer.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/EMEVD/Parameter.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/ENFL.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/ESD.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/F2TR.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Font texture descriptor (deferred)
---
path: SoulsFormats/Formats/FFXDLSE/Action.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE action node
---
path: SoulsFormats/Formats/FFXDLSE/Evaluatable.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE evaluatable expression
---
path: SoulsFormats/Formats/FFXDLSE/FFXDLSE.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE root (Dark Souls 2 SFX)
---
path: SoulsFormats/Formats/FFXDLSE/FXEffect.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE effect node
---
path: SoulsFormats/Formats/FFXDLSE/Param.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE parameter
---
path: SoulsFormats/Formats/FFXDLSE/ParamList.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE param list
---
path: SoulsFormats/Formats/FFXDLSE/Primitive.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE primitive
---
path: SoulsFormats/Formats/FFXDLSE/ResourceSet.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE resource set
---
path: SoulsFormats/Formats/FFXDLSE/State.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE state
---
path: SoulsFormats/Formats/FFXDLSE/StateMap.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE state map
---
path: SoulsFormats/Formats/FFXDLSE/Trigger.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FFXDLSE trigger
---
path: SoulsFormats/Formats/FLVER/Dummy.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/FLVER0/BufferLayout.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: FLVER0 buffer layout
---
path: SoulsFormats/Formats/FLVER/FLVER0/FLVER0.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: FLVER0 (Demon's Souls / early DS1) model root
---
path: SoulsFormats/Formats/FLVER/FLVER0/Material.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: FLVER0 material
---
path: SoulsFormats/Formats/FLVER/FLVER0/Mesh.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: FLVER0 mesh
---
path: SoulsFormats/Formats/FLVER/FLVER0/Texture.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: FLVER0 texture entry
---
path: SoulsFormats/Formats/FLVER/FLVER0/VertexBuffer.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: FLVER0 vertex buffer
---
path: SoulsFormats/Formats/FLVER/FLVER2/BufferLayout.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/FLVER2/EdgeGeomSpuConfigInfo.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: FLVER2 PS3 Edge geometry SPU config
---
path: SoulsFormats/Formats/FLVER/FLVER2/EdgeIndexBuffer.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: FLVER2 PS3 Edge index buffer
---
path: SoulsFormats/Formats/FLVER/FLVER2/EdgeIndexGroup.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: FLVER2 PS3 Edge index group
---
path: SoulsFormats/Formats/FLVER/FLVER2/EdgeVertexBuffer.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: FLVER2 PS3 Edge vertex buffer
---
path: SoulsFormats/Formats/FLVER/FLVER2/FLVER2.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/FLVER2/FaceSet.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/FLVER2/GXList.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/FLVER2/Material.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/FLVER2/Mesh.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/FLVER2/SkeletonSet.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/FLVER2/Texture.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/FLVER2/VertexBuffer.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/IFlver.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/LayoutMember.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/Node.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/Vertex.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/VertexBoneIndices.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/VertexBoneWeights.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FLVER/VertexColor.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FMB.cs
state: NOT-IMPLEMENTED
cluster: text-script-misc
description: Field map binary (placeholder map metadata)
---
path: SoulsFormats/Formats/FMG.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/FSDATA.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: Armored Core filesystem data manifest
---
path: SoulsFormats/Formats/FSLIBLZS.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: Armored Core FSLIB LZS-compressed archive
---
path: SoulsFormats/Formats/FXR1/DS1RExtraParams.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 DS1R extra params
---
path: SoulsFormats/Formats/FXR1/FXAction.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 action node
---
path: SoulsFormats/Formats/FXR1/FXActionData.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 action data
---
path: SoulsFormats/Formats/FXR1/FXContainer.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 container
---
path: SoulsFormats/Formats/FXR1/FXField.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 field
---
path: SoulsFormats/Formats/FXR1/FXModifier.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 modifier
---
path: SoulsFormats/Formats/FXR1/FXNode.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 node
---
path: SoulsFormats/Formats/FXR1/FXNodePointer.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 node pointer
---
path: SoulsFormats/Formats/FXR1/FXR1.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 root (DS1/DS1R SFX format)
---
path: SoulsFormats/Formats/FXR1/FXState.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 state
---
path: SoulsFormats/Formats/FXR1/FXTransition.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 transition
---
path: SoulsFormats/Formats/FXR1/FxrEnvironment.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 environment
---
path: SoulsFormats/Formats/FXR1/Ticks.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 tick helpers
---
path: SoulsFormats/Formats/FXR1/XIDable.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: FXR1 base XID-able
---
path: SoulsFormats/Formats/FXR3.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/GPARAM.cs
state: NOT-IMPLEMENTED
cluster: lighting
description: Global environment parameters (sky/light/fog)
---
path: SoulsFormats/Formats/GRASS.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Foliage placement (deferred)
---
path: SoulsFormats/Formats/LUAGNL.cs
state: NOT-IMPLEMENTED
cluster: text-script-misc
description: Lua global name list
---
path: SoulsFormats/Formats/LUAINFO.cs
state: NOT-IMPLEMENTED
cluster: text-script-misc
description: Lua script info table
---
path: SoulsFormats/Formats/MATBIN.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MCG.cs
state: NOT-IMPLEMENTED
cluster: navmesh
description: Map collision graph (navmesh nodes)
---
path: SoulsFormats/Formats/MCP.cs
state: NOT-IMPLEMENTED
cluster: navmesh
description: Map collision points (navmesh rooms)
---
path: SoulsFormats/Formats/MLB/IMLB.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: MLB interface
---
path: SoulsFormats/Formats/MLB/IMlbResource.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: MLB resource interface
---
path: SoulsFormats/Formats/MLB/MLB_AC4.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: MLB AC4 variant
---
path: SoulsFormats/Formats/MLB/MLB_AC5.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: MLB AC5 variant
---
path: SoulsFormats/Formats/MQB/Cut.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: MQB cut
---
path: SoulsFormats/Formats/MQB/Event.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: MQB event
---
path: SoulsFormats/Formats/MQB/MQB.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: MQB cutscene/movie root
---
path: SoulsFormats/Formats/MQB/Parameter.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: MQB parameter
---
path: SoulsFormats/Formats/MQB/Resource.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: MQB resource
---
path: SoulsFormats/Formats/MQB/Timeline.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: MQB timeline
---
path: SoulsFormats/Formats/MQB/Transform.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: MQB transform
---
path: SoulsFormats/Formats/MSB/FormatReference.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/IMsb.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSB.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSB1/EventParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB1 (Dark Souls 1) event param
---
path: SoulsFormats/Formats/MSB/MSB1/MSB1.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB1 (Dark Souls 1) root
---
path: SoulsFormats/Formats/MSB/MSB1/ModelParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB1 model param
---
path: SoulsFormats/Formats/MSB/MSB1/PartsParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB1 parts param
---
path: SoulsFormats/Formats/MSB/MSB1/PointParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB1 point/region param
---
path: SoulsFormats/Formats/MSB/MSB2/EventParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB2 (Dark Souls 2) event param
---
path: SoulsFormats/Formats/MSB/MSB2/LayerParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB2 layer param
---
path: SoulsFormats/Formats/MSB/MSB2/MSB2.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB2 (Dark Souls 2) root
---
path: SoulsFormats/Formats/MSB/MSB2/MapstudioBoneName.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB2 bone-name section
---
path: SoulsFormats/Formats/MSB/MSB2/MapstudioPartsPose.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB2 parts-pose section
---
path: SoulsFormats/Formats/MSB/MSB2/ModelParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB2 model param
---
path: SoulsFormats/Formats/MSB/MSB2/PartsParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB2 parts param
---
path: SoulsFormats/Formats/MSB/MSB2/PointParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB2 point/region param
---
path: SoulsFormats/Formats/MSB/MSB2/RouteParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB2 route param
---
path: SoulsFormats/Formats/MSB/MSB3/EventParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB3 (Dark Souls 3) event param
---
path: SoulsFormats/Formats/MSB/MSB3/LayerParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB3 layer param
---
path: SoulsFormats/Formats/MSB/MSB3/MSB3.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB3 (Dark Souls 3) root
---
path: SoulsFormats/Formats/MSB/MSB3/MapstudioBoneName.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB3 bone-name section
---
path: SoulsFormats/Formats/MSB/MSB3/MapstudioPartsPose.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB3 parts-pose section
---
path: SoulsFormats/Formats/MSB/MSB3/ModelParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB3 model param
---
path: SoulsFormats/Formats/MSB/MSB3/PartsParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB3 parts param
---
path: SoulsFormats/Formats/MSB/MSB3/PointParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB3 point/region param
---
path: SoulsFormats/Formats/MSB/MSB3/RouteParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSB3 route param
---
path: SoulsFormats/Formats/MSB/MSBAC4/EventParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBAC4 (Armored Core 4) event param
---
path: SoulsFormats/Formats/MSB/MSBAC4/LayerParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBAC4 layer param
---
path: SoulsFormats/Formats/MSB/MSBAC4/MSBAC4.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBAC4 root
---
path: SoulsFormats/Formats/MSB/MSBAC4/MapStudioTreeParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBAC4 tree section
---
path: SoulsFormats/Formats/MSB/MSBAC4/ModelParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBAC4 model param
---
path: SoulsFormats/Formats/MSB/MSBAC4/PartsParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBAC4 parts param
---
path: SoulsFormats/Formats/MSB/MSBAC4/PointParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBAC4 point/region param
---
path: SoulsFormats/Formats/MSB/MSBAC4/RouteParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBAC4 route param
---
path: SoulsFormats/Formats/MSB/MSBB/EventParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBB (Bloodborne) event param
---
path: SoulsFormats/Formats/MSB/MSBB/MSBB.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBB (Bloodborne) root
---
path: SoulsFormats/Formats/MSB/MSBB/ModelParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBB model param
---
path: SoulsFormats/Formats/MSB/MSBB/PartsParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBB parts param
---
path: SoulsFormats/Formats/MSB/MSBB/PointParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBB point/region param
---
path: SoulsFormats/Formats/MSB/MSBD/EventParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBD (Demon's Souls) event param
---
path: SoulsFormats/Formats/MSB/MSBD/MSBD.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBD (Demon's Souls) root
---
path: SoulsFormats/Formats/MSB/MSBD/MapstudioTree.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBD tree section
---
path: SoulsFormats/Formats/MSB/MSBD/ModelParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBD model param
---
path: SoulsFormats/Formats/MSB/MSBD/PartsParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBD parts param
---
path: SoulsFormats/Formats/MSB/MSBD/PointParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBD point/region param
---
path: SoulsFormats/Formats/MSB/MSBDR/EventParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBDR (Dark Souls Remastered) event param
---
path: SoulsFormats/Formats/MSB/MSBDR/MSBDR.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBDR root
---
path: SoulsFormats/Formats/MSB/MSBDR/MapstudioTree.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBDR tree section
---
path: SoulsFormats/Formats/MSB/MSBDR/ModelParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBDR model param
---
path: SoulsFormats/Formats/MSB/MSBDR/PartsParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBDR parts param
---
path: SoulsFormats/Formats/MSB/MSBDR/PointParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBDR point/region param
---
path: SoulsFormats/Formats/MSB/MSBE/EventParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBE/MSBE.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBE/ModelParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBE/PartsParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBE/PointParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBE/RouteParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBFA/EventParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBFA (Armored Core For Answer) event param
---
path: SoulsFormats/Formats/MSB/MSBFA/LayerParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBFA layer param
---
path: SoulsFormats/Formats/MSB/MSBFA/MSBFA.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBFA root
---
path: SoulsFormats/Formats/MSB/MSBFA/MapStudioTreeParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBFA tree section
---
path: SoulsFormats/Formats/MSB/MSBFA/ModelParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBFA model param
---
path: SoulsFormats/Formats/MSB/MSBFA/PartsParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBFA parts param
---
path: SoulsFormats/Formats/MSB/MSBFA/PointParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBFA point/region param
---
path: SoulsFormats/Formats/MSB/MSBFA/RouteParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBFA route param
---
path: SoulsFormats/Formats/MSB/MSBN/MSBN.ModelSection.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBN (Ninja Blade) model section
---
path: SoulsFormats/Formats/MSB/MSBN/MSBN.PartsSection.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBN parts section
---
path: SoulsFormats/Formats/MSB/MSBN/MSBN.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBN (Ninja Blade) root
---
path: SoulsFormats/Formats/MSB/MSBReference.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBS/EventParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBS/MSBS.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBS/ModelParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBS/PartsParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBS/PointParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBS/RouteParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBV/EventParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBV (Armored Core V) event param
---
path: SoulsFormats/Formats/MSB/MSBV/LayerParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBV layer param
---
path: SoulsFormats/Formats/MSB/MSBV/MSBV.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBV root
---
path: SoulsFormats/Formats/MSB/MSBV/MapStudioTreeParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBV tree section
---
path: SoulsFormats/Formats/MSB/MSBV/ModelParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBV model param
---
path: SoulsFormats/Formats/MSB/MSBV/PartsParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBV parts param
---
path: SoulsFormats/Formats/MSB/MSBV/PointParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBV point/region param
---
path: SoulsFormats/Formats/MSB/MSBV/RouteParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBV route param
---
path: SoulsFormats/Formats/MSB/MSBVD/EventParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBVD (Armored Core Verdict Day) event param
---
path: SoulsFormats/Formats/MSB/MSBVD/LayerParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBVD layer param
---
path: SoulsFormats/Formats/MSB/MSBVD/MSBVD.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBVD root
---
path: SoulsFormats/Formats/MSB/MSBVD/MapStudioTreeParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBVD tree section
---
path: SoulsFormats/Formats/MSB/MSBVD/ModelParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBVD model param
---
path: SoulsFormats/Formats/MSB/MSBVD/PartsParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBVD parts param
---
path: SoulsFormats/Formats/MSB/MSBVD/PointParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBVD point/region param
---
path: SoulsFormats/Formats/MSB/MSBVD/RouteParam.cs
state: NOT-IMPLEMENTED
cluster: legacy-msb
description: MSBVD route param
---
path: SoulsFormats/Formats/MSB/MSBVI/EventParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBVI/LayerParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBVI/MSBVI.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBVI/ModelParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBVI/PartsParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBVI/PointParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MSBVI/RouteParam.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/MsbBoundingBox.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MSB/Shape.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/MTD.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/AnimToRigTableMap.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme anim-to-rig table map
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Event.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme event
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/EventTrack.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme event track
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/FileNameLookupTable.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme filename lookup table
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/LookupTable.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme lookup table
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeBundleEnums.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme bundle enums
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeBundleGUID.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme bundle GUID
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeBundleGeneric.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme generic bundle
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeBundle_Base.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme bundle base
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeFileHeader.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme file header
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/MorphemeSizeAlignFormatting.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme size/align formatting
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/MessageDef.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme network message def
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/MorphemeNodeDef.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme node def
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeAttrib/NodeAttribBase.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme node attr base
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeAttrib/NodeAttribBool.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme node attr bool
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeAttrib/NodeAttribSourceAnim.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme node attr source anim
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeAttrib/NodeAttribSourceEventTrack.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme node attr source event track
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeAttrib/NodeAttribUnknown.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme node attr unknown
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeDataSet.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme node data set
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/NodeDef.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme node def
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Network/SmStateList.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme state machine state list
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/NetworkBundle.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme network bundle
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/Rig.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme rig
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/RigToAnimEntryMap.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme rig-to-anim entry map
---
path: SoulsFormats/Formats/Morpheme/MorphemeBundle/RigToAnimMap.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme rig-to-anim map
---
path: SoulsFormats/Formats/Morpheme/NMB.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme network bundle root
---
path: SoulsFormats/Formats/Morpheme/NSA/DequantizationFactor.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme NSA dequantization factor
---
path: SoulsFormats/Formats/Morpheme/NSA/DequantizationInfo.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme NSA dequantization info
---
path: SoulsFormats/Formats/Morpheme/NSA/DynamicSegment.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme NSA dynamic segment
---
path: SoulsFormats/Formats/Morpheme/NSA/NSA.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme NSA animation root
---
path: SoulsFormats/Formats/Morpheme/NSA/NSAHeader.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme NSA header
---
path: SoulsFormats/Formats/Morpheme/NSA/NSAVec3.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme NSA Vec3
---
path: SoulsFormats/Formats/Morpheme/NSA/RootMotionSegment.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme NSA root motion segment
---
path: SoulsFormats/Formats/Morpheme/NSA/RotationData.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme NSA rotation data
---
path: SoulsFormats/Formats/Morpheme/NSA/StaticSegment.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme NSA static segment
---
path: SoulsFormats/Formats/Morpheme/NSA/TranslationData.cs
state: NOT-IMPLEMENTED
cluster: effects-misc
description: Morpheme NSA translation data
---
path: SoulsFormats/Formats/NGP.cs
state: NOT-IMPLEMENTED
cluster: navmesh
description: Navmesh group/path container
---
path: SoulsFormats/Formats/NVA.cs
state: NOT-IMPLEMENTED
cluster: navmesh
description: Navmesh area data
---
path: SoulsFormats/Formats/NVM.cs
state: NOT-IMPLEMENTED
cluster: navmesh
description: Navmesh polygon mesh
---
path: SoulsFormats/Formats/Other/AC3SL/BND.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC3 Silent Line BND binder
---
path: SoulsFormats/Formats/Other/AC4/ANC.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 ANC asset
---
path: SoulsFormats/Formats/Other/AC4/AcAttachInfo.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 attach info
---
path: SoulsFormats/Formats/Other/AC4/AcColorSet4.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 color set
---
path: SoulsFormats/Formats/Other/AC4/AcConflictInfo.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 conflict info
---
path: SoulsFormats/Formats/Other/AC4/AcPartCategory.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 part category enum
---
path: SoulsFormats/Formats/Other/AC4/DBSUB.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC4 DBSUB
---
path: SoulsFormats/Formats/Other/ACE3/BND.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: AC Last Raven / ACE3 BND binder
---
path: SoulsFormats/Formats/Other/Dreamcast/MGF.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Dreamcast MGF (Eternal Ring / Evergrace era)
---
path: SoulsFormats/Formats/Other/KF4/CHR.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: King's Field IV character
---
path: SoulsFormats/Formats/Other/KF4/DAT.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: King's Field IV data
---
path: SoulsFormats/Formats/Other/KF4/MAP.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: King's Field IV map
---
path: SoulsFormats/Formats/Other/KF4/OM2.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: King's Field IV OM2
---
path: SoulsFormats/Formats/Other/Kuon/BND.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Kuon BND binder
---
path: SoulsFormats/Formats/Other/Kuon/DVDBND.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Kuon DVDBND
---
path: SoulsFormats/Formats/Other/LDMU.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Lost Kingdoms / LDMU container
---
path: SoulsFormats/Formats/Other/MDL.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: Legacy MDL model format
---
path: SoulsFormats/Formats/Other/MDL0.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: Legacy MDL0 model format
---
path: SoulsFormats/Formats/Other/MDL4/Dummy.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: MDL4 dummy point
---
path: SoulsFormats/Formats/Other/MDL4/MDL4.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: Legacy MDL4 model root
---
path: SoulsFormats/Formats/Other/MDL4/Material.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: MDL4 material
---
path: SoulsFormats/Formats/Other/MDL4/Mesh.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: MDL4 mesh
---
path: SoulsFormats/Formats/Other/MDL4/Node.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: MDL4 node
---
path: SoulsFormats/Formats/Other/MDL4/Vertex.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: MDL4 vertex
---
path: SoulsFormats/Formats/Other/MDL4/VertexBoneIndices.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: MDL4 vertex bone indices
---
path: SoulsFormats/Formats/Other/MDL4/VertexBoneWeights.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: MDL4 vertex bone weights
---
path: SoulsFormats/Formats/Other/MDL4/VertexColor.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: MDL4 vertex color
---
path: SoulsFormats/Formats/Other/MDLEnum.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: MDL/MDL4 shared enums
---
path: SoulsFormats/Formats/Other/MWC/DEV.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Metal Wolf Chaos DEV
---
path: SoulsFormats/Formats/Other/MWC/MDAT.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Metal Wolf Chaos MDAT
---
path: SoulsFormats/Formats/Other/MWC/MMD.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Metal Wolf Chaos MMD
---
path: SoulsFormats/Formats/Other/MWC/OTR.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Metal Wolf Chaos OTR
---
path: SoulsFormats/Formats/Other/MWC/SMD.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Metal Wolf Chaos SMD
---
path: SoulsFormats/Formats/Other/MWC/TDAT.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Metal Wolf Chaos TDAT
---
path: SoulsFormats/Formats/Other/Murakumo/DDL.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Murakumo DDL
---
path: SoulsFormats/Formats/Other/Otogi2/DAT.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Otogi 2 DAT
---
path: SoulsFormats/Formats/Other/SOM/MDO.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Shadow of Memories MDO
---
path: SoulsFormats/Formats/Other/Zero3.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Zero3 container
---
path: SoulsFormats/Formats/PARAM/Deprecated/Enum.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Deprecated PARAM Enum (XML-pre-PARAMDEF)
---
path: SoulsFormats/Formats/PARAM/Deprecated/Layout.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Deprecated PARAM Layout (XML-pre-PARAMDEF)
---
path: SoulsFormats/Formats/PARAM/PARAM/Cell.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/PARAM/PARAM/PARAM.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/PARAM/PARAM/Row.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/PARAM/PARAMDBP/DBPPARAM.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: PARAMDBP-backed PARAM (AC-style)
---
path: SoulsFormats/Formats/PARAM/PARAMDBP/PARAMDBP.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: PARAMDBP definition (AC-style)
---
path: SoulsFormats/Formats/PARAM/PARAMDBP/TxtSerializer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: PARAMDBP text serializer
---
path: SoulsFormats/Formats/PARAM/PARAMDBP/XmlSerializer.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: PARAMDBP XML serializer
---
path: SoulsFormats/Formats/PARAM/PARAMDEF/Field.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/PARAM/PARAMDEF/XmlSerializer.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/PARAM/PARAMTDF.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/PARAM/ParamDbpUtil.cs
state: NOT-IMPLEMENTED
cluster: ac-specific
description: PARAMDBP utilities
---
path: SoulsFormats/Formats/PARAM/ParamUtil.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/PMDCL.cs
state: NOT-IMPLEMENTED
cluster: lighting
description: Per-map decal placement
---
path: SoulsFormats/Formats/RMB.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: RMB (deferred)
---
path: SoulsFormats/Formats/SMD4/Mesh.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: SMD4 mesh
---
path: SoulsFormats/Formats/SMD4/Node.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: SMD4 node
---
path: SoulsFormats/Formats/SMD4/SMD4.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: SMD4 model (deferred)
---
path: SoulsFormats/Formats/SMD4/Unk10.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: SMD4 Unk10
---
path: SoulsFormats/Formats/SMD4/Vertex.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: SMD4 vertex
---
path: SoulsFormats/Formats/SMD4/VertexBoneIndices.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: SMD4 vertex bone indices
---
path: SoulsFormats/Formats/SMD4/VertexBoneWeights.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: SMD4 vertex bone weights
---
path: SoulsFormats/Formats/TAE/Animation.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/TAE/Event.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/TAE/EventGroup.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/TAE/TAE.cs
state: PARTIAL
cluster: tae-templates
description: TAE container; v1 implements only Sekiro/SDT version 0x1000D. Non-SDT TAE variants deferred.
---
path: SoulsFormats/Formats/TAE/Template.cs
state: NOT-IMPLEMENTED
cluster: tae-templates
description: TAE template/binding subsystem (TAE event schema templates)
---
path: SoulsFormats/Formats/TPF/DDS.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/TPF/Headerizer.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Formats/TPF/SecretHeaderizer.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: TPF secret headerizer (out of scope per format-tpf.md)
---
path: SoulsFormats/Formats/TPF/TPF.cs
state: IMPLEMENTED
description: Implemented (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Attributes/HideProperty.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# attribute (not applicable to C)
---
path: SoulsFormats/Utilities/Attributes/NoRenderGroupInheritance.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# attribute (not applicable to C)
---
path: SoulsFormats/Utilities/Attributes/RotationRadians.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# attribute (not applicable to C)
---
path: SoulsFormats/Utilities/Attributes/RotationXZY.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# attribute (not applicable to C)
---
path: SoulsFormats/Utilities/Attributes/SupportsAlphaAttribute.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# attribute (not applicable to C)
---
path: SoulsFormats/Utilities/BitConverterHelper.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: BitConverter helper (subsumed by sf_io binary readers)
---
path: SoulsFormats/Utilities/Collections/ListExtensions.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# list extension methods (not applicable to C)
---
path: SoulsFormats/Utilities/Compression/Oodle/IOodleCompressor.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Compression/Oodle/Oodle.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Compression/Oodle/Oodle26.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Compression/Oodle/Oodle28.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Compression/Oodle/Oodle29.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Compression/ZlibHelper.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Compression/ZstdHelper.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Cryptography/HashHelper.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Cryptography/RegulationDecryptor.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Cryptography/SL2Decryptor.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/EdgeGeom.cs
state: NOT-IMPLEMENTED
cluster: legacy-flver
description: PS3 Edge geometry helper (used by FLVER2 EdgeBuffers)
---
path: SoulsFormats/Utilities/EndianHelper.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Endian helper (subsumed by sf_io reader/writer endianness flag)
---
path: SoulsFormats/Utilities/Exceptions/NoOodleException.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# exception (not applicable to C)
---
path: SoulsFormats/Utilities/Formats/ISoulsFile.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# generic ISoulsFile interface (subsumed by sf_*_read/write)
---
path: SoulsFormats/Utilities/Formats/SoulsFile.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# abstract SoulsFile base (subsumed by sf_*_read/write)
---
path: SoulsFormats/Utilities/Guessing/ExtensionGuesser.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: File-extension guesser (deferred)
---
path: SoulsFormats/Utilities/HexHelper.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: Hex print helper (deferred)
---
path: SoulsFormats/Utilities/IO/BinaryReaderEx.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/IO/BinaryWriterEx.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/IO/PathHelper.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/MathHelper.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/NativeLibrary.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# native library loader (we have our own native loader)
---
path: SoulsFormats/Utilities/SFUtil.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Text/SFEncoding.cs
state: IMPLEMENTED
description: Implemented utility (Tier A row-level mapping in docs/api-mapping/)
---
path: SoulsFormats/Utilities/Xml/XmlNodeExtensions.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# XML node extensions (used by PARAMDEF XML; subsumed)
---
path: SoulsFormats/Utilities/Xml/XmlWriterExtensions.cs
state: NOT-IMPLEMENTED
cluster: uncategorized-deferred
description: C# XML writer extensions (used by PARAMDEF XML; subsumed)
---
