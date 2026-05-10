# FLVER Common Mapping

| C Symbol | Upstream C# Symbol | Status | Phase | Description |
| :--- | :--- | :--- | :--- | :--- |
| `sf_flver_dummy_t` | `FLVER.Dummy` | 未实现 | 6 | Dummy polygon point |
| `sf_flver_dummy_create` | `new FLVER.Dummy()` | 未实现 | 6 | Allocate a dummy point |
| `sf_flver_dummy_destroy` | `n/a` | 未实现 | 6 | Free a dummy point |
| `sf_flver_dummy_read` | `internal Dummy(BinaryReaderEx br, int version)` | 未实现 | 6 | Read dummy from stream |
| `sf_flver_dummy_write` | `internal void Write(BinaryWriterEx bw, int version)` | 未实现 | 6 | Write dummy to stream |
| `sf_flver_node_t` | `FLVER.Node` | 未实现 | 6 | Transform node / bone |
| `sf_flver_node_flags_t` | `FLVER.Node.NodeFlags` | 未实现 | 6 | Node property flags |
| `sf_flver_node_create` | `new FLVER.Node()` | 未实现 | 6 | Allocate a node |
| `sf_flver_node_destroy` | `n/a` | 未实现 | 6 | Free a node |
| `sf_flver_node_read` | `internal Node(BinaryReaderEx br, bool unicode)` | 未实现 | 6 | Read node from stream |
| `sf_flver_node_write` | `internal void Write(BinaryWriterEx bw, int index)` | 未实现 | 6 | Write node to stream |
| `sf_flver_node_compute_local_transform` | `public Matrix4x4 ComputeLocalTransform()` | 未实现 | 6 | Compute 4x4 local matrix |
| `sf_flver_vertex_t` | `FLVER.Vertex` | 未实现 | 6 | Single mesh vertex |
| `sf_flver_vertex_create` | `new FLVER.Vertex(...)` | 未实现 | 6 | Allocate a vertex |
| `sf_flver_vertex_destroy` | `n/a` | 未实现 | 6 | Free a vertex |
| `sf_flver_vertex_read` | `internal void Read(BinaryReaderEx br, List<LayoutMember> layout, float uvFactor)` | 未实现 | 6 | Read vertex from stream |
| `sf_flver_vertex_write` | `internal void Write(BinaryWriterEx bw, List<LayoutMember> layout, float uvFactor)` | 未实现 | 6 | Write vertex to stream |
| `sf_flver_layout_member_t` | `FLVER.LayoutMember` | 未实现 | 6 | Vertex property definition |
| `sf_flver_layout_type_t` | `FLVER.LayoutType` | 未实现 | 6 | Vertex property format |
| `sf_flver_layout_semantic_t` | `FLVER.LayoutSemantic` | 未实现 | 6 | Vertex property semantic |
| `sf_flver_vertex_color_t` | `FLVER.VertexColor` | 未实现 | 6 | ARGB vertex color |
| `sf_flver_vertex_bone_indices_t` | `FLVER.VertexBoneIndices` | 未实现 | 6 | 4-bone indices |
| `sf_flver_vertex_bone_weights_t` | `FLVER.VertexBoneWeights` | 未实现 | 6 | 4-bone weights |
| `sf_flver_i_flver_t` | `IFlver` | 未实现 | 6 | Common FLVER interface |
| `sf_flver_i_material_t` | `IFlverMaterial` | 未实现 | 6 | Common material interface |
| `sf_flver_i_texture_t` | `IFlverTexture` | 未实现 | 6 | Common texture interface |
| `sf_flver_i_mesh_t` | `IFlverMesh` | 未实现 | 6 | Common mesh interface |
