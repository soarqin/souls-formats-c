## TL;DR

Implement 10 legacy MapStudio Binary (MSB) variants covering Demon's Souls, Dark Souls 1/2/3, Bloodborne, Ninja Blade, and Armored Core 4/FA/V/VD. This cluster brings full map data support for the entire FromSoftware catalog supported by SoulsFormatsNEXT.

## Upstream formats covered

- MSB1 (Dark Souls 1)
- MSB2 (Dark Souls 2)
- MSB3 (Dark Souls 3)
- MSBAC4 (Armored Core 4)
- MSBB (Bloodborne)
- MSBD (Demon's Souls)
- MSBDR (Dark Souls Remastered)
- MSBFA (Armored Core For Answer)
- MSBN (Ninja Blade)
- MSBV (Armored Core V)
- MSBVD (Armored Core Verdict Day)

## Must Have

- Full implementation of all 10 legacy MSB variants.
- Support for all standard sections: Model, Event, Point/Region, Parts, and Route.
- Support for game-specific sections like BoneName, PartsPose, and Tree.
- Shared infrastructure for MSB shapes and bounding boxes.

## Must NOT Have

- MSBS (Sekiro), MSBE (Elden Ring), MSBVI (Armored Core VI) — already in v1.

## Dependencies on prior clusters

- Phase 1 (Core IO)
- Phase 5 (MSB base infrastructure)

## Acceptance criteria

- All 10 legacy MSB variants pass read/write round-trip tests.
- Verification via:
```bash
cmake --build build-mingw
ctest --test-dir build-mingw -L legacy-msb --output-on-failure
find build-mingw/tests -name "test_msb*" -executable -exec {} \;
```

## STRICT UPSTREAM REFERENCE

| Format | Upstream Path |
|--------|---------------|
| MSB1 Root | SoulsFormats/Formats/MSB/MSB1/MSB1.cs |
| MSB1 Event | SoulsFormats/Formats/MSB/MSB1/EventParam.cs |
| MSB1 Model | SoulsFormats/Formats/MSB/MSB1/ModelParam.cs |
| MSB1 Parts | SoulsFormats/Formats/MSB/MSB1/PartsParam.cs |
| MSB1 Point | SoulsFormats/Formats/MSB/MSB1/PointParam.cs |
| MSB2 Root | SoulsFormats/Formats/MSB/MSB2/MSB2.cs |
| MSB2 Event | SoulsFormats/Formats/MSB/MSB2/EventParam.cs |
| MSB2 Layer | SoulsFormats/Formats/MSB/MSB2/LayerParam.cs |
| MSB2 Bone | SoulsFormats/Formats/MSB/MSB2/MapstudioBoneName.cs |
| MSB2 Pose | SoulsFormats/Formats/MSB/MSB2/MapstudioPartsPose.cs |
| MSB2 Model | SoulsFormats/Formats/MSB/MSB2/ModelParam.cs |
| MSB2 Parts | SoulsFormats/Formats/MSB/MSB2/PartsParam.cs |
| MSB2 Point | SoulsFormats/Formats/MSB/MSB2/PointParam.cs |
| MSB2 Route | SoulsFormats/Formats/MSB/MSB2/RouteParam.cs |
| MSB3 Root | SoulsFormats/Formats/MSB/MSB3/MSB3.cs |
| MSB3 Event | SoulsFormats/Formats/MSB/MSB3/EventParam.cs |
| MSB3 Layer | SoulsFormats/Formats/MSB/MSB3/LayerParam.cs |
| MSB3 Bone | SoulsFormats/Formats/MSB/MSB3/MapstudioBoneName.cs |
| MSB3 Pose | SoulsFormats/Formats/MSB/MSB3/MapstudioPartsPose.cs |
| MSB3 Model | SoulsFormats/Formats/MSB/MSB3/ModelParam.cs |
| MSB3 Parts | SoulsFormats/Formats/MSB/MSB3/PartsParam.cs |
| MSB3 Point | SoulsFormats/Formats/MSB/MSB3/PointParam.cs |
| MSB3 Route | SoulsFormats/Formats/MSB/MSB3/RouteParam.cs |
| MSBAC4 Root | SoulsFormats/Formats/MSB/MSBAC4/MSBAC4.cs |
| MSBAC4 Event | SoulsFormats/Formats/MSB/MSBAC4/EventParam.cs |
| MSBAC4 Layer | SoulsFormats/Formats/MSB/MSBAC4/LayerParam.cs |
| MSBAC4 Tree | SoulsFormats/Formats/MSB/MSBAC4/MapStudioTreeParam.cs |
| MSBAC4 Model | SoulsFormats/Formats/MSB/MSBAC4/ModelParam.cs |
| MSBAC4 Parts | SoulsFormats/Formats/MSB/MSBAC4/PartsParam.cs |
| MSBAC4 Point | SoulsFormats/Formats/MSB/MSBAC4/PointParam.cs |
| MSBAC4 Route | SoulsFormats/Formats/MSB/MSBAC4/RouteParam.cs |
| MSBB Root | SoulsFormats/Formats/MSB/MSBB/MSBB.cs |
| MSBB Event | SoulsFormats/Formats/MSB/MSBB/EventParam.cs |
| MSBB Model | SoulsFormats/Formats/MSB/MSBB/ModelParam.cs |
| MSBB Parts | SoulsFormats/Formats/MSB/MSBB/PartsParam.cs |
| MSBB Point | SoulsFormats/Formats/MSB/MSBB/PointParam.cs |
| MSBD Root | SoulsFormats/Formats/MSB/MSBD/MSBD.cs |
| MSBD Event | SoulsFormats/Formats/MSB/MSBD/EventParam.cs |
| MSBD Tree | SoulsFormats/Formats/MSB/MSBD/MapstudioTree.cs |
| MSBD Model | SoulsFormats/Formats/MSB/MSBD/ModelParam.cs |
| MSBD Parts | SoulsFormats/Formats/MSB/MSBD/PartsParam.cs |
| MSBD Point | SoulsFormats/Formats/MSB/MSBD/PointParam.cs |
| MSBDR Root | SoulsFormats/Formats/MSB/MSBDR/MSBDR.cs |
| MSBDR Event | SoulsFormats/Formats/MSB/MSBDR/EventParam.cs |
| MSBDR Tree | SoulsFormats/Formats/MSB/MSBDR/MapstudioTree.cs |
| MSBDR Model | SoulsFormats/Formats/MSB/MSBDR/ModelParam.cs |
| MSBDR Parts | SoulsFormats/Formats/MSB/MSBDR/PartsParam.cs |
| MSBDR Point | SoulsFormats/Formats/MSB/MSBDR/PointParam.cs |
| MSBFA Root | SoulsFormats/Formats/MSB/MSBFA/MSBFA.cs |
| MSBFA Event | SoulsFormats/Formats/MSB/MSBFA/EventParam.cs |
| MSBFA Layer | SoulsFormats/Formats/MSB/MSBFA/LayerParam.cs |
| MSBFA Tree | SoulsFormats/Formats/MSB/MSBFA/MapStudioTreeParam.cs |
| MSBFA Model | SoulsFormats/Formats/MSB/MSBFA/ModelParam.cs |
| MSBFA Parts | SoulsFormats/Formats/MSB/MSBFA/PartsParam.cs |
| MSBFA Point | SoulsFormats/Formats/MSB/MSBFA/PointParam.cs |
| MSBFA Route | SoulsFormats/Formats/MSB/MSBFA/RouteParam.cs |
| MSBN Root | SoulsFormats/Formats/MSB/MSBN/MSBN.cs |
| MSBN Model | SoulsFormats/Formats/MSB/MSBN/MSBN.ModelSection.cs |
| MSBN Parts | SoulsFormats/Formats/MSB/MSBN/MSBN.PartsSection.cs |
| MSBV Root | SoulsFormats/Formats/MSB/MSBV/MSBV.cs |
| MSBV Event | SoulsFormats/Formats/MSB/MSBV/EventParam.cs |
| MSBV Layer | SoulsFormats/Formats/MSB/MSBV/LayerParam.cs |
| MSBV Tree | SoulsFormats/Formats/MSB/MSBV/MapStudioTreeParam.cs |
| MSBV Model | SoulsFormats/Formats/MSB/MSBV/ModelParam.cs |
| MSBV Parts | SoulsFormats/Formats/MSB/MSBV/PartsParam.cs |
| MSBV Point | SoulsFormats/Formats/MSB/MSBV/PointParam.cs |
| MSBV Route | SoulsFormats/Formats/MSB/MSBV/RouteParam.cs |
| MSBVD Root | SoulsFormats/Formats/MSB/MSBVD/MSBVD.cs |
| MSBVD Event | SoulsFormats/Formats/MSB/MSBVD/EventParam.cs |
| MSBVD Layer | SoulsFormats/Formats/MSB/MSBVD/LayerParam.cs |
| MSBVD Tree | SoulsFormats/Formats/MSB/MSBVD/MapStudioTreeParam.cs |
| MSBVD Model | SoulsFormats/Formats/MSB/MSBVD/ModelParam.cs |
| MSBVD Parts | SoulsFormats/Formats/MSB/MSBVD/PartsParam.cs |
| MSBVD Point | SoulsFormats/Formats/MSB/MSBVD/PointParam.cs |
| MSBVD Route | SoulsFormats/Formats/MSB/MSBVD/RouteParam.cs |

## Estimated effort

- 14 days (High volume of boilerplate, but logic is repetitive across variants).

## Risk

- Medium. The sheer number of variants increases the surface area for minor field-mapping errors.
