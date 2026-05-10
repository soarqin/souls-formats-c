# FLVER2 Mapping

Contributing Files: 13 (`GXList.cs`, `EdgeVertexBuffer.cs`, `VertexBuffer.cs`, `Mesh.cs`, `Texture.cs`, `EdgeGeomSpuConfigInfo.cs`, `FLVER2.cs`, `EdgeIndexGroup.cs`, `SkeletonSet.cs`, `FaceSet.cs`, `EdgeIndexBuffer.cs`, `BufferLayout.cs`, `Material.cs`)

| C Symbol | Upstream C# Symbol | Status | Phase | Description |
| :--- | :--- | :--- | :--- | :--- |
| `sf_flver2_t` | `FLVER2` | 未实现 | 6 | FLVER2 model container |
| `sf_flver2_header_t` | `FLVER2.FLVERHeader` | 未实现 | 6 | FLVER2 file header |
| `sf_flver2_gx_list_t` | `FLVER2.GXList` | 未实现 | 6 | Collection of GX items |
| `sf_flver2_gx_item_t` | `FLVER2.GXItem` | 未实现 | 6 | Material rendering parameter |
| `sf_flver2_material_t` | `FLVER2.Material` | 未实现 | 6 | FLVER2 material |
| `sf_flver2_texture_t` | `FLVER2.Texture` | 未实现 | 6 | FLVER2 texture reference |
| `sf_flver2_texture_tiling_type_t` | `FLVER2.Texture.TilingType` | 未实现 | 6 | Texture tiling mode |
| `sf_flver2_mesh_t` | `FLVER2.Mesh` | 未实现 | 6 | FLVER2 geometry mesh |
| `sf_flver2_mesh_bounding_boxes_t` | `FLVER2.Mesh.BoundingBoxes` | 未实现 | 6 | Mesh-specific bounding box |
| `sf_flver2_face_set_t` | `FLVER2.FaceSet` | 未实现 | 6 | Triangle index buffer |
| `sf_flver2_face_set_flags_t` | `FLVER2.FaceSet.FSFlags` | 未实现 | 6 | FaceSet property flags |
| `sf_flver2_vertex_buffer_t` | `FLVER2.VertexBuffer` | 未实现 | 6 | Vertex data block |
| `sf_flver2_buffer_layout_t` | `FLVER2.BufferLayout` | 未实现 | 6 | Vertex layout definition |
| `sf_flver2_skeleton_set_t` | `FLVER2.SkeletonSet` | 未实现 | 6 | Skeleton hierarchy set |
| `sf_flver2_skeleton_bone_t` | `FLVER2.SkeletonSet.Bone` | 未实现 | 6 | Skeleton bone mapping |
| `sf_flver2_edge_vertex_buffer_t` | `FLVER2.EdgeVertexBuffer` | 未实现 | 6 | Edge compressed vertex buffer |
| `sf_flver2_edge_index_buffer_t` | `FLVER2.EdgeIndexBuffer` | 未实现 | 6 | Edge compressed index buffer |
| `sf_flver2_edge_index_group_t` | `FLVER2.EdgeIndexGroup` | 未实现 | 6 | Group of edge index buffers |
| `sf_flver2_edge_geom_spu_config_info_t` | `FLVER2.EdgeGeomSpuConfigInfo` | 未实现 | 6 | SPU edge geometry config |

## Vertex Element Layout (LayoutType)

| Value | Name | Size | Description |
| :--- | :--- | :--- | :--- |
| 0 | `Float1` | 4 | One single-precision float |
| 1 | `Float2` | 8 | Two single-precision floats |
| 2 | `Float3` | 12 | Three single-precision floats |
| 3 | `Float4` | 16 | Four single-precision floats |
| 16 | `Color` | 4 | Four bytes (RGBA/ARGB) |
| 17 | `UByte4` | 4 | Four unsigned bytes |
| 18 | `Byte4` | 4 | Four signed bytes |
| 19 | `UByte4Norm` | 4 | Four unsigned normalized bytes |
| 20 | `Byte4Norm` | 4 | Four signed normalized bytes |
| 21 | `Short2` | 4 | Two signed shorts |
| 22 | `Short4` | 8 | Four signed shorts |
| 23 | `UShort2` | 4 | Two unsigned shorts |
| 24 | `UShort4` | 8 | Four unsigned shorts |
| 26 | `Short4Norm` | 8 | Four signed normalized shorts |
| 45 | `Half2` | 4 | Two half-precision floats |
| 46 | `Half4` | 8 | Four half-precision floats |
| 47 | `Byte4E` | 4 | Unknown (4 bytes) |
| 240 | `EdgeCompressed` | 1 | Edge compression |

## Vertex Format Dispatch (Internal Logic)

| Semantic | LayoutType | Logic / Notes |
| :--- | :--- | :--- |
| `Position` | `Float3`, `Float4` | `Float4` asserts W=0. |
| `BoneWeights` | `Color`, `UByte4Norm`, `Short4`, `Short4Norm` | Normalized to 0.0-1.0. `Short4` has special 0x8000 bias. |
| `BoneIndices` | `UByte4`, `UShort2`, `UShort4`, `Byte4E`, `Byte4` | Direct index values. |
| `Normal` | `Float3`, `Float4`, `Color`, `UByte4`, `Byte4`, `UByte4Norm`, `Short4Norm`, `Half4`, `Byte4E`, `UShort4` | `Float4` W is `NormalW`. `UShort4` uses AC6-specific normalization. |
| `UV` | `Float2`, `Float3`, `Float4`, `Color`, `UByte4`, `Byte4`, `UByte4Norm`, `Short2`, `Half2`, `Short4`, `Half4` | `uvFactor` is 1024 (<0x2000E) or 2048 (>=0x2000E). |
| `Tangent` | `Float4`, `Color`, `UByte4`, `UByte4Norm`, `Byte4Norm`, `Short4Norm`, `Byte4E` | Normalized vectors. |
| `Bitangent` | `Color`, `UByte4`, `UByte4Norm`, `Byte4E` | Normalized vectors. |
| `VertexColor` | `Float4`, `Color`, `UByte4Norm` | `Color` and `UByte4Norm` are typically RGBA. |

## Edge Geometry Enums

### SpuVertexFormat / RsxVertexFormat
- 0: `Float3`
- 1: `Float3PackedNorm2`
- 2: `Float3PackedNormShortNorm4`
- 3: `Float3PackedNorm3`
- 4: `EdgeFixedUnit2`
- 5: `EdgeFixedUnit3`
- 254: `EdgeFixed`
- 255: `Custom`

### EdgeGeomSkin
- 0: `None`
- 1: `NoScaling`
- 2: `UniformScaling`
- 3: `NonUniformScaling`
- 4: `SingleBoneNoScaling`
- 5: `SingleBoneUniformScaling`
- 6: `SingleBoneNonUniformScaling`
