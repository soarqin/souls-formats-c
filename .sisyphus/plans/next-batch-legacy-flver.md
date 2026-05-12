## TL;DR

Implement legacy model formats FLVER0 (Demon's Souls / early DS1) and MDL/MDL0/MDL4, along with PS3 Edge geometry support for FLVER2. This cluster completes the geometry coverage for legacy titles.

## Upstream formats covered

- FLVER0 (Legacy model format)
- MDL, MDL0, MDL4 (Legacy model formats)
- PS3 Edge geometry buffers for FLVER2

## Must Have

- Full implementation of `sf_flver0_t` mirroring upstream `FLVER0.cs`.
- Implementation of `sf_mdl4_t` and related legacy MDL formats.
- Support for PS3 Edge geometry decompression via `EdgeGeom` utility.
- Integration with existing `sf_flver_vertex_t` and `sf_flver_layout_member_t` where possible.

## Must NOT Have

- FLVER2 (already in v1, except for PS3 Edge extensions).

## Dependencies on prior clusters

- Phase 1 (Core IO)
- Phase 6 (FLVER2 infrastructure)

## Acceptance criteria

- FLVER0 and MDL4 models pass read/write round-trip tests.
- PS3 Edge buffers in FLVER2 are correctly handled.
- Verification via:
```bash
cmake --build build-mingw
ctest --test-dir build-mingw -L legacy-flver --output-on-failure
grep -r "sf_flver0_" include/souls_formats/
```

## STRICT UPSTREAM REFERENCE

| Format | Upstream Path |
|--------|---------------|
| FLVER0 Root | SoulsFormats/Formats/FLVER/FLVER0/FLVER0.cs |
| FLVER0 Layout | SoulsFormats/Formats/FLVER/FLVER0/BufferLayout.cs |
| FLVER0 Material | SoulsFormats/Formats/FLVER/FLVER0/Material.cs |
| FLVER0 Mesh | SoulsFormats/Formats/FLVER/FLVER0/Mesh.cs |
| FLVER0 Texture | SoulsFormats/Formats/FLVER/FLVER0/Texture.cs |
| FLVER0 Buffer | SoulsFormats/Formats/FLVER/FLVER0/VertexBuffer.cs |
| Edge SPU Config | SoulsFormats/Formats/FLVER/FLVER2/EdgeGeomSpuConfigInfo.cs |
| Edge Index Buffer | SoulsFormats/Formats/FLVER/FLVER2/EdgeIndexBuffer.cs |
| Edge Index Group | SoulsFormats/Formats/FLVER/FLVER2/EdgeIndexGroup.cs |
| Edge Vertex Buffer | SoulsFormats/Formats/FLVER/FLVER2/EdgeVertexBuffer.cs |
| MDL | SoulsFormats/Formats/Other/MDL.cs |
| MDL0 | SoulsFormats/Formats/Other/MDL0.cs |
| MDL4 Root | SoulsFormats/Formats/Other/MDL4/MDL4.cs |
| MDL4 Dummy | SoulsFormats/Formats/Other/MDL4/Dummy.cs |
| MDL4 Material | SoulsFormats/Formats/Other/MDL4/Material.cs |
| MDL4 Mesh | SoulsFormats/Formats/Other/MDL4/Mesh.cs |
| MDL4 Node | SoulsFormats/Formats/Other/MDL4/Node.cs |
| MDL4 Vertex | SoulsFormats/Formats/Other/MDL4/Vertex.cs |
| MDL4 Bone Indices | SoulsFormats/Formats/Other/MDL4/VertexBoneIndices.cs |
| MDL4 Bone Weights | SoulsFormats/Formats/Other/MDL4/VertexBoneWeights.cs |
| MDL4 Color | SoulsFormats/Formats/Other/MDL4/VertexColor.cs |
| MDL Enums | SoulsFormats/Formats/Other/MDLEnum.cs |
| EdgeGeom Utility | SoulsFormats/Utilities/EdgeGeom.cs |

## Estimated effort

- 7 days (FLVER0 is similar to FLVER2, but EdgeGeom and MDL4 add complexity).

## Risk

- Medium. PS3 Edge geometry decompression is non-trivial and requires careful implementation of the SPU-optimized logic.
