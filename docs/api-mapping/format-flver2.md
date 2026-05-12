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
| `public partial class FLVER2 : SoulsFile<FLVER2>, IFlver` | `FLVER2.cs:11` | Class | `sf_flver2_t` | 已实现 | Phase 6 target. FLVER2 model container |
| `public FLVERHeader Header { get; set; }` | `FLVER2.cs:16` | Property | `sf_flver2_header_*` | 已实现 | Accessors for version, unicode, bbox |
| `public List<FLVER.Dummy> Dummies { get; set; }` | `FLVER2.cs:21` | Property | `sf_flver2_dummy_*` | 已实现 | Count + index accessors |
| `public List<Material> Materials { get; set; }` | `FLVER2.cs:27` | Property | `sf_flver2_material_*` | 已实现 | Count + index accessors |
| `public List<GXList> GXLists { get; set; }` | `FLVER2.cs:33` | Property | `sf_flver2_gx_list` | 已实现 | Accessor by mesh index |
| `public List<FLVER.Node> Nodes { get; set; }` | `FLVER2.cs:38` | Property | `sf_flver2_node_*` | 已实现 | Count + index accessors |
| `public List<Mesh> Meshes { get; set; }` | `FLVER2.cs:44` | Property | `sf_flver2_mesh_*` | 已实现 | Count + index accessors |
| `public List<BufferLayout> BufferLayouts { get; set; }` | `FLVER2.cs:50` | Property | `sf_flver2_buffer_layout_*` | 已实现 | Count + index accessors |
| `public SkeletonSet Skeletons { get; set; }` | `FLVER2.cs:55` | Property | `sf_flver2_skeleton_set` | 已实现 | Accessor for skeleton block |
| `protected override bool Is(BinaryReaderEx br)` | `FLVER2.cs:74` | Method | `n/a` | 已实现 | Internal magic check |
| `protected override void Read(BinaryReaderEx br)` | `FLVER2.cs:89` | Method | `sf_flver2_read_*` | 已实现 | Memory and path variants |
| `protected override void Write(BinaryWriterEx bw)` | `FLVER2.cs:225` | Method | `sf_flver2_write_*` | 已实现 | Memory and path variants |
| `public class FLVERHeader` | `FLVER2.cs:467` | Class | `sf_flver2_header_*` | 已实现 | Flattened into top-level accessors |
| `public class GXList : List<GXItem>` | `GXList.cs:11` | Class | `sf_flver2_gx_list_t` | 已实现 | Phase 6 target. Collection of GX items |
| `public class GXItem` | `GXList.cs:77` | Class | `sf_flver2_gx_item_t` | 已实现 | Phase 6 target. Material rendering parameter |
| `public class Material : IFlverMaterial` | `Material.cs:11` | Class | `sf_flver2_material_t` | 已实现 | Phase 6 target. FLVER2 material |
| `public class Texture : IFlverTexture` | `Texture.cs:11` | Class | `sf_flver2_texture_t` | 已实现 | Phase 6 target. FLVER2 texture reference |
| `public enum TilingType : byte` | `Texture.cs:13` | Enum | `sf_flver2_tiling_type_t` | 已实现 | Phase 6 target. Texture tiling mode |
| `public class Mesh : IFlverMesh` | `Mesh.cs:13` | Class | `sf_flver2_mesh_t` | 已实现 | Phase 6 target. FLVER2 geometry mesh |
| `public class BoundingBoxes` | `Mesh.cs:269` | Class | `n/a` | `_skipped_` | Mesh-specific bounding box deferred to v2 |
| `public partial class FaceSet` | `FaceSet.cs:13` | Class | `sf_flver2_face_set_t` | 已实现 | Phase 6 target. Triangle index buffer |
| `public enum FSFlags : uint` | `FaceSet.cs:19` | Enum | `sf_flver2_fs_flags_t` | 已实现 | Phase 6 target. FaceSet property flags |
| `public class VertexBuffer` | `VertexBuffer.cs:11` | Class | `sf_flver2_vertex_buffer_t` | 已实现 | Phase 6 target. Vertex data block |
| `public class BufferLayout : List<FLVER.LayoutMember>` | `BufferLayout.cs:12` | Class | `sf_flver2_buffer_layout_t` | 已实现 | Phase 6 target. Vertex layout definition |
| `public class SkeletonSet` | `SkeletonSet.cs:10` | Class | `sf_flver2_skeleton_set_t` | 已实现 | Phase 6 target. Skeleton hierarchy set |
| `public class Bone` | `SkeletonSet.cs:84` | Class | `sf_flver2_bone_t` | 已实现 | Phase 6 target. Skeleton bone mapping |
| `internal class EdgeVertexBuffer` | `EdgeVertexBuffer.cs:12` | Class | `n/a` | `_skipped_` | v1 OUT-of-scope |
| `internal class EdgeIndexBuffer` | `EdgeIndexBuffer.cs:11` | Class | `n/a` | `_skipped_` | v1 OUT-of-scope |
| `internal class EdgeIndexGroup` | `EdgeIndexGroup.cs:10` | Class | `n/a` | `_skipped_` | v1 OUT-of-scope |
| `public struct EdgeGeomSpuConfigInfo` | `EdgeGeomSpuConfigInfo.cs:11` | Struct | `n/a` | `_skipped_` | v1 OUT-of-scope |

## Vertex Element Layout (LayoutType)

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `Float1 = 0` | `LayoutMember.cs:159` | Enum Value | `SF_FLVER_LAYOUT_TYPE_FLOAT1` | 已实现 | Size: 4. One single-precision float |
| `Float2 = 1` | `LayoutMember.cs:164` | Enum Value | `SF_FLVER_LAYOUT_TYPE_FLOAT2` | 已实现 | Size: 8. Two single-precision floats |
| `Float3 = 2` | `LayoutMember.cs:169` | Enum Value | `SF_FLVER_LAYOUT_TYPE_FLOAT3` | 已实现 | Size: 12. Three single-precision floats |
| `Float4 = 3` | `LayoutMember.cs:174` | Enum Value | `SF_FLVER_LAYOUT_TYPE_FLOAT4` | 已实现 | Size: 16. Four single-precision floats |
| `Color = 16` | `LayoutMember.cs:179` | Enum Value | `SF_FLVER_LAYOUT_TYPE_COLOR` | 已实现 | Size: 4. Four bytes (RGBA/ARGB) |
| `UByte4 = 17` | `LayoutMember.cs:184` | Enum Value | `SF_FLVER_LAYOUT_TYPE_UBYTE4` | 已实现 | Size: 4. Four unsigned bytes |
| `Byte4 = 18` | `LayoutMember.cs:189` | Enum Value | `SF_FLVER_LAYOUT_TYPE_BYTE4` | 已实现 | Size: 4. Four signed bytes |
| `UByte4Norm = 19` | `LayoutMember.cs:194` | Enum Value | `SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM` | 已实现 | Size: 4. Four unsigned normalized bytes |
| `Byte4Norm = 20` | `LayoutMember.cs:199` | Enum Value | `SF_FLVER_LAYOUT_TYPE_BYTE4_NORM` | 已实现 | Size: 4. Four signed normalized bytes |
| `Short2 = 21` | `LayoutMember.cs:204` | Enum Value | `SF_FLVER_LAYOUT_TYPE_SHORT2` | 已实现 | Size: 4. Two signed shorts |
| `Short4 = 22` | `LayoutMember.cs:209` | Enum Value | `SF_FLVER_LAYOUT_TYPE_SHORT4` | 已实现 | Size: 8. Four signed shorts |
| `UShort2 = 23` | `LayoutMember.cs:214` | Enum Value | `SF_FLVER_LAYOUT_TYPE_USHORT2` | 已实现 | Size: 4. Two unsigned shorts |
| `UShort4 = 24` | `LayoutMember.cs:219` | Enum Value | `SF_FLVER_LAYOUT_TYPE_USHORT4` | 已实现 | Size: 8. Four unsigned shorts |
| `Short4Norm = 26` | `LayoutMember.cs:224` | Enum Value | `SF_FLVER_LAYOUT_TYPE_SHORT4_NORM` | 已实现 | Size: 8. Four signed normalized shorts |
| `Half2 = 45` | `LayoutMember.cs:229` | Enum Value | `SF_FLVER_LAYOUT_TYPE_HALF2` | 已实现 | Size: 4. Two half-precision floats |
| `Half4 = 46` | `LayoutMember.cs:234` | Enum Value | `SF_FLVER_LAYOUT_TYPE_HALF4` | 已实现 | Size: 8. Four half-precision floats |
| `Byte4E = 47` | `LayoutMember.cs:239` | Enum Value | `SF_FLVER_LAYOUT_TYPE_BYTE4E` | 已实现 | Size: 4. Unknown (4 bytes) |
| `EdgeCompressed = 240` | `LayoutMember.cs:244` | Enum Value | `SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED` | 已实现 | Size: 1. Edge compression |

## Vertex Format Dispatch (Internal Logic)

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `Position` | `Vertex.cs:122` | Logic | `sf_flver2_decode_mesh` | 已实现 | `Float3`, `Float4`. `Float4` asserts W=0. |
| `BoneWeights` | `Vertex.cs:140` | Logic | `sf_flver2_decode_mesh` | 已实现 | `Color`, `UByte4Norm`, `Short4`, `Short4Norm`. Normalized to 0.0-1.0. `Short4` has special 0x8000 bias. |
| `BoneIndices` | `Vertex.cs:176` | Logic | `sf_flver2_decode_mesh` | 已实现 | `UByte4`, `UShort2`, `UShort4`, `Byte4E`, `Byte4`. Direct index values. |
| `Normal` | `Vertex.cs:206` | Logic | `sf_flver2_decode_mesh` | 已实现 | `Float3`, `Float4`, `Color`, `UByte4`, `Byte4`, `UByte4Norm`, `Short4Norm`, `Half4`, `Byte4E`, `UShort4`. `Float4` W is `NormalW`. `UShort4` uses AC6-specific normalization. |
| `UV` | `Vertex.cs:263` | Logic | `sf_flver2_decode_mesh` | 已实现 | `Float2`, `Float3`, `Float4`, `Color`, `UByte4`, `Byte4`, `UByte4Norm`, `Short2`, `Half2`, `Short4`, `Half4`. `uvFactor` is 1024 (<0x2000E) or 2048 (>=0x2000E). |
| `Tangent` | `Vertex.cs:316` | Logic | `sf_flver2_decode_mesh` | 已实现 | `Float4`, `Color`, `UByte4`, `UByte4Norm`, `Byte4Norm`, `Short4Norm`, `Byte4E`. Normalized vectors. |
| `Bitangent` | `Vertex.cs:349` | Logic | `sf_flver2_decode_mesh` | 已实现 | `Color`, `UByte4`, `UByte4Norm`, `Byte4E`. Normalized vectors. |
| `VertexColor` | `Vertex.cs:370` | Logic | `sf_flver2_decode_mesh` | 已实现 | `Float4`, `Color`, `UByte4Norm`. `Color` and `UByte4Norm` are typically RGBA. |

## Edge Geometry Enums

Edge / SPU / RSX 子表 20 行均 v1 OUT-of-scope，标 `_skipped_`。
FLVER2 主表、Vertex Element Layout 与 Vertex Format Dispatch 均已实现。
Edge 历史压缩/皮肤路径本身也暂未实现。

### SpuVertexFormat

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `Float3 = 0` | `EdgeGeomSpuConfigInfo.cs:130` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_FLOAT3` | `_skipped_` | Position in 3 floats. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `Float3PackedNorm2 = 1` | `EdgeGeomSpuConfigInfo.cs:137` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_FLOAT3_PACKED_NORM2` | `_skipped_` | Position in 3 floats. Normal/Tangent packed. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `Float3PackedNormShortNorm4 = 2` | `EdgeGeomSpuConfigInfo.cs:144` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_FLOAT3_PACKED_NORM_SHORT_NORM4` | `_skipped_` | Position in 3 floats. Normal packed, Tangent short4. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `Float3PackedNorm3 = 3` | `EdgeGeomSpuConfigInfo.cs:152` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_FLOAT3_PACKED_NORM3` | `_skipped_` | Position in 3 floats. Normal/Tangent/BiNormal packed. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `EdgeFixedUnit2 = 4` | `EdgeGeomSpuConfigInfo.cs:159` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_EDGE_FIXED_UNIT2` | `_skipped_` | Position fixed, Normal/Tangent unit. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `EdgeFixedUnit3 = 5` | `EdgeGeomSpuConfigInfo.cs:167` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_EDGE_FIXED_UNIT3` | `_skipped_` | Position fixed, Normal/Tangent/BiNormal unit. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `EdgeFixed = 254` | `EdgeGeomSpuConfigInfo.cs:173` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_EDGE_FIXED` | `_skipped_` | Position as an edge fixed point. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `Custom = 255` | `EdgeGeomSpuConfigInfo.cs:178` | Enum Value | `SF_FLVER2_SPU_VERTEX_FORMAT_CUSTOM` | `_skipped_` | A user defined format. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |

### RsxVertexFormat

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `Float3 = 0` | `EdgeGeomSpuConfigInfo.cs:189` | Enum Value | `SF_FLVER2_RSX_VERTEX_FORMAT_FLOAT3` | `_skipped_` | Position in 3 floats. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `Float3PackedNorm2 = 1` | `EdgeGeomSpuConfigInfo.cs:196` | Enum Value | `SF_FLVER2_RSX_VERTEX_FORMAT_FLOAT3_PACKED_NORM2` | `_skipped_` | Position in 3 floats. Normal/Tangent packed. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `Float3PackedNormShortNorm4 = 2` | `EdgeGeomSpuConfigInfo.cs:203` | Enum Value | `SF_FLVER2_RSX_VERTEX_FORMAT_FLOAT3_PACKED_NORM_SHORT_NORM4` | `_skipped_` | Position in 3 floats. Normal packed, Tangent short4. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `Float3PackedNorm3 = 3` | `EdgeGeomSpuConfigInfo.cs:211` | Enum Value | `SF_FLVER2_RSX_VERTEX_FORMAT_FLOAT3_PACKED_NORM3` | `_skipped_` | Position in 3 floats. Normal/Tangent/BiNormal packed. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `Custom = 255` | `EdgeGeomSpuConfigInfo.cs:216` | Enum Value | `SF_FLVER2_RSX_VERTEX_FORMAT_CUSTOM` | `_skipped_` | A user defined format. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |

### EdgeGeomSkin

| Upstream signature | Upstream loc (File.cs:LINE) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `None = 0` | `EdgeGeomSpuConfigInfo.cs:66` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_NONE` | `_skipped_` | No skinning. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `NoScaling = 1` | `EdgeGeomSpuConfigInfo.cs:71` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_NO_SCALING` | `_skipped_` | Do skinning by unit matrix. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `UniformScaling = 2` | `EdgeGeomSpuConfigInfo.cs:76` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_UNIFORM_SCALING` | `_skipped_` | Do skinning. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `NonUniformScaling = 3` | `EdgeGeomSpuConfigInfo.cs:81` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_NON_UNIFORM_SCALING` | `_skipped_` | Do skinning and compute cofactor matrices. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `SingleBoneNoScaling = 4` | `EdgeGeomSpuConfigInfo.cs:86` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_SINGLE_BONE_NO_SCALING` | `_skipped_` | Do skinning by a single bone unit matrix. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `SingleBoneUniformScaling = 5` | `EdgeGeomSpuConfigInfo.cs:91` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_SINGLE_BONE_UNIFORM_SCALING` | `_skipped_` | Do skinning by a single bone. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
| `SingleBoneNonUniformScaling = 6` | `EdgeGeomSpuConfigInfo.cs:96` | Enum Value | `SF_FLVER2_EDGE_GEOM_SKIN_SINGLE_BONE_NON_UNIFORM_SCALING` | `_skipped_` | Do skinning by a single bone and compute cofactor matrices. v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md |
