# FLVER2 Mapping

## Contributing Files

| File | Path |
| :--- | :--- |
| `FLVER2.cs` | `SoulsFormats/Formats/FLVER/FLVER2/FLVER2.cs` |
| `GXList.cs` | `SoulsFormats/Formats/FLVER/FLVER2/GXList.cs` |
| `Material.cs` | `SoulsFormats/Formats/FLVER/FLVER2/Material.cs` |
| `Texture.cs` | `SoulsFormats/Formats/FLVER/FLVER2/Texture.cs` |
| `Mesh.cs` | `SoulsFormats/Formats/FLVER/FLVER2/Mesh.cs` |
| `FaceSet.cs` | `SoulsFormats/Formats/FLVER/FLVER2/FaceSet.cs` |
| `VertexBuffer.cs` | `SoulsFormats/Formats/FLVER/FLVER2/VertexBuffer.cs` |
| `BufferLayout.cs` | `SoulsFormats/Formats/FLVER/FLVER2/BufferLayout.cs` |
| `SkeletonSet.cs` | `SoulsFormats/Formats/FLVER/FLVER2/SkeletonSet.cs` |
| `EdgeVertexBuffer.cs` | `SoulsFormats/Formats/FLVER/FLVER2/EdgeVertexBuffer.cs` |
| `EdgeIndexBuffer.cs` | `SoulsFormats/Formats/FLVER/FLVER2/EdgeIndexBuffer.cs` |
| `EdgeIndexGroup.cs` | `SoulsFormats/Formats/FLVER/FLVER2/EdgeIndexGroup.cs` |
| `EdgeGeomSpuConfigInfo.cs` | `SoulsFormats/Formats/FLVER/FLVER2/EdgeGeomSpuConfigInfo.cs` |
| `LayoutMember.cs` | `SoulsFormats/Formats/FLVER/LayoutMember.cs` |
| `Vertex.cs` | `SoulsFormats/Formats/FLVER/Vertex.cs` |

## API Mapping

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public partial class FLVER2 : SoulsFile<FLVER2>, IFlver` | `FLVER2.cs:11` | Class | `sf_flver2_t` | 未实现 | Phase 6 target. FLVER2 model container |
| `public FLVERHeader Header { get; set; }` | `FLVER2.cs:16` | Property | 未实现 | 未实现 | |
| `public List<FLVER.Dummy> Dummies { get; set; }` | `FLVER2.cs:21` | Property | 未实现 | 未实现 | |
| `public List<Material> Materials { get; set; }` | `FLVER2.cs:27` | Property | 未实现 | 未实现 | |
| `public List<GXList> GXLists { get; set; }` | `FLVER2.cs:33` | Property | 未实现 | 未实现 | |
| `public List<FLVER.Node> Nodes { get; set; }` | `FLVER2.cs:38` | Property | 未实现 | 未实现 | |
| `public List<Mesh> Meshes { get; set; }` | `FLVER2.cs:44` | Property | 未实现 | 未实现 | |
| `public List<BufferLayout> BufferLayouts { get; set; }` | `FLVER2.cs:50` | Property | 未实现 | 未实现 | |
| `public SkeletonSet Skeletons { get; set; }` | `FLVER2.cs:55` | Property | 未实现 | 未实现 | |
| `protected override bool Is(BinaryReaderEx br)` | `FLVER2.cs:74` | Method | 未实现 | 未实现 | |
| `protected override void Read(BinaryReaderEx br)` | `FLVER2.cs:89` | Method | 未实现 | 未实现 | |
| `protected override void Write(BinaryWriterEx bw)` | `FLVER2.cs:225` | Method | 未实现 | 未实现 | |
| `public class FLVERHeader` | `FLVER2.cs:467` | Class | `sf_flver2_header_t` | 未实现 | Phase 6 target. FLVER2 file header |
| `public class GXList : List<GXItem>` | `GXList.cs:11` | Class | `sf_flver2_gx_list_t` | 未实现 | Phase 6 target. Collection of GX items |
| `public class GXItem` | `GXList.cs:77` | Class | `sf_flver2_gx_item_t` | 未实现 | Phase 6 target. Material rendering parameter |
| `public class Material : IFlverMaterial` | `Material.cs:11` | Class | `sf_flver2_material_t` | 未实现 | Phase 6 target. FLVER2 material |
| `public class Texture : IFlverTexture` | `Texture.cs:11` | Class | `sf_flver2_texture_t` | 未实现 | Phase 6 target. FLVER2 texture reference |
| `public enum TilingType : byte` | `Texture.cs:13` | Enum | `sf_flver2_texture_tiling_type_t` | 未实现 | Phase 6 target. Texture tiling mode |
| `public class Mesh : IFlverMesh` | `Mesh.cs:13` | Class | `sf_flver2_mesh_t` | 未实现 | Phase 6 target. FLVER2 geometry mesh |
| `public class BoundingBoxes` | `Mesh.cs:269` | Class | `sf_flver2_mesh_bounding_boxes_t` | 未实现 | Phase 6 target. Mesh-specific bounding box |
| `public partial class FaceSet` | `FaceSet.cs:13` | Class | `sf_flver2_face_set_t` | 未实现 | Phase 6 target. Triangle index buffer |
| `public enum FSFlags : uint` | `FaceSet.cs:19` | Enum | `sf_flver2_face_set_flags_t` | 未实现 | Phase 6 target. FaceSet property flags |
| `public class VertexBuffer` | `VertexBuffer.cs:11` | Class | `sf_flver2_vertex_buffer_t` | 未实现 | Phase 6 target. Vertex data block |
| `public class BufferLayout : List<FLVER.LayoutMember>` | `BufferLayout.cs:12` | Class | `sf_flver2_buffer_layout_t` | 未实现 | Phase 6 target. Vertex layout definition |
| `public class SkeletonSet` | `SkeletonSet.cs:10` | Class | `sf_flver2_skeleton_set_t` | 未实现 | Phase 6 target. Skeleton hierarchy set |
| `public class Bone` | `SkeletonSet.cs:84` | Class | `sf_flver2_skeleton_bone_t` | 未实现 | Phase 6 target. Skeleton bone mapping |
| `internal class EdgeVertexBuffer` | `EdgeVertexBuffer.cs:12` | Class | `sf_flver2_edge_vertex_buffer_t` | 未实现 | Phase 6 target. Edge compressed vertex buffer |
| `internal class EdgeIndexBuffer` | `EdgeIndexBuffer.cs:11` | Class | `sf_flver2_edge_index_buffer_t` | 未实现 | Phase 6 target. Edge compressed index buffer |
| `internal class EdgeIndexGroup` | `EdgeIndexGroup.cs:10` | Class | `sf_flver2_edge_index_group_t` | 未实现 | Phase 6 target. Group of edge index buffers |
| `public struct EdgeGeomSpuConfigInfo` | `EdgeGeomSpuConfigInfo.cs:11` | Struct | `sf_flver2_edge_geom_spu_config_info_t` | 未实现 | Phase 6 target. SPU edge geometry config |

## Vertex Element Layout (LayoutType)

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `Float1 = 0` | `LayoutMember.cs:159` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_FLOAT1` | 未实现 | Size: 4. One single-precision float |
| `Float2 = 1` | `LayoutMember.cs:164` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_FLOAT2` | 未实现 | Size: 8. Two single-precision floats |
| `Float3 = 2` | `LayoutMember.cs:169` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_FLOAT3` | 未实现 | Size: 12. Three single-precision floats |
| `Float4 = 3` | `LayoutMember.cs:174` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_FLOAT4` | 未实现 | Size: 16. Four single-precision floats |
| `Color = 16` | `LayoutMember.cs:179` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_COLOR` | 未实现 | Size: 4. Four bytes (RGBA/ARGB) |
| `UByte4 = 17` | `LayoutMember.cs:184` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_UBYTE4` | 未实现 | Size: 4. Four unsigned bytes |
| `Byte4 = 18` | `LayoutMember.cs:189` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_BYTE4` | 未实现 | Size: 4. Four signed bytes |
| `UByte4Norm = 19` | `LayoutMember.cs:194` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_UBYTE4_NORM` | 未实现 | Size: 4. Four unsigned normalized bytes |
| `Byte4Norm = 20` | `LayoutMember.cs:199` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_BYTE4_NORM` | 未实现 | Size: 4. Four signed normalized bytes |
| `Short2 = 21` | `LayoutMember.cs:204` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_SHORT2` | 未实现 | Size: 4. Two signed shorts |
| `Short4 = 22` | `LayoutMember.cs:209` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_SHORT4` | 未实现 | Size: 8. Four signed shorts |
| `UShort2 = 23` | `LayoutMember.cs:214` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_USHORT2` | 未实现 | Size: 4. Two unsigned shorts |
| `UShort4 = 24` | `LayoutMember.cs:219` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_USHORT4` | 未实现 | Size: 8. Four unsigned shorts |
| `Short4Norm = 26` | `LayoutMember.cs:224` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_SHORT4_NORM` | 未实现 | Size: 8. Four signed normalized shorts |
| `Half2 = 45` | `LayoutMember.cs:229` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_HALF2` | 未实现 | Size: 4. Two half-precision floats |
| `Half4 = 46` | `LayoutMember.cs:234` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_HALF4` | 未实现 | Size: 8. Four half-precision floats |
| `Byte4E = 47` | `LayoutMember.cs:239` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_BYTE4E` | 未实现 | Size: 4. Unknown (4 bytes) |
| `EdgeCompressed = 240` | `LayoutMember.cs:244` | Enum Value | `SF_FLVER2_LAYOUT_TYPE_EDGE_COMPRESSED` | 未实现 | Size: 1. Edge compression |

## Vertex Format Dispatch (Internal Logic)

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `Position` | `Vertex.cs:122` | Logic | 未实现 | 未实现 | `Float3`, `Float4`. `Float4` asserts W=0. |
| `BoneWeights` | `Vertex.cs:140` | Logic | 未实现 | 未实现 | `Color`, `UByte4Norm`, `Short4`, `Short4Norm`. Normalized to 0.0-1.0. `Short4` has special 0x8000 bias. |
| `BoneIndices` | `Vertex.cs:176` | Logic | 未实现 | 未实现 | `UByte4`, `UShort2`, `UShort4`, `Byte4E`, `Byte4`. Direct index values. |
| `Normal` | `Vertex.cs:206` | Logic | 未实现 | 未实现 | `Float3`, `Float4`, `Color`, `UByte4`, `Byte4`, `UByte4Norm`, `Short4Norm`, `Half4`, `Byte4E`, `UShort4`. `Float4` W is `NormalW`. `UShort4` uses AC6-specific normalization. |
| `UV` | `Vertex.cs:263` | Logic | 未实现 | 未实现 | `Float2`, `Float3`, `Float4`, `Color`, `UByte4`, `Byte4`, `UByte4Norm`, `Short2`, `Half2`, `Short4`, `Half4`. `uvFactor` is 1024 (<0x2000E) or 2048 (>=0x2000E). |
| `Tangent` | `Vertex.cs:316` | Logic | 未实现 | 未实现 | `Float4`, `Color`, `UByte4`, `UByte4Norm`, `Byte4Norm`, `Short4Norm`, `Byte4E`. Normalized vectors. |
| `Bitangent` | `Vertex.cs:349` | Logic | 未实现 | 未实现 | `Color`, `UByte4`, `UByte4Norm`, `Byte4E`. Normalized vectors. |
| `VertexColor` | `Vertex.cs:370` | Logic | 未实现 | 未实现 | `Float4`, `Color`, `UByte4Norm`. `Color` and `UByte4Norm` are typically RGBA. |

## Edge Geometry Enums

### SpuVertexFormat

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `Float3 = 0` | `EdgeGeomSpuConfigInfo.cs:130` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_FLOAT3` | 未实现 | Position in 3 floats. |
| `Float3PackedNorm2 = 1` | `EdgeGeomSpuConfigInfo.cs:137` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_FLOAT3_PACKED_NORM2` | 未实现 | Position in 3 floats. Normal/Tangent packed. |
| `Float3PackedNormShortNorm4 = 2` | `EdgeGeomSpuConfigInfo.cs:144` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_FLOAT3_PACKED_NORM_SHORT_NORM4` | 未实现 | Position in 3 floats. Normal packed, Tangent short4. |
| `Float3PackedNorm3 = 3` | `EdgeGeomSpuConfigInfo.cs:152` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_FLOAT3_PACKED_NORM3` | 未实现 | Position in 3 floats. Normal/Tangent/BiNormal packed. |
| `EdgeFixedUnit2 = 4` | `EdgeGeomSpuConfigInfo.cs:159` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_EDGE_FIXED_UNIT2` | 未实现 | Position fixed, Normal/Tangent unit. |
| `EdgeFixedUnit3 = 5` | `EdgeGeomSpuConfigInfo.cs:167` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_EDGE_FIXED_UNIT3` | 未实现 | Position fixed, Normal/Tangent/BiNormal unit. |
| `EdgeFixed = 254` | `EdgeGeomSpuConfigInfo.cs:173` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_EDGE_FIXED` | 未实现 | Position as an edge fixed point. |
| `Custom = 255` | `EdgeGeomSpuConfigInfo.cs:178` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_CUSTOM` | 未实现 | A user defined format. |

### RsxVertexFormat

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `Float3 = 0` | `EdgeGeomSpuConfigInfo.cs:189` | Enum Value | `SF_FLVER2_RSX_VERTEX_FORMAT_FLOAT3` | 未实现 | Position in 3 floats. |
| `Float3PackedNorm2 = 1` | `EdgeGeomSpuConfigInfo.cs:196` | Enum Value | `SF_FLVER2_RSX_VERTEX_FORMAT_FLOAT3_PACKED_NORM2` | 未实现 | Position in 3 floats. Normal/Tangent packed. |
| `Float3PackedNormShortNorm4 = 2` | `EdgeGeomSpuConfigInfo.cs:203` | Enum Value | `SF_FLVER2_RSX_VERTEX_FORMAT_FLOAT3_PACKED_NORM_SHORT_NORM4` | 未实现 | Position in 3 floats. Normal packed, Tangent short4. |
| `Float3PackedNorm3 = 3` | `EdgeGeomSpuConfigInfo.cs:211` | Enum Value | `SF_FLVER2_RSX_VERTEX_FORMAT_FLOAT3_PACKED_NORM3` | 未实现 | Position in 3 floats. Normal/Tangent/BiNormal packed. |
| `Custom = 255` | `EdgeGeomSpuConfigInfo.cs:216` | Enum Value | `SF_FLVER2_RSX_VERTEX_FORMAT_CUSTOM` | 未实现 | A user defined format. |

### EdgeGeomSkin

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `None = 0` | `EdgeGeomSpuConfigInfo.cs:66` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_NONE` | 未实现 | No skinning. |
| `NoScaling = 1` | `EdgeGeomSpuConfigInfo.cs:71` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_NO_SCALING` | 未实现 | Do skinning by unit matrix. |
| `UniformScaling = 2` | `EdgeGeomSpuConfigInfo.cs:76` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_UNIFORM_SCALING` | 未实现 | Do skinning. |
| `NonUniformScaling = 3` | `EdgeGeomSpuConfigInfo.cs:81` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_NON_UNIFORM_SCALING` | 未实现 | Do skinning and compute cofactor matrices. |
| `SingleBoneNoScaling = 4` | `EdgeGeomSpuConfigInfo.cs:86` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_SINGLE_BONE_NO_SCALING` | 未实现 | Do skinning by a single bone unit matrix. |
| `SingleBoneUniformScaling = 5` | `EdgeGeomSpuConfigInfo.cs:91` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_SINGLE_BONE_UNIFORM_SCALING` | 未实现 | Do skinning by a single bone. |
| `SingleBoneNonUniformScaling = 6` | `EdgeGeomSpuConfigInfo.cs:96` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_SINGLE_BONE_NON_UNIFORM_SCALING` | 未实现 | Do skinning by a single bone and compute cofactor matrices. |
