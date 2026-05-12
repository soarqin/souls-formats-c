# Phase 6 — 几何与材质（FLVER common + FLVER2 + MTD + MATBIN）

> **状态**：⏳ 待执行 · **预估**：~3 周 · **依赖**：Phase 1-5 全部完成
>
> **策略**：先把 Phase 5 状态切换并锁定 Edge geometry OUT-of-scope（Wave 0），再奠定 FLVER common 层（half-float / 11_11_10 / Dummy / Node / LayoutMember）与 4 个公共头（Wave 1），随后并行展开 FLVER2 6 个子模块（Wave 2）、MTD + MATBIN（Wave 3），再 decode helper + 合成 + e2e（Wave 4），文档收尾（Wave 5），4 个 reviewer 并行（Wave Final）。
>
> **绑定**：本计划严格遵守 [AGENTS.md](../../AGENTS.md) §5.x「STRICT UPSTREAM REFERENCE / API MIRRORS UPSTREAM」。
> 上游锁定提交：`9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a`（见 `docs/api-mapping/UPSTREAM.md`）。

---

## TL;DR

> **核心目标**：实现 FLVER common（公共骨架 + 顶点辅助）、FLVER2（现代网格：Sekiro / ER / Nightreign / AC6）、MTD（Sekiro 材质）、MATBIN（ER / AC6 材质）的读 + 写双向支持，对齐上游 net9.0 分支语义；并提供 `sf_flver2_decode_mesh` 扩展 API 把 packed 顶点字节展开为 typed 数组。
>
> **交付物**：
> - `sf_flver.h` + `src/geom/flver_common.c`：half-float / 11_11_10 / Dummy / Node / LayoutMember / 共享顶点辅助。
> - `sf_flver2.h` + `src/geom/flver2.c` + 5 个子模块：FLVER2 完整读写。
> - `src/geom/flver2_vertex.c`：mirror 上游 Vertex.cs 的 foreach + semantic-first if/else dispatch（**非静态表**）。
> - `sf_flver2_decode_mesh` 扩展 API + `docs/api-mapping/extensions.md` 录入。
> - `sf_mtd.h` + `src/geom/mtd.c`：MTD 读 + 写（含 Sekiro Extended texture info）。
> - `sf_matbin.h` + `src/geom/matbin.c`：MATBIN 读 + 写（含 8 个 ParamType 变体）。
> - 单元测试：每个格式合成 round-trip fixture。
> - e2e 测试：FLVER2 / MATBIN via ER；MTD via Sekiro（SKIP-allowed）。
> - 文档：4 份 api-mapping md 全量刷新 + extensions.md 增补；PLAN.md / AGENTS.md / roadmap 状态切换。
>
> **预估工作量**：~3 周（上游 in-scope ~4666 LOC C# → C；Edge geometry 694 LOC 推 v2）。
> **并行执行**：是 —— 6 个 wave（Wave 0-5 + Final），最宽 wave 8 个并行 task（Wave 4）。**总计 31 个实施 task + 4 个 Final reviewer = 35 个 task-level checkbox**（T1-T30 + T27b，T27b 与 T27 平行的 Sekiro MTD e2e SKIP-allowed task）。
> **关键路径**：T4 (vertex layout probe) → T7 (sf_flver.h) → T8 (flver_common.c) → T9 (sf_flver2.h) → T12 (flver2.c top) → T17 (flver2_vertex.c) → T21 (decode_mesh) → T26 (FLVER2 e2e) → F1-F4 → 用户验收。

---

## Context

### 原始请求

> 「规划第6阶段的计划」

### 当前项目状态（2026-05-12）

- Phase 0-5 ✅ 已完成（4/5/13/32/20/38 测试 PASS；详 PLAN.md §7）。
- Phase 6 ⏳ pending —— 本 plan 范围。
- Phase 7 ⏳ optional / v1.1 —— 本 plan 不动。

### 访谈结论

| 决策点 | 用户确认值 |
|---|---|
| Phase 5 状态 | ✅ 已完成（user okay 过）；Phase 6 Wave 0 须切换状态表 |
| Edge Geometry / SPU / RSX 顶点格式 | **OUT-of-scope** → 推 v2 legacy（PS3-era console-specific，v1 4 款目标游戏均不用） |
| `sf_flver2_decode_mesh` 顶点解码 helper | **纳入 must-have**（消费者使用 friendly） |
| MTD e2e 策略 | 合成 fixture 强制 + Sekiro e2e **SKIP-allowed**（沿用 PLAN.md §8.6） |

### Metis 复核要点（已纳入下方设计）

1. **顶点 dispatch 结构**：上游 Vertex.cs 是 `foreach (LayoutMember m in layout)` + 内部 semantic-first if/else 阶梯（`Vertex.cs:112-390` 读、`447-743` 写）。**不是静态 (Type × Semantic × Index) 表**。C 端必须 mirror 这个结构，**不要**优化为 hash 表 / 函数指针表（破坏 upstream alignment）。
2. **GXItem.Data 是 opaque `byte[]`**：上游不结构化解析；C 端必须 opaque `uint8_t*` + size 透传（不要尝试结构化）。
3. **MATBIN ParamType 8 个变体**：Bool / Int / Int2 / Float / Float2 / Float3 / Float4 / Float5。**无 String、无 Vec3**。原 roadmap 描述错误。
4. **FaceSet 三角条带重启符号**：仅 `0xFFFF`（u16），32-bit 索引路径无 restart 概念。
5. **SkeletonSet 无 v1/v2 hierarchy 分支**：简单 `Version >= 0x2001A` 门控 + 双 `List<Bone>`（**`BaseSkeleton` + `AllSkeletons`**，上游 `SkeletonSet.cs:12-20`）。Sekiro 头 `0x20013` 无 SkeletonSet；ER + Nightreign + AC6 有。每个 `Bone` 是完整 struct（ParentIndex/FirstChildIndex/NextSiblingIndex/PreviousSiblingIndex/NodeIndex）。
6. **FLVER2 BE byte 在 0x06，不是 0x0C**：上游实际支持读 BE。**v1 决策：拒绝 BE**（offset 0x06 探测，BE → `SF_ERR_UNSUPPORTED_VERSION`），并在 extensions.md 文档化。
7. **`sf_flver2_decode_mesh` 是 EXTENSION**：上游仅 `Mesh.GetFaces()` 做三角化；把 packed vertex 展开为 typed 数组是 C 端独有 helper。必须录入 `docs/api-mapping/extensions.md`。
8. **BufferLayout / VertexBuffer 是 mesh 间共享对象**：上游通过 global index 引用。C 端 accessor 必须为 `sf_flver2_buffer_layout(flver, layout_idx)` / `sf_flver2_vertex_buffer(flver, buffer_idx)`（**不是** `sf_flver2_mesh_layout(mesh)`）。
9. **`BufferLayout.Size` 用 `SpecialModifier == -32768` sentinel 表示成员零字节**：C 端必须保留同义。
10. **EdgeCompression flag**：上游写出时静默丢弃；**v1 拒绝**读 + 写都返回 `SF_ERR_UNSUPPORTED_VERSION`。
11. **Half-float helpers 是 Wave 1 hard requirement**：Vertex Half2 / Half4 layout type 必需。
12. **UV factor / AC6 normalization flag**：作为 threaded context 参数传给 decoder（不放注册表）。版本检测在 VertexBuffer/Mesh 层。
13. **AC6 UShort4 normals 特殊归一化路径**：threaded flag 启用。
14. **Triangulation 默认 filter degenerate ON**：与上游一致。
15. **Bounding box 写出原样、无重算**；tangent/bitangent **无重算**（Phase 6 OUT-of-scope）。
16. **Header version whitelist**：T4 probe 收集 c0000.flver 实际版本号；Sekiro = 0x20013、ER 系列 ≥ 0x2001A、AC6 = TBD。

### 上游 .cs 文件清点（in-scope ~4666 LOC）

| 模块 | 文件 | LOC | 备注 |
|---|---|---|---|
| FLVER common | `Dummy.cs` / `Node.cs` / `LayoutMember.cs` / `Vertex.cs` / `VertexBoneIndices.cs` / `VertexBoneWeights.cs` / `VertexColor.cs` / `IFlver.cs` | 1788 | Vertex.cs (824 LOC) 是核心 dispatch 源 |
| FLVER2（in-scope）| `FLVER2.cs` (545) / `BufferLayout.cs` (95) / `GXList.cs` (148) / `Material.cs` (192) / `Texture.cs` (155) / `Mesh.cs` (313) / `FaceSet.cs` (309) / `VertexBuffer.cs` (156) / `SkeletonSet.cs` (145) | 1963 | 9 文件；不含 Edge 4 文件 694 LOC |
| MTD | `MTD.cs` | 577 | 含 Sekiro Extended texture |
| MATBIN | `MATBIN.cs` | 338 | 8 ParamType 变体 |

### Mapping doc 现状（4 份，141 mapping row，全部 `未实现`）

| Doc | Rows |
|---|---|
| `format-flver-common.md` | 27 |
| `format-flver2.md` | 78（含 20 行 Edge 子表，本 plan 将其标 `_skipped_`） |
| `format-mtd.md` | 19 |
| `format-matbin.md` | 17 |

---

## Work Objectives

### Core Objective

实现 FLVER common 公共层 + FLVER2 现代网格 + MTD（Sekiro）+ MATBIN（ER/AC6）的双向读写，对齐上游 net9.0 分支语义；提供 `sf_flver2_decode_mesh` 扩展 API；锁定 Edge geometry OUT-of-scope；e2e 验证 c0000.flver 在 ER 中完整 round-trip。

### Concrete Deliverables

**公共头**（5 个）：
- `include/souls_formats/sf_flver.h`
- `include/souls_formats/sf_flver2.h`
- `include/souls_formats/sf_mtd.h`
- `include/souls_formats/sf_matbin.h`
- `umbrella` 头 `souls_formats.h` 同步含入

**源码**（10 个）：
- `src/geom/flver_common.c`
- `src/geom/flver2.c`（top-level + GXList opaque + header + Wave 2 dispatch）
- `src/geom/flver2_material.c`（Material + Texture + TilingType）
- `src/geom/flver2_mesh.c`（Mesh + BoundingBoxes）
- `src/geom/flver2_faceset.c`（FaceSet + FSFlags + 三角条带解码）
- `src/geom/flver2_vertex_buffer.c`（VertexBuffer + BufferLayout）
- `src/geom/flver2_vertex.c`（mirror 上游 Vertex.cs dispatch）
- `src/geom/flver2_skeleton.c`（SkeletonSet + Bone，版本门控）
- `src/geom/flver2_decode.c`（`sf_flver2_decode_mesh` 扩展）
- `src/geom/mtd.c`
- `src/geom/matbin.c`

**单元 / e2e 测试**（≥ 7 个）：
- `tests/geom/test_flver2_synthetic.c`（1 mesh × 1 material × 8 顶点 × 12 索引）
- `tests/geom/test_mtd_synthetic.c`（3 param × 2 sampler）
- `tests/geom/test_matbin_synthetic.c`（5 param × 3 sampler + 8 ParamType 各至少 1 个）
- `tests/geom/test_flver2_decode.c`（decode_mesh 合成 fixture 验证）
- `tests/geom/test_flver2_e2e_er.c`（c0000.flver）
- `tests/geom/test_matbin_e2e_er.c`（allmaterial.matbinbnd.dcx）
- `tests/geom/test_mtd_e2e_sekiro.c`（SKIP-allowed）

**文档**：
- `docs/api-mapping/format-flver-common.md` 全量刷新（27 行 → 100% 已完成）
- `docs/api-mapping/format-flver2.md` 全量刷新（API 31 行 + Vertex Element Layout 19 行 + Vertex Format Dispatch 8 行 = 58 in-scope 行 → 100%；Edge 20 行 → `_skipped_` + v2 注脚）
- `docs/api-mapping/format-mtd.md` 全量刷新（19 行 → 100%）
- `docs/api-mapping/format-matbin.md` 全量刷新（17 行 → 100%）
- `docs/api-mapping/extensions.md` 增补：`sf_flver2_decode_mesh` + BE 拒绝政策 + EdgeCompression 拒绝政策
- `docs/roadmap/phase-6-geometry-material.md` 任务清单同步本 plan
- `docs/roadmap/README.md` Phase 6 行状态切换
- `AGENTS.md` §2 Current status 表 Phase 5 ✅ + Phase 6 🚧
- `.sisyphus/plans/PLAN.md` §1 + §7 同步 + §2.2 v1 不实现清单补 Edge geometry

### Definition of Done

- [ ] `ctest --test-dir build-mingw -L geom --output-on-failure` 全绿。
- [ ] `ctest --test-dir build-mingw -L 'e2e_er'` Phase 6 部分全绿（FLVER2 + MATBIN，**不允许 SKIP**）。
- [ ] `ctest --test-dir build-mingw -L 'e2e_sekiro'` 全绿（**Sekiro 副本不存在时允许 SKIP**）。
- [ ] `grep -rn 'Edge' include/souls_formats/sf_flver2.h` 仅出现在 OUT-of-scope 注释中（不暴露 Edge API）。
- [ ] `docs/api-mapping/format-{flver-common,flver2,mtd,matbin}.md` in-scope 行 status 全 ≠ "未实现"；Edge 行 status = `_skipped_`。
- [ ] `docs/api-mapping/extensions.md` 含 3 条新增：`sf_flver2_decode_mesh` + BE refusal + EdgeCompression refusal。
- [ ] AGENTS.md §2 表 Phase 5 行 = ✅；Phase 6 行 = 🚧 → ✅（F1-F4 通过后切换）。
- [ ] PLAN.md §7 Phase 6 章节 0 个未勾 checkbox；§2.2 v1 不实现清单含 Edge geometry。
- [x] F1-F4 全部 APPROVE，用户最终 okay。

### Must Have

- 上游所有 in-scope (~4666 LOC) 类 + 字段 + 公共方法均有 C 等价；对齐顺序与字段次序逐位 round-trip。
- 顶点 dispatch **mirror 上游 Vertex.cs**（foreach + semantic-first if/else 阶梯），不优化为静态表。
- `sf_flver2_decode_mesh` 支持 c0000.flver 全部 layout type 解码；对未识别 layout 返回 `SF_ERR_UNSUPPORTED_VERSION` 而非 crash。
- MATBIN 8 个 ParamType 变体全部读 + 写正确（Bool / Int / Int2 / Float / Float2 / Float3 / Float4 / Float5）。
- FLVER2 LE-only（BE 探测后拒绝）；Header version whitelist。
- AC6 UShort4 normal 归一化通过 threaded flag 实现。
- UV factor 通过 threaded context 参数传给 decoder。
- ER e2e（c0000.flver + allmaterial.matbinbnd.dcx）必过；Sekiro e2e SKIP-allowed。
- 所有公共符号 `SF_API` 装饰；所有公共 enum 后置 `_Static_assert`。

### Must NOT Have（Guardrails）

- ❌ **不实现 Edge geometry / SPU vertex format / RSX vertex format** —— 推 v2 legacy。public 头不暴露任何 Edge 相关 enum / 类型。
- ❌ **不实现 FLVER0**（DS1）—— 已在 PLAN.md §2.2 v1 不实现清单。
- ❌ **不支持 FLVER2 BE 字节序** —— offset 0x06 探测，BE 文件直接返回 `SF_ERR_UNSUPPORTED_VERSION`。
- ❌ **不优化顶点 dispatch 结构**（不要 hash 表 / 函数指针表 / 代码生成）—— 必须 mirror 上游 if/else 阶梯。
- ❌ **不结构化 GXItem.Data** —— 保持 opaque `uint8_t*` + size，上游即透传。
- ❌ **不重算 bounding box** —— 写出原样。
- ❌ **不重算 tangent / bitangent / normal** —— 消费者自行重算。
- ❌ **不暴露 Win32 句柄 / mxml / zstd 内部类型** 到公共头。
- ❌ **不在 `_destroy` 之外调用 free** —— 沿用 Phase 1-5 一致性。
- ❌ **不为 c0000.flver 之外的 layout 引入新枚举** —— 仅添加 c0000 + e2e 实际出现的；T4 probe 收集，T17 在注册表实现。其他未知 layout fallback 到 `SF_ERR_UNSUPPORTED_VERSION` 并日志记录（**不阻塞 Phase 6 退出**，仅记 KNOWN_LAYOUT_GAP）。
- ❌ **不实现 PARAMDEF XML 写出**（与 Phase 4 一致约束）。
- ❌ **不引入新第三方依赖**。
- ❌ **不在公共头加 implementation details**（如顶点 stride 计算的内部 helper）。

---

## Verification Strategy（MANDATORY）

> **ZERO HUMAN INTERVENTION** —— 所有验收均由 agent 通过命令执行，禁止「用户手动确认」。
> 证据保存到 `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`。

### Test Decision

- **Infrastructure exists**：YES（Unity ThrowTheSwitch + ctest + `sf_add_test()` 已支持 label 路由）。
- **Automated tests**：tests-after（沿用 Phase 4/5 策略）。
- **Framework**：Unity（基础）+ ctest（驱动）+ `sf_add_test()`（路由）。
- **Labels**：`geom`（公共 + FLVER2 + MTD + MATBIN 单元）、`e2e_er`（ER 真实数据 e2e）、`e2e_sekiro`（Sekiro，SKIP-allowed）。

### QA Policy

每个 task 都必须包含 agent-executed QA scenarios（见每个 TODO 的「QA Scenarios」段）。证据落到 `.sisyphus/evidence/`。

- **CLI / 构建工具**：用 `bash` + `cmake` + `ctest`（在 WSL2 中执行 MinGW cross 产物）。
- **二进制证据**：`xxd` / `objdump` / `nm` 对比 + 保存 hex dump 到 evidence。
- **Round-trip**：`bash` 跑测试 binary，对比 input/output 字节级一致；保留 input.bin 与 output.bin 到 evidence。
- **静态检查**：`grep` 直接验证 API 命名前缀、`_Static_assert` 存在性、Edge symbol 不泄露。

---

## Execution Strategy

### 并行执行 Wave 总览

```
Wave 0 — Preflight（清理 Phase 5 遗债 + 锁定 OUT-of-scope + 探测）:
├── T1: AGENTS.md / PLAN.md / roadmap 三处状态表 Phase 5 ✅ + Phase 6 🚧    [quick]
├── T2: PLAN.md §2.2 v1 不实现清单补 Edge geometry + format-flver2.md 标记  [quick]
├── T3: docs/api-mapping/extensions.md 起草 3 条新增 entry stub             [writing]
├── T4: Empirical probe — c0000.flver 实际 vertex layout type 与 header version 集合 [deep]
├── T5: Empirical probe — allmaterial.matbinbnd.dcx ParamType 分布 + 样本 MATBIN 选定 [deep]
└── T6: UPSTREAM.md game data snapshot 增补 c0000.chrbnd.dcx + allmaterial.matbinbnd.dcx 的 sha256 [quick]

Wave 1 — Foundation（FLVER 公共层 + 4 个公共头）:
├── T7:  sf_flver.h —— Dummy / Node / VertexColor / VertexBoneIndices/Weights typedef + LayoutType/Semantic 枚举 + half-float / 11_11_10 helper 声明 [quick]
├── T8:  src/geom/flver_common.c —— half-float 双向 + 11_11_10 双向 + Dummy / Node / LayoutMember read+write [unspecified-high]
├── T9:  sf_flver2.h —— opaque FLVER2 / Mesh / Material / Texture / FaceSet / VertexBuffer / BufferLayout / SkeletonSet / Bone / GXList + 共享 accessor 签名（含 `sf_flver2_buffer_layout(flver, i)` global 索引模式）[quick]
├── T10: sf_mtd.h —— opaque MTD / Param / Texture + BlendMode / LightingType 枚举 + 公共 API [quick]
└── T11: sf_matbin.h —— opaque MATBIN / Param / Sampler + 8 个 ParamType 枚举 + 公共 API [quick]

Wave 2 — FLVER2 子模块矩阵（Wave 1 全绿后 7 路并行）:
├── T12: src/geom/flver2.c —— top-level dispatch + FLVERHeader + GXList opaque transit + 整体 read/write 流程 [deep]
├── T13: src/geom/flver2_material.c —— Material + Texture + TilingType [unspecified-high]
├── T14: src/geom/flver2_mesh.c —— Mesh + BoundingBoxes + 与 FaceSet/VertexBuffer 的 global index 引用 [unspecified-high]
├── T15: src/geom/flver2_faceset.c —— FaceSet + FSFlags + 三角条带解码（0xFFFF restart + degenerate filter ON default）[deep]
├── T16: src/geom/flver2_vertex_buffer.c —— VertexBuffer + BufferLayout（含 -32768 sentinel 处理）[deep]
├── T17: src/geom/flver2_vertex.c —— **THE 顶点 dispatch** —— mirror Vertex.cs foreach + semantic-first if/else ladder；UV factor + AC6 normalize 通过 threaded context [artistry]
└── T18: src/geom/flver2_skeleton.c —— SkeletonSet + Bone（Version >= 0x2001A 门控，Sekiro 0x20013 路径返回空）[unspecified-high]

Wave 3 — MTD + MATBIN（Wave 1 全绿后 2 路并行，与 Wave 2 并行）:
├── T19: src/geom/mtd.c —— MTD 读 + 写 + Sekiro Extended texture info [deep]
└── T20: src/geom/matbin.c —— MATBIN 读 + 写 + 8 ParamType union [deep]

Wave 4 — Decode helper + 合成 round-trip + ER e2e（Wave 2 + Wave 3 全绿后 8 路并行）:
├── T21:  src/geom/flver2_decode.c —— sf_flver2_decode_mesh 实现（layout 驱动展开为 typed 数组）[artistry]
├── T22:  tests/geom/test_flver2_synthetic.c —— unit cube round-trip 字节级一致 [unspecified-high]
├── T23:  tests/geom/test_mtd_synthetic.c —— 合成 fixture round-trip [quick]
├── T24:  tests/geom/test_matbin_synthetic.c —— 8 ParamType 全覆盖 + round-trip [unspecified-high]
├── T25:  tests/geom/test_flver2_decode.c —— decode_mesh 合成 fixture + c0000 layout 子集验证 [unspecified-high]
├── T26:  tests/geom/test_flver2_e2e_er.c —— c0000.flver 全链路 e2e [unspecified-high]
├── T27:  tests/geom/test_matbin_e2e_er.c —— allmaterial.matbinbnd.dcx 任一 .matbin e2e [unspecified-high]
└── T27b: tests/geom/test_mtd_e2e_sekiro.c —— Sekiro MTD e2e（SKIP-allowed） [unspecified-high]

Wave 5 — Docs + 状态表 final pass（Wave 4 全绿后 3 路并行）:
├── T28: 4 mapping doc 全量刷新（flver-common / flver2 / mtd / matbin）+ Edge 子表标 _skipped_ + extensions.md 三条 final [writing]
├── T29: PLAN.md §7 Phase 6 章节 checkbox 全勾 + §1 状态表 final [writing]
└── T30: docs/roadmap/phase-6-geometry-material.md 与本 plan 收尾对齐 + AGENTS.md §2 Phase 6 = ✅ [writing]

Wave FINAL（4 reviewer 并行 — 全部 wave 完成后启动；必须 ALL APPROVE 才向用户索取 okay）:
├── F1: 计划合规审计              [oracle]
├── F2: 代码质量审查              [unspecified-high]
├── F3: Real Manual QA            [unspecified-high]
└── F4: Scope fidelity check      [deep]
→ 4 reviewer 全 APPROVE → 向用户展示 → 等待用户显式 okay 才标记 Phase 6 完成。
```

### Dependency Matrix（关键路径）

- **T1-T6**：- (Wave 0 preflight, 全 5 个独立) → Wave 1
- **T7**：T1, T2 → T8, T9, T13-T18
- **T8**：T7 → T12, T16, T17
- **T9**：T7 → T12-T18, T21, T22, T25, T26
- **T10**：T7 → T19, T23
- **T11**：T7 → T20, T24
- **T12**：T8, T9 → T13-T18 接入；T26（e2e）
- **T13**：T9, T12 → T22, T26
- **T14**：T9, T12, T15, T16 → T22, T26
- **T15**：T9, T12 → T14, T22, T26
- **T16**：T8, T9, T12 → T14, T17, T22, T26
- **T17**：T8, T9, T12, T16 → T21, T22, T25, T26
- **T18**：T9, T12 → T22, T26
- **T19**：T10 → T23, T27b
- **T20**：T11 → T24, T27
- **T21**：T17 → T25, T26
- **T22-T25**：对应实现 task → Wave 5
- **T26-T27**：对应实现 task + er_test_helper → Wave 5
- **T27b**：T19 + Phase 5 sekiro_test_helper → Wave 5（SKIP-allowed 分支）
- **T28-T30**：Wave 4 全绿 → Wave Final
- **F1-F4**：全部 wave 完成 → user okay

### Agent Dispatch 总结

- **Wave 0**：6 tasks（4 × quick + 1 × deep + 1 × writing） —— 全独立
- **Wave 1**：5 tasks（4 × quick + 1 × unspecified-high）
- **Wave 2**：7 tasks（3 × deep + 3 × unspecified-high + 1 × artistry）
- **Wave 3**：2 tasks（2 × deep）
- **Wave 4**：8 tasks（T21-T27 + T27b；1 × artistry + 1 × quick + 6 × unspecified-high）—— **最大并行度**
- **Wave 5**：3 tasks（writing）
- **Wave Final**：4 tasks（oracle + unspecified-high × 2 + deep）

**总计**：31 实施 task + 4 reviewer = **35 task-level checkbox**。

---

## TODOs

### Wave 0 — Preflight & Cleanup（清理 Phase 5 遗债 + 锁定 OUT-of-scope + 探测）

- [x] 1. **AGENTS.md / PLAN.md / docs/roadmap/README.md 三处状态表 Phase 5 → ✅、Phase 6 → 🚧**

  **What to do**：
  - **Step A：实测 Phase 5 真实 ctest 数**（再生成 evidence，不依赖陈旧 log）：
    1. `cmake --build build-mingw 2>&1 | tail -5`（确保 build clean）
    2. `ctest --test-dir build-mingw -L 'script|map' --output-on-failure 2>&1 | tee .sisyphus/evidence/task-1-phase5-ctest.log`
    3. 从 log 末尾 `100% tests passed, 0 tests failed out of Y` 抓 Y；`awk '/^test [0-9]+/{c++} END{print c}'` 得 M。
  - **Step B：AGENTS.md §2「Current status」表格**：
    - Phase 5 行：`🚧 in progress` → `✅ done`；Tests 列填 `Y/Y PASS across M test binaries`（与 Phase 3/4 风格一致）。
    - Phase 6 行：`⏳ pending` → `🚧 in progress`；Tests 列保留 `—`（T30 完成时再改）。
  - **Step C：PLAN.md §7**：
    - Phase 5 子标题（`### Phase 5 — 脚本与地图`）后追加 `✅ 完成 (2026-05-12) — Y/Y PASS across M test binaries`。
    - Phase 5 章节内所有 `- [ ]` checkbox 改 `- [x]`（与实际完成对齐）。
    - Phase 6 子标题保持现有，但 estimate 校准为「3 周」。
  - **Step D：docs/roadmap/README.md Phase index 表**：
    - Phase 5 行：state = `✅ done`、Tests 列填 `Y/Y PASS (2026-05-12)`。
    - Phase 6 行：state = `🚧 in progress`、estimate = `3 wk`。

  **Must NOT do**：
  - ❌ 不动 PLAN.md §3-§6 架构 / 技术决策章节。
  - ❌ 不动 Phase 6 / 7 子标题里除 estimate 之外的内容。
  - ❌ 不发明 Tests 数；必须用 Step A 实测命令生成。
  - ❌ 不删除已 stale 的 checkbox 历史；保留 git 可追溯。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 3 文件 + 测试数实测。
  - **Skills**: `tech-doc-style-chinese`（PLAN.md 中文文档）。
  - **Skills Evaluated but Omitted**: 无英文风格 skill（roadmap/README.md 中英混排，与中文一致优先）。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T2-T6 并行）
  - **Parallel Group**: Wave 0（与 T2, T3, T4, T5, T6 同组）
  - **Blocks**: T7 起所有 Wave 1 任务
  - **Blocked By**: 无（独立 preflight）

  **References**：

  **Pattern References**：
  - `AGENTS.md:24-35` —— 状态表当前形态，Phase 3 行 `32/32 PASS across 12 test binaries` 是测试数填法范本。
  - `PLAN.md:L429` Phase 0 子标题 `✅ 完成 (2026-05-10)` 是完成标记格式范本。
  - `PLAN.md:L579` Phase 4 子标题 `✅ 完成 (2026-05-11) — 20/20 PASS across 20 test binaries` 是带测试数完整格式范本。
  - Phase 5 plan `.sisyphus/plans/phase-5-script-map.md:T2` —— Phase 5 起步时切 Phase 4 状态的同款模式，可逐字对照。

  **Test References**：
  - `tests/CMakeLists.txt` —— Phase 5 注册的全部 test binary（搜 `sf_add_test` 关键字统计 script/map label）。

  **External References**：
  - GNU awk —— 跨 mingw/WSL 都可用。

  **WHY Each Reference Matters**：
  - Phase 4/5 plan 已建立「先实测 ctest 再写文档」的金标准；T1 完全复用避免凭印象写数字（Phase 4 出现过 reviewer 没跑测试就 APPROVE 的回归案例）。
  - 三处状态表（AGENTS.md / PLAN.md / roadmap/README.md）历史上出现过不同步漂移；Phase 6 起步就一起改，避免再次进入「PLAN 写完成、AGENTS 写 pending」状态。
  - 中文风格 skill 防止改动时不知不觉用了第二人称或宣传腔。

  **Acceptance Criteria**：
  - [ ] `.sisyphus/evidence/task-1-phase5-ctest.log` 存在且末尾含 `100% tests passed, 0 tests failed out of Y`，其中 Y > 0。
  - [ ] AGENTS.md §2 表 Phase 5 行 = `✅ done` + 实测 `Y/Y PASS across M test binaries`。
  - [ ] AGENTS.md §2 表 Phase 6 行 = `🚧 in progress`。
  - [ ] PLAN.md §7 Phase 5 子标题已附完成标记；章节内 0 个未勾 checkbox。
  - [ ] PLAN.md §7 Phase 6 子标题 estimate = 3 周。
  - [ ] docs/roadmap/README.md Phase 5 行 = `✅ done` + Y/Y PASS；Phase 6 行 = `🚧 in progress` + 3 wk。

  **QA Scenarios**：

  ```
  Scenario: Phase 5 ctest 实测 0 failed 且抓到测试数
    Tool: Bash
    Preconditions: Phase 5 已完成；build-mingw 存在
    Steps:
      1. `cmake --build build-mingw 2>&1 | tail -5`
      2. `ctest --test-dir build-mingw -L 'script|map' --output-on-failure 2>&1 | tee .sisyphus/evidence/task-1-phase5-ctest.log`
      3. `tail -3 .sisyphus/evidence/task-1-phase5-ctest.log | grep -E '100% tests passed, 0 tests failed'`
      4. `tail -3 .sisyphus/evidence/task-1-phase5-ctest.log | grep -oE 'out of [0-9]+'`
    Expected Result: 步骤 3 命中（0 failed）；步骤 4 输出 `out of Y` 且 Y > 30（Phase 5 合成 + e2e 测试规模）
    Failure Indicators: 步骤 3 未命中；或 Y < 30（label 语义错或 Phase 5 实际未完成）
    Evidence: .sisyphus/evidence/task-1-phase5-ctest.log

  Scenario: 状态表三处一致
    Tool: Bash
    Preconditions: T1 改动落地
    Steps:
      1. `grep -E '\| 5 \|' AGENTS.md | grep -c '✅ done'`
      2. `grep -A 2 'Phase 5' .sisyphus/plans/PLAN.md | head -5 | grep -c '✅ 完成'`
      3. `grep -E '\| 5 \|' docs/roadmap/README.md | grep -c '✅ done'`
    Expected Result: 三步全部输出 ≥ 1
    Failure Indicators: 任一步骤 = 0（状态表漂移）
    Evidence: .sisyphus/evidence/task-1-state-table-sync.log

  Scenario: Phase 5 章节无未勾 checkbox
    Tool: Bash
    Steps:
      1. `awk '/^### Phase 5/,/^### Phase 6/' .sisyphus/plans/PLAN.md | grep -c '^- \[ \]'`
    Expected Result: 输出 0
    Failure Indicators: > 0
    Evidence: .sisyphus/evidence/task-1-phase5-checkboxes.log

  Scenario: 中文风格未漂移
    Tool: Bash
    Preconditions: T1 改动落地
    Steps:
      1. `git diff AGENTS.md .sisyphus/plans/PLAN.md docs/roadmap/README.md | grep '^+' | grep -E '(你|您|让我们|快来|赶紧)' | tee .sisyphus/evidence/task-1-style.log`
    Expected Result: 输出空
    Failure Indicators: 命中
    Evidence: .sisyphus/evidence/task-1-style.log
  ```

  **Commit**: YES
  - Message: `phase6(state): switch Phase 5 to done, Phase 6 to in-progress in three status tables`
  - Files: `AGENTS.md`, `.sisyphus/plans/PLAN.md`, `docs/roadmap/README.md`, `.sisyphus/evidence/task-1-*`
  - Pre-commit: `ctest --test-dir build-mingw -L 'script|map'` PASS（0 failed）

- [x] 2. **PLAN.md §2.2 v1 不实现清单补 Edge geometry + format-flver2.md 三子表标 `_skipped_`**

  **What to do**：
  - **Step A：PLAN.md §2.2 「v1 显式不实现」表格**（第 49-60 行附近，`### 2.2 v1 显式**不**实现` 节）：
    - 在「遗留几何」子条目末尾追加：「Edge Geometry / SPU vertex format / RSX vertex format（PS3-era console-specific 顶点压缩；v1 4 款目标游戏均不使用，推迟到 v2）」。
    - 在 PLAN.md §11 风险表追加一行：风险=「Edge geometry 字段 sneak into BHD 数据」、缓解=「文件中含 EdgeCompression flag 时返回 `SF_ERR_UNSUPPORTED_VERSION`，T17/T15 显式分支拒绝」。
  - **Step B：docs/api-mapping/format-flver2.md 三个 Edge 子表**：
    - `Edge Geometry Enums`（8 行）：status 列全改 `_skipped_`，Notes 列追加「v1 OUT-of-scope, see PLAN.md §2.2 / extensions.md」。
    - `RsxVertexFormat`（5 行）：同上。
    - `EdgeGeomSkin`（7 行）：同上。
    - 顶部「Status」段落补充：「Edge / SPU / RSX 子表 20 行均 v1 OUT-of-scope，标 `_skipped_`」。
  - **Step C：docs/api-mapping/README.md status legend** 段落：确认 `_skipped_` 已在 legend 中（若没有则添加：「`_skipped_` — 显式不实现，推 v2+ 或 不在范围内」）。

  **Must NOT do**：
  - ❌ 不动 format-flver2.md API 主表（31 行）和 Vertex Element Layout / Vertex Format Dispatch 子表（27 行）—— 这些是 in-scope，留给 Wave 5 T28 刷新。
  - ❌ 不在 PLAN.md §2.1 v1 必交付里删除 FLVER2 / FLVER common / MTD / MATBIN 任何条目。
  - ❌ 不创建 `format-flver2-edge.md` 独立文档；Edge 行就留在 format-flver2.md 下，仅 status 切换。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 2 个文件文本编辑 + 1 个 legend 检查。
  - **Skills**: `tech-doc-style-chinese`。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T7（sf_flver.h 起草时 Edge OUT-of-scope 决策须已定）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - `PLAN.md:L49-60` Phase 5 plan T2 已在此处加过 EMEVD 完成备注；本 task 沿用同款 in-place 增补 pattern。
  - `docs/api-mapping/format-flver2.md` 现有 status 列分布。
  - `docs/api-mapping/POLICY.md` 中 `_skipped_` legend 项的定义（若已有）。

  **API/Type References**：无（纯文档）。

  **External References**：无。

  **WHY Each Reference Matters**：
  - PLAN.md §2.2 是 canonical OUT-of-scope 清单；不在此处明确登记，后续 reviewer 容易判定 Edge 缺失为「待补」而非「不做」。
  - format-flver2.md 是 row-level alignment 状态源；标 `_skipped_` 后 F1 reviewer grep 时不会误判 Edge 行未完成。

  **Acceptance Criteria**：
  - [ ] `grep 'Edge Geometry' .sisyphus/plans/PLAN.md` 命中 ≥ 1 行（§2.2 表中）。
  - [ ] `grep -c '_skipped_' docs/api-mapping/format-flver2.md` ≥ 20（Edge 三子表共 20 行）。
  - [ ] `grep -c '未实现' docs/api-mapping/format-flver2.md`：本 task 完成后应仍 ≥ 58（API 主表 31 + Vertex Element Layout 19 + Vertex Format Dispatch 8）— Wave 5 T28 才会清零。
  - [ ] PLAN.md §11 风险表新增一行含「Edge」「`SF_ERR_UNSUPPORTED_VERSION`」关键字。

  **QA Scenarios**：

  ```
  Scenario: Edge 子表全部标 _skipped_
    Tool: Bash
    Steps:
      1. `awk '/Edge Geometry Enums/,/^##/' docs/api-mapping/format-flver2.md | grep -c '_skipped_'`
      2. `awk '/RsxVertexFormat/,/^##/' docs/api-mapping/format-flver2.md | grep -c '_skipped_'`
      3. `awk '/EdgeGeomSkin/,/^##/' docs/api-mapping/format-flver2.md | grep -c '_skipped_'`
    Expected Result: 步骤 1 = 8；步骤 2 = 5；步骤 3 = 7（与 Metis 实测行数一致）
    Failure Indicators: 任一数量不一致 → 行未全部改 status
    Evidence: .sisyphus/evidence/task-2-edge-skipped.log

  Scenario: PLAN.md §2.2 注册 Edge 不实现
    Tool: Bash
    Steps:
      1. `awk '/### 2.2/,/### 2.3/' .sisyphus/plans/PLAN.md | grep -E 'Edge|SPU|RSX' | tee .sisyphus/evidence/task-2-plan-edge.log`
    Expected Result: 输出 ≥ 1 行
    Failure Indicators: 输出空
    Evidence: .sisyphus/evidence/task-2-plan-edge.log
  ```

  **Commit**: YES
  - Message: `phase6(scope): lock Edge geometry OUT-of-scope in PLAN.md §2.2 and format-flver2.md`
  - Files: `.sisyphus/plans/PLAN.md`, `docs/api-mapping/format-flver2.md`, `.sisyphus/evidence/task-2-*`
  - Pre-commit: 无（纯文档）

- [x] 3. **`docs/api-mapping/extensions.md` 起草 3 条新增 entry（decode_mesh / BE refusal / EdgeCompression refusal）**

  **What to do**：
  - 读 `docs/api-mapping/extensions.md` 当前结构（Phase 4/5 期间可能已有若干 entry；保持 schema 一致）。
  - 添加 3 条新 entry：
    1. **`sf_flver2_decode_mesh`（C-style decode helper）**：
       - 类型：Extension（C 端独有，无上游对应）。
       - 上游对应：仅 `Mesh.GetFaces()` 做三角化，无完整 attribute 解码 helper。
       - C 端 API 签名（草）：`SF_API sf_result_t sf_flver2_decode_mesh(const sf_flver2_t *f, size_t mesh_index, sf_flver2_decoded_mesh_t *out, const sf_allocator_t *a);`
       - 理由：消费者需 typed positions / normals / uvs / bones 数组；上游 C# 用户通过 LINQ / List 访问 Vertex 字段，C 端无对应惯用语，必须提供 helper。
       - 范围：layout-driven 解码；遇未识别 layout 返回 `SF_ERR_UNSUPPORTED_VERSION`。
    2. **FLVER2 BE 字节序拒绝**：
       - 类型：Functional divergence（C 端比上游严格）。
       - 上游行为：通过 offset 0x06 的 `bigEndian` byte 检测并支持 BE 读写。
       - C 端行为：探测到 BE → `SF_ERR_UNSUPPORTED_VERSION`，不解析。
       - 理由：v1 目标 4 款游戏均为 x86_64 LE；BE 路径上游主要为 PS3-era 兼容性；v2 legacy phase 再开。
       - 影响：上游 BE 文件无法读；写出始终 LE。
    3. **EdgeCompression flag 拒绝**：
       - 类型：Functional divergence（C 端比上游严格）。
       - 上游行为：写出时静默丢弃 EdgeCompression flag。
       - C 端行为：读到 EdgeCompression flag → `SF_ERR_UNSUPPORTED_VERSION`；写出不接受任何 Edge-related 输入。
       - 理由：明确 OUT-of-scope；避免数据静默损失。
  - 每条 entry 含：标题、类型、上游 ref（file:line）、C 端 API、理由、影响范围。
  - 若 `extensions.md` 之前不存在或为 stub，建立完整 schema。

  **Must NOT do**：
  - ❌ 不实现任何 C 代码（本 task 仅文档）。
  - ❌ 不在 entries 里宣称 "TODO later"；Phase 6 必须拍板，每条 entry 都是 final 决策。
  - ❌ 不为 Edge geometry 单独建 entry（PLAN.md §2.2 已总括 OUT-of-scope，extensions.md 只记 EdgeCompression flag 这一具体接口面差异）。

  **Recommended Agent Profile**：
  - **Category**: `writing` —— 纯文档撰写。
  - **Skills**: `tech-doc-style-chinese`（若 extensions.md 是中文）；否则空。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T28（Wave 5 final pass 会引用本 task 起草的 entry）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - `docs/api-mapping/POLICY.md` —— C-style adaptation 与 extension 的区分定义。
  - `docs/api-mapping/extensions.md` 现状 —— 若 Phase 4/5 已有 entry，沿用其 schema。
  - Phase 5 plan T2 / T3 同款「起草 stub，Wave 5 final pass」两段式 pattern。

  **API/Type References**：
  - `Mesh.cs:GetFaces()` —— 上游唯一相关 helper（仅三角化，不解码 attribute）。
  - `FLVER2.cs:Read()` BE 检测代码段（Metis 标 offset 0x06）—— 解释 C 端拒绝路径的差异点。

  **External References**：
  - AGENTS.md §5.x rule 2(b) —— extension 必须录入 extensions.md 的硬约束。

  **WHY Each Reference Matters**：
  - POLICY.md 是 extension vs C-style adaptation 的判定依据；3 条 entry 都需要按此 schema 分类。
  - AGENTS.md §5.x 是项目硬约束；不录入 extensions.md = F1 reviewer 直接 REJECT。

  **Acceptance Criteria**：
  - [ ] `docs/api-mapping/extensions.md` 存在且含 3 条新 entry（关键字：`decode_mesh`、`BE refusal`/`big endian`、`EdgeCompression`/`Edge refusal`）。
  - [ ] 每条 entry 含「Type」「Upstream Ref」「C API」「Rationale」「Impact」5 段（或对应 schema）。
  - [ ] entry 内引用 file:line 真实存在（不发明）。

  **QA Scenarios**：

  ```
  Scenario: 3 条 entry 齐全
    Tool: Bash
    Steps:
      1. `grep -c -E '(decode_mesh|BE refusal|big endian|EdgeCompression|Edge refusal)' docs/api-mapping/extensions.md`
    Expected Result: ≥ 3
    Failure Indicators: < 3
    Evidence: .sisyphus/evidence/task-3-extensions-count.log

  Scenario: file:line 引用真实可达
    Tool: Bash
    Steps:
      1. `grep -oE '[A-Z][a-zA-Z]+\.cs:[0-9]+' docs/api-mapping/extensions.md | sort -u | while read ref; do
            file=$(echo $ref | cut -d: -f1)
            line=$(echo $ref | cut -d: -f2)
            full_path=$(find /home/soar/src/SoulsFormatsNEXT -name $file | head -1)
            if [ -z "$full_path" ]; then echo "MISSING: $ref"; else
              total=$(wc -l < "$full_path")
              if [ $line -gt $total ]; then echo "OUT-OF-RANGE: $ref ($total lines)"; fi
            fi
          done | tee .sisyphus/evidence/task-3-refs.log`
    Expected Result: 输出空（所有引用文件存在且行号在范围内）
    Failure Indicators: 任何 MISSING 或 OUT-OF-RANGE
    Evidence: .sisyphus/evidence/task-3-refs.log
  ```

  **Commit**: YES
  - Message: `phase6(docs): seed extensions.md entries for decode_mesh + BE refusal + EdgeCompression refusal`
  - Files: `docs/api-mapping/extensions.md`, `.sisyphus/evidence/task-3-*`
  - Pre-commit: 无

- [x] 4. **Empirical probe — c0000.flver 实际 vertex layout type 与 header version 集合**

  **What to do**：
  - 写一次性 probe 程序 `tests/probes/probe_flver2_layouts.c`：
    1. 用 Phase 3 `er_extract_from_data0` 提取 `/chr/c0000.chrbnd.dcx`。
    2. 用 BND4 reader 找 `c0000.flver` entry。
    3. **手写最小 FLVER2 header parser**（对照 `FLVER2.cs:94-134` 与 `FLVER2.cs:160-189`）：
       - 读 magic `"FLVER\0"` (6 字节)。
       - 读 **endian marker (2 字节 ASCII)** at offset 0x06-0x07：上游 `FLVER2.cs:95` 用 `AssertASCII("L\0", "B\0")`。`"L\0"` = LE，`"B\0"` = BE。**BE → 返回 `SF_ERR_UNSUPPORTED_VERSION`**（v1 拒绝 BE）。
       - 读 FLVERHeader（从 offset 0x08 起）：version (i32) / dataOffset (i32) / dataLength (i32) / dummyCount (i32) / materialCount (i32) / boneCount (i32) / meshCount (i32) / vertexBufferCount (i32) / bbox_min vec3 / bbox_max vec3 / 多个 i32 unk字段 / triangleCount / vertexIndicesSize / textureCount / unicode / 等若干 byte flags / faceSetCount / bufferLayoutCount。**header 中没有指向 BufferLayouts 的 offset**；必须顺序读。
       - Version whitelist（按 `FLVER2.cs:109`）：{`0x20005, 0x20007, 0x20009, 0x2000B, 0x2000C, 0x2000D, 0x2000E, 0x2000F, 0x20010, 0x20013, 0x20014, 0x20016, 0x20017, 0x2001A, 0x2001B, 0x20021`}。其他 version → `SF_ERR_UNSUPPORTED_VERSION`。
    4. **顺序 minimal-skip 前置可变段**（按 `FLVER2.cs:160-189` 顺序），用上游 verified stride 跳过：
       - Dummies：跳 `dummyCount * 64` 字节（**Dummy 固定 64 字节**，对照 `Dummy.cs:98-117`：Position vec3 12 + Color u32 4 + Forward vec3 12 + ReferenceID i16 2 + ParentBoneIndex i16 2 + Upward vec3 12 + AttachBoneIndex i16 2 + Flag1 bool 1 + UseUpwardVector bool 1 + Unk30 i32 4 + Unk34 i32 4 + 2×AssertInt32(0) 8 = **64 bytes**）。
       - Materials：跳 `materialCount * 32` 字节（Material `Read` 读 8 个 i32 = 32 字节，**不读 string 段**因为 string 在文件尾；对照 `Material.cs:82-93`）。
       - Nodes（aka Bones）：跳 `boneCount * 128` 字节（**Node 固定 128 字节**，对照 `Node.cs:137-156`：Translation vec3 12 + nameOffset i32 4 + Rotation vec3 12 + ParentIndex i16 2 + FirstChildIndex i16 2 + Scale vec3 12 + NextSiblingIndex i16 2 + PreviousSiblingIndex i16 2 + BoundingBoxMin vec3 12 + Flags i32 4 + BoundingBoxMax vec3 12 + AssertPattern(0x34, 0x00) 52 = **128 bytes**）。
       - Meshes：跳 `meshCount * sizeof(MeshFixed)` 字节（Mesh 字节数从 `Mesh.cs:Read` 量出；本 probe 实施时读上游精确值并写入 evidence）。
       - FaceSets：跳 `faceSetCount * sizeof(FaceSetFixed)` 字节（同上）。
       - VertexBuffers：跳 `vertexBufferCount * sizeof(VertexBufferFixed)` 字节（同上）。
    5. **到达 BufferLayouts 段**，逐个读 BufferLayout：每个 layout 头 4×i32 = (memberCount, 0, 0, membersOffset)；StepIn 到 membersOffset 读 N 个 LayoutMember。
       - **LayoutMember 固定 20 字节**（非 SpeedTree 情况，对照 `LayoutMember.cs:104-122`）：Stream i32 4 + AssertInt32(structOffset) 4 + Type uint32 4 + Semantic uint32 4 + Index i32 4 = **20 bytes**。
       - SpeedTree 情况（**v1 极少见，c0000 不会走此分支**）字段布局相同总字节：Stream i16 2 + SpecialModifier i16 2 + localStructOffset i32 4 + Type 4 + Semantic 4 + Index 4 = 20 bytes（仅前 4 字节字段类型不同；probe 默认按 non-SpeedTree 解析；遇 SpecialModifier == -32768 → 标记后续 layout member 字节贡献为 0，且 fallback 警告 SpeedTree 路径）。
    6. 收集所有出现的 (LayoutType, LayoutSemantic, Index) 三元组。
  - 输出 `.sisyphus/evidence/task-4-c0000-layouts.md`：
    - Header version（如 0x2001A）。
    - Endianness byte 值（应为 0）。
    - Unicode flag 值。
    - BufferLayout 数 N。
    - Mesh 数 M。
    - 全部出现的 (Type, Semantic, Index) 三元组（去重后列表）。
    - 各前置段跳过的字节数（diagnostic，验证 probe 跳过位置正确）。
  - **不**实现完整 FLVER2 解析；本 task 是 read-only 探测，仅生成报告，为 T17 顶点 dispatch 提供 ground truth。**字段 stride 数字若在 probe 实施时与上游 source 实测不一致，停下来更新本 task 描述**（这是 v1 probe 阶段允许的 plan 微调）。

  **Must NOT do**：
  - ❌ 不复用 `src/geom/` 任何文件（Wave 1-2 还没写）；独立实现 probe。
  - ❌ 不解码 vertex buffer 数据（只读 layout 定义）。
  - ❌ 不修改 BND4 / BHD5 / DCX 代码（已完成阶段不动）。
  - ❌ 不为 Sekiro / AC6 / Nightreign 做同款 probe（本 task 仅 ER；可在 Phase 6 中后期补充其他游戏 probe）。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— 涉及多个 service 层 helper 组合（BHD5 + DCX + BND4 + 手写 minimal FLVER2 header parse）+ 字节级解释。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T17（顶点 dispatch 实现需 probe 输出的 layout 集合作为最小覆盖目标）；T26（e2e 必须覆盖 probe 列出的全部 layout）
  - **Blocked By**: 无（ER 数据已就位，BHD5/DCX/BND4 已完成）

  **References**：

  **Pattern References**：
  - `tests/probes/probe_nightreign_msb.c`（Phase 5 T4）—— 同款一次性 probe 程序结构，含 evidence 输出 + 决策档分类。
  - `tests/e2e/er_test_helper.c` —— BHD5 + DCX + BND4 提取链路（直接 link 复用 helper 函数）。

  **API/Type References**：
  - `include/souls_formats/sf_bnd4.h` —— BND4 entry API。
  - `include/souls_formats/sf_io.h` —— binary reader API（已有 LE u16/u32 读取）。
  - `Formats/FLVER/FLVER2/FLVER2.cs:Read()` —— 上游 header parse 模式参考（**仅参考**，不照抄；本 probe 只读到 BufferLayout 段）。
  - `Formats/FLVER/LayoutMember.cs:Read()` —— LayoutMember 字节布局参考。

  **Test References**：
  - `tests/e2e/test_bnd4_e2e_er.c` —— c0000.chrbnd.dcx 提取 c0000.flver 的成熟模式。

  **External References**：
  - 上游 `FLVER2.cs:467 FLVERHeader` 内嵌结构 —— 字段顺序参考。

  **WHY Each Reference Matters**：
  - probe 必须独立于未来 T12 实现（顺序依赖 = probe 在 T12 之前）；不能 link 还没写的 src/geom/flver2.c。
  - 复用 er_test_helper 避免重复实现 BHD5 提取。
  - LayoutMember.Read 字节布局是 probe 的核心；上游 5 字段 + 字节偏移要严格对齐。

  **Acceptance Criteria**：
  - [ ] `tests/probes/probe_flver2_layouts.c` 提交且通过 cmake 构建（`cmake --build build-mingw --target probe_flver2_layouts`）。
  - [ ] `.sisyphus/evidence/task-4-c0000-layouts.md` 存在且含：
    - Header version（具体十六进制值，如 `0x2001A`）
    - Endianness byte = 0
    - BufferLayout 总数 N（具体数）
    - 全部 (Type, Semantic) 对（去重列表）—— 这是 T17 实现的最小覆盖目标
  - [ ] probe 输出的 (Type, Semantic) 对数 ≥ 5（c0000 是高复杂 mesh，必有多个 layout）。

  **QA Scenarios**：

  ```
  Scenario: probe 跑通且抓到 layout 集合
    Tool: Bash
    Preconditions: ER 数据在；Phase 3 er_test_helper 可用
    Steps:
      1. `cmake --build build-mingw --target probe_flver2_layouts`
      2. `./build-mingw/tests/probes/probe_flver2_layouts.exe > .sisyphus/evidence/task-4-c0000-layouts.txt 2>&1; echo "exit=$?"`
      3. `grep -E 'HEADER_VERSION: 0x[0-9A-F]+' .sisyphus/evidence/task-4-c0000-layouts.txt`
      4. `grep -E 'LAYOUT_PAIR_COUNT: [0-9]+' .sisyphus/evidence/task-4-c0000-layouts.txt`
      5. 解析输出，统计 (Type, Semantic) 唯一对数 ≥ 5
    Expected Result: 退出码 0；步骤 3 命中；步骤 4 命中且数 ≥ 5
    Failure Indicators: 退出码 ≠ 0；endianness != 0（BE 文件不预期）；layout 数 < 5
    Evidence: .sisyphus/evidence/task-4-c0000-layouts.txt + .md（人工可读总结）

  Scenario: probe 不污染 src/geom/
    Tool: Bash
    Preconditions: T4 落地
    Steps:
      1. `ls src/geom/ 2>/dev/null; echo "exit=$?"`
    Expected Result: 退出码 1（src/geom/ 目录尚不存在，Wave 1/2 才创建）；或目录存在但为空
    Failure Indicators: src/geom/ 已有文件 → probe 误把实现代码放进去
    Evidence: .sisyphus/evidence/task-4-no-pollution.log
  ```

  **Commit**: YES
  - Message: `phase6(probe): empirical vertex layout types in c0000.flver`
  - Files: `tests/probes/probe_flver2_layouts.c`, `tests/probes/CMakeLists.txt`, `.sisyphus/evidence/task-4-*`
  - Pre-commit: probe 跑通且 evidence 写齐

- [x] 5. **Empirical probe — allmaterial.matbinbnd.dcx ParamType 分布 + 样本 MATBIN 选定**

  **What to do**：
  - 写一次性 probe 程序 `tests/probes/probe_matbin_paramtypes.c`：
    1. 用 `er_extract_from_data0("/material/allmaterial.matbinbnd.dcx")` 提取。
    2. 用 BND4 reader 列出全部 entry（应为大量 `.matbin` 文件）。
    3. 取前 10 个 `.matbin` 作为样本。
    4. **手写最小 MATBIN header parser**（magic / version / shader path / source path / key / param count / sampler count）—— 因为完整 MATBIN parser 还没实现。
    5. 对每个 sample MATBIN：
       - 遍历 Params，统计各 ParamType 出现次数。
       - 遍历 Samplers，统计 Type 字符串去重集合。
  - 输出 `.sisyphus/evidence/task-5-matbin-survey.md`：
    - allmaterial.matbinbnd.dcx entry 总数。
    - 选定的 10 个 sample MATBIN 路径列表。
    - ParamType 直方图（8 种变体各出现多少次；验证 Metis 的 8 个 ParamType 假设）。
    - Sampler Type 去重集合（如 `g_DiffuseTexture` 等）。
    - 一个**最小** MATBIN 的字节级 hex dump（< 500 字节那种），用于 T24 合成 fixture 参考。
  - **不**实现完整 MATBIN 解析；本 task 是 read-only 探测。

  **Must NOT do**：
  - ❌ 不复用 `src/geom/matbin.c`（Wave 3 才写）；独立实现 probe。
  - ❌ 不对 Sampler 或 Param 做任何 transformation（直接读上游字节布局）。
  - ❌ 不为 AC6 做同款 probe（AC6 副本未就位）。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— BND4 提取 + 手写 minimal MATBIN parser + 直方图统计 + hex dump 摘取。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T11（sf_matbin.h 的 ParamType 枚举值需 probe 验证 Metis 假设）；T20（matbin.c 实现需 probe 出的样本）；T24（合成 fixture 应覆盖 probe 出的 ParamType 分布）；T27（e2e 测试 hint）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - T4 probe 程序结构（同款一次性 probe）。
  - `tests/e2e/er_test_helper.c` —— BND4 entry 列举 + UTF-8 名查找。

  **API/Type References**：
  - `include/souls_formats/sf_bnd4.h` —— BND4 reader API。
  - `Formats/MATBIN.cs:Read()` —— 上游字节布局参考；尤其 Param Read 段（ParamType 枚举值映射）。

  **External References**：
  - Metis 报告中关于 MATBIN ParamType 8 变体的实测结论（Bool/Int/Int2/Float/Float2/Float3/Float4/Float5）。

  **WHY Each Reference Matters**：
  - probe 主要价值是**验证** Metis 的 8 ParamType 假设；若真实数据中出现第 9 种 ParamType，必须立刻警告（plan 需要修正）。
  - 选定 10 个样本 MATBIN 可让 T27 e2e 测试有具体目标（而非每次随机挑），保证可重现。
  - 摘取的最小 MATBIN hex dump 是 T24 合成 fixture 的字节级 ground truth。

  **Acceptance Criteria**：
  - [ ] `tests/probes/probe_matbin_paramtypes.c` 提交且通过 cmake 构建。
  - [ ] `.sisyphus/evidence/task-5-matbin-survey.md` 存在且含：
    - allmaterial.matbinbnd.dcx entry 总数（数字）
    - 10 个 sample 路径列表
    - ParamType 直方图（覆盖至少 4 个不同 ParamType；若 < 4 → 选样不够代表性需调整）
    - Sampler Type 去重集合（≥ 3 种）
    - 一个 minimum MATBIN 的 hex dump
  - [ ] 若 probe 发现 Metis 假设外的 ParamType 值 → evidence 中显式 ALERT 标记，task 仍 PASS 但 plan 需修正（标 follow-up）。

  **QA Scenarios**：

  ```
  Scenario: probe 跑通且 ParamType 集合 ⊆ 上游 8 个非连续值 {0,4,5,8,9,10,11,12}
    Tool: Bash
    Preconditions: ER 数据在
    Steps:
      1. `cmake --build build-mingw --target probe_matbin_paramtypes`
      2. `./build-mingw/tests/probes/probe_matbin_paramtypes.exe > .sisyphus/evidence/task-5-matbin-survey.txt 2>&1`
      3. `grep -E 'PARAMTYPE_HIST:' .sisyphus/evidence/task-5-matbin-survey.txt`
      4. `grep -E 'UNKNOWN_PARAMTYPE: [0-9]+' .sisyphus/evidence/task-5-matbin-survey.txt; echo "exit=$?"`
      5. probe 内部逻辑：把读出的 raw uint32 与上游真值集 `{0, 4, 5, 8, 9, 10, 11, 12}` 比对；命中即按对应 ParamType 计数，否则输出 `UNKNOWN_PARAMTYPE: <value>` 行。上游真值含义：Bool=0 / Int=4 / Int2=5 / Float=8 / Float2=9 / Float3=10 / Float4=11 / Float5=12（**注意跳号 1,2,3,6,7**）。
    Expected Result: 退出码 0；步骤 3 命中且 raw enum 值 ⊆ `{0, 4, 5, 8, 9, 10, 11, 12}`（**非连续 0..7**，与 `MATBIN.cs:126-167` 一致）；步骤 4 grep 退出码 1（无 UNKNOWN_PARAMTYPE）
    Failure Indicators: 步骤 4 命中 UNKNOWN_PARAMTYPE → 上游有第 9 种 ParamType，plan 需要补；或 raw enum 值出现 {1,2,3,6,7} 之一 → 实地数据与 `MATBIN.cs` 不一致，plan 需要重审
    Evidence: .sisyphus/evidence/task-5-matbin-survey.txt + .md

  Scenario: 最小 MATBIN hex dump 可用作 T24 fixture 参考
    Tool: Bash
    Steps:
      1. `grep -A 30 'MINIMUM_MATBIN_HEX:' .sisyphus/evidence/task-5-matbin-survey.txt | wc -l`
    Expected Result: ≥ 10 行（hex dump 不能为空）
    Failure Indicators: < 10
    Evidence: 同上
  ```

  **Commit**: YES
  - Message: `phase6(probe): empirical ParamType distribution in allmaterial.matbinbnd.dcx`
  - Files: `tests/probes/probe_matbin_paramtypes.c`, `tests/probes/CMakeLists.txt`, `.sisyphus/evidence/task-5-*`
  - Pre-commit: probe 跑通且 evidence 写齐 + 0 UNKNOWN_PARAMTYPE

- [x] 6. **`docs/api-mapping/UPSTREAM.md` Game Data Snapshot 增补 c0000.chrbnd.dcx + allmaterial.matbinbnd.dcx sha256**

  **What to do**：
  - 读 Phase 5 在 `docs/api-mapping/UPSTREAM.md` 已建的「Game Data Snapshots」段落。
  - 在 ER 子段下追加：
    - c0000.chrbnd.dcx 的提取路径（`/chr/c0000.chrbnd.dcx` via Data0）+ 提取后的 sha256 + 抓取日期。
    - allmaterial.matbinbnd.dcx 的提取路径（`/material/allmaterial.matbinbnd.dcx` via Data0）+ 提取后的 sha256 + 抓取日期。
  - 加注：「Phase 6 e2e 测试将 sha256 校验为 sanity check；若用户升级游戏到新 patch，sha256 不匹配则提示 SKIP 并日志记录（避免误判为 fail）」。
  - 在 risk 列加一行：「Phase 6 e2e 锁定 c0000.flver 与 allmaterial.matbinbnd.dcx 的 snapshot；游戏 patch 后字段可能微变」。

  **Must NOT do**：
  - ❌ 不嵌入任何 game-derived 字节（即仅记录 hash 不嵌内容）。
  - ❌ 不改动 Phase 5 已建的 ER / Sekiro / NR / AC6 段落，仅在 ER 段内追加 2 行。
  - ❌ 不在 git 提交里包含 hash 之外的任何 game-derived 内容。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— sha256sum + 文档插入。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T26（FLVER2 e2e 测试将引用 snapshot hash 作 sanity check）；T27（MATBIN e2e 同上）
  - **Blocked By**: 无（ER 数据已就位）

  **References**：

  **Pattern References**：
  - `docs/api-mapping/UPSTREAM.md` Phase 5 「Game Data Snapshots」段已建结构 —— 沿用 schema。

  **External References**：
  - `sha256sum` CLI（mingw / WSL2 都自带）。

  **WHY Each Reference Matters**：
  - Phase 5 已在 UPSTREAM.md 建立 snapshot 记录政策；Phase 6 不另起炉灶，复用并扩展。
  - e2e 测试加 sha256 sanity check 是防止「Reviewer 跑测试时游戏升了 patch、断言失败被误判 bug」的成熟手段。

  **Acceptance Criteria**：
  - [ ] `docs/api-mapping/UPSTREAM.md` 「Game Data Snapshots」段 ER 子段含新增 2 行（c0000.chrbnd.dcx + allmaterial.matbinbnd.dcx 的 sha256）。
  - [ ] 每条新增含：BHD5 内路径、sha256、抓取日期（YYYY-MM-DD）。

  **QA Scenarios**：

  ```
  Scenario: sha256 实测可重现
    Tool: Bash
    Steps:
      1. 用 er_test_helper 提取 /chr/c0000.chrbnd.dcx 到 /tmp，记 sha256
      2. `grep c0000.chrbnd.dcx docs/api-mapping/UPSTREAM.md` 抓出文档中记录的 sha256
      3. diff 两者
    Expected Result: 一致
    Failure Indicators: 不一致 → 用户已升级游戏，需要重抓 snapshot
    Evidence: .sisyphus/evidence/task-6-hash-verify.log

  Scenario: UPSTREAM.md 结构完整
    Tool: Bash
    Steps:
      1. `grep -A 5 'c0000.chrbnd.dcx' docs/api-mapping/UPSTREAM.md | grep -c 'sha256'`
      2. `grep -A 5 'allmaterial.matbinbnd.dcx' docs/api-mapping/UPSTREAM.md | grep -c 'sha256'`
    Expected Result: 两步均 ≥ 1
    Failure Indicators: 任一 = 0
    Evidence: .sisyphus/evidence/task-6-upstream-md.log
  ```

  **Commit**: YES
  - Message: `phase6(docs): pin c0000.chrbnd.dcx + allmaterial.matbinbnd.dcx sha256 in UPSTREAM.md`
  - Files: `docs/api-mapping/UPSTREAM.md`, `.sisyphus/evidence/task-6-*`
  - Pre-commit: 无

### Wave 1 — Foundation（FLVER 公共层 + 4 个公共头）

- [x] 7. **`sf_flver.h` —— Dummy / Node / VertexColor / VertexBoneIndices/Weights typedef + LayoutType/Semantic 枚举 + half-float / 11_11_10 helper 声明**

  **What to do**：
  - 起草 `include/souls_formats/sf_flver.h`：
    - **公共 POD typedef**（值类型，直接结构体）：
      - `sf_flver_dummy_t`：Position (vec3), Forward (vec3), Upward (vec3), ReferenceID (i16), DummyBoneIndex (i16), AttachBoneIndex (i16), Color (u32), Flag1 (bool), UseUpwardVector (bool), Unk30 (i32), Unk34 (i32)。**字段顺序与 `Dummy.cs` 完全一致**。
      - `sf_flver_node_t`：**严格对照 `Node.cs:37-101` 公共字段**，**仅含以下字段（按字段次序）**：
        - `char *name`（UTF-8 string, owned by allocator；上游 `Name`）
        - `int16_t parent_index`（**short**，默认 -1；上游 `ParentIndex`）
        - `int16_t first_child_index`（short，默认 -1）
        - `int16_t next_sibling_index`（short，默认 -1）
        - `int16_t previous_sibling_index`（short，默认 -1）
        - `sf_vec3_t translation`
        - `sf_vec3_t rotation`（XZY euler radians）
        - `sf_vec3_t scale`（默认 (1,1,1)）
        - `sf_vec3_t bbox_min`
        - `sf_vec3_t bbox_max`
        - `sf_flver_node_flags_t flags`（**i32 bit flags**，上游 `NodeFlags`）
        - **NO standalone Position 字段**（上游无此字段；Translation 是位置）
        - **NO Unk44_46_48 字段**（上游 read 时是 `AssertPattern(0x34, 0x00)` 即 52 字节零，不是用户字段）
      - `sf_flver_node_flags_t` enum（上游 `Node.NodeFlags` `[Flags]`）：
        - `SF_FLVER_NODE_FLAG_DISABLED = 1`
        - `SF_FLVER_NODE_FLAG_DUMMY_OWNER = 1 << 1` (2)
        - `SF_FLVER_NODE_FLAG_MESH = 1 << 2` (4)
        - `SF_FLVER_NODE_FLAG_BONE = 1 << 3` (8)
      - `sf_flver_vertex_color_t`：A/R/G/B (f32 × 4)。
      - `sf_flver_vertex_bone_indices_t`：i32[4]。
      - `sf_flver_vertex_bone_weights_t`：f32[4]。
    - **公共 enum**（**精确对照** `LayoutMember.cs:LayoutType / LayoutSemantic` 上游 byte 值，验证命令见 QA scenario）：
      - `sf_flver_layout_type_t`（18 个值，上游 `LayoutType : uint`）：
        - `SF_FLVER_LAYOUT_TYPE_FLOAT1 = 0`
        - `SF_FLVER_LAYOUT_TYPE_FLOAT2 = 1`
        - `SF_FLVER_LAYOUT_TYPE_FLOAT3 = 2`
        - `SF_FLVER_LAYOUT_TYPE_FLOAT4 = 3`
        - `SF_FLVER_LAYOUT_TYPE_COLOR = 16`（0x10，4 字节）
        - `SF_FLVER_LAYOUT_TYPE_UBYTE4 = 17`
        - `SF_FLVER_LAYOUT_TYPE_BYTE4 = 18`
        - `SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM = 19`
        - `SF_FLVER_LAYOUT_TYPE_BYTE4_NORM = 20`
        - `SF_FLVER_LAYOUT_TYPE_SHORT2 = 21`
        - `SF_FLVER_LAYOUT_TYPE_SHORT4 = 22`
        - `SF_FLVER_LAYOUT_TYPE_USHORT2 = 23`
        - `SF_FLVER_LAYOUT_TYPE_USHORT4 = 24`
        - `SF_FLVER_LAYOUT_TYPE_SHORT4_NORM = 26`
        - `SF_FLVER_LAYOUT_TYPE_HALF2 = 45`
        - `SF_FLVER_LAYOUT_TYPE_HALF4 = 46`
        - `SF_FLVER_LAYOUT_TYPE_BYTE4E = 47`（上游标注 Unknown）
        - `SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED = 240`（**OUT-of-scope 但纳入枚举**，触发 `SF_ERR_UNSUPPORTED_VERSION`）
      - `sf_flver_layout_semantic_t`（8 个值，上游 `LayoutSemantic : uint`）：
        - `SF_FLVER_LAYOUT_SEMANTIC_POSITION = 0`
        - `SF_FLVER_LAYOUT_SEMANTIC_BONE_WEIGHTS = 1`
        - `SF_FLVER_LAYOUT_SEMANTIC_BONE_INDICES = 2`
        - `SF_FLVER_LAYOUT_SEMANTIC_NORMAL = 3`
        - `SF_FLVER_LAYOUT_SEMANTIC_UV = 5`（跳号：上游无 4）
        - `SF_FLVER_LAYOUT_SEMANTIC_TANGENT = 6`
        - `SF_FLVER_LAYOUT_SEMANTIC_BITANGENT = 7`
        - `SF_FLVER_LAYOUT_SEMANTIC_VERTEX_COLOR = 10`（跳号：上游无 8/9）
    - 每个 enum 后置 `_Static_assert`：
      - `_Static_assert(SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED == 240, "LayoutType drift")`（最高值哨兵，由于枚举值非连续，**不使用 COUNT** sentinel）
      - `_Static_assert(SF_FLVER_LAYOUT_SEMANTIC_VERTEX_COLOR == 10, "LayoutSemantic drift")`
    - **公共 helper 函数声明**（实现在 T8）：
      ```c
      SF_API float    sf_half_to_float(uint16_t half);
      SF_API uint16_t sf_float_to_half(float f);
      SF_API void     sf_unpack_11_11_10(uint32_t packed, float *out_x, float *out_y, float *out_z);  /* signed [-1,1] */
      SF_API uint32_t sf_pack_11_11_10(float x, float y, float z);
      SF_API uint32_t sf_flver_layout_type_size(sf_flver_layout_type_t t, int32_t special_modifier);  /* 处理 -32768 sentinel */
      ```
    - **所有公共符号 `SF_API` 装饰**。

  **Must NOT do**：
  - ❌ 不暴露 Vertex struct（顶点数据是 mesh-internal，通过 decode_mesh 提供 typed access）。
  - ❌ 不暴露任何 FLVER2-specific 类型（如 FaceSet/Mesh —— 留给 sf_flver2.h）。
  - ❌ 不暴露任何 IFlver* 接口（C 端无对应惯用语）。
  - ❌ 不暴露 Edge geometry 相关枚举/struct（EdgeCompressed 仅作为 LayoutType 一个值存在，便于错误返回，不再额外暴露）。
  - ❌ 不放 implementation（仅 typedef + extern 声明）。
  - ❌ 不加 `#include <stdio.h>` 等无关 system header（仅 `<stdint.h>` / `<stdbool.h>` 必需）。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 头文件起草 + 枚举对齐 + `_Static_assert`。
  - **Skills**: 无（项目内已有同款 header 范本）。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T9, T10, T11 并行；T8 依赖 T7）
  - **Parallel Group**: Wave 1
  - **Blocks**: T8, T9-T11, T12-T18, T21, T22
  - **Blocked By**: T1, T2（状态切换 + Edge OUT-of-scope 落定后才安心起头）

  **References**：

  **Pattern References**：
  - `include/souls_formats/sf_paramdef.h:34-50` —— enum + `_Static_assert` 写法范本（Phase 4 实施过）。
  - `include/souls_formats/sf_bnd4.h` —— opaque + accessor 公共模式。
  - `include/souls_formats/sf_emevd.h` —— Phase 4 enum-heavy header 范本。
  - `include/souls_formats/sf_msb.h:T7`（Phase 5 sister task）—— 公共类型分割（公共层不含 variant 字段）的同款分层模式。

  **API/Type References**：
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/Dummy.cs:全文` —— sf_flver_dummy_t 字段次序的 ground truth。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/Node.cs:全文` —— sf_flver_node_t 字段次序与字符串字段所有权。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/LayoutMember.cs:LayoutType 与 LayoutSemantic enum 定义` —— 枚举值与上游 byte 值的对齐。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/VertexColor.cs` —— `sf_flver_vertex_color_t` 4 通道 f32。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/VertexBoneIndices.cs` / `VertexBoneWeights.cs` —— 数组长度 4 的固定结构。

  **Test References**：无（本 task 仅头文件，T8 才写测试）。

  **External References**：
  - IEEE 754 half-precision 规范（用于 T8 实现注释，但 T7 已声明 API）。
  - Metis 报告：UV factor / AC6 flag 是 threaded context 参数 —— 但本 header 不需要为它们建 enum；这些 flag 通过 `sf_flver2_context_t`（在 sf_flver2.h 中）传递。

  **WHY Each Reference Matters**：
  - Dummy.cs / Node.cs 字段顺序错位 = 全部 round-trip 失败；必须逐字段对照。
  - LayoutType / LayoutSemantic 的枚举值必须与上游 byte 值一致（FLVER2 是 ABI-level binary format，枚举值不是 C 端自由命名，是 file-format-defined）。
  - PARAMDEF / EMEVD / MSB 已建立同款 `_Static_assert` 守护模式，复用避免重复造轮。

  **Acceptance Criteria**：
  - [ ] `include/souls_formats/sf_flver.h` 提交且 `cmake --build build-mingw --target souls_formats` 通过（即便 T8 还没实现，header 应可被 dummy `.c` include 编译）。
  - [ ] `grep '_Static_assert' include/souls_formats/sf_flver.h` ≥ 2（LayoutType + LayoutSemantic 各一）。
  - [ ] `grep 'SF_API' include/souls_formats/sf_flver.h` ≥ 5（5 个 helper 函数声明）。
  - [ ] `grep -E 'enum sf_flver_layout_(type|semantic)' include/souls_formats/sf_flver.h` 命中 2 行。
  - [ ] `grep 'EDGE_COMPRESSED' include/souls_formats/sf_flver.h` 命中 1 行（仅 enum 值，无其他 Edge 暴露）。
  - [ ] LayoutType 18 个值（Float1-4 / Color / UByte4 / Byte4 / UByte4Norm / Byte4Norm / Short2 / Short4 / UShort2 / UShort4 / Short4Norm / Half2 / Half4 / Byte4E / EdgeCompressed）byte 值与 `LayoutMember.cs:LayoutType` 1:1 对齐。
  - [ ] LayoutSemantic 8 个值（Position=0 / BoneWeights=1 / BoneIndices=2 / Normal=3 / UV=5 / Tangent=6 / Bitangent=7 / VertexColor=10）byte 值与 `LayoutMember.cs:LayoutSemantic` 1:1 对齐（**含跳号 4、8、9**）。

  **QA Scenarios**：

  ```
  Scenario: 头文件 C + C++ 双向 include 通过
    Tool: Bash
    Preconditions: T7 完成
    Steps:
      1. 临时写 `tests/core/_flver_header_smoke.c`: `#include "souls_formats/sf_flver.h"\nint main(){return 0;}`
      2. 临时写 `tests/core/_flver_header_smoke.cpp`: 同内容
      3. `cmake --build build-mingw --target ...smoke_c ...smoke_cpp`
    Expected Result: 两个编译都通过（即便 helper 未实现，应仅有 link 阶段错误，编译阶段过）
    Failure Indicators: `extern "C"` 包装漏；或 typedef 冲突；或 `<stdint.h>` 缺失
    Evidence: .sisyphus/evidence/task-7-header-smoke.log

  Scenario: LayoutType 18 个值与上游对齐
    Tool: Bash
    Preconditions: T7 完成
    Steps:
      1. `grep -oE 'SF_FLVER_LAYOUT_TYPE_[A-Z0-9_]+ = [0-9]+' include/souls_formats/sf_flver.h | sort -t= -k2 -n | tee .sisyphus/evidence/task-7-our-layouttype.log`
      2. `awk '/public enum LayoutType/,/^        \}/' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/LayoutMember.cs | grep -oE '(Float[1-4]|Color|UByte4(Norm)?|Byte4(Norm|E)?|U?Short[24](Norm)?|Half[24]|EdgeCompressed) = [0-9]+' | tee .sisyphus/evidence/task-7-upstream-layouttype.log`
      3. `diff <(wc -l < .sisyphus/evidence/task-7-our-layouttype.log) <(wc -l < .sisyphus/evidence/task-7-upstream-layouttype.log)`
      4. 验证关键 byte 值：`grep 'EDGE_COMPRESSED = 240' include/souls_formats/sf_flver.h && grep 'COLOR = 16' include/souls_formats/sf_flver.h && grep 'HALF2 = 45' include/souls_formats/sf_flver.h`
    Expected Result: 步骤 3 输出空（两边 18 行）；步骤 4 三个 grep 都命中
    Failure Indicators: 行数不一致；或关键 byte 值不匹配
    Evidence: .sisyphus/evidence/task-7-{our,upstream}-layouttype.log

  Scenario: LayoutSemantic 8 个值（含跳号）与上游对齐
    Tool: Bash
    Steps:
      1. `grep -oE 'SF_FLVER_LAYOUT_SEMANTIC_[A-Z_]+ = [0-9]+' include/souls_formats/sf_flver.h | sort -t= -k2 -n > .sisyphus/evidence/task-7-our-sem.log`
      2. 验证：含 `_UV = 5`（跳过 4）、`_VERTEX_COLOR = 10`（跳过 8、9）
      3. `grep -E '(UV = 5|VERTEX_COLOR = 10)' .sisyphus/evidence/task-7-our-sem.log | wc -l`
    Expected Result: 步骤 3 = 2（两个跳号点都正确）
    Failure Indicators: 缺跳号 → UV / VertexColor 字段全部解析错位
    Evidence: .sisyphus/evidence/task-7-our-sem.log

  Scenario: Edge 暴露面最小化
    Tool: Bash
    Steps:
      1. `grep -ciE 'edge|spu|rsx' include/souls_formats/sf_flver.h`
    Expected Result: ≤ 1（仅 `EDGE_COMPRESSED = 240` 枚举值出现 1 次；不出现 SPU / RSX 任何字符）
    Failure Indicators: > 1
    Evidence: .sisyphus/evidence/task-7-edge-exposure.log
  ```

  **Commit**: YES
  - Message: `phase6(flver-common): sf_flver.h with Dummy/Node/LayoutMember + half-float helpers`
  - Files: `include/souls_formats/sf_flver.h`, `CMakeLists.txt`（更新 `SF_PUBLIC_HEADERS`）, `include/souls_formats/souls_formats.h`（umbrella 引入）
  - Pre-commit: header-only smoke build + grep guard

- [x] 8. **`src/geom/flver_common.c` —— half-float / 11_11_10 / Dummy / Node / LayoutMember 双向实现 + 测试**

  **What to do**：
  - 创建 `src/geom/flver_common.c`，实现 T7 声明的 5 个 helper：
    - **`sf_half_to_float` / `sf_float_to_half`**：IEEE 754-compliant 转换。处理 subnormal / inf / NaN。优先使用编译器 intrinsic（`__builtin_convertvector` / `_cvtss_sh` / `_cvtsh_ss`）；fallback 到 bit-twiddling reference 实现（上游 `BinaryReaderEx.ReadHalf` 风格 + IEEE 754 互转代码段）。
    - **`sf_unpack_11_11_10` / `sf_pack_11_11_10`**：从 u32 解 11_11_10 packed normal/tangent 到 vec3 [-1, 1]，与 Phase 1 已有的 `sf_binary_reader_read_vec3_11_11_10` 内部逻辑一致（DRY：提取出共用 helper，Phase 1 binary_reader 改为调用此 helper）。
    - **`sf_flver_layout_type_size`**：返回 LayoutType 在字节流中占用字节数；处理 `SpecialModifier == -32768` sentinel（return 0）。映射规则上游在 `BufferLayout.cs:Size` / `LayoutMember.cs:Size`。
  - 内部添加 `flver_common_internal.h`（在 `src/internal/` 下）声明 Dummy / Node / LayoutMember 的 read+write 实现：
    - `sf_internal_flver_dummy_read(reader, out *dummy)` / `_write`
    - `sf_internal_flver_node_read(reader, unicode_flag, out *node, allocator)` / `_write`（name 字段需 allocator + unicode flag 决定 UTF-16 / Shift-JIS）
    - `sf_internal_flver_layout_member_read(reader, out *member)` / `_write`
  - 实现上述 6 个 internal helper，对照 `Dummy.cs:Read`/`Write`、`Node.cs:137-181`、`LayoutMember.cs:Read`/`Write`。
  - **Node 字节布局严格对照 `Node.cs:137-156` Read 顺序**（不可调整）：
    1. `Translation` (vec3, 12 字节)
    2. `nameOffset` (i32, 4 字节) — 不存储为 Node 字段，仅用于晚期 string 段读取
    3. `Rotation` (vec3, 12 字节)
    4. `ParentIndex` (**i16**, 2 字节)
    5. `FirstChildIndex` (**i16**, 2 字节)
    6. `Scale` (vec3, 12 字节)
    7. `NextSiblingIndex` (**i16**, 2 字节)
    8. `PreviousSiblingIndex` (**i16**, 2 字节)
    9. `BoundingBoxMin` (vec3, 12 字节)
    10. `Flags` (i32, 4 字节，按 NodeFlags `[Flags]` enum 处理)
    11. `BoundingBoxMax` (vec3, 12 字节)
    12. `AssertPattern(0x34, 0x00)` — 52 字节零 padding（不存储为字段，写出时 `WritePattern(0x34, 0x00)`）
    - **总字节数**：12 + 4 + 12 + 2 + 2 + 12 + 2 + 2 + 12 + 4 + 12 + 52 = **128 bytes per Node**
  - Write 对称（`Node.cs:158-172`）：先调 `bw.ReserveInt32($"BoneNameOffset{index}")`，string 段写出时调 `WriteStrings` 填回。
  - 添加测试 `tests/geom/test_flver_common.c`：
    - half-float 双向 16 个 golden 值（含 +0/-0/+inf/-inf/NaN/最大正常值/最小正常值/最小 subnormal）。
    - 11_11_10 双向 8 个 golden 值（含 (1,0,0) / (-1,0,0) / (0,0,0) / 三个轴正向 / 三个轴负向）。
    - Dummy round-trip（16 字段全填）字节级一致。
    - Node round-trip（含 UTF-8 + 日文 name `骨头` 的混合）字节级一致。
    - LayoutMember round-trip（4 个 LayoutType 各采样）字节级一致。
    - `sf_flver_layout_type_size` 对每个 LayoutType + special_modifier=-32768 sentinel 各测一次。

  **Must NOT do**：
  - ❌ 不暴露 `sf_internal_flver_*` 到公共头。
  - ❌ 不在 `flver_common.c` 里实现 FLVER2-specific 内容（Material/Texture/Mesh/FaceSet/VertexBuffer/BufferLayout/SkeletonSet/Bone/GXList 全留给 Wave 2）。
  - ❌ 不假设 half-float intrinsic 一定可用；必须有 fallback 实现并 unit test 验证（intrinsic 路径与 fallback 路径输出一致）。
  - ❌ 不实现 Edge geometry layout type 的 size 计算（EdgeCompressed = 返回 0 或 SIZE_MAX 作 sentinel；调用方检测后返回 `SF_ERR_UNSUPPORTED_VERSION`）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— 涉及多 helper 实现 + 多类型 round-trip + IEEE 754 边界 + intrinsic fallback。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（T7 阻塞）
  - **Parallel Group**: Wave 1（与 T9-T11 并行，但本身依赖 T7）
  - **Blocks**: T12, T16, T17（vertex dispatch 需要 half-float + 11_11_10 + layout member）
  - **Blocked By**: T7

  **References**：

  **Pattern References**：
  - `src/core/binary_reader.c:sf_binary_reader_read_vec3_11_11_10` —— 既有 11_11_10 解码逻辑，本 task 抽取为共用 helper。
  - `src/core/binary_reader.c:sf_binary_reader_read_*` —— LE/BE 读取与字段顺序模式。
  - `src/core/binary_writer.c:sf_binary_writer_write_*` —— 写出模式。
  - `src/core/encoding_win32.c` —— Node.Name 字段是 Shift-JIS-or-UTF8 字符串，需通过 sf_encoding 走 Win32 转换。
  - Phase 5 `src/script/esd_bytecode.c` —— 类似的 internal helper + 单元测试 pattern。

  **API/Type References**：
  - `Dummy.cs:Read()` / `Write()` —— Dummy 字节布局。
  - `Node.cs:Read()` / `Write()` —— Node 字节布局（含可选字段，需检查 version flag）。
  - `LayoutMember.cs:Read()` / `Write()` —— LayoutMember 字节布局。
  - `LayoutMember.cs:Size` —— LayoutType → 字节数映射。
  - `BinaryReaderEx.cs:ReadHalf` —— 上游 half-float 读取参考（验证 IEEE 754 实现）。

  **Test References**：
  - `tests/core/test_binary_reader.c` —— 既有 vec3_11_11_10 测试可对照新 helper 输出。
  - `tests/core/test_encoding.c` —— 日文字符串测试范本（Node.Name 测试参考）。

  **External References**：
  - IEEE 754-2008 half-precision spec（subnormal 处理参考）。
  - 上游 `BinaryReaderEx.ReadHalf` 实现（参考 fallback 路径）。

  **WHY Each Reference Matters**：
  - half-float fallback 必须与上游 ReadHalf 字节级输出一致；否则 c0000.flver round-trip 在 Half2/Half4 处全失败。
  - 11_11_10 helper 提取是必要 DRY：Phase 1 已实现一次，本 task 必须复用而非重复造轮；Phase 1 binary_reader 改为调用 sf_unpack_11_11_10。
  - Node.Name 字段需经过 sf_encoding，避免日文字符在中间环节损坏。

  **Acceptance Criteria**：
  - [ ] `src/geom/flver_common.c` 编译通过（`cmake --build build-mingw`，无 -Werror 报错）。
  - [ ] `tests/geom/test_flver_common.c` 注册 label `geom` 且 `ctest -L geom -R flver_common` 全 PASS。
  - [ ] half-float 测试至少 16 个 golden 值 PASS（包括 NaN / inf / subnormal）。
  - [ ] 11_11_10 测试至少 8 个 golden 值 PASS。
  - [ ] `sf_binary_reader_read_vec3_11_11_10` 内部改为调用 `sf_unpack_11_11_10`（DRY check：`grep -c '11_11_10' src/core/binary_reader.c` 应 ≤ 2）。
  - [ ] Dummy / Node / LayoutMember 各一个 round-trip 字节级一致测试 PASS。
  - [ ] Node round-trip 字节数 = **128 字节**（验证 `Node.cs:137-156` 字段布局正确）；含日文 name `骨头` 的 round-trip。

  **QA Scenarios**：

  ```
  Scenario: half-float intrinsic + fallback 一致性
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. `cmake --build build-mingw --target souls_formats_test_flver_common`
      2. `ctest -R flver_common --output-on-failure 2>&1 | tee .sisyphus/evidence/task-8-flver-common.log`
      3. `grep -c 'PASS' .sisyphus/evidence/task-8-flver-common.log`
    Expected Result: PASS 数 ≥ 30（16 half-float + 8 11_11_10 + Dummy + Node + LayoutMember + LayoutType size 各 1）
    Failure Indicators: 任一 FAIL；或 IEEE 754 边界（NaN / inf / subnormal）输出与上游不一致
    Evidence: .sisyphus/evidence/task-8-flver-common.log

  Scenario: Phase 1 binary_reader 重构后既有测试不退化
    Tool: Bash
    Preconditions: T8 落地（包括 binary_reader 重构）
    Steps:
      1. `ctest -L core --output-on-failure 2>&1 | tee .sisyphus/evidence/task-8-core-regression.log`
      2. `grep -E '0 tests failed' .sisyphus/evidence/task-8-core-regression.log`
    Expected Result: 步骤 2 命中（Phase 1 测试全 PASS）
    Failure Indicators: 任一 Phase 1 测试退化
    Evidence: .sisyphus/evidence/task-8-core-regression.log

  Scenario: Edge layout type 触发错误而非 crash
    Tool: Bash
    Steps:
      1. 单元测试：调 `sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED, 0)`
      2. assert 返回值为 sentinel（SIZE_MAX 或 0）且不 abort
    Expected Result: graceful sentinel return
    Failure Indicators: crash / abort
    Evidence: .sisyphus/evidence/task-8-edge-graceful.log
  ```

  **Commit**: YES
  - Message: `phase6(flver-common): flver_common.c implementations + IEEE 754 half-float + 11_11_10 helpers`
  - Files: `src/geom/flver_common.c`, `src/internal/flver_common_internal.h`, `tests/geom/test_flver_common.c`, `tests/CMakeLists.txt`, `src/core/binary_reader.c`（DRY 重构）
  - Pre-commit: `ctest -L 'core|geom' -R 'binary_reader|flver_common'` 全 PASS

- [x] 9. **`sf_flver2.h` —— opaque FLVER2 / Mesh / Material / Texture / FaceSet / VertexBuffer / BufferLayout / SkeletonSet / Bone / GXList + 共享 accessor 签名（含全局索引模式）**

  **What to do**：
  - 起草 `include/souls_formats/sf_flver2.h`：
    - **Opaque forward declarations**：`sf_flver2_t`, `sf_flver2_mesh_t`, `sf_flver2_material_t`, `sf_flver2_texture_t`, `sf_flver2_face_set_t`, `sf_flver2_vertex_buffer_t`, `sf_flver2_buffer_layout_t`, `sf_flver2_skeleton_set_t`, `sf_flver2_bone_t`, `sf_flver2_gx_list_t`, `sf_flver2_gx_item_t`。
    - **公共 enum**：
      - `sf_flver2_tiling_type_t`（对应上游 `Texture.cs:TilingType`）。
      - `sf_flver2_fs_flags_t`（对应上游 `FaceSet.cs:FSFlags` —— 6 值 uint）。
    - 每枚举后置 `_Static_assert`。
    - **公共 API**：
      ```c
      /* Read / Write */
      SF_API sf_result_t sf_flver2_read_from_memory(sf_flver2_t **out, const void *bytes, size_t size, const sf_allocator_t *a);
      SF_API sf_result_t sf_flver2_read_from_path  (sf_flver2_t **out, const wchar_t *path,         const sf_allocator_t *a);
      SF_API sf_result_t sf_flver2_write_to_memory (const sf_flver2_t *f, void **out_bytes, size_t *out_size, const sf_allocator_t *a);
      SF_API sf_result_t sf_flver2_write_to_path   (const sf_flver2_t *f, const wchar_t *path);
      SF_API void        sf_flver2_destroy         (sf_flver2_t *f);

      /* 顶层 count accessor */
      SF_API uint32_t sf_flver2_header_version  (const sf_flver2_t *f);
      SF_API size_t   sf_flver2_dummy_count     (const sf_flver2_t *f);
      SF_API size_t   sf_flver2_node_count      (const sf_flver2_t *f);  /* aka bone_count for FLVER0 alias */
      SF_API size_t   sf_flver2_material_count  (const sf_flver2_t *f);
      SF_API size_t   sf_flver2_mesh_count      (const sf_flver2_t *f);
      SF_API size_t   sf_flver2_buffer_layout_count(const sf_flver2_t *f);
      SF_API size_t   sf_flver2_vertex_buffer_count(const sf_flver2_t *f);
      SF_API size_t   sf_flver2_face_set_count  (const sf_flver2_t *f);  /* global; mesh references by index */
      SF_API size_t   sf_flver2_texture_count   (const sf_flver2_t *f);

      /* Index accessor（注意 buffer_layout / vertex_buffer / face_set 是 mesh 间共享，按 global index 引用，不是 mesh-1:1） */
      SF_API const sf_flver_dummy_t                *sf_flver2_dummy        (const sf_flver2_t *f, size_t i);
      SF_API const sf_flver_node_t                 *sf_flver2_node         (const sf_flver2_t *f, size_t i);
      SF_API const sf_flver2_material_t            *sf_flver2_material     (const sf_flver2_t *f, size_t i);
      SF_API const sf_flver2_mesh_t                *sf_flver2_mesh         (const sf_flver2_t *f, size_t i);
      SF_API const sf_flver2_buffer_layout_t       *sf_flver2_buffer_layout(const sf_flver2_t *f, size_t i);
      SF_API const sf_flver2_vertex_buffer_t       *sf_flver2_vertex_buffer(const sf_flver2_t *f, size_t i);
      SF_API const sf_flver2_face_set_t             *sf_flver2_face_set    (const sf_flver2_t *f, size_t i);

      /* GXList opaque transit */
      SF_API const sf_flver2_gx_list_t  *sf_flver2_gx_list      (const sf_flver2_t *f, size_t mesh_index);
      SF_API size_t                      sf_flver2_gx_list_item_count(const sf_flver2_gx_list_t *gx);
      SF_API const sf_flver2_gx_item_t  *sf_flver2_gx_item      (const sf_flver2_gx_list_t *gx, size_t i);
      SF_API uint32_t                    sf_flver2_gx_item_id   (const sf_flver2_gx_item_t *item);
      SF_API uint32_t                    sf_flver2_gx_item_unk04(const sf_flver2_gx_item_t *item);
      SF_API const uint8_t              *sf_flver2_gx_item_data (const sf_flver2_gx_item_t *item, size_t *out_size);  /* opaque bytes */

      /* SkeletonSet（可能不存在；version >= 0x2001A 才有；上游 SkeletonSet.cs 含 BaseSkeleton + AllSkeletons 双 List<Bone>） */
      SF_API const sf_flver2_skeleton_set_t *sf_flver2_skeleton_set(const sf_flver2_t *f);  /* NULL if version < 0x2001A */
      /* BaseSkeleton（标准骨架，对应 node hierarchy）*/
      SF_API size_t                          sf_flver2_skeleton_set_base_count(const sf_flver2_skeleton_set_t *set);
      SF_API const sf_flver2_bone_t         *sf_flver2_skeleton_set_base_bone (const sf_flver2_skeleton_set_t *set, size_t i);
      /* AllSkeletons（含 control rig 与 ragdoll 全部骨架）*/
      SF_API size_t                          sf_flver2_skeleton_set_all_count (const sf_flver2_skeleton_set_t *set);
      SF_API const sf_flver2_bone_t         *sf_flver2_skeleton_set_all_bone  (const sf_flver2_skeleton_set_t *set, size_t i);
      /* Bone 字段 accessor（上游 SkeletonSet.cs:84-110，5 个 short/int 字段）*/
      SF_API int16_t  sf_flver2_bone_parent_index           (const sf_flver2_bone_t *b);
      SF_API int16_t  sf_flver2_bone_first_child_index      (const sf_flver2_bone_t *b);
      SF_API int16_t  sf_flver2_bone_next_sibling_index     (const sf_flver2_bone_t *b);
      SF_API int16_t  sf_flver2_bone_previous_sibling_index (const sf_flver2_bone_t *b);
      SF_API int32_t  sf_flver2_bone_node_index             (const sf_flver2_bone_t *b);  /* 指向 FLVER2.Nodes list */

      /* Mesh 字段 accessor —— 共享对象通过 index 引用 */
      SF_API int32_t sf_flver2_mesh_material_index    (const sf_flver2_mesh_t *m);
      SF_API int32_t sf_flver2_mesh_node_index        (const sf_flver2_mesh_t *m);  /* 上游 Mesh.NodeIndex（不是 DefaultBoneIndex）；-1 表示无默认 node */
      SF_API size_t  sf_flver2_mesh_bone_index_count  (const sf_flver2_mesh_t *m);
      SF_API int32_t sf_flver2_mesh_bone_index        (const sf_flver2_mesh_t *m, size_t i);
      SF_API size_t  sf_flver2_mesh_face_set_index_count(const sf_flver2_mesh_t *m);
      SF_API int32_t sf_flver2_mesh_face_set_index    (const sf_flver2_mesh_t *m, size_t i);
      SF_API size_t  sf_flver2_mesh_vertex_buffer_index_count(const sf_flver2_mesh_t *m);
      SF_API int32_t sf_flver2_mesh_vertex_buffer_index(const sf_flver2_mesh_t *m, size_t i);

      /* Decode helper（EXTENSION，docs/api-mapping/extensions.md 录入）*/
      typedef struct sf_flver2_decoded_mesh {
          uint32_t   vertex_count;
          sf_vec3_t *positions;
          sf_vec3_t *normals;
          sf_vec4_t *tangents;
          sf_vec3_t *bitangents;
          sf_vec2_t *uvs[8];
          sf_flver_vertex_color_t *colors[4];
          sf_flver_vertex_bone_indices_t *bone_indices;
          sf_flver_vertex_bone_weights_t *bone_weights;
          uint32_t  *indices;
          uint32_t   index_count;
      } sf_flver2_decoded_mesh_t;
      SF_API sf_result_t sf_flver2_decode_mesh(const sf_flver2_t *f, size_t mesh_index, sf_flver2_decoded_mesh_t *out, const sf_allocator_t *a);
      SF_API void        sf_flver2_decoded_mesh_free(sf_flver2_decoded_mesh_t *m, const sf_allocator_t *a);
      ```

  **Must NOT do**：
  - ❌ 不暴露任何 Edge geometry / SPU / RSX 类型或 enum。
  - ❌ 不暴露 Material / Texture / FaceSet 等 opaque 类型的内部字段直接 struct；全部 accessor。
  - ❌ 不暴露 BE 支持（不出现 `sf_flver2_set_big_endian` 之类 API）。
  - ❌ 不为 mesh 提供 `sf_flver2_mesh_buffer_layout()` 这种「直接拿 layout」的 1:1 accessor —— 必须 mesh→index→`sf_flver2_buffer_layout(f, idx)` 两段式（与上游 global 共享一致）。
  - ❌ 不提供 mesh 修改 API（add_face_set / remove_vertex_buffer 等）；v1 仅支持完整 round-trip（读 → 写），不支持原地编辑。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 头文件起草 + 大量 accessor 声明 + `_Static_assert`。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T10, T11 并行；T7 阻塞）
  - **Parallel Group**: Wave 1
  - **Blocks**: T12-T18（FLVER2 子模块全部）, T21, T22, T25, T26
  - **Blocked By**: T7（公共 FLVER 类型先就位）

  **References**：

  **Pattern References**：
  - `include/souls_formats/sf_msbe.h` —— Phase 5 大量 opaque + accessor 模式范本（最近一次同款分层 header）。
  - `include/souls_formats/sf_bnd4.h` —— opaque 容器 + index-based accessor。
  - `include/souls_formats/sf_param.h` —— 大对象 + 子对象 accessor 范本（按 index 取行 + 取字段）。

  **API/Type References**：
  - `Formats/FLVER/FLVER2/FLVER2.cs:全部 class 与字段` —— 顶层 FLVER2 字段次序与 count 列表的 ground truth。
  - `Formats/FLVER/FLVER2/Mesh.cs:Mesh 字段` —— Mesh 引用的 face_set / vertex_buffer / bone index 列表是 global 索引（与上游一致）。
  - `Formats/FLVER/FLVER2/GXList.cs:全文` —— GXList = `List<GXItem>`；GXItem.Data 是 `byte[]`（opaque）。
  - `Formats/FLVER/FLVER2/Texture.cs:TilingType enum`。
  - `Formats/FLVER/FLVER2/FaceSet.cs:FSFlags enum`（6 值）。
  - `Formats/FLVER/FLVER2/SkeletonSet.cs:全文` —— SkeletonSet = **BaseSkeleton + AllSkeletons** 双 `List<Bone>`（每个 Bone 是完整 struct，不是 index）。

  **Test References**：无（T22-T26 才是测试）。

  **External References**：
  - Metis 报告：BufferLayout / VertexBuffer 是 mesh 间共享，按 global index 引用。
  - AGENTS.md §5.x：API MIRRORS UPSTREAM 强约束。

  **WHY Each Reference Matters**：
  - 「共享对象 + index 引用」是 FLVER2 与 BND4 的关键不同点；BND4 是 mesh ⊥ entry 1:1，FLVER2 是 mesh→{layout_idx, buffer_idx, face_set_idx[]}。误设计为 1:1 = 上游字节流意义偏差 = 全部 round-trip 失败。
  - GXItem.Data opaque 是 Metis 实测发现；避免 sneak in 结构化解析。
  - SkeletonSet 可能 NULL（Sekiro 0x20013）—— accessor 必须设计为可返回 NULL 而非 0 个 bone。

  **Acceptance Criteria**：
  - [ ] `include/souls_formats/sf_flver2.h` 提交且 header smoke build 通过（C + C++ 双向 include）。
  - [ ] `grep 'SF_API' include/souls_formats/sf_flver2.h` ≥ 40（accessor + decode 共计）。
  - [ ] `grep '_Static_assert' include/souls_formats/sf_flver2.h` ≥ 2（TilingType + FSFlags 各一）。
  - [ ] 无任何 Edge / SPU / RSX 关键字：`grep -ciE 'edge|spu|rsx' include/souls_formats/sf_flver2.h` ≤ 1（仅 OUT-of-scope 注释）。
  - [ ] `sf_flver2_buffer_layout(f, i)` / `sf_flver2_vertex_buffer(f, i)` / `sf_flver2_face_set(f, i)` 三个 global accessor 存在；Mesh 上对应字段名为 `*_index`（而非直接返回对象指针）。
  - [ ] `sf_flver2_skeleton_set(f)` 文档化 NULL 返回路径。
  - [ ] `sf_flver2_decoded_mesh_t` struct 定义存在且与本 task 描述一致。

  **QA Scenarios**：

  ```
  Scenario: header smoke build (C + C++)
    Tool: Bash
    Steps: 同 T7 模式，跑 C 与 C++ 各一次 include
    Expected Result: 两个编译通过
    Failure Indicators: typedef 冲突 / `extern "C"` 漏 / 缺失 `<stdbool.h>`
    Evidence: .sisyphus/evidence/task-9-header-smoke.log

  Scenario: global-shared accessor 设计正确
    Tool: Bash
    Steps:
      1. `grep -E 'sf_flver2_mesh_(buffer_layout|vertex_buffer|face_set)\(' include/souls_formats/sf_flver2.h; echo "exit=$?"`
      2. `grep -E 'sf_flver2_(buffer_layout|vertex_buffer|face_set)\(const sf_flver2_t' include/souls_formats/sf_flver2.h | wc -l`
      3. `grep -E 'sf_flver2_mesh_(face_set|vertex_buffer)_index\(' include/souls_formats/sf_flver2.h | wc -l`
    Expected Result: 步骤 1 退出码 1（无 1:1 mesh accessor）；步骤 2 = 3（3 个 global accessor 都在）；步骤 3 ≥ 2（mesh 给 index 而非对象）
    Failure Indicators: 步骤 1 退出码 0（设计错为 1:1）
    Evidence: .sisyphus/evidence/task-9-global-accessor.log

  Scenario: Edge 暴露面最小化
    Tool: Bash
    Steps:
      1. `grep -ciE 'edge|spu|rsx' include/souls_formats/sf_flver2.h`
    Expected Result: ≤ 1
    Failure Indicators: > 1
    Evidence: .sisyphus/evidence/task-9-edge-exposure.log
  ```

  **Commit**: YES
  - Message: `phase6(flver2): sf_flver2.h with opaque types + global-shared accessors + decode struct`
  - Files: `include/souls_formats/sf_flver2.h`, `CMakeLists.txt`, `include/souls_formats/souls_formats.h`
  - Pre-commit: header-only smoke build + grep guard

- [x] 10. **`sf_mtd.h` —— opaque MTD / Param / Texture + BlendMode / LightingType 枚举 + 公共 API**

  **What to do**：
  - 起草 `include/souls_formats/sf_mtd.h`：
    - **Opaque forward declarations**：`sf_mtd_t`, `sf_mtd_param_t`, `sf_mtd_texture_t`。
    - **公共 enum**（精确对照 `MTD.cs`）：
      - `sf_mtd_param_type_t`：上游 MTD.Param.ParamType（如 Int, Float, Bool 等；按上游具体定义对齐）。
      - `sf_mtd_blend_mode_t`：上游 BlendMode。
      - `sf_mtd_lighting_type_t`：上游 LightingType。
    - 每枚举后置 `_Static_assert`。
    - **公共 API**：
      ```c
      /* Read / Write */
      SF_API sf_result_t sf_mtd_read_from_memory(sf_mtd_t **out, const void *bytes, size_t size, const sf_allocator_t *a);
      SF_API sf_result_t sf_mtd_read_from_path  (sf_mtd_t **out, const wchar_t *path,         const sf_allocator_t *a);
      SF_API sf_result_t sf_mtd_write_to_memory (const sf_mtd_t *mtd, void **out_bytes, size_t *out_size, const sf_allocator_t *a);
      SF_API sf_result_t sf_mtd_write_to_path   (const sf_mtd_t *mtd, const wchar_t *path);
      SF_API void        sf_mtd_destroy         (sf_mtd_t *mtd);

      /* 顶层字段 accessor */
      SF_API const char *sf_mtd_shader_path(const sf_mtd_t *mtd);  /* UTF-8, owned by MTD */
      SF_API const char *sf_mtd_description(const sf_mtd_t *mtd);

      /* Params */
      SF_API size_t                   sf_mtd_param_count(const sf_mtd_t *mtd);
      SF_API const sf_mtd_param_t    *sf_mtd_param      (const sf_mtd_t *mtd, size_t i);
      SF_API const char              *sf_mtd_param_name (const sf_mtd_param_t *p);  /* UTF-8 */
      SF_API sf_mtd_param_type_t      sf_mtd_param_type (const sf_mtd_param_t *p);
      SF_API sf_result_t              sf_mtd_param_value_int  (const sf_mtd_param_t *p, int32_t *out);
      SF_API sf_result_t              sf_mtd_param_value_float(const sf_mtd_param_t *p, float   *out);
      SF_API sf_result_t              sf_mtd_param_value_bool (const sf_mtd_param_t *p, bool    *out);
      /* 其他 ParamType 按上游需要 */

      /* Textures */
      SF_API size_t                  sf_mtd_texture_count(const sf_mtd_t *mtd);
      SF_API const sf_mtd_texture_t *sf_mtd_texture      (const sf_mtd_t *mtd, size_t i);
      SF_API const char             *sf_mtd_texture_type (const sf_mtd_texture_t *t);  /* 上游 Type 字段，如 "g_DiffuseTexture" */
      SF_API int32_t                 sf_mtd_texture_uv_number          (const sf_mtd_texture_t *t);
      SF_API int32_t                 sf_mtd_texture_shader_data_index  (const sf_mtd_texture_t *t);
      /* Sekiro Extended 字段（textureBlock.Version == 5 才有；对照 MTD.cs:385-407） */
      SF_API bool                    sf_mtd_texture_has_extended (const sf_mtd_texture_t *t);  /* version 5 = true，version 3 = false */
      SF_API const char             *sf_mtd_texture_path         (const sf_mtd_texture_t *t);  /* Extended 时含 path；non-Extended 时返回空串 */
      SF_API size_t                  sf_mtd_texture_unk_float_count(const sf_mtd_texture_t *t);
      SF_API float                   sf_mtd_texture_unk_float    (const sf_mtd_texture_t *t, size_t i);
      ```

  **Must NOT do**：
  - ❌ 不暴露内部 hierarchical block 结构（MTD 是嵌套块格式，但消费者关心 flat fields）。
  - ❌ 不为 MTD 提供修改 API（read + write 完整对象，不原地编辑）。
  - ❌ 不假设 BlendMode 与 LightingType 是相同 enum（上游是分开两个 enum）。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 头文件起草。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T9, T11 并行）
  - **Parallel Group**: Wave 1
  - **Blocks**: T19, T23
  - **Blocked By**: T7（FLVER 公共 enum 在 T7 已定）

  **References**：

  **Pattern References**：
  - `include/souls_formats/sf_fmg.h`（Phase 4 完成）—— opaque + accessor + 字符串字段 owner 语义模式。
  - `include/souls_formats/sf_paramdef.h` —— enum + `_Static_assert` + 子对象 accessor。

  **API/Type References**：
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MTD.cs:全文` —— Class 字段次序 + Param/Texture 子结构 + BlendMode/LightingType enum。

  **WHY Each Reference Matters**：
  - MTD 字段映射相对线性（无 cross-format 依赖），主要工作是上游 → C 的 1:1 对齐。
  - Sekiro `Extended` texture info 是 textureBlock.Version == 5 特有；header 必须暴露 `has_extended` + `path` + `unk_float_count` + `unk_float` 四类 accessor（**非 `extended_scale`**，上游字段是 `Path` + `UnkFloats`，详见 T19）。

  **Acceptance Criteria**：
  - [ ] `include/souls_formats/sf_mtd.h` 提交且 header smoke build 通过。
  - [ ] `grep '_Static_assert' include/souls_formats/sf_mtd.h` ≥ 3（ParamType + BlendMode + LightingType）。
  - [ ] `grep 'SF_API' include/souls_formats/sf_mtd.h` ≥ 15。
  - [ ] 含 `sf_mtd_texture_has_extended` + `sf_mtd_texture_path` + `sf_mtd_texture_unk_float_count` + `sf_mtd_texture_unk_float`（Sekiro Extended 字段，对照 `MTD.cs:385-413`）。**不**含 `sf_mtd_texture_extended_scale`（上游无此字段）。

  **QA Scenarios**：

  ```
  Scenario: header smoke build
    Tool: Bash
    Steps: 同 T7 模式
    Expected Result: C + C++ 双向通过
    Evidence: .sisyphus/evidence/task-10-header-smoke.log

  Scenario: 上游 enum 对齐
    Tool: Bash
    Steps:
      1. `grep -oE '(BlendMode|LightingType|ParamType)\s*[:{]' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MTD.cs | wc -l`
      2. `grep -E 'enum sf_mtd_(blend_mode|lighting_type|param_type)' include/souls_formats/sf_mtd.h | wc -l`
    Expected Result: 两步均 = 3
    Failure Indicators: 不一致
    Evidence: .sisyphus/evidence/task-10-enum-align.log
  ```

  **Commit**: YES
  - Message: `phase6(mtd): sf_mtd.h public API with Sekiro Extended texture support`
  - Files: `include/souls_formats/sf_mtd.h`, `CMakeLists.txt`, `include/souls_formats/souls_formats.h`
  - Pre-commit: header smoke build

- [x] 11. **`sf_matbin.h` —— opaque MATBIN / Param / Sampler + 8 个 ParamType 枚举 + 公共 API**

  **What to do**：
  - 起草 `include/souls_formats/sf_matbin.h`：
    - **Opaque forward declarations**：`sf_matbin_t`, `sf_matbin_param_t`, `sf_matbin_sampler_t`。
    - **公共 enum**（**严格对照 `MATBIN.cs:126-167`**）：
      - `sf_matbin_param_type_t`（**8 个非连续值**，跳号 1/2/3/6/7）：
        - `SF_MATBIN_PARAM_TYPE_BOOL = 0`
        - `SF_MATBIN_PARAM_TYPE_INT = 4`
        - `SF_MATBIN_PARAM_TYPE_INT2 = 5`
        - `SF_MATBIN_PARAM_TYPE_FLOAT = 8`
        - `SF_MATBIN_PARAM_TYPE_FLOAT2 = 9`
        - `SF_MATBIN_PARAM_TYPE_FLOAT3 = 10`
        - `SF_MATBIN_PARAM_TYPE_FLOAT4 = 11`
        - `SF_MATBIN_PARAM_TYPE_FLOAT5 = 12`
        - **不使用 `_COUNT` sentinel**（枚举值非连续，会误导）；改用：
          - `_Static_assert(SF_MATBIN_PARAM_TYPE_FLOAT5 == 12, "MATBIN ParamType drift")`
    - **公共 API**：
      ```c
      /* Read / Write */
      SF_API sf_result_t sf_matbin_read_from_memory(sf_matbin_t **out, const void *bytes, size_t size, const sf_allocator_t *a);
      SF_API sf_result_t sf_matbin_read_from_path  (sf_matbin_t **out, const wchar_t *path,         const sf_allocator_t *a);
      SF_API sf_result_t sf_matbin_write_to_memory (const sf_matbin_t *m, void **out_bytes, size_t *out_size, const sf_allocator_t *a);
      SF_API sf_result_t sf_matbin_write_to_path   (const sf_matbin_t *m, const wchar_t *path);
      SF_API void        sf_matbin_destroy         (sf_matbin_t *m);

      /* 顶层字段 */
      SF_API const char *sf_matbin_shader_path(const sf_matbin_t *m);
      SF_API const char *sf_matbin_source_path(const sf_matbin_t *m);
      SF_API uint32_t    sf_matbin_key        (const sf_matbin_t *m);  /* upstream Key field */

      /* Params */
      SF_API size_t                       sf_matbin_param_count(const sf_matbin_t *m);
      SF_API const sf_matbin_param_t     *sf_matbin_param      (const sf_matbin_t *m, size_t i);
      SF_API const char                  *sf_matbin_param_name (const sf_matbin_param_t *p);
      SF_API sf_matbin_param_type_t       sf_matbin_param_type (const sf_matbin_param_t *p);
      SF_API uint32_t                     sf_matbin_param_key  (const sf_matbin_param_t *p);
      /* 8 个 typed value accessor（按上游 union 字段次序）*/
      SF_API sf_result_t sf_matbin_param_value_bool   (const sf_matbin_param_t *p, bool *out);
      SF_API sf_result_t sf_matbin_param_value_int    (const sf_matbin_param_t *p, int32_t *out);
      SF_API sf_result_t sf_matbin_param_value_int2   (const sf_matbin_param_t *p, int32_t out[2]);
      SF_API sf_result_t sf_matbin_param_value_float  (const sf_matbin_param_t *p, float *out);
      SF_API sf_result_t sf_matbin_param_value_float2 (const sf_matbin_param_t *p, float out[2]);
      SF_API sf_result_t sf_matbin_param_value_float3 (const sf_matbin_param_t *p, float out[3]);
      SF_API sf_result_t sf_matbin_param_value_float4 (const sf_matbin_param_t *p, float out[4]);
      SF_API sf_result_t sf_matbin_param_value_float5 (const sf_matbin_param_t *p, float out[5]);
      /* 错误返回 SF_ERR_INVALID_ARG 若 ParamType 不匹配 */

      /* Samplers */
      SF_API size_t                   sf_matbin_sampler_count(const sf_matbin_t *m);
      SF_API const sf_matbin_sampler_t *sf_matbin_sampler    (const sf_matbin_t *m, size_t i);
      SF_API const char               *sf_matbin_sampler_type(const sf_matbin_sampler_t *s);
      SF_API const char               *sf_matbin_sampler_path(const sf_matbin_sampler_t *s);
      SF_API uint32_t                  sf_matbin_sampler_key (const sf_matbin_sampler_t *s);
      SF_API sf_result_t               sf_matbin_sampler_unk14(const sf_matbin_sampler_t *s, sf_vec2_t *out);
      ```

  **Must NOT do**：
  - ❌ 不添加 String / Vec3 / 其他 Metis 已排除的 ParamType 变体。
  - ❌ 不暴露上游内部「offset 表」结构；消费者只看 flat fields。
  - ❌ 不对 sampler.Path 做任何 path 转换 / 编码假设；按上游字节保留。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 头文件起草。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T9, T10 并行）
  - **Parallel Group**: Wave 1
  - **Blocks**: T20, T24, T27
  - **Blocked By**: T5（probe 验证 ParamType 8 假设）

  **References**：

  **Pattern References**：
  - `include/souls_formats/sf_mtd.h:T10` —— sister header 的 enum + accessor 模式。
  - Phase 4 `include/souls_formats/sf_param.h` —— union-style typed value accessor 模式。

  **API/Type References**：
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MATBIN.cs:全文` —— Class 字段次序 + Param/Sampler 子结构。
  - `MATBIN.cs:Param.ParamType` —— 8 个 ParamType 值的 byte mapping。

  **External References**：
  - T5 probe 输出（`.sisyphus/evidence/task-5-matbin-survey.md`）—— 实测验证 8 个 ParamType 假设无遗漏。

  **WHY Each Reference Matters**：
  - 8 个 ParamType 是 Metis + T5 probe 双重确认的 ground truth；少一个 = 实际 MATBIN 读不出来；多一个 = 写出与上游字节不一致。
  - typed value accessor 比 union struct 暴露更 ABI-stable（消费者不会因 union 增删而 break）。

  **Acceptance Criteria**：
  - [ ] `include/souls_formats/sf_matbin.h` 提交且 header smoke build 通过。
  - [ ] `grep -cE 'SF_MATBIN_PARAM_TYPE_(BOOL|INT2?|FLOAT[2-5]?)\s*=' include/souls_formats/sf_matbin.h` = 8（8 个变体）。
  - [ ] 8 个 typed value accessor 存在：`grep -c 'sf_matbin_param_value_' include/souls_formats/sf_matbin.h` ≥ 8。
  - [ ] `_Static_assert(SF_MATBIN_PARAM_TYPE_FLOAT5 == 12, ...)` 出现（替代 COUNT sentinel，因为枚举值非连续）。
  - [ ] **byte 值正确**：`grep -E '_BOOL = 0' include/souls_formats/sf_matbin.h && grep -E '_INT = 4' && grep -E '_INT2 = 5' && grep -E '_FLOAT = 8' && grep -E '_FLOAT2 = 9' && grep -E '_FLOAT3 = 10' && grep -E '_FLOAT4 = 11' && grep -E '_FLOAT5 = 12'` 全部命中。
  - [ ] **不出现** `SF_MATBIN_PARAM_TYPE_STRING` 或 `_VEC3`：`grep -E 'SF_MATBIN_PARAM_TYPE_(STRING|VEC3)' include/souls_formats/sf_matbin.h` 输出空。

  **QA Scenarios**：

  ```
  Scenario: 8 个 ParamType 全且仅
    Tool: Bash
    Steps:
      1. `grep -oE 'SF_MATBIN_PARAM_TYPE_[A-Z0-9_]+' include/souls_formats/sf_matbin.h | grep -v COUNT | sort -u | tee .sisyphus/evidence/task-11-paramtypes.log`
      2. `wc -l < .sisyphus/evidence/task-11-paramtypes.log`
    Expected Result: 步骤 2 = 8
    Failure Indicators: ≠ 8（多或少）
    Evidence: .sisyphus/evidence/task-11-paramtypes.log

  Scenario: 与 T5 probe 输出一致
    Tool: Bash
    Preconditions: T5 evidence 存在
    Steps:
      1. `grep -oE 'PARAMTYPE[_:]\s*[A-Z][a-zA-Z0-9]*' .sisyphus/evidence/task-5-matbin-survey.txt | sort -u | tee .sisyphus/evidence/task-11-probe-types.log`
      2. 手工 diff 与 task-11-paramtypes.log
    Expected Result: 集合相等（仅大小写 / 前缀差异）
    Failure Indicators: probe 出现的 type 不在 header 里 → header 漏；header 有但 probe 没出现 → 可能 c0000 用不到但仍合规
    Evidence: 同上
  ```

  **Commit**: YES
  - Message: `phase6(matbin): sf_matbin.h with 8 ParamType variants (Bool/Int/Int2/Float/Float2-5)`
  - Files: `include/souls_formats/sf_matbin.h`, `CMakeLists.txt`, `include/souls_formats/souls_formats.h`
  - Pre-commit: header smoke build + ParamType count check

### Wave 2 — FLVER2 子模块矩阵（Wave 1 全绿后 7 路并行）

- [x] 12. **`src/geom/flver2.c` —— top-level dispatch + FLVERHeader + GXList opaque transit + 整体 read/write 流程**

  **What to do**：
  - 创建 `src/geom/flver2.c`，实现 `sf_flver2_read_from_memory` / `_from_path` / `_write_to_memory` / `_to_path` / `_destroy`：
    - **Read 流程**（**严格 mirror `FLVER2.cs:90-217`**）：
      1. 读 magic `"FLVER\0"` (6 字节)，`br.AssertASCII("FLVER\0")` 等价。
      2. 读 **endian marker (2 字节 ASCII)** at offset 0x06-0x07：上游 `FLVER2.cs:95` `br.AssertASCII("L\0", "B\0")`。**`"L\0"` → 继续解析；`"B\0"` → 返回 `SF_ERR_UNSUPPORTED_VERSION`**（v1 拒绝 BE，对应 extensions.md 第 2 条 entry）。任何其他字节组合 → `SF_ERR_BAD_MAGIC`。
      3. 读 FLVERHeader（offset 0x08 起）：version / dataOffset / dataLength / count fields / bbox / Unicode flag / 等 —— 字段次序对照 `FLVER2.cs:109-134` + `:467 FLVERHeader`。
      4. **Header version whitelist 检查**（按 `FLVER2.cs:109`）：version ∈ {`0x20005, 0x20007, 0x20009, 0x2000B, 0x2000C, 0x2000D, 0x2000E, 0x2000F, 0x20010, 0x20013, 0x20014, 0x20016, 0x20017, 0x2001A, 0x2001B, 0x20021`} → 否则返回 `SF_ERR_UNSUPPORTED_VERSION`。
      5. **段读取顺序**（每段直接按 reader 当前位置读，**不靠 offset 跳转**，因为段间是顺序）：
         - Dummies（顶层 `List<Dummy>`）
         - Materials（**仅读基础字段** —— nameOffset / mtdOffset / textureCount / textureIndex / numStringBytes / gxOffset / Index / 0；GXList 内嵌读但 Texture 不内嵌）
         - Nodes（顶层 `List<Node>`，亦 FLVER1 alias 为 "Bone"）
         - Meshes
         - FaceSets
         - VertexBuffers
         - BufferLayouts
         - **Textures（全局 list，非 per-Material embedded）** ← 上游 `FLVER2.cs:191-193`
         - **SkeletonSet（条件：`Header.Version >= 0x2001A` 才读）** ← 上游 `FLVER2.cs:195-196`
      6. **TakeTextures 分发**：读完 textures 后，对每个 Material 调 `TakeTextures(textureDict)`，按 (textureIndex, textureCount) 把 global Texture 子集分发给 Material（上游 `FLVER2.cs:198-204`，`Material.cs:125-139`）。textureDict 用 `SFUtil.Dictionize` 等价（按 read 顺序 i 编号）。**完成后 textureDict 应为空**（孤立 Texture → `SF_ERR_INTERNAL` "orphaned texture"）。
      7. **FaceSet / VertexBuffer 分发**：对每个 Mesh 调对应 `TakeFaceSets` / `TakeVertexBuffers`（上游 `FLVER2.cs:206-213`），按 index 子集分发。
      8. GXList：在 Material read 时若 `gxOffset != 0` → `StepIn(gxOffset)` 读一个 GXList 入全局 `gxLists`，并通过 `gxListIndices: Dictionary<int, int>` 去重（同一 offset 仅读一次）；Material.GXIndex 指向 `gxLists` 中的索引。GXItem.Data 保留 opaque `uint8_t*` + size。
    - **Write 流程**（**严格 mirror `FLVER2.cs:Write()` 上游 line ~230-440**）：
      1. 写 magic `"FLVER\0"` (6 字节) + endian marker **`"L\0"` (2 字节 ASCII)**（v1 永远写 LE，对应 `FLVER2.cs:228-229` 的 `bw.WriteASCII("L\0")` 等价）。
      2. 写 FLVERHeader（version, count fields；reserve offsets 用 `sf_binary_writer_reserve_u32` 留位）。
      3. 段写入顺序：Dummies → Materials（带 reserved offsets）→ Nodes → Meshes → FaceSets → VertexBuffers → BufferLayouts → **Textures（全局 list，按 Material 顺序展开）** → SkeletonSet（版本条件）→ GXLists → strings 段 → vertex/index 数据段。
      4. 每个 reserve 用 `_fill_u32` 回填实际 offset；Material.WriteTextures 时根据 textureIndex base 偏移分配 sub-range（`Material.cs:161-166`）。
      5. `sf_binary_writer_finish` 验证 reserve 全 fill。
    - **`sf_flver2_destroy`**：释放所有子对象 + GXList opaque buffer + 字符串。
  - 实现公共 accessor 中的顶层 count / version：`sf_flver2_header_version` / `_dummy_count` 等。

  **Must NOT do**：
  - ❌ 不在 flver2.c 实现 Material/Mesh/FaceSet 等子模块的字节解析（留给 T13-T18）；只做 dispatch + 顶层。
  - ❌ 不结构化解析 GXItem.Data —— 必须 opaque 透传。
  - ❌ 不接受 BE 字节序文件（offset 0x06 探测后立即拒绝）。
  - ❌ 不重算 bounding box；写出原样。
  - ❌ 不为未知 version 「猜测」字段顺序；whitelist 之外 → 错误返回。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— 顶层 dispatch + reserve/fill 模式 + 多子模块协调 + 错误路径覆盖。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: 部分（与 T13-T18 概念上并行；实际依赖关系是 T12 提供子模块 internal API 的「契约」，T13-T18 都 link 进 T12）
  - **Parallel Group**: Wave 2
  - **Blocks**: T13-T18 的 link 阶段；T21-T26
  - **Blocked By**: T8, T9（公共 helper + 头文件）

  **References**：

  **Pattern References**：
  - `src/archive/bnd4.c:bnd4_read_from_memory` —— reserve/fill 顶层 dispatch 模式范本。
  - `src/map/msb_common.c`（Phase 5）—— list-of-lists 顶层骨架。
  - `src/core/binary_reader.c` + `binary_writer.c` —— reserve/fill API 使用。

  **API/Type References**：
  - `Formats/FLVER/FLVER2/FLVER2.cs:Read()` —— 顶层 read 流程的 ground truth（精确字段顺序）。
  - `Formats/FLVER/FLVER2/FLVER2.cs:Write()` —— 顶层 write 流程。
  - `Formats/FLVER/FLVER2/FLVER2.cs:467 FLVERHeader` —— Header struct 字段。
  - `Formats/FLVER/FLVER2/GXList.cs:全文` —— GXList Read/Write（GXItem.Data 是 `byte[]`）。

  **Test References**：
  - 本 task 不直接测；T22 (synthetic) 与 T26 (e2e) 验证。

  **External References**：
  - T4 probe 输出（`.sisyphus/evidence/task-4-c0000-layouts.md`）—— version whitelist 的实测起点。
  - Metis 报告：FLVERHeader 字段次序 + BE byte 在 0x06。

  **WHY Each Reference Matters**：
  - BND4 / MSB common 已建立 reserve/fill + list-of-lists 顶层模式；复用避免重复设计。
  - FLVER2.cs:Read 是字段顺序的唯一可信参考；任何顺序偏差都会导致 e2e 失败。
  - GXItem opaque 透传是 Metis 修正过的设计决策；不结构化是 upstream alignment 的硬要求。

  **Acceptance Criteria**：
  - [ ] `src/geom/flver2.c` 编译通过（`cmake --build build-mingw --target souls_formats`，无 -Werror 报错）。
  - [ ] `sf_flver2_read_from_memory` 在 T22 合成 fixture 上工作（即便其他子模块还在写，至少能读出 0 mesh / 0 material 的最小 FLVER2）。
  - [ ] BE 字节序文件（offset 0x06 = 1）触发 `SF_ERR_UNSUPPORTED_VERSION`（单元测试）。
  - [ ] 未知 version（如 0x99999999）触发 `SF_ERR_UNSUPPORTED_VERSION`。
  - [ ] GXItem.Data accessor 返回原始字节指针 + size（无结构化）。
  - [ ] `sf_binary_writer_finish` 在 write 流程末调用，未 fill 的 reserve 触发 `SF_ERR_INTERNAL`。

  **QA Scenarios**：

  ```
  Scenario: BE marker `"B\0"` 文件正确拒绝
    Tool: Bash + Unity
    Preconditions: T12 完成
    Steps:
      1. 单元测试：构造一个 12-byte buffer = `"FLVER\0B\0\x13\x00\x02\x00"`（offset 0x06-0x07 = `"B\0"` 表示 BE；offset 0x08-0x0B = 0x20013 LE）
      2. 调 `sf_flver2_read_from_memory(&out, buf, 12, NULL)`
      3. assert 返回 `SF_ERR_UNSUPPORTED_VERSION`，out == NULL，无 leak
    Expected Result: 错误返回 + 无 crash + 无 leak
    Failure Indicators: 返回 SF_OK / crash / leak
    Evidence: .sisyphus/evidence/task-12-be-refusal.log

  Scenario: 非法 endian marker 拒绝
    Tool: Bash + Unity
    Steps:
      1. 构造 buffer = `"FLVER\0X\0..."`（offset 0x06 = 'X'，非法）
      2. 调 read
      3. assert 返回 `SF_ERR_BAD_MAGIC`（或 `SF_ERR_UNSUPPORTED_VERSION`，按实施决定）
    Expected Result: 错误返回，无 crash
    Failure Indicators: SF_OK 返回 / crash
    Evidence: .sisyphus/evidence/task-12-bad-marker.log

  Scenario: 未知 version 拒绝
    Tool: Bash + Unity
    Steps: 构造 `"FLVER\0L\0\x99\x99\x99\x99..."`（marker LE，version 0x99999999 不在 whitelist）
    Expected Result: `SF_ERR_UNSUPPORTED_VERSION`
    Evidence: .sisyphus/evidence/task-12-unknown-version.log

  Scenario: 整体编译 + 与 T13-T18 link 不冲突
    Tool: Bash
    Steps:
      1. `cmake --build build-mingw --target souls_formats 2>&1 | grep -E '(error|undefined)' | tee .sisyphus/evidence/task-12-link.log`
    Expected Result: 输出空（或仅有 T13-T18 未实现造成的 undefined reference，本 task 自身无 error）
    Failure Indicators: T12 自身代码有 error
    Evidence: .sisyphus/evidence/task-12-link.log
  ```

  **Commit**: YES
  - Message: `phase6(flver2): top-level reader/writer dispatch with BE refusal + GXList opaque transit`
  - Files: `src/geom/flver2.c`, `src/internal/flver2_internal.h`, `CMakeLists.txt`
  - Pre-commit: 单元测试（BE refusal + unknown version）PASS

 - [x] 13. **`src/geom/flver2_material.c` —— Material + Texture + TilingType read/write**

  **What to do**：
  - 创建 `src/geom/flver2_material.c`，实现：
    - `sf_internal_flver2_material_read(reader, header, out *material, gx_lists_ctx, allocator)` / `_write`：**严格对照 `Material.cs:82-123` Read 顺序**：
      1. `nameOffset` (i32) — 延迟读 string
      2. `mtdOffset` (i32) — 延迟读 string（C 端 `Mtd` 字段；**不是 `MtdPath`**）
      3. `textureCount` (i32) — internal，read 后用于 TakeTextures，不暴露公共 accessor
      4. `textureIndex` (i32) — internal，read 后用于 TakeTextures，不暴露公共 accessor
      5. `numStringBytes` (i32) — read 后丢弃（`Material.cs:88-89` 注释 "result of CalculateNumStringBytes"，仅校验用）
      6. `gxOffset` (i32) — 若 != 0 → StepIn 读 GXList 入全局
      7. `Index` (i32)
      8. `AssertInt32(0)` — 必须为 0；否则返回 `SF_ERR_INTERNAL`
      - 然后按 header.Unicode flag 读 `Name` 与 `Mtd` 字符串（UTF-16 或 Shift-JIS）。
    - Material **公共字段**（暴露 accessor）= `Name` (string) / `Mtd` (string，**不是 MtdPath**) / `Index` (i32) / `GXIndex` (i32, -1 表示无) / `Textures` (list，后期 TakeTextures 注入)。**NO `Unk18`、NO `Flags`、NO 任何其他 Unk\* 字段**（上游不存在）。
    - `sf_internal_flver2_texture_read(reader, header, out *texture, allocator)` / `_write`：对照 `Texture.cs:Read()`/`Write()`，字段 = `ParamName` (string，**上游字段名是 `ParamName`，非 `Type`**) / `Path` (string) / `Scale` (vec2) + 若干 Unk 字段。具体次序与 byte 大小须查 `Texture.cs`（本 task 实施时再精确对照；roadmap 已列上游引用）。
    - 公共 accessor 实现：`sf_flver2_material_count`、`sf_flver2_material(f, i)`，以及 material 字段 accessor（`sf_flver2_material_name` / `_mtd` / `_index` / `_gx_index` / `_texture_count` / `_texture`）。**NO `_mtd_path`**（命名错）。
    - `sf_flver2_tiling_type_t` enum 值与上游 `TilingType` byte 值对齐（已在 T9 头声明，本 task 实现 byte ↔ enum 互转）。
  - **Material 不内嵌 Texture 数据**：只读 textureCount + textureIndex；实际 Texture 对象由 T12 顶层读出 global texture list 后通过 `TakeTextures` 分发（上游 `Material.cs:125-139`）。本 task 实现 TakeTextures 等价的 helper。
  - Texture 字符串字段通过 sf_encoding 走 Shift-JIS 或 UTF-16（按 header.Unicode flag 决策）。

  **Must NOT do**：
  - ❌ 不结构化解析 Texture.Path 路径内容（仅作为字符串保留）。
  - ❌ 不为 Material 提供修改 API。
  - ❌ 不实现 GXList 解析（T12 已 opaque 透传）。
  - ❌ 不假设 TilingType 全部值都用得到（保留全 enum，但 e2e 只覆盖 c0000 出现的）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— Material + Texture 双子结构 + 字符串字段 + enum 映射。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T14-T18 并行）
  - **Parallel Group**: Wave 2
  - **Blocks**: T22, T26
  - **Blocked By**: T9, T12

  **References**：

  **Pattern References**：
  - `src/archive/bnd4.c:bnd4_entry_read` —— 字符串字段 + 子结构 read 模式。
  - `src/core/encoding_win32.c` —— Shift-JIS / UTF-16 字符串读取。

  **API/Type References**：
  - `Formats/FLVER/FLVER2/Material.cs:Read()` / `Write()` —— 字段次序 ground truth。
  - `Formats/FLVER/FLVER2/Texture.cs:Read()` / `Write()` —— 字段次序 ground truth + Scale (vec2) 字段。
  - `Formats/FLVER/FLVER2/Texture.cs:TilingType enum` —— byte 值。

  **WHY Each Reference Matters**：
  - Material 字段次序错位 = 整个 .flver 后续 mesh / face_set 全跑偏。
  - Texture.Path 字符串编码是 round-trip 关键；上游用 `ReadUTF16` 决策（不是 ASCII / Shift-JIS）。

  **Acceptance Criteria**：
  - [ ] `src/geom/flver2_material.c` 编译通过（-Werror）。
  - [ ] `sf_internal_flver2_material_read` 在 T22 合成 cube fixture 上读出 1 个 material（name = "test_material"）。
  - [ ] `sf_flver2_material(f, 0)` 返回非空指针；`sf_flver2_material_texture_count(...)` 返回符合 fixture 设定的数。
  - [ ] Material + Texture round-trip 字节级一致（在 T22 fixture 内验证）。

  **QA Scenarios**：

  ```
  Scenario: material + texture 字段次序对齐上游
    Tool: Bash
    Preconditions: T13 完成
    Steps:
      1. 单元测试（在 T22 fixture 框架内）：构造 1 mesh × 1 material × 2 texture（type/path 不同）的最小 FLVER2 字节流；调 read；验证字段次序。
      2. 写回；diff input.bin vs output.bin
    Expected Result: 字节级一致
    Failure Indicators: 任一字段错位
    Evidence: .sisyphus/evidence/task-13-material-rt.log

  Scenario: TilingType enum byte 值与上游对齐
    Tool: Bash
    Steps:
      1. `grep -oE 'TilingType\s*[:{][^}]+' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/FLVER2/Texture.cs | grep -oE '[A-Z][a-zA-Z]+\s*=\s*[0-9]+'`
      2. `grep -oE 'SF_FLVER2_TILING_TYPE_[A-Z_]+\s*=\s*[0-9]+' include/souls_formats/sf_flver2.h`
      3. 手工 diff 两边数量与值
    Expected Result: 一致
    Evidence: .sisyphus/evidence/task-13-tilingtype.log
  ```

  **Commit**: YES
  - Message: `phase6(flver2): Material + Texture + TilingType`
  - Files: `src/geom/flver2_material.c`, `src/internal/flver2_internal.h`（按需扩展声明）
  - Pre-commit: 编译通过 + T22 fixture 在 material 段 PASS（其他段允许暂时 fail）

 - [x] 14. **`src/geom/flver2_mesh.c` —— Mesh + BoundingBoxes + global index 引用**

  **What to do**：
  - 创建 `src/geom/flver2_mesh.c`，实现：
    - `sf_internal_flver2_mesh_read(reader, out *mesh, allocator)` / `_write`：对照 `Mesh.cs:Read()`/`Write()`，字段 = Dynamic (bool) / MaterialIndex (i32) / **NodeIndex (i32)** / BoneIndices (list) / FaceSetIndices (list) / VertexBufferIndices (list) / Vertices (count + offset，但 vertex bytes 实际属于 VertexBuffer) / BoundingBoxes (optional sub-struct)。**上游字段名是 `NodeIndex`（`Mesh.cs:37`），不是 `DefaultBoneIndex`**。
    - `sf_internal_flver2_bbox_read(reader, out *bbox)` / `_write`：BoundingBoxes 内嵌（Min vec3 / Max vec3 / Unk?）—— 对照 `Mesh.cs:269 BoundingBoxes`。
    - 公共 accessor 实现：`sf_flver2_mesh_count` / `sf_flver2_mesh(f, i)` / mesh 字段 accessor（material_index / **node_index**（对应上游 `NodeIndex`，非 `DefaultBoneIndex`）/ bone_index_count / bone_index / face_set_index_count / face_set_index / vertex_buffer_index_count / vertex_buffer_index）。
  - **关键**：mesh 字段是 **index 列表**指向 global FaceSet / VertexBuffer / Bone（不是嵌入对象）；accessor 必须明确暴露这一点（已在 T9 sf_flver2.h 设计）。
  - **不重算 bounding box**：read 时原样保留；write 时原样写出。

  **Must NOT do**：
  - ❌ 不重算 / 重计算 bounding box（即便 vertex bytes 变了）。
  - ❌ 不为 mesh 提供 `sf_flver2_mesh_face_set()` 这种直接 dereference accessor —— 只给 `_face_set_index()`。
  - ❌ 不在 mesh.c 中实际解析 face_set / vertex_buffer 字节（仅记录 index）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— Mesh + BBox 双子结构 + 多 index list + global 引用模式。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T13, T15-T18 并行）
  - **Parallel Group**: Wave 2
  - **Blocks**: T22, T26
  - **Blocked By**: T9, T12

  **References**：

  **Pattern References**：
  - `src/archive/bnd4.c` —— index list 解析模式（BND4 entries 也是 index-driven）。

  **API/Type References**：
  - `Formats/FLVER/FLVER2/Mesh.cs:Read()` / `Write()` —— 字段次序 + index list 处理。
  - `Formats/FLVER/FLVER2/Mesh.cs:269 BoundingBoxes` —— BBox 字段。

  **External References**：
  - Metis 报告：BufferLayout / VertexBuffer 是 mesh 间共享，按 global index 引用。

  **WHY Each Reference Matters**：
  - 错误把 mesh 的 index list 解析为「mesh 内嵌对象」 = 字段重复 / 上游字节流偏移完全错位。
  - BBox 不重算的政策已在 must-not-do 锁定；本 task 实现时不要试图「优化」。

  **Acceptance Criteria**：
  - [ ] `src/geom/flver2_mesh.c` 编译通过。
  - [ ] T22 fixture 在 mesh 段读出 1 个 mesh，material_index = 0、face_set_index[0] = 0、vertex_buffer_index[0] = 0。
  - [ ] BBox round-trip 字节级一致（在 T22 内验证）。
  - [ ] `grep -E 'bbox.*recompute|recalculate.*bound' src/geom/flver2_mesh.c` 输出空（无重算代码）。

  **QA Scenarios**：

  ```
  Scenario: mesh index list 正确读出
    Tool: Bash + Unity
    Preconditions: T14 完成
    Steps: T22 fixture 框架内验证 mesh 字段
    Expected Result: 全部 index 字段与构造时设定一致
    Failure Indicators: 任一 index 不匹配
    Evidence: .sisyphus/evidence/task-14-mesh-rt.log

  Scenario: BBox 不重算
    Tool: Bash
    Steps:
      1. `grep -rE 'recompute|recalculate|min/max|aabb' src/geom/flver2_mesh.c | tee .sisyphus/evidence/task-14-bbox-no-recompute.log`
    Expected Result: 输出空
    Failure Indicators: 命中
    Evidence: .sisyphus/evidence/task-14-bbox-no-recompute.log
  ```

  **Commit**: YES
  - Message: `phase6(flver2): Mesh + BoundingBoxes with global-index references (no bbox recompute)`
  - Files: `src/geom/flver2_mesh.c`, `src/internal/flver2_internal.h`
  - Pre-commit: T22 mesh-段 PASS

- [x] 15. **`src/geom/flver2_faceset.c` —— FaceSet + FSFlags + 三角条带解码（0xFFFF restart + degenerate filter ON default）**

  **What to do**：
  - 创建 `src/geom/flver2_faceset.c`，实现：
    - `sf_internal_flver2_face_set_read(reader, out *face_set, allocator)` / `_write`：对照 `FaceSet.cs:Read()`/`Write()`，字段 = Flags (FSFlags uint32) / TriangleStrip (bool) / CullBackfaces (bool) / Unk06 / Unk07 / Indices (u16 或 u32，按 IndexSize 字段决定)。
    - 三角化 helper：`sf_internal_flver2_face_set_triangulate(face_set, filter_degenerate, out **triangles, *count, allocator)`：
      - 若 `!TriangleStrip` → 直接拷贝 indices 为三角列表。
      - 若 `TriangleStrip` → 按 0xFFFF restart sentinel 拆条带 → 展开为三角列表。
      - **`filter_degenerate` 默认 ON**：去除三个 index 任两个相等的三角。
      - 输出永远是 u32 三角列表（与 `sf_flver2_decoded_mesh_t.indices` 类型一致）。
    - 公共 accessor 实现：`sf_flver2_face_set_count` / `sf_flver2_face_set(f, i)` / face_set 字段 accessor（flags / triangle_strip / cull / index_count / index）。

  **Must NOT do**：
  - ❌ 不实现 32-bit index 的 restart sentinel（u32 路径无 restart 概念，按 Metis 实测）。
  - ❌ 不假设 0xFFFFFFFF 是 restart（**仅 0xFFFF**）。
  - ❌ 不实现 EdgeCompression flag —— 读到该 flag → `SF_ERR_UNSUPPORTED_VERSION`。
  - ❌ 不暴露三角化结果在公共头 —— 仅作为 internal helper 供 T21 decode_mesh 使用。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— 三角条带解码 + degenerate filter + 多 IndexSize 路径 + edge case（empty / single triangle）。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T13, T14, T16-T18 并行）
  - **Parallel Group**: Wave 2
  - **Blocks**: T22, T26, T21（decode_mesh 调用三角化 helper）
  - **Blocked By**: T9, T12

  **References**：

  **Pattern References**：
  - 无（三角条带解码是 Phase 6 新引入逻辑）。

  **API/Type References**：
  - `Formats/FLVER/FLVER2/FaceSet.cs:Read()` / `Write()` —— 字段次序。
  - `Formats/FLVER/FLVER2/FaceSet.cs:19 FSFlags enum` —— 6 个 flag 值。
  - `Formats/FLVER/FLVER2/Mesh.cs:GetFaces()` —— 上游三角化 reference（filter_degenerate 行为）。

  **External References**：
  - Metis 报告：FaceSet 重启符号仅 0xFFFF；filter_degenerate 默认 ON；u32 路径无 restart。
  - DirectX `D3DPRIMITIVE_TYPE_TRIANGLESTRIP` 文档（restart sentinel 历史背景）。

  **WHY Each Reference Matters**：
  - 三角化逻辑直接影响 decode_mesh 输出 → 进而影响 e2e 渲染验证（虽然 Phase 6 不渲染，但 index 数量必须可对照预期）。
  - 0xFFFF only 是 Metis 实测；用错值会导致大量「假三角」插入或 strip 拆错。
  - filter_degenerate 默认 ON 是上游惯例；改默认会让 decode_mesh 输出与开发者预期不符。

  **Acceptance Criteria**：
  - [ ] `src/geom/flver2_faceset.c` 编译通过。
  - [ ] FaceSet round-trip 字节级一致（在 T22 fixture 内）。
  - [ ] 三角化 helper 对「strip = [0, 1, 2, 3]」（4 顶点 strip）输出 2 个三角（[0,1,2] + [2,1,3] 或上游具体顺序）。
  - [ ] 三角化 helper 对「strip = [0, 1, 2, 0xFFFF, 3, 4, 5]」（含 restart）正确拆分为 2 个独立 strip。
  - [ ] degenerate filter ON：strip = [0, 0, 0] → 输出 0 三角。
  - [ ] EdgeCompression flag 触发 `SF_ERR_UNSUPPORTED_VERSION`（单元测试，构造一个 flag = EdgeCompressed 的 FaceSet 字节流）。

  **QA Scenarios**：

  ```
  Scenario: 三角条带 0xFFFF restart 正确处理
    Tool: Bash + Unity
    Preconditions: T15 完成
    Steps:
      1. 单元测试：strip indices = [0, 1, 2, 3, 0xFFFF, 4, 5, 6, 7]
      2. 调三角化 helper（filter_degenerate=true）
      3. assert 输出 2 + 2 = 4 个三角（每段 4 顶点产生 2 三角）
    Expected Result: 4 个三角
    Failure Indicators: 错误数量；或 0xFFFF 被当做合法 index 引入假三角
    Evidence: .sisyphus/evidence/task-15-strip-restart.log

  Scenario: degenerate filter
    Tool: Bash + Unity
    Steps:
      1. strip = [0, 0, 1, 2, 2, 3]（含 degenerate）
      2. 调三角化 helper（filter_degenerate=true）vs (false)
      3. true 输出 ≤ false 输出
    Expected Result: filter ON 时排除 (0,0,1) 与 (2,2,3) 这样的三角
    Failure Indicators: 数量一致（filter 没起作用）
    Evidence: .sisyphus/evidence/task-15-degenerate.log

  Scenario: EdgeCompression flag 触发错误
    Tool: Bash + Unity
    Steps:
      1. 构造 FSFlags = EdgeCompressed 的 FaceSet 字节流
      2. 调 read
      3. assert 返回 `SF_ERR_UNSUPPORTED_VERSION`
    Expected Result: 拒绝
    Failure Indicators: 读入成功
    Evidence: .sisyphus/evidence/task-15-edge-refusal.log
  ```

  **Commit**: YES
  - Message: `phase6(flver2): FaceSet + FSFlags + tri-strip decode (0xFFFF restart, filter ON)`
  - Files: `src/geom/flver2_faceset.c`, `src/internal/flver2_internal.h`, 单元测试
  - Pre-commit: 3 个 QA scenarios PASS

 - [x] 16. **`src/geom/flver2_vertex_buffer.c` —— VertexBuffer + BufferLayout（含 -32768 sentinel）**

  **What to do**：
  - 创建 `src/geom/flver2_vertex_buffer.c`，实现：
    - `sf_internal_flver2_vertex_buffer_read(reader, out *vb, allocator)` / `_write`：对照 `VertexBuffer.cs:Read()`/`Write()`，字段 = BufferIndex (i32) / LayoutIndex (i32) / VertexSize (i32) / VertexCount (i32) / Unk10 (i32) / Unk14 (i32) / BufferLength (i32) / BufferOffset (i32) / vertex bytes 区段。
    - `sf_internal_flver2_buffer_layout_read(reader, out *bl, allocator)` / `_write`：对照 `BufferLayout.cs:Read()`/`Write()`，本质 = N 个 LayoutMember（已在 T8 实现 read/write）。
    - **`BufferLayout.Size` 计算**：遍历 members，调 `sf_flver_layout_type_size(member.type, member.special_modifier)`；special_modifier == -32768 时该 member 字节数为 0（sentinel 处理）。
    - 公共 accessor 实现：`sf_flver2_vertex_buffer_count` / `sf_flver2_vertex_buffer(f, i)` / `sf_flver2_buffer_layout_count` / `sf_flver2_buffer_layout(f, i)`。
    - vertex bytes 区段：read 时记录原始字节（不解码 —— 解码由 T17 + T21 处理）；write 时原样写出。

  **Must NOT do**：
  - ❌ 不在 vertex_buffer.c 解码 vertex 字节内容（仅记录原始字节）—— 解码在 T17 dispatch。
  - ❌ 不忽略 `SpecialModifier == -32768` —— 必须 sentinel 处理（zero 字节贡献）。
  - ❌ 不假设 VertexSize == sum(member.size)；某些版本可能有额外 padding，原样保留。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— BufferLayout sentinel 处理 + vertex bytes 透传 + size 计算。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T13-T15, T17, T18 并行）
  - **Parallel Group**: Wave 2
  - **Blocks**: T17, T21, T22, T26
  - **Blocked By**: T8（layout_type_size）, T9, T12

  **References**：

  **Pattern References**：
  - `src/geom/flver_common.c:T8` —— sf_flver_layout_type_size 实现。

  **API/Type References**：
  - `Formats/FLVER/FLVER2/VertexBuffer.cs:Read()` / `Write()` —— 字段次序 + vertex bytes 区段。
  - `Formats/FLVER/FLVER2/BufferLayout.cs:Read()` / `Write()` —— LayoutMember list。
  - `Formats/FLVER/FLVER2/BufferLayout.cs:Size` —— size 计算逻辑 + sentinel。
  - `Formats/FLVER/LayoutMember.cs:Size` —— 上游 size getter，含 -32768 sentinel 处理。

  **External References**：
  - Metis 报告：BufferLayout.Size 用 SpecialModifier == -32768 表示零字节成员。

  **WHY Each Reference Matters**：
  - sentinel 处理错误 = layout size 计算偏，vertex bytes 解析时 offset 偏移 = 全部 vertex 数据乱码。
  - vertex bytes 透传是必要的：T17 dispatch 需要原始字节才能按 layout 解码；提前结构化会丢信息。

  **Acceptance Criteria**：
  - [ ] `src/geom/flver2_vertex_buffer.c` 编译通过。
  - [ ] BufferLayout round-trip 字节级一致（T22 fixture 内）。
  - [ ] sentinel 测试：构造 LayoutMember.SpecialModifier = -32768；调 size getter；返回 0；写回原样。
  - [ ] vertex bytes 透传：read 后 raw bytes pointer / size 可访问；write 后原样输出。

  **QA Scenarios**：

  ```
  Scenario: -32768 sentinel 正确处理
    Tool: Bash + Unity
    Preconditions: T16 完成
    Steps:
      1. 单元测试：构造 BufferLayout 含一个 SpecialModifier = -32768 的 member
      2. 调 size 计算
      3. assert 该 member 贡献 0 字节，但 member 本身仍在 list 中
    Expected Result: total size 不包含该 member 的常规 size
    Failure Indicators: total size 把 sentinel member 当常规算
    Evidence: .sisyphus/evidence/task-16-sentinel.log

  Scenario: vertex bytes 透传
    Tool: Bash + Unity
    Steps:
      1. 构造 VertexBuffer 含 N=10 字节的 vertex 区
      2. read → write
      3. diff input vs output vertex bytes
    Expected Result: 字节级一致
    Failure Indicators: 任何偏差
    Evidence: .sisyphus/evidence/task-16-vbytes-passthrough.log
  ```

  **Commit**: YES
  - Message: `phase6(flver2): VertexBuffer + BufferLayout with -32768 sentinel`
  - Files: `src/geom/flver2_vertex_buffer.c`, `src/internal/flver2_internal.h`, 单元测试
  - Pre-commit: 2 个 QA scenarios PASS

 - [x] 17. **`src/geom/flver2_vertex.c` —— THE 顶点 dispatch（mirror Vertex.cs foreach + semantic-first if/else ladder）**

  **What to do**：
  - 创建 `src/geom/flver2_vertex.c`。**本 task 是 Phase 6 最高风险点**，必须严格 mirror 上游 `Vertex.cs:112-390` (Read) / `:447-743` (Write) 的结构：
    - **NOT** 静态 `(Type × Semantic × Index) -> decoder` 表。
    - **NOT** 函数指针数组。
    - **IS** `foreach (LayoutMember m in layout) { switch (m.Semantic) { case Position: switch (m.Type) { case Float3: ... case Half4: ... } case Normal: ... } }` 的 C 翻译。
    - 用 nested `switch` 或 `if-else` ladder；**保留上游每个 case 的字节级行为**（如 AC6 UShort4 normal 的特殊归一化路径）。
  - **API 函数签名**：
    ```c
    typedef struct sf_flver2_vertex_context {
        float    uv_factor;       /* version-specific UV scaling factor */
        bool     is_ac6;          /* AC6-specific UShort4 normal normalization */
        uint32_t header_version;  /* for any other version-gated decode */
    } sf_flver2_vertex_context_t;

    /* 把一个 vertex 的 raw bytes 按 layout 解码到 typed output struct */
    sf_result_t sf_internal_flver2_vertex_decode_one(
        const sf_flver2_buffer_layout_t *layout,
        const uint8_t *vertex_bytes,
        const sf_flver2_vertex_context_t *ctx,
        sf_flver2_decoded_vertex_t *out);

    /* 把一个 typed output struct 编回 raw bytes */
    sf_result_t sf_internal_flver2_vertex_encode_one(
        const sf_flver2_buffer_layout_t *layout,
        const sf_flver2_decoded_vertex_t *in,
        const sf_flver2_vertex_context_t *ctx,
        uint8_t *vertex_bytes);
    ```
  - **`sf_flver2_decoded_vertex_t`** 是 internal struct（不在公共头）：含 Position / Normal / Tangent / Bitangent / UV[8] / Color[4] / BoneIndices / BoneWeights，与公共 `sf_flver2_decoded_mesh_t` 字段对齐。
  - 处理 LayoutType 字节大小：Float2 = 8, Float3 = 12, Float4 = 16, Byte4* = 4, Half2 = 4, Half4 = 8, Short2toFloat2 = 4, Short4toFloat4* = 8, UByte4Norm = 4, UV = 4, UVPair = 8, ShortBoneIndices = 8, UV3Short4toFloat4 = 24（按上游）。
  - 未识别 LayoutType / Semantic 组合 → 返回 `SF_ERR_UNSUPPORTED_VERSION` + 日志记录 `KNOWN_LAYOUT_GAP: type=0xXX semantic=0xYY`。
  - **EdgeCompressed LayoutType**：直接返回 `SF_ERR_UNSUPPORTED_VERSION`。

  **Must NOT do**：
  - ❌ 不优化为静态表 / hash 表 / 函数指针数组（**violates upstream alignment**）。
  - ❌ 不引入元编程 / 代码生成（保持手写直接对齐）。
  - ❌ 不假设 LayoutType 与 Semantic 是独立的（dispatch 是 nested 的，semantic-first）。
  - ❌ 不忽略 UV factor 与 AC6 flag（这些是 threaded context 而非 dispatch 表的一部分）。

  **Recommended Agent Profile**：
  - **Category**: `artistry` —— 需要 mirror 上游 ~600 行 if/else ladder 的非常规精确翻译工作；不能凭直觉重写为更「优雅」结构。
  - **Skills**: 无（必须严格对照上游，无第三方库辅助）。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T13-T16, T18 并行）
  - **Parallel Group**: Wave 2
  - **Blocks**: T21, T22, T25, T26
  - **Blocked By**: T8（half-float + 11_11_10 + layout_type_size）, T9, T12, T16（VertexBuffer + BufferLayout 读完）

  **References**：

  **Pattern References**：
  - 无（这是 Phase 6 独有的 dispatch 代码）。

  **API/Type References**：
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/Vertex.cs:112-390` —— Read dispatch 的 ground truth。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/Vertex.cs:447-743` —— Write dispatch 的 ground truth。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/LayoutMember.cs` —— LayoutType 枚举 + Size getter。

  **Test References**：
  - T4 probe 输出 `.sisyphus/evidence/task-4-c0000-layouts.md` —— c0000 实际出现的 (Type, Semantic) 对，作为最小覆盖目标。

  **External References**：
  - Metis 报告：dispatch 结构是 foreach + semantic-first if/else，**不是** static table。

  **WHY Each Reference Matters**：
  - Vertex.cs 上游代码是 dispatch 的唯一可信参考；任何「smart」重写都会引入字段对齐 bug。
  - T4 probe 提供 c0000 实际 layout 集合 = T17 必须覆盖的最小子集（其他 layout fallback 到 unsupported）。
  - Metis 修正避免本 task 走错路（项目内首个 strict-mirror 类型 task；以前 module 都是字段对齐为主）。

  **Acceptance Criteria**：
  - [ ] `src/geom/flver2_vertex.c` 编译通过（-Werror）。
  - [ ] Dispatch 结构对照：`grep -c 'case SF_FLVER_LAYOUT_SEMANTIC_' src/geom/flver2_vertex.c` ≥ 7（Position/Normal/Tangent/Bitangent/UV/BoneWeights/BoneIndices/VertexColor 8 个 semantic 各一个外层 case）。
  - [ ] **不存在** static table：`grep -E 'static.*decoder|const.*table.*decode' src/geom/flver2_vertex.c` 输出空。
  - [ ] T4 probe 中列出的所有 (Type, Semantic) 对均在 dispatch 中处理（无 `SF_ERR_UNSUPPORTED_VERSION` 路径）。
  - [ ] EdgeCompressed LayoutType → `SF_ERR_UNSUPPORTED_VERSION`（单元测试）。
  - [ ] Unknown LayoutType (0xFF) → `SF_ERR_UNSUPPORTED_VERSION` + 日志（单元测试）。
  - [ ] Round-trip 验证：构造一个 8-vertex unit cube 的字节流，read → encode_one → write → diff，字节级一致。

  **QA Scenarios**：

  ```
  Scenario: dispatch 结构对齐上游（无 static table）
    Tool: Bash
    Preconditions: T17 完成
    Steps:
      1. `grep -c 'switch.*semantic' src/geom/flver2_vertex.c`
      2. `grep -E 'static.*decoder|const.*table.*decode' src/geom/flver2_vertex.c; echo "exit=$?"`
      3. `grep -c 'case SF_FLVER_LAYOUT_SEMANTIC_' src/geom/flver2_vertex.c`
    Expected Result: 步骤 1 ≥ 2（read + write 各 1 个外层 switch）；步骤 2 退出码 1（无 static table）；步骤 3 ≥ 16（8 semantic × 2 方向）
    Failure Indicators: 步骤 2 命中（违反 Metis 决策）
    Evidence: .sisyphus/evidence/task-17-dispatch-shape.log

  Scenario: 覆盖 T4 probe 列出的全部 (Type, Semantic)
    Tool: Bash
    Preconditions: T17 完成 + T4 evidence 在
    Steps:
      1. 单元测试：对 T4 probe 列出的每个 (Type, Semantic) 对，构造一个最小 layout 调 decode_one
      2. assert 无 `SF_ERR_UNSUPPORTED_VERSION` 返回
    Expected Result: T4 列出的 N 对全部 PASS
    Failure Indicators: 任一对返回 unsupported
    Evidence: .sisyphus/evidence/task-17-coverage.log

  Scenario: round-trip 字节级一致
    Tool: Bash + Unity
    Steps:
      1. 构造 unit cube（8 顶点）的 Position + Normal layout
      2. read decode → encode write → diff
    Expected Result: 字节级一致
    Failure Indicators: 任何字节偏差
    Evidence: .sisyphus/evidence/task-17-roundtrip.log
  ```

  **Commit**: YES
  - Message: `phase6(flver2): mirror upstream vertex dispatch (foreach + semantic-first if/else ladder)`
  - Files: `src/geom/flver2_vertex.c`, `src/internal/flver2_internal.h`, 单元测试
  - Pre-commit: 3 个 QA scenarios PASS

 - [x] 18. **`src/geom/flver2_skeleton.c` —— SkeletonSet + Bone（version >= 0x2001A 门控，BaseSkeleton + AllSkeletons 双 List<Bone>）**

  **What to do**：
  - 创建 `src/geom/flver2_skeleton.c`，实现：
    - `sf_internal_flver2_skeleton_set_read(reader, header_version, out *set, allocator)` / `_write`：
      - **版本门控**：若 `header_version < 0x2001A`（Sekiro 等）→ 不读 SkeletonSet 区段；set 留 NULL。
      - 否则：**严格对照 `SkeletonSet.cs:31-58` Read**：
        1. `count1 = ReadInt16()` (BaseSkeleton count)
        2. `count2 = ReadInt16()` (AllSkeletons count)
        3. `offset1 = ReadUInt32()` (BaseSkeleton offset)
        4. `offset2 = ReadUInt32()` (AllSkeletons offset)
        5. 5 × `AssertInt32(0)` (20 字节零 padding)
        6. `StepIn(offset1)` → 读 `count1` 个 Bone → `BaseSkeleton`
        7. `StepIn(offset2)` → 读 `count2` 个 Bone → `AllSkeletons`
      - **`SkeletonSet` 是 two `List<Bone>`，不是「BoneIndices + Bones」**；每个 Bone 是完整 struct（ParentIndex/FirstChildIndex/NextSiblingIndex/PreviousSiblingIndex/NodeIndex）。
    - `sf_internal_flver2_bone_read(reader, out *bone)` / `_write`：**严格对照 `SkeletonSet.cs:123-141` Bone struct**，字节布局（共 16 字节）：
      1. `ParentIndex` (i16, 2 字节)
      2. `FirstChildIndex` (i16, 2 字节)
      3. `NextSiblingIndex` (i16, 2 字节)
      4. `PreviousSiblingIndex` (i16, 2 字节)
      5. `NodeIndex` (i32, 4 字节) —— 指向 FLVER2.Nodes list 中的 index
      6. `AssertInt32(0)` (4 字节零)
    - 公共 accessor 实现：`sf_flver2_skeleton_set(f)` —— 若 version < 0x2001A 返回 NULL；否则返回 opaque pointer。**双 list 访问**：`sf_flver2_skeleton_set_base_count` / `_base_bone(set, i)`、`_all_count` / `_all_bone(set, i)`、Bone 字段 accessor。
    - Write 对称：`SkeletonSet.cs:60-79`，先 reserve 两 offset，foreach 写 BaseSkeleton 与 AllSkeletons。

  **Must NOT do**：
  - ❌ 不在 Sekiro (0x20013) 版本下尝试读 SkeletonSet —— 直接返回 NULL。
  - ❌ 不引入 v1/v2 hierarchy 分支（Metis 已确认无此分支）。
  - ❌ 不为 NULL skeleton_set 返回「0 个 bone」假象 —— 必须明确 NULL（消费者按 NULL 处理）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— 版本门控 + 两 list read/write + 可选返回 NULL。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T13-T17 并行）
  - **Parallel Group**: Wave 2
  - **Blocks**: T22, T26
  - **Blocked By**: T9, T12

  **References**：

  **Pattern References**：
  - `src/archive/bnd4.c` 中可选段处理（BND4 有 optional hash table）。

  **API/Type References**：
  - `Formats/FLVER/FLVER2/SkeletonSet.cs:Read()` / `Write()` —— 字段次序。
  - `Formats/FLVER/FLVER2/SkeletonSet.cs:84 Bone` —— Bone 字段。
  - `Formats/FLVER/FLVER2/FLVER2.cs:Read` —— 版本门控的位置（在 reader flow 中条件分支）。

  **External References**：
  - Metis 报告：SkeletonSet gated by Version >= 0x2001A，无 v1/v2 hierarchy 分支。

  **WHY Each Reference Matters**：
  - 错误对 Sekiro 0x20013 调 skeleton_set read 会读到错误偏移的字节 → 全部后续段 corrupt。
  - 公共 API 设计为可返回 NULL（已在 T9 头声明）；本 task 必须遵守这个语义。

  **Acceptance Criteria**：
  - [ ] `src/geom/flver2_skeleton.c` 编译通过。
  - [ ] 单元测试：构造 header version = 0x20013，调 `sf_flver2_skeleton_set(f)` → 返回 NULL。
  - [ ] 单元测试：构造 header version = 0x2001A，调 `sf_flver2_skeleton_set(f)` → 返回非 NULL，bone_count ≥ 0。
  - [ ] Bone round-trip 字节级一致（在 T22 fixture 内验证）。

  **QA Scenarios**：

  ```
  Scenario: 版本门控正确
    Tool: Bash + Unity
    Preconditions: T18 完成
    Steps:
      1. 单元测试：构造 minimal FLVER2 字节流，header version = 0x20013（Sekiro），无 skeleton_set 段
      2. 调 `sf_flver2_read_from_memory` 与 `sf_flver2_skeleton_set(f)`
      3. assert 返回 NULL，无 crash，无 leak
    Expected Result: NULL
    Failure Indicators: 非 NULL 或 crash
    Evidence: .sisyphus/evidence/task-18-version-gate.log

  Scenario: SkeletonSet round-trip（version >= 0x2001A）
    Tool: Bash + Unity
    Steps:
      1. 构造 FLVER2 含 1 个 SkeletonSet（version 0x2001A）+ 3 个 Bone
      2. read → write → diff
    Expected Result: 字节级一致
    Failure Indicators: 任何字段偏差
    Evidence: .sisyphus/evidence/task-18-roundtrip.log
  ```

  **Commit**: YES
  - Message: `phase6(flver2): SkeletonSet + Bone gated by version 0x2001A (Sekiro returns NULL)`
  - Files: `src/geom/flver2_skeleton.c`, `src/internal/flver2_internal.h`, 单元测试
  - Pre-commit: 2 个 QA scenarios PASS

### Wave 3 — MTD + MATBIN（Wave 1 全绿后 2 路并行，可与 Wave 2 并行）

- [x] 19. **`src/geom/mtd.c` —— MTD 读 + 写 + Sekiro Extended texture info**

  **What to do**：
  - 创建 `src/geom/mtd.c`，实现 T10 头声明的全部 API：
    - 读：对照 `MTD.cs:Read()`，按嵌套块结构解析：Header / Data / Lists / Params / Textures。
    - 写：对照 `MTD.cs:Write()`。
    - **Sekiro Extended texture info**：textureBlock.Version 探测（**3 = non-Extended，5 = Extended**，对照 `MTD.cs:385-413`）。Extended 字段是 **`Path` (string) + `UnkFloats` (List<float>，长度由前置 `floatCount` i32 决定)**，**不是 `Scale vec2`**。non-Extended 时 Path 默认空串、UnkFloats 默认空 list。
    - Texture **公共字段**（暴露 accessor）= `Type` / `UVNumber` / `ShaderDataIndex` / `Extended` flag / `Path`（仅 Extended）/ `UnkFloats`（仅 Extended）。
    - Param value typed accessor（int / float / bool 等）—— 按 MTD.Param.ParamType 决策。
  - 添加单元测试 fixture（在 T23 中详细，本 task 仅提供 module 实现 + minimal sanity test）。

  **Must NOT do**：
  - ❌ 不暴露内部嵌套块结构到公共 API。
  - ❌ 不实现 MTD XML 序列化（不在 v1 范围）。
  - ❌ 不为非 Sekiro 版本提供 Extended accessor 数据（返回 has_extended = false）。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— 嵌套块解析 + 版本差异处理 + Param typed value。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T20 并行；与 Wave 2 整体并行）
  - **Parallel Group**: Wave 3
  - **Blocks**: T23（合成 fixture）, e2e_sekiro
  - **Blocked By**: T10

  **References**：

  **Pattern References**：
  - `src/text/fmg.c`（Phase 4）—— 嵌套块解析模式 + 多版本探测。
  - `src/param/param.c`（Phase 4）—— typed value accessor 模式。

  **API/Type References**：
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MTD.cs:Read()` / `Write()` —— ground truth。
  - `MTD.cs:Param` 内嵌类 + `MTD.cs:Texture` 内嵌类 + `BlendMode` / `LightingType` enum。

  **WHY Each Reference Matters**：
  - MTD 是 Phase 6 中相对独立模块；主要工作是上游 1:1 翻译。
  - Sekiro Extended texture info 是版本差异的唯一复杂点；不处理 = Sekiro e2e fail。

  **Acceptance Criteria**：
  - [ ] `src/geom/mtd.c` 编译通过。
  - [ ] `sf_mtd_read_from_memory` 在 T23 合成 fixture 上工作。
  - [ ] `sf_mtd_texture_has_extended` 在 textureBlock.Version 5（Sekiro）= true，version 3 = false。
  - [ ] Extended texture 的 `sf_mtd_texture_path` 返回非空字符串；`sf_mtd_texture_unk_float_count` ≥ 0；`sf_mtd_texture_unk_float(t, i)` 返回值与构造时一致。
  - [ ] non-Extended texture 的 `_path` 返回空串；`_unk_float_count` = 0。
  - [ ] Param typed value accessor 对错误 type 调用返回 `SF_ERR_INVALID_ARG`（不 crash）。

  **QA Scenarios**：

  ```
  Scenario: MTD 编译 + minimal sanity
    Tool: Bash + Unity
    Steps:
      1. `cmake --build build-mingw --target souls_formats_test_mtd_synthetic`
      2. 单元测试：构造一个最小 MTD（1 param + 1 texture）→ read → 验证 shader_path / param 字段
    Expected Result: PASS
    Failure Indicators: 编译错；或字段读出错误
    Evidence: .sisyphus/evidence/task-19-mtd-sanity.log

  Scenario: Extended texture info 版本探测
    Tool: Bash + Unity
    Steps:
      1. 构造一个 textureBlock.Version = 5（Sekiro）MTD 字节流，含 Extended Path = "smdm:test.tga" + UnkFloats = [1.0, 2.0, 3.0]
      2. read
      3. assert `sf_mtd_texture_has_extended(...)` = true
      4. assert `sf_mtd_texture_path(...)` 返回 "smdm:test.tga"
      5. assert `sf_mtd_texture_unk_float_count(...)` = 3 且 `_unk_float(t, 0/1/2)` = 1.0/2.0/3.0
      6. 构造 version 3 MTD：assert `_has_extended` = false、`_path` = ""、`_unk_float_count` = 0
    Expected Result: 全部 assert PASS
    Failure Indicators: version 5 下 has_extended = false；或 path 读不出；或 unk_float 数量错
    Evidence: .sisyphus/evidence/task-19-extended.log
  ```

  **Commit**: YES
  - Message: `phase6(mtd): MTD reader/writer with Sekiro Extended texture info`
  - Files: `src/geom/mtd.c`, `tests/geom/test_mtd_smoke.c`（与 T23 共享）
  - Pre-commit: 2 个 QA scenarios PASS

- [x] 20. **`src/geom/matbin.c` —— MATBIN 读 + 写 + 8 ParamType union**

  **What to do**：
  - 创建 `src/geom/matbin.c`，实现 T11 头声明的全部 API：
    - 读：对照 `MATBIN.cs:Read()`，字段 = magic / version / ShaderPath / SourcePath / Key / Params / Samplers。
    - 写：对照 `MATBIN.cs:Write()`。
    - Param 子结构：Name / Key / ParamType (8 个变体) / Value。
    - **每个 ParamType 在文件中的实际字节占用（对照 `MATBIN.cs:218-228` Read 与 `MATBIN.cs:255-273` Write）**：
      - `Bool`：1 byte（read 1 bool / write 1 bool）
      - `Int`：4 bytes
      - `Int2`：8 bytes (2 × i32)
      - `Float`：4 bytes
      - `Float2`：8 bytes
      - **`Float3`：⚠️ 文件中实际占 **20 bytes**（5 × float），但 C 端 API 仅暴露前 3 个 float**（与上游 lossy 行为一致：`MATBIN.cs:223-226` 注释「For colors that use this type, there are actually five floats in the file. Because the extra values appear to be useless, they are being discarded」）。**Write 时 C 端必须写出 5 floats**：前 3 个是 user 提供值，**后 2 个固定写 1.0f / 1.0f**（对齐 `MATBIN.cs:263-269`）。
      - `Float4`：16 bytes
      - `Float5`：20 bytes
    - **Float3 字节级 round-trip 约束**：仅对**末尾两个 float 为 1.0f / 1.0f 的源数据**可保证 byte-level round-trip；非 1.0 尾部的 Float3 文件经 read → write 后尾部会被改写为 1.0/1.0（与上游一致的 lossy 行为）。**这不算 functional divergence**，因为 C 端 mirror 上游 lossy 语义；但 T24 / T27 测试 fixture 必须用 1.0/1.0 尾部来获得 byte-level 一致。
    - Sampler 子结构：Type / Path / Key / Unk14 (vec2)。
    - 8 个 typed value accessor 全实现：错误 type 调用返回 `SF_ERR_INVALID_ARG`。

  **Must NOT do**：
  - ❌ 不实现 Metis 已排除的 String / Vec3 ParamType。
  - ❌ 不暴露内部 offset 表结构。
  - ❌ 不为 sampler.Path 做路径转换（按上游字节保留）。
  - ❌ **不为 Float3 暴露第 4/5 个 float**（保持与上游 API 一致；尾部两个 1.0 是 C 端写出时硬编码）。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— 8 个 ParamType 路径 + typed value union + 字段次序对齐。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T19 并行）
  - **Parallel Group**: Wave 3
  - **Blocks**: T24, T27
  - **Blocked By**: T5（probe 验证 8 ParamType）, T11

  **References**：

  **Pattern References**：
  - `src/param/param.c`（Phase 4）—— typed value union 范式。
  - `src/text/fmg.c` —— 字符串字段 owner 语义。

  **API/Type References**：
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MATBIN.cs:全文` —— ground truth。
  - `MATBIN.cs:Param.ParamType` —— 8 个 ParamType 值与字节大小。

  **External References**：
  - T5 probe 输出 `.sisyphus/evidence/task-5-matbin-survey.md` —— 实测验证 8 ParamType。

  **WHY Each Reference Matters**：
  - 8 个 ParamType 是 Metis + T5 双重确认；少一个 = 实际 .matbin 读不出来。
  - Sampler.Unk14 是 vec2（非 vec4），上游字节布局明确。

  **Acceptance Criteria**：
  - [ ] `src/geom/matbin.c` 编译通过。
  - [ ] 8 个 typed value accessor 全实现，错误 type 返回 `SF_ERR_INVALID_ARG`。
  - [ ] T5 probe 选定的 10 个 sample MATBIN 全部 read 成功（在 T27 e2e 验证）。
  - [ ] 字段次序对齐上游（在 T24 合成 fixture 字节级 round-trip）。

  **QA Scenarios**：

  ```
  Scenario: 8 个 ParamType 全部 read+write 正确
    Tool: Bash + Unity
    Preconditions: T20 完成
    Steps:
      1. 构造 1 个 MATBIN 含 8 种 ParamType 各 1 个 param
      2. read → 用对应 typed accessor 读出值 → assert 一致
      3. write → diff
    Expected Result: 全部 PASS；字节级一致
    Failure Indicators: 任一 ParamType 失败
    Evidence: .sisyphus/evidence/task-20-paramtype.log

  Scenario: 错误 typed accessor 返回 invalid_arg
    Tool: Bash + Unity
    Steps:
      1. 构造一个 Param ParamType = Float
      2. 调 `sf_matbin_param_value_int(param, &out)` （错误 type）
      3. assert 返回 `SF_ERR_INVALID_ARG`
    Expected Result: 错误返回，无 crash
    Failure Indicators: 返回 SF_OK 或 crash
    Evidence: .sisyphus/evidence/task-20-wrong-type.log
  ```

  **Commit**: YES
  - Message: `phase6(matbin): MATBIN reader/writer with 8 ParamType union (Bool/Int/Int2/Float/Float2-5)`
  - Files: `src/geom/matbin.c`, `tests/geom/test_matbin_smoke.c`（与 T24 共享）
  - Pre-commit: 2 个 QA scenarios PASS

### Wave 4 — Decode helper + 合成 round-trip + ER e2e（Wave 2 + Wave 3 全绿后 7 路并行）

- [x] 21. **`src/geom/flver2_decode.c` —— `sf_flver2_decode_mesh` 实现（layout-driven 展开为 typed 数组）**

  **What to do**：
  - 创建 `src/geom/flver2_decode.c`，实现 `sf_flver2_decode_mesh` + `sf_flver2_decoded_mesh_free`：
    - 输入：FLVER2 + mesh_index + allocator。
    - 输出：`sf_flver2_decoded_mesh_t` 含 typed 数组。
    - **Algorithm**：
      1. 从 mesh 获取 face_set_index[0]（第一个 face_set 通常 LOD 0 高细节）+ vertex_buffer_index[0]。
      2. 通过 face_set 调 T15 的 `sf_internal_flver2_face_set_triangulate`（filter_degenerate = true）获取 u32 索引数组。
      3. 通过 vertex_buffer 获取 vertex_count + raw bytes pointer。
      4. 通过 vertex_buffer.LayoutIndex 取 BufferLayout。
      5. 计算 vertex stride（BufferLayout.Size 调 T16 实现）。
      6. **分配 typed 数组**：positions[N], normals[N]（若 layout 含 Normal semantic）, tangents[N]（若含）, uvs[k][N]（k 由 layout 中 UV semantic 出现次数决定）, colors[k][N], bone_indices[N]（若 skinned）, bone_weights[N], indices[index_count]。
      7. 构造 `sf_flver2_vertex_context_t`：uv_factor 按 header version 决定（如 ER 后 uvFactor=2048，旧版 1024；具体规则查 FLVER2.cs）；is_ac6 按 header version 判断。
      8. **逐顶点循环**：调 `sf_internal_flver2_vertex_decode_one(layout, &raw[i*stride], &ctx, &decoded_vertex)`；把 decoded_vertex 字段写入对应 typed 数组的第 i 项。
      9. NULL 字段：layout 中不存在的 semantic → typed 数组指针为 NULL（不分配，节省内存）。
    - `sf_flver2_decoded_mesh_free`：用同一 allocator 释放所有非 NULL 字段。
  - **错误处理**：
    - mesh_index 越界 → `SF_ERR_OUT_OF_RANGE`。
    - face_set_index[] / vertex_buffer_index[] 为空 → `SF_ERR_INVALID_ARG`（无法解码）。
    - decode_one 返回 unsupported → 立即停止，释放已分配 buffer，返回 `SF_ERR_UNSUPPORTED_VERSION`。
  - 仅处理 face_set[0]：上游 FLVER2 mesh 可有多个 face_set（不同 LOD），decode_mesh 默认取 LOD 0。后续 v1.1 可扩展 LOD index 参数。

  **Must NOT do**：
  - ❌ 不调用 std `malloc` —— 严格通过 allocator。
  - ❌ 不假设 layout 一定含 Position（虽然 99% 情况下含）；若无 Position → 返回 `SF_ERR_INVALID_ARG`。
  - ❌ 不重算 normal / tangent / bitangent —— 直接来自 vertex bytes 解码。
  - ❌ 不返回 LOD > 0 的 face_set（v1 只 LOD 0；v1.1 扩展时再加参数）。

  **Recommended Agent Profile**：
  - **Category**: `artistry` —— layout-driven 动态 buffer 分配 + 多 attribute 协调 + allocator 一致性 + 部分字段 NULL 处理。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T22-T27 并行；T17 阻塞）
  - **Parallel Group**: Wave 4
  - **Blocks**: T25, T26
  - **Blocked By**: T15, T16, T17

  **References**：

  **Pattern References**：
  - `src/core/binary_reader.c` —— allocator 一致性使用。

  **API/Type References**：
  - `Formats/FLVER/FLVER2/Mesh.cs:GetFaces()` —— 上游三角化 reference（filter_degenerate 行为）。
  - `Formats/FLVER/FLVER2/FLVER2.cs:Read()` 中的 uvFactor 计算逻辑 —— 版本对应的 uv_factor 值。
  - `Formats/FLVER/Vertex.cs:全文` —— attribute 字段集合（决定 sf_flver2_decoded_mesh_t 字段）。

  **External References**：
  - `docs/api-mapping/extensions.md` T3 草稿 —— decode_mesh 是 EXTENSION，必须文档化。

  **WHY Each Reference Matters**：
  - allocator 一致性：错用 std malloc 会导致 destroy 时 mismatch crash。
  - uv_factor 错误 = UV 坐标全部偏移（c0000.flver 上贴图错位）；版本探测要严格。
  - LOD 0 默认避免 v1 API 过度设计；v1.1 增加 LOD 参数不破坏 ABI。

  **Acceptance Criteria**：
  - [ ] `src/geom/flver2_decode.c` 编译通过。
  - [ ] `sf_flver2_decode_mesh` 在 T22 unit-cube fixture 上输出 8 positions + 8 normals + 12 indices（u32），全部值与构造时一致。
  - [ ] mesh_index 越界返回 `SF_ERR_OUT_OF_RANGE`，无 crash 无 leak。
  - [ ] decoded_mesh_free 正确释放所有非 NULL 字段（valgrind / ASAN clean）。
  - [ ] T25（decode_mesh 测试）+ T26（FLVER2 e2e）都引用本 task 实现。

  **QA Scenarios**：

  ```
  Scenario: decode_mesh 在 unit-cube 上输出正确
    Tool: Bash + Unity
    Preconditions: T21 完成 + T22 fixture 已写
    Steps:
      1. 构造 T22 unit cube FLVER2
      2. 调 `sf_flver2_decode_mesh(f, 0, &decoded, alloc)`
      3. assert decoded.vertex_count == 8、decoded.index_count == 36（6 面 × 2 三角 × 3 index），positions 第 0 项 = (0,0,0) 等
    Expected Result: 全部字段值正确
    Failure Indicators: count 错；或 positions 值错
    Evidence: .sisyphus/evidence/task-21-decode-cube.log

  Scenario: decode_mesh 错误路径
    Tool: Bash + Unity
    Steps:
      1. mesh_index = 999 → assert `SF_ERR_OUT_OF_RANGE`
      2. 构造无 face_set_index 的 mesh → assert `SF_ERR_INVALID_ARG`
      3. 构造含 unknown LayoutType 的 mesh → assert `SF_ERR_UNSUPPORTED_VERSION`
    Expected Result: 三个错误路径全部正确
    Failure Indicators: 任一返回 SF_OK 或 crash
    Evidence: .sisyphus/evidence/task-21-decode-errors.log

  Scenario: allocator 一致性（ASAN）
    Tool: Bash
    Preconditions: build-asan 可用（SF_ENABLE_SANITIZERS=ON）
    Steps:
      1. 重新构建 build-asan
      2. 跑 decode_mesh 测试
      3. ASAN 报告 0 leak / 0 mismatch
    Expected Result: clean
    Failure Indicators: 任何 leak / mismatch
    Evidence: .sisyphus/evidence/task-21-asan.log
  ```

  **Commit**: YES
  - Message: `phase6(flver2): sf_flver2_decode_mesh extension (layout-driven typed-array decode)`
  - Files: `src/geom/flver2_decode.c`, `src/internal/flver2_internal.h`, 单元测试
  - Pre-commit: 3 个 QA scenarios PASS（含 ASAN clean）

- [x] 22. **`tests/geom/test_flver2_synthetic.c` —— unit cube round-trip 字节级一致**

  **What to do**：
  - 创建 `tests/geom/test_flver2_synthetic.c`，构造一个手写的最小 FLVER2 字节流：
    - 1 mesh × 1 material × **8 vertices × 12 triangles × 3 = 36 indices**（unit cube 标准：6 面 × 2 三角 × 3）。
    - Header version = 0x2001A（ER 风格，便于 SkeletonSet 也覆盖一个 case）。
    - 1 BufferLayout：含 Position (Float3) + Normal (Byte4Norm) + UV (Short2) 3 个 LayoutMember（**使用上游真实 LayoutType 名**：Float3 / Byte4Norm / Short2）。
    - 1 VertexBuffer：8 vertex × (12 + 4 + 4) = 160 字节。
    - 1 FaceSet：**36 indices（u16，非 triangle_strip）= 72 字节** —— 对应 12 个三角形。
    - 1 Material + 1 Texture（type="g_DiffuseTexture", path="dummy.tpf")。
    - 1 Dummy（任意位置）。
    - 1 Node（root bone）。
    - 1 SkeletonSet 含 1 Bone（指向 Node 0）。
    - 0 GXList（GXListIndex = -1）。
  - 把整个字节流硬编码为 C const array（`static const uint8_t FIXTURE[] = { 0x46, 0x4C, ... };`）—— 大小 < 1KB 入仓。
  - 测试 cases：
    1. `sf_flver2_read_from_memory(FIXTURE, sizeof(FIXTURE))` → 成功；assert mesh_count = 1, material_count = 1, ...。
    2. `sf_flver2_write_to_memory(...)` → 输出字节数组。
    3. `memcmp(FIXTURE, output, sizeof(FIXTURE)) == 0` —— 字节级 round-trip。
    4. 各 accessor 值与构造时设定一致（mesh.material_index = 0、material.name = "test_material" 等）。

  **Must NOT do**：
  - ❌ 不动态生成 fixture（必须 hardcoded const array，保证可重现）。
  - ❌ 不依赖外部数据文件（fixture 自包含）。
  - ❌ 不超过 4KB（PLAN.md §8.5 已规定）。
  - ❌ 不引入 Edge / SPU / RSX 字段。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— 手工构造字节流（精确每字节）+ 大型 const array + 多 assert。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T23-T27 并行）
  - **Parallel Group**: Wave 4
  - **Blocks**: 无（叶子测试）
  - **Blocked By**: T12-T18 全部完成（FLVER2 implementation 就绪）

  **References**：

  **Pattern References**：
  - `tests/archive/test_bnd4_synthetic.c`（Phase 3）—— hardcoded fixture + round-trip 模式范本。
  - `tests/param/test_param_synthetic.c`（Phase 4）—— 类似。

  **API/Type References**：
  - 全部 T7-T18 的 API。

  **External References**：
  - T4 probe 输出 —— 参考 c0000.flver 的真实 byte structure 帮助手写 fixture。

  **WHY Each Reference Matters**：
  - 手写 fixture 是首个 round-trip 验证，比 e2e 更严格（e2e 可能因 c0000 太复杂而漏掉某些 corner），且每字节都在测试控制下。
  - 4KB 上限确保 fixture 入仓不污染 git history。

  **Acceptance Criteria**：
  - [ ] `tests/geom/test_flver2_synthetic.c` 提交且注册 `geom` label。
  - [ ] `ctest -L geom -R flver2_synthetic` PASS。
  - [ ] `memcmp(FIXTURE, output, ...)` 字节级一致。
  - [ ] Fixture 字节大小 < 1024。
  - [ ] 所有 8 vertex 的 positions 通过 decode_mesh（T21）取出后与构造值一致（间接验证 T21）。
  - [ ] FaceSet 含 **36 个 u16 索引**（12 三角 × 3 vertex/三角）。
  - [ ] 通过 decode_mesh 后 `decoded.index_count == 36`、`decoded.vertex_count == 8`。

  **QA Scenarios**：

  ```
  Scenario: synthetic round-trip 字节级一致
    Tool: Bash + Unity
    Preconditions: Wave 2 完成
    Steps:
      1. `cmake --build build-mingw --target souls_formats_test_flver2_synthetic`
      2. `ctest -R flver2_synthetic --output-on-failure 2>&1 | tee .sisyphus/evidence/task-22-rt.log`
      3. `grep -E 'PASS|FAIL' .sisyphus/evidence/task-22-rt.log`
    Expected Result: 全部 PASS
    Failure Indicators: 任何 FAIL（尤其 memcmp）
    Evidence: .sisyphus/evidence/task-22-rt.log + fixture.bin（写出的字节）

  Scenario: fixture 大小约束
    Tool: Bash
    Steps:
      1. `grep -E 'static const uint8_t FIXTURE\[\]' tests/geom/test_flver2_synthetic.c`
      2. 编译后 binary 中 fixture 的大小估算（或测试代码硬编码 sizeof(FIXTURE) < 1024 assert）
    Expected Result: < 1024
    Failure Indicators: > 1024
    Evidence: .sisyphus/evidence/task-22-fixture-size.log
  ```

  **Commit**: YES
  - Message: `phase6(test): flver2 synthetic unit-cube round-trip (byte-level)`
  - Files: `tests/geom/test_flver2_synthetic.c`, `tests/CMakeLists.txt`
  - Pre-commit: `ctest -L geom -R flver2_synthetic` PASS

- [x] 23. **`tests/geom/test_mtd_synthetic.c` —— MTD 合成 fixture round-trip**

  **What to do**：
  - 创建 `tests/geom/test_mtd_synthetic.c`：
    - 构造一个 MTD 字节流（textureBlock.Version = 5，模拟 Sekiro）：
      - ShaderPath = "M[L_Fur]_Hair.spx"
      - Description = "Hair material"
      - 3 个 Param（不同 ParamType：Int、Float、Bool）
      - 2 个 Texture：第 1 个 Extended（Path = "smdm:test.tga"、UnkFloats = [1.0, 2.0]）；第 2 个 non-Extended（textureBlock.Version = 3，Path 空、UnkFloats 空）。
    - hardcoded fixture < 1KB。
    - 测试 cases：read → assert 全部字段（含 Extended path 与 unk_floats，non-Extended 字段为空）→ write → memcmp 字节级一致。

  **Must NOT do**：
  - ❌ 不依赖任何 Sekiro 真实数据（合成 fixture 是闭合的）。
  - ❌ 不超过 1KB。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 小型 fixture + 直接 round-trip。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocks**: 无
  - **Blocked By**: T19

  **References**：

  **Pattern References**：
  - `tests/geom/test_flver2_synthetic.c:T22` —— hardcoded fixture 模式。
  - `tests/param/test_paramdef_binary.c`（Phase 4）—— 多 ParamType / multi-field fixture。

  **API/Type References**：
  - 全部 T10 + T19 的 API。

  **WHY Each Reference Matters**：
  - 合成 fixture 是 MTD 在 Sekiro 副本缺失时的唯一 verification 途径（e2e SKIP-allowed）；必须严格。
  - 版本 5 覆盖 Sekiro Extended，验证 T19 Extended 路径。

  **Acceptance Criteria**：
  - [ ] `tests/geom/test_mtd_synthetic.c` 提交且注册 `geom` label。
  - [ ] `ctest -L geom -R mtd_synthetic` PASS。
  - [ ] memcmp 字节级一致。
  - [ ] Extended texture 字段读出后与构造值一致：`path = "smdm:test.tga"`、`unk_float_count = 2`、`unk_float(t, 0) = 1.0` / `unk_float(t, 1) = 2.0`。
  - [ ] non-Extended texture：`has_extended = false`、`path = ""`、`unk_float_count = 0`。

  **QA Scenarios**：

  ```
  Scenario: MTD synthetic round-trip
    Tool: Bash + Unity
    Steps: 标准 round-trip + memcmp
    Expected Result: PASS
    Evidence: .sisyphus/evidence/task-23-mtd-rt.log
  ```

  **Commit**: YES
  - Message: `phase6(test): mtd synthetic round-trip with Sekiro Extended texture`
  - Files: `tests/geom/test_mtd_synthetic.c`, `tests/CMakeLists.txt`
  - Pre-commit: `ctest -L geom -R mtd_synthetic` PASS

- [x] 24. **`tests/geom/test_matbin_synthetic.c` —— MATBIN 合成 fixture round-trip + 8 ParamType 全覆盖**

  **What to do**：
  - 创建 `tests/geom/test_matbin_synthetic.c`：
    - 参考 T5 probe 输出的 minimum MATBIN hex dump 作为字节布局起点。
    - 构造一个 MATBIN 字节流，含**8 个 ParamType 各 1 个 param**（Bool / Int / Int2 / Float / Float2 / Float3 / Float4 / Float5）+ 3 个 Sampler。
    - **Float3 fixture 必须以 `1.0f / 1.0f` 结尾**（5 floats 实际占 20 字节）—— 这是上游 lossy round-trip 的兼容约束，T20 已说明。例如 Float3 param = `(0.5, 0.5, 0.5, 1.0, 1.0)` 文件字节，读 API 仅暴露 `(0.5, 0.5, 0.5)`。
    - hardcoded fixture < 1.5KB（8 ParamType 占空间略多）。
    - 测试 cases：
      1. read → 用对应 typed accessor 读出 8 个 param 值 → assert 一致（**Float3 只验证前 3 个 float**）。
      2. write → memcmp 字节级一致（**因为 fixture 的 Float3 尾部已设为 1.0/1.0，与 T20 write 行为对称**）。
      3. 错误 typed accessor 调用（Float param 调 `_value_int`）→ assert `SF_ERR_INVALID_ARG`。
      4. **额外 lossy 验证**：构造第二个 fixture 含 Float3 = `(0.5, 0.5, 0.5, 2.0, 3.0)` —— read → write → diff：尾部 2.0/3.0 会变成 1.0/1.0（**预期 lossy**）。该 case 不要求 byte-level 一致，**仅 verify 写出的尾部是 1.0/1.0**。

  **Must NOT do**：
  - ❌ 不引入 String / Vec3 / 第 9 个 ParamType（Metis 已排除）。
  - ❌ 不依赖 T5 probe 的 evidence 文件（生产代码不读 evidence）；但**结构**参考 probe 输出。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— 8 个 ParamType 全覆盖 + typed accessor 矩阵 + 错误路径。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocks**: 无
  - **Blocked By**: T20

  **References**：

  **Pattern References**：
  - `tests/geom/test_mtd_synthetic.c:T23` —— sister test。
  - `tests/param/test_param_synthetic.c`（Phase 4）—— 多 ParamType 测试模式。

  **API/Type References**：
  - 全部 T11 + T20 的 API。

  **External References**：
  - T5 probe 输出 `.sisyphus/evidence/task-5-matbin-survey.md` —— 最小 MATBIN hex dump 参考。

  **WHY Each Reference Matters**：
  - 8 ParamType 全覆盖是 ABI 完整性的关键验证；漏 1 个 = 真实 MATBIN 可能读不出来。
  - 错误 typed accessor 验证 API 一致性（Phase 4 已建立 SF_ERR_INVALID_ARG 约定）。

  **Acceptance Criteria**：
  - [ ] `tests/geom/test_matbin_synthetic.c` 提交且注册 `geom` label。
  - [ ] `ctest -L geom -R matbin_synthetic` PASS。
  - [ ] memcmp 字节级一致（**Float3 fixture 尾部为 1.0/1.0 时**）。
  - [ ] 8 个 typed accessor 全部验证（Bool / Int / Int2 / Float / Float2 / Float3 / Float4 / Float5）。
  - [ ] 错误 typed accessor 调用返回 `SF_ERR_INVALID_ARG`。
  - [ ] **Float3 lossy 验证**：非 1.0 尾部 fixture 经 read → write 后，输出文件尾部两 float 必为 1.0/1.0（与上游一致 lossy 行为）。

  **QA Scenarios**：

  ```
  Scenario: 8 ParamType round-trip + typed accessor
    Tool: Bash + Unity
    Steps: round-trip + 8 个 accessor 各一次 + 至少 2 个错误调用
    Expected Result: PASS
    Evidence: .sisyphus/evidence/task-24-matbin-rt.log

  Scenario: ParamType 完整性
    Tool: Bash
    Steps:
      1. `grep -E 'SF_MATBIN_PARAM_TYPE_(BOOL|INT|INT2|FLOAT|FLOAT2|FLOAT3|FLOAT4|FLOAT5)' tests/geom/test_matbin_synthetic.c | sort -u | wc -l`
    Expected Result: = 8
    Failure Indicators: < 8（漏类型）
    Evidence: .sisyphus/evidence/task-24-paramtype-coverage.log
  ```

  **Commit**: YES
  - Message: `phase6(test): matbin synthetic round-trip + 8 ParamType + 3 sampler`
  - Files: `tests/geom/test_matbin_synthetic.c`, `tests/CMakeLists.txt`
  - Pre-commit: `ctest -L geom -R matbin_synthetic` PASS

- [x] 25. **`tests/geom/test_flver2_decode.c` —— decode_mesh 合成 fixture 验证 + c0000 layout 子集**

  **What to do**：
  - 创建 `tests/geom/test_flver2_decode.c`：
    - 复用 T22 的 unit cube fixture，调 `sf_flver2_decode_mesh(f, 0, &decoded, alloc)`。
    - assert：
      - `decoded.vertex_count == 8`
      - `decoded.index_count == 36`（6 面 × 2 三角 × 3）
      - `decoded.positions != NULL`、8 个值与构造时一致
      - `decoded.normals != NULL`（fixture 含 normal layout）
      - `decoded.uvs[0] != NULL`（fixture 含 UV layout）
      - `decoded.tangents == NULL`（fixture 不含 tangent）
      - `decoded.bone_indices == NULL`（fixture unskinned）
      - `decoded.indices` 是 u32 数组
    - 调 `sf_flver2_decoded_mesh_free(&decoded, alloc)` 不 crash。
    - **额外测试**：构造一个**只有 c0000 子集 layout** 的 fixture（用 T4 probe 输出的 5 个最常见 (Type, Semantic) 对），各跑一遍 decode，无 `SF_ERR_UNSUPPORTED_VERSION`。

  **Must NOT do**：
  - ❌ 不依赖 ER 真实数据（合成）；c0000 e2e 在 T26。
  - ❌ 不假设 normals 字段一定存在（layout 决定）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— 多 attribute 验证 + NULL 字段检测 + multi-fixture。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T22-T24, T26-T27 并行）
  - **Parallel Group**: Wave 4
  - **Blocks**: 无
  - **Blocked By**: T17, T21, T22

  **References**：

  **Pattern References**：
  - `tests/geom/test_flver2_synthetic.c:T22` —— fixture 复用。
  - `tests/param/test_param_apply_paramdef_e2e.c`（Phase 4 / 概念类比，多 sub-fixture）。

  **API/Type References**：
  - T21 的 `sf_flver2_decode_mesh` API。
  - T15 的三角化 helper（间接通过 decode_mesh 调用）。

  **External References**：
  - T4 probe 输出 —— c0000 5 个最常见 (Type, Semantic) 对的 ground truth。

  **WHY Each Reference Matters**：
  - 合成 fixture 验证 decode_mesh 的功能；e2e 验证 c0000 真实 data 上的 robustness。
  - NULL 字段检测确保 decoded_mesh struct 正确表达 layout 中 absent semantic。

  **Acceptance Criteria**：
  - [ ] `tests/geom/test_flver2_decode.c` 提交且注册 `geom` label。
  - [ ] `ctest -L geom -R flver2_decode` PASS。
  - [ ] decoded fields 与 expected 一致；NULL 字段正确为 NULL。
  - [ ] 第二 fixture 跑通 c0000 子集（≥ 5 个 (Type, Semantic)）。
  - [ ] `sf_flver2_decoded_mesh_free` 不 crash 无 leak（ASAN 下）。

  **QA Scenarios**：

  ```
  Scenario: decode unit cube
    Tool: Bash + Unity
    Steps: 标准 decode + assert
    Expected Result: PASS
    Evidence: .sisyphus/evidence/task-25-decode-cube.log

  Scenario: c0000 layout 子集 覆盖
    Tool: Bash
    Steps:
      1. 第二 fixture 跑通 T4 probe 列出的 ≥ 5 个 (Type, Semantic)
      2. assert 无 `SF_ERR_UNSUPPORTED_VERSION`
    Expected Result: 全部覆盖
    Failure Indicators: 任一 (Type, Semantic) 返回 unsupported → T17 dispatch 未覆盖
    Evidence: .sisyphus/evidence/task-25-c0000-subset.log
  ```

  **Commit**: YES
  - Message: `phase6(test): flver2_decode_mesh synthetic + c0000-subset coverage`
  - Files: `tests/geom/test_flver2_decode.c`, `tests/CMakeLists.txt`
  - Pre-commit: `ctest -L geom -R flver2_decode` PASS

- [x] 26. **`tests/geom/test_flver2_e2e_er.c` —— c0000.flver via er_extract_from_data0 完整 e2e**

  **What to do**：
  - 创建 `tests/geom/test_flver2_e2e_er.c`：
    - 调 `er_helper_init()`（Phase 3 helper，复用）。
    - 调 `er_extract_from_data0("/chr/c0000.chrbnd.dcx", &out, &out_size)` → 得 BND4 字节。
    - 用 BND4 reader 找 `c0000.flver` entry → 得 FLVER2 字节。
    - 调 `sf_flver2_read_from_memory(...)` → 成功。
    - assert：
      - mesh_count > 0
      - bone_count > 0（c0000 是有 skeleton 的 player 模型）
      - material_count > 0
      - header version ∈ whitelist
      - **关键**：遍历所有 buffer_layout 的所有 LayoutMember，每个 (Type, Semantic) 对必须能被 T17 dispatch 处理（无 `SF_ERR_UNSUPPORTED_VERSION`）—— 即 `sf_flver2_decode_mesh` 对**每个 mesh** 都 PASS（**不允许 KNOWN_LAYOUT_GAP**）。
    - 取第 0 个 mesh 跑 decode_mesh → assert decoded.vertex_count > 0 且 == vertex_buffer.vertex_count（一致性）。
    - **Sanity check**：UPSTREAM.md 记录的 c0000.chrbnd.dcx sha256 = 实际提取的 sha256（若不一致，TEST_IGNORE_MESSAGE）。
    - **Round-trip**：`sf_flver2_write_to_memory` 写出 → diff 与 input 字节级一致（**c0000 round-trip 是 Phase 6 最高 fidelity 验证**）。

  **Must NOT do**：
  - ❌ 不假设特定 mesh / bone 数量（c0000 可能因 patch 微变）；只 assert > 0 + 一致性。
  - ❌ 不依赖 oodle DLL 存在（er_helper_init 已处理 SKIP）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— e2e 全链路 + 多 assert + round-trip + sha256 check。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocks**: 无
  - **Blocked By**: T12-T18 + T21（FLVER2 + decode 全实现）, T6（sha256 在 UPSTREAM.md）

  **References**：

  **Pattern References**：
  - `tests/e2e/test_bnd4_e2e_er.c`（Phase 3）—— c0000.chrbnd.dcx 提取模式范本。
  - `tests/param/test_fmg_e2e_er.c`（Phase 4）—— 经 er_extract_from_data0 提取 BND4 → 查 entry 模式。

  **API/Type References**：
  - 全部 T7-T21 的 API。
  - `tests/e2e/er_test_helper.h` —— `er_helper_init` / `er_extract_from_data0`。

  **External References**：
  - T6 UPSTREAM.md sha256 snapshot。

  **WHY Each Reference Matters**：
  - c0000.flver 是 Phase 6 e2e 的核心目标（PLAN.md §8.6 明确）；全链路 PASS = Phase 6 成功标志。
  - Round-trip 字节级一致是验证 read + write 完全对称的最高 fidelity 手段。
  - sha256 sanity check 防止用户升级游戏导致测试断言被误判 bug。

  **Acceptance Criteria**：
  - [ ] `tests/geom/test_flver2_e2e_er.c` 提交且注册 `e2e_er` label。
  - [ ] `ctest -L e2e_er -R flver2` PASS（**SKIP-disallowed**，除非 ER 数据 / Oodle DLL 不可达）。
  - [ ] c0000.flver round-trip 字节级一致（`memcmp` 验证）。
  - [ ] decode_mesh 对 c0000 第 0 个 mesh PASS。
  - [ ] 全部 BufferLayout member 在 T17 dispatch 中处理（无 `SF_ERR_UNSUPPORTED_VERSION`）。

  **QA Scenarios**：

  ```
  Scenario: c0000.flver 完整 e2e
    Tool: Bash + Unity
    Preconditions: ER 数据 + Oodle DLL 可达
    Steps:
      1. `ctest -L e2e_er -R flver2 --output-on-failure 2>&1 | tee .sisyphus/evidence/task-26-flver2-e2e.log`
      2. `grep -E 'PASS|FAIL|SKIP' .sisyphus/evidence/task-26-flver2-e2e.log`
    Expected Result: PASS（无 FAIL；SKIP 只在 ER 数据缺失时可接受）
    Failure Indicators: FAIL 或 `SF_ERR_UNSUPPORTED_VERSION` 出现
    Evidence: .sisyphus/evidence/task-26-flver2-e2e.log

  Scenario: round-trip 字节级一致
    Tool: Bash
    Steps:
      1. 测试代码保存 input.bin 与 output.bin 到 evidence
      2. `diff .sisyphus/evidence/task-26-input.bin .sisyphus/evidence/task-26-output.bin`
    Expected Result: 无差异
    Failure Indicators: 任何字节偏差
    Evidence: .sisyphus/evidence/task-26-{input,output}.bin

  Scenario: sha256 sanity
    Tool: Bash
    Steps:
      1. 实际提取 c0000.chrbnd.dcx 计算 sha256
      2. 与 UPSTREAM.md 记录值对比
    Expected Result: 一致；若不一致测试 TEST_IGNORE_MESSAGE 提示重抓 snapshot
    Failure Indicators: 不一致但测试未触发 SKIP → bug
    Evidence: .sisyphus/evidence/task-26-sha256.log
  ```

  **Commit**: YES
  - Message: `phase6(test): flver2 e2e via c0000.flver with byte-level round-trip`
  - Files: `tests/geom/test_flver2_e2e_er.c`, `tests/CMakeLists.txt`
  - Pre-commit: `ctest -L e2e_er -R flver2` PASS

- [x] 27. **`tests/geom/test_matbin_e2e_er.c` —— allmaterial.matbinbnd.dcx e2e**

  **What to do**：
  - 创建 `tests/geom/test_matbin_e2e_er.c`：
    - 调 `er_extract_from_data0("/material/allmaterial.matbinbnd.dcx", &out, &out_size)`。
    - 用 BND4 reader 取**任一** `.matbin` entry（用 T5 probe 选定的 10 个样本之一）。
    - 调 `sf_matbin_read_from_memory(...)` → 成功。
    - assert：
      - shader_path 非空（应为 `ER_*.spx` pattern）
      - source_path 非空
      - param_count > 0
      - sampler_count > 0
      - 至少一个 param 的 ParamType ∈ {Bool, Int, Int2, Float, Float2, Float3, Float4, Float5}（8 变体的子集，验证 T20 实现 dispatch）
    - Round-trip：write → diff 字节级一致。
    - **可选**：跑 T5 probe 选定的全 10 个 sample，全部 read 成功。

  **Must NOT do**：
  - ❌ 不假设 sampler_count > 1（某些 shader-only material 可能 0 个 sampler）。
  - ❌ 不假设特定 param 名存在。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— e2e 多 sample + 全 ParamType 覆盖验证。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocks**: 无
  - **Blocked By**: T20, T6, T5

  **References**：

  **Pattern References**：
  - `tests/e2e/test_bnd4_e2e_er.c`（Phase 3）—— BND4 entry 提取模式。
  - `tests/param/test_fmg_e2e_er.c`（Phase 4）—— 类似 e2e 多 entry 测试。

  **API/Type References**：
  - 全部 T11 + T20 的 API。

  **External References**：
  - T5 probe 输出 —— 10 个 sample 路径列表。
  - T6 UPSTREAM.md sha256。

  **WHY Each Reference Matters**：
  - MATBIN e2e 是验证 T20 + T11 + 8 ParamType 在真实数据上 robust 的关键。
  - 10 个 sample 是 T5 probe 选定的代表性集合，覆盖不同 shader / 不同 ParamType 分布。

  **Acceptance Criteria**：
  - [ ] `tests/geom/test_matbin_e2e_er.c` 提交且注册 `e2e_er` label。
  - [ ] `ctest -L e2e_er -R matbin` PASS。
  - [ ] 至少 1 个 sample MATBIN read 成功并 round-trip 字节级一致。
  - [ ] T5 probe 选定的 10 个 sample 全 read 成功（**可选 stretch goal**，至少 8/10）。

  **QA Scenarios**：

  ```
  Scenario: MATBIN e2e 基础
    Tool: Bash + Unity
    Steps:
      1. `ctest -L e2e_er -R matbin --output-on-failure 2>&1 | tee .sisyphus/evidence/task-27-matbin-e2e.log`
    Expected Result: PASS
    Failure Indicators: FAIL 或 SKIP（除非数据缺失）
    Evidence: .sisyphus/evidence/task-27-matbin-e2e.log

  Scenario: 10 个 sample 多数 PASS
    Tool: Bash
    Steps:
      1. 测试代码循环 T5 probe 列出的 10 个路径，统计 PASS 数
      2. assert PASS 数 ≥ 8
    Expected Result: ≥ 8/10
    Failure Indicators: < 8 → 可能有未发现的 ParamType variant
    Evidence: .sisyphus/evidence/task-27-matbin-samples.log
  ```

  **Commit**: YES
  - Message: `phase6(test): matbin e2e via allmaterial.matbinbnd.dcx`
  - Files: `tests/geom/test_matbin_e2e_er.c`, `tests/CMakeLists.txt`
  - Pre-commit: `ctest -L e2e_er -R matbin` PASS

- [x] 27b. **`tests/geom/test_mtd_e2e_sekiro.c` —— Sekiro MTD e2e（SKIP-allowed）**

  > **注**：编号 27b 以保持 27 个 task 总数；本 task 与 T27 平行属于 Wave 4，但 SKIP-allowed。

  **What to do**：
  - 创建 `tests/geom/test_mtd_e2e_sekiro.c`：
    - 调用 Phase 5 `sekiro_test_helper`（复用，不新建）。
    - 从 Sekiro Data1-5 BHD5 提取一个 `.mtd` 或含 .mtd 的 BND（具体路径需 Sekiro 数据探测；commonly `/chr/c0000.partsbnd.dcx` 或 `/parts/wp_a_0010.partsbnd.dcx`）。
    - read 该 .mtd；assert shader_path 非空、param_count > 0、texture_count > 0。
    - **若 Sekiro 副本不可达**：`sekiro_test_helper` 初始化失败 → `TEST_IGNORE_MESSAGE("Sekiro data not available, skipping")`。
  - 此 task 是 PLAN.md §8.6 SKIP-allowed 政策的实施。

  **Must NOT do**：
  - ❌ 不把 SKIP 当 FAIL（CI 与本地 dev loop 都不应阻塞）。
  - ❌ 不复用 ER helper（Sekiro Data 布局不同，必须用 sekiro_test_helper）。
  - ❌ 不写硬编码 Sekiro 路径假设；用 sekiro_test_helper 抽象。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— 多游戏 e2e helper 复用 + 条件 SKIP。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocks**: 无
  - **Blocked By**: T19（MTD 实现）；Phase 5 sekiro_test_helper（已完成）

  **References**：

  **Pattern References**：
  - Phase 5 `tests/e2e/test_msbs_e2e_sekiro.c`（若存在）—— Sekiro e2e + SKIP 模式。
  - `tests/e2e/sekiro_test_helper.h` —— Phase 5 已建好。

  **API/Type References**：
  - 全部 T10 + T19 的 API。

  **External References**：
  - PLAN.md §8.6 SKIP-allowed 政策。

  **WHY Each Reference Matters**：
  - Sekiro 副本不可达是常态（用户尚未提供）；SKIP-allowed 政策是 Phase 6 不被阻塞的前提。
  - sekiro_test_helper 在 Phase 5 已建好；本 task 直接复用，不新建。

  **Acceptance Criteria**：
  - [ ] `tests/geom/test_mtd_e2e_sekiro.c` 提交且注册 `e2e_sekiro` label。
  - [ ] `ctest -L e2e_sekiro -R mtd` 返回 PASS 或 SKIP（取决于 Sekiro 数据）。
  - [ ] 若 PASS：shader_path 非空、param_count > 0。
  - [ ] 若 SKIP：日志含 `"Sekiro data not available"` 字样，不被误判 FAIL。

  **QA Scenarios**：

  ```
  Scenario: Sekiro 数据存在时 PASS
    Tool: Bash + Unity
    Preconditions: /mnt/c/Games/Sekiro/ 存在
    Steps:
      1. `ctest -L e2e_sekiro -R mtd --output-on-failure 2>&1 | tee .sisyphus/evidence/task-27b-mtd-sekiro.log`
    Expected Result: PASS
    Evidence: .sisyphus/evidence/task-27b-mtd-sekiro.log

  Scenario: Sekiro 数据缺失时 SKIP
    Tool: Bash
    Preconditions: /mnt/c/Games/Sekiro/ 不存在
    Steps:
      1. 同上
      2. `grep -E 'IGNORED|SKIP' .sisyphus/evidence/task-27b-mtd-sekiro.log`
    Expected Result: 命中 SKIP / IGNORED
    Failure Indicators: 命中 FAIL 而非 SKIP
    Evidence: .sisyphus/evidence/task-27b-mtd-sekiro.log
  ```

  **Commit**: YES
  - Message: `phase6(test): mtd e2e via Sekiro (SKIP-allowed)`
  - Files: `tests/geom/test_mtd_e2e_sekiro.c`, `tests/CMakeLists.txt`
  - Pre-commit: `ctest -L e2e_sekiro -R mtd` PASS or SKIP

### Wave 5 — Docs + 状态表 final pass（Wave 4 全绿后 3 路并行）

- [x] 28. **4 份 mapping doc 全量刷新（flver-common / flver2 / mtd / matbin）+ Edge 子表保持 `_skipped_` + extensions.md 三条 final**

  **What to do**：
  - **format-flver-common.md**（27 行）：
    - 每行 status 列从 `未实现` 改为 `✓ aligned` 或对应状态。
    - 每行 References 列补 `src/geom/flver_common.c:行号` 或 `include/souls_formats/sf_flver.h:行号`。
    - Notes 列补关键 C 端语义差异（如 string fields owned by allocator）。
  - **format-flver2.md**（78 行，58 in-scope）：
    - API 主表 31 行 + Vertex Element Layout 19 行 + Vertex Format Dispatch 8 行 全部刷新（58 in-scope）。
    - **Edge 子表 20 行保持 `_skipped_`**（T2 已设；本 task verify 未漂移）。
    - 顶部 Status 段补 「Phase 6 完成 (YYYY-MM-DD)」标记。
    - 关键差异 Notes：BufferLayout / VertexBuffer 是 global 共享、GXItem 是 opaque、SkeletonSet 版本门控、decode_mesh 是扩展。
  - **format-mtd.md**（19 行）：
    - 全部 status 刷新。
    - Sekiro Extended texture info 在对应行 Notes 中标注。
  - **format-matbin.md**（17 行）：
    - 全部 status 刷新。
    - 8 个 ParamType 各对应行 Notes 中标注 typed accessor 名。
  - **extensions.md 三条 entry final pass**：
    - T3 起草的 stub → 完整 entry（含真实 file:line 引用、实际 C API 签名、最终 impact 描述）。
    - 验证每条 entry 含「Type / Upstream Ref / C API / Rationale / Impact」5 段。

  **Must NOT do**：
  - ❌ 不动 Edge 三子表的 `_skipped_` 标记（永久 OUT-of-scope）。
  - ❌ 不增删 mapping row（保持 Phase 6 起步时的总行数）。
  - ❌ 不写 "TODO" / "later" —— Phase 6 完成时所有 status 必须 final。

  **Recommended Agent Profile**：
  - **Category**: `writing` —— 4 份大型 mapping doc 文档刷新。
  - **Skills**: `tech-doc-style-chinese`（mapping doc 是中文）。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T29, T30 并行）
  - **Parallel Group**: Wave 5
  - **Blocks**: Wave Final F1（plan compliance 会 verify status 完整）
  - **Blocked By**: T7-T27（所有实现完成才能刷新 status）

  **References**：

  **Pattern References**：
  - Phase 5 完成时 `docs/api-mapping/format-msb*.md` 刷新结果（参考 status 列填法）。
  - Phase 4 完成时 `docs/api-mapping/format-paramdef.md` 刷新（参考 references 列填法）。

  **API/Type References**：
  - 全部 Phase 6 实现源码（T7-T21）—— 每个 mapping row 的 References 列必须指向实际代码 file:line。

  **External References**：无。

  **WHY Each Reference Matters**：
  - Phase 4/5 已建立 mapping doc final pass 的 schema 范本；复用避免风格漂移。
  - References 列指向实际代码 = F1 reviewer 可机器验证 alignment。

  **Acceptance Criteria**：
  - [ ] `grep -c '未实现' docs/api-mapping/format-flver-common.md` = 0。
  - [ ] `grep -c '未实现' docs/api-mapping/format-flver2.md` = 0（Edge 行已转 `_skipped_`）。
  - [ ] `grep -c '_skipped_' docs/api-mapping/format-flver2.md` ≥ 20（Edge 三子表保持）。
  - [ ] `grep -c '未实现' docs/api-mapping/format-mtd.md` = 0。
  - [ ] `grep -c '未实现' docs/api-mapping/format-matbin.md` = 0。
  - [ ] `docs/api-mapping/extensions.md` 含 3 条完整 entry（decode_mesh / BE refusal / EdgeCompression refusal）。
  - [ ] 至少 50% 的 mapping row 在 References 列含 `src/geom/` 或 `include/souls_formats/` file:line 引用。

  **QA Scenarios**：

  ```
  Scenario: 4 doc 全部 in-scope row 状态 final
    Tool: Bash
    Steps:
      1. `grep -c '未实现' docs/api-mapping/format-flver-common.md docs/api-mapping/format-flver2.md docs/api-mapping/format-mtd.md docs/api-mapping/format-matbin.md`
    Expected Result: 4 个 0
    Failure Indicators: 任一 > 0
    Evidence: .sisyphus/evidence/task-28-mapping-final.log

  Scenario: Edge 子表保持 _skipped_
    Tool: Bash
    Steps:
      1. `awk '/Edge Geometry Enums/,/^##/' docs/api-mapping/format-flver2.md | grep -c '_skipped_'`
      2. `awk '/RsxVertexFormat/,/^##/' docs/api-mapping/format-flver2.md | grep -c '_skipped_'`
      3. `awk '/EdgeGeomSkin/,/^##/' docs/api-mapping/format-flver2.md | grep -c '_skipped_'`
    Expected Result: 步骤 1 = 8、2 = 5、3 = 7
    Failure Indicators: 任一数量偏差
    Evidence: .sisyphus/evidence/task-28-edge-skipped-stable.log

  Scenario: extensions.md 3 条 entry 完整
    Tool: Bash
    Steps:
      1. `grep -c -E '(decode_mesh|BE refusal|EdgeCompression)' docs/api-mapping/extensions.md`
      2. `grep -c -E '(Type:|Upstream Ref:|C API:|Rationale:|Impact:)' docs/api-mapping/extensions.md`
    Expected Result: 步骤 1 ≥ 3；步骤 2 ≥ 15（3 条 × 5 段）
    Failure Indicators: 缺段
    Evidence: .sisyphus/evidence/task-28-extensions-final.log

  Scenario: References 列填充率
    Tool: Bash
    Steps:
      1. `total=$(grep -c '^|' docs/api-mapping/format-flver2.md); refs=$(grep -c 'src/geom\|include/souls_formats' docs/api-mapping/format-flver2.md); echo "refs=$refs total=$total ratio=$((refs*100/total))%"`
    Expected Result: ratio ≥ 30%（合理填充；不要求 100% 因部分 row 可能是 helper 不直接对应文件）
    Failure Indicators: ratio < 30%
    Evidence: .sisyphus/evidence/task-28-ref-fill.log
  ```

  **Commit**: YES
  - Message: `phase6(docs): refresh 4 mapping docs + finalize extensions.md`
  - Files: `docs/api-mapping/format-flver-common.md`, `format-flver2.md`, `format-mtd.md`, `format-matbin.md`, `extensions.md`
  - Pre-commit: 4 个 grep 检查 PASS

- [x] 29. **`.sisyphus/plans/PLAN.md` §7 Phase 6 章节 checkbox 全勾 + §1 状态表 final**

  **What to do**：
  - **PLAN.md §7 Phase 6 章节**（第 637-663 行附近）：
    - 子标题 `### Phase 6 — 几何与材质（预估 3 周）` 追加 `✅ 完成 (YYYY-MM-DD) — N/N PASS across M test binaries`（N/M 由实测命令 `ctest -L geom --output-on-failure` 得出）。
    - 章节内 11 个 `- [ ]` checkbox 改 `- [x]`：
      - `sf_flver.h` / `sf_flver2.{h,c}` / `sf_mtd.{h,c}` / `sf_matbin.{h,c}` / QA 场景 / 合成 fixture / ER e2e。
  - **PLAN.md §1 表格**（第 16-23 行附近）：
    - Phase 6 行：state = `✅ done`、Tests 列填 `N/N PASS across M test binaries (YYYY-MM-DD)`。
  - **AGENTS.md §2** 表（第 24-35 行附近）：
    - Phase 6 行：state = `✅ done`、Tests 列同步。

  **Must NOT do**：
  - ❌ 不动 Phase 7 / v2 章节。
  - ❌ 不动 PLAN.md §2-§6 / §8-§13 任何架构 / 测试策略章节。
  - ❌ 不发明 Tests 数；必须实测。

  **Recommended Agent Profile**：
  - **Category**: `writing` —— PLAN + AGENTS 文档刷新。
  - **Skills**: `tech-doc-style-chinese`。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T28, T30 并行）
  - **Parallel Group**: Wave 5
  - **Blocks**: Wave Final
  - **Blocked By**: T7-T27（实现 + 测试全完成）

  **References**：

  **Pattern References**：
  - PLAN.md Phase 4/5 完成时的 final pass 格式（已建立 `✅ 完成 (date) — N/N PASS across M test binaries` 范本）。
  - AGENTS.md §2 既有 Phase 3 / 4 / 5 行格式。

  **External References**：
  - 实测命令：`ctest --test-dir build-mingw -L 'geom|e2e_er' --output-on-failure`。

  **WHY Each Reference Matters**：
  - PLAN.md 是 Momus-audited canonical 状态源；Phase 6 完成后必须同步否则项目 onboarding 文档断层。
  - Phase 4/5 已建立范本；复用避免风格漂移。

  **Acceptance Criteria**：
  - [ ] PLAN.md §7 Phase 6 子标题含 `✅ 完成`。
  - [ ] PLAN.md §7 Phase 6 章节 0 个 `- [ ]`。
  - [ ] PLAN.md §1 表 Phase 6 行 = `✅ done`。
  - [ ] AGENTS.md §2 表 Phase 6 行 = `✅ done`。
  - [ ] 实测 ctest N/N PASS（0 failed），数字与文档一致。

  **QA Scenarios**：

  ```
  Scenario: Phase 6 章节 0 未勾 checkbox
    Tool: Bash
    Steps:
      1. `awk '/^### Phase 6/,/^### Phase 7/' .sisyphus/plans/PLAN.md | grep -c '^- \[ \]'`
    Expected Result: 0
    Failure Indicators: > 0
    Evidence: .sisyphus/evidence/task-29-phase6-checkboxes.log

  Scenario: PLAN.md / AGENTS.md 三处状态一致
    Tool: Bash
    Steps:
      1. `grep -E '\| 6 \|.*✅' AGENTS.md`
      2. `grep -E '\| 6 \|.*✅' .sisyphus/plans/PLAN.md`
      3. `grep -E 'Phase 6.*✅' .sisyphus/plans/PLAN.md`
    Expected Result: 三处全部命中
    Failure Indicators: 任一 = 空
    Evidence: .sisyphus/evidence/task-29-state-sync.log

  Scenario: 实测测试数与文档一致
    Tool: Bash
    Steps:
      1. `ctest --test-dir build-mingw -L 'geom|e2e_er' --output-on-failure 2>&1 | tail -3 | grep -oE 'out of [0-9]+'`
      2. 与 AGENTS.md / PLAN.md 中记录的 N 对比
    Expected Result: 一致
    Failure Indicators: 不一致 → 文档凭空写
    Evidence: .sisyphus/evidence/task-29-test-count.log
  ```

  **Commit**: YES
  - Message: `phase6(docs): PLAN.md §7 Phase 6 checkboxes complete + AGENTS.md status`
  - Files: `.sisyphus/plans/PLAN.md`, `AGENTS.md`
  - Pre-commit: `ctest -L 'geom|e2e_er'` 0 failed

- [x] 30. **`docs/roadmap/phase-6-geometry-material.md` 与本 plan 收尾对齐 + `docs/roadmap/README.md` Phase 6 状态切换**

  **What to do**：
  - **`docs/roadmap/phase-6-geometry-material.md`**：
    - 顶部 `Status` 行从 `⏳ Pending` 改为 `✅ Done (YYYY-MM-DD)`。
    - `Deliverables` 段同步本 plan 实际交付（5 公共头 + 10 源码 + 7 测试 + 4 mapping doc 刷新）。
    - `File structure` 段补 `src/geom/flver2_decode.c`（roadmap 原稿未列出此文件）。
    - `Implementation notes` 段补 Metis 修正后的关键决策（顶点 dispatch foreach 结构、BE 拒绝、Edge OUT-of-scope、SkeletonSet 版本门控、GXItem opaque、`sf_flver2_decode_mesh` 是扩展）。
    - `QA scenarios` 段更新为本 plan 实际跑过的测试集（7 个测试 + 4 reviewer）。
    - `Risks` 段标注实际发现 + 缓解。
    - `Exit criteria` 全部 box 改 `- [x]`。
  - **`docs/roadmap/README.md`**：
    - Phase index 表 Phase 6 行：state = `✅ done`、estimate 实际值（如 `3 wk actual`）、doc 列保持。
    - 总 v1 effort 段：若 Phase 6 实际超时间，调整估算。

  **Must NOT do**：
  - ❌ 不重写 roadmap 整体结构；增量更新。
  - ❌ 不动 Phase 7 / v2 / post-v1 章节。

  **Recommended Agent Profile**：
  - **Category**: `writing` —— 英文 roadmap 文档同步。
  - **Skills**: 无（roadmap 是英文）。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T28, T29 并行）
  - **Parallel Group**: Wave 5
  - **Blocks**: Wave Final
  - **Blocked By**: T7-T27

  **References**：

  **Pattern References**：
  - Phase 5 完成后的 `docs/roadmap/phase-5-script-map.md` final 形态（参考）。
  - `docs/roadmap/phase-4-param-text.md` 完成形态。

  **API/Type References**：无（纯文档）。

  **External References**：无。

  **WHY Each Reference Matters**：
  - Roadmap doc 是 Phase 6 期间执行者的工作 spec；完成后须 final pass 保留为「已完成阶段的 retrospective 记录」。
  - Phase 4/5 final pass 范本已建立结构。

  **Acceptance Criteria**：
  - [ ] `docs/roadmap/phase-6-geometry-material.md` 顶部 Status = `✅ Done`。
  - [ ] roadmap doc 含 Metis 修正后 6 条决策（顶点 dispatch / BE / Edge / SkeletonSet / GXItem / decode_mesh）。
  - [ ] roadmap doc Exit criteria 全 `- [x]`。
  - [ ] `docs/roadmap/README.md` Phase 6 行 state = `✅ done`。

  **QA Scenarios**：

  ```
  Scenario: roadmap doc final pass
    Tool: Bash
    Steps:
      1. `grep -E '^> \*\*Status\*\*: ✅ Done' docs/roadmap/phase-6-geometry-material.md`
      2. `awk '/^## Exit criteria/,/^---/' docs/roadmap/phase-6-geometry-material.md | grep -c '^- \[ \]'`
    Expected Result: 步骤 1 命中；步骤 2 = 0
    Failure Indicators: 任一未达成
    Evidence: .sisyphus/evidence/task-30-roadmap-final.log

  Scenario: README.md Phase index 同步
    Tool: Bash
    Steps:
      1. `grep -E '\| 6 \|.*✅ done' docs/roadmap/README.md`
    Expected Result: 命中
    Failure Indicators: 未命中
    Evidence: .sisyphus/evidence/task-30-readme.log
  ```

  **Commit**: YES
  - Message: `phase6(docs): roadmap + README final pass to ✅ done`
  - Files: `docs/roadmap/phase-6-geometry-material.md`, `docs/roadmap/README.md`
  - Pre-commit: 2 个 QA scenarios PASS

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.
>
> **Do NOT auto-proceed after verification. Wait for user's explicit approval before marking work complete.**
> **Never mark F1-F4 as checked before getting user's okay.** Rejection or user feedback → fix → re-run → present again → wait for okay.

- [x] F1. **Plan Compliance Audit** — `oracle`

  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, run command, check headers). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found. Specifically check:
  - Edge symbols not in public headers: `grep -E 'edge|spu|rsx' include/souls_formats/sf_flver2.h` empty
  - BE refusal: `grep -n 'SF_ERR_UNSUPPORTED_VERSION' src/geom/flver2.c` 含 BE 检测分支
  - 顶点 dispatch 结构合规：`grep -c 'switch.*layout_type' src/geom/flver2_vertex.c` 期望低值（mirror if/else 而非 switch table）
  - extensions.md 3 条 entry 齐全：`grep -E '(decode_mesh|BE refusal|EdgeCompression)' docs/api-mapping/extensions.md` 3+ 命中
  - Check evidence files exist in `.sisyphus/evidence/`. Compare deliverables against plan.

  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`

  Run `tsc --noEmit equivalent` (即 `cmake --build build-mingw` with -Werror) + `bun test` 等价（即 `ctest --test-dir build-mingw -L geom`）。Review all changed files (Wave 1-4 创建的) for: `as any` 等价（C 端 `(void*)` 强转）/ silent failure (`return 0` 忽略错误) / `console.log` 等价（printf debug 残留）/ commented-out code / 未使用 include / 未使用 static helper。Check AI slop: 过度评论、过度抽象（顶点 element decoder 出现「BaseDecoder」「DecoderRegistry」等架构性命名）、generic names (data/result/item/temp 出现在 src/geom/)。

  Output: `Build [PASS/FAIL] | Tests [N pass/N fail] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`

  Start from clean state. Execute EVERY QA scenario from EVERY task (T1-T30) — follow exact steps, capture evidence. Test cross-task integration: c0000.flver → BND4 → FLVER2 → decode_mesh → 8 顶点 unit cube 同样跑通；allmaterial.matbinbnd.dcx → BND4 → 任一 .matbin → 8 ParamType 均可访问。Test edge cases:
  - Empty mesh (0 vertices) round-trip
  - 0 bones (static prop FLVER2)
  - 0 textures material
  - 0 samplers MATBIN
  - 0 params MATBIN
  - FLVER2 header version 不在 whitelist → 返回 `SF_ERR_UNSUPPORTED_VERSION`（非 crash）
  - Edge buffer 出现 → 返回 `SF_ERR_UNSUPPORTED_VERSION`（非 crash）
  - BE 字节序 → 返回 `SF_ERR_UNSUPPORTED_VERSION`（非 crash）

  Save evidence to `.sisyphus/evidence/final-qa/`.

  Output: `Scenarios [N/N pass] | Integration [N/N] | Edge Cases [N tested] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`

  For each task (T1-T30): read "What to do", read actual diff (`git log --since=phase-6-start`). Verify 1:1 — everything in spec was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance specifically: no Edge implementation snuck in, no FLVER0 code added, no static-table vertex registry, no GXItem structured parsing, no bbox/tangent recomputation. Detect cross-task contamination: Task N touching Task M's files. Flag unaccounted changes (random files touched outside src/geom/ + include/souls_formats/ + tests/geom/ + 4 docs + 4 plan files).

  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

- **1**: `phase6(state): switch status tables Phase 5→done, Phase 6→in-progress` - AGENTS.md, PLAN.md, docs/roadmap/README.md
- **2**: `phase6(scope): lock Edge geometry OUT-of-scope in PLAN.md and mapping doc` - PLAN.md, docs/api-mapping/format-flver2.md
- **3**: `phase6(docs): seed extensions.md entries for decode_mesh + BE refusal + Edge refusal` - docs/api-mapping/extensions.md
- **4**: `phase6(probe): empirical vertex layout types in c0000.flver` - tests/probes/probe_flver2_layouts.c, evidence
- **5**: `phase6(probe): empirical ParamType distribution in allmaterial.matbinbnd.dcx` - tests/probes/probe_matbin_paramtypes.c, evidence
- **6**: `phase6(docs): snapshot c0000.chrbnd.dcx + allmaterial.matbinbnd.dcx sha256` - docs/api-mapping/UPSTREAM.md
- **7**: `phase6(flver-common): sf_flver.h with Dummy/Node/LayoutMember + half-float helpers` - include/souls_formats/sf_flver.h
- **8**: `phase6(flver-common): flver_common.c implementations` - src/geom/flver_common.c, tests
- **9**: `phase6(flver2): sf_flver2.h opaque types + shared-index accessors` - include/souls_formats/sf_flver2.h
- **10**: `phase6(mtd): sf_mtd.h public API` - include/souls_formats/sf_mtd.h
- **11**: `phase6(matbin): sf_matbin.h with 8 ParamType variants` - include/souls_formats/sf_matbin.h
- **12**: `phase6(flver2): top-level reader/writer dispatch + GXList opaque transit` - src/geom/flver2.c
- **13**: `phase6(flver2): Material + Texture + TilingType` - src/geom/flver2_material.c
- **14**: `phase6(flver2): Mesh + BoundingBoxes` - src/geom/flver2_mesh.c
- **15**: `phase6(flver2): FaceSet + FSFlags + tri-strip decode` - src/geom/flver2_faceset.c
- **16**: `phase6(flver2): VertexBuffer + BufferLayout sentinel handling` - src/geom/flver2_vertex_buffer.c
- **17**: `phase6(flver2): mirror upstream vertex dispatch (foreach + if/else ladder)` - src/geom/flver2_vertex.c
- **18**: `phase6(flver2): SkeletonSet gated by version 0x2001A` - src/geom/flver2_skeleton.c
- **19**: `phase6(mtd): MTD reader/writer with Sekiro Extended texture` - src/geom/mtd.c
- **20**: `phase6(matbin): MATBIN reader/writer with 8 ParamType union` - src/geom/matbin.c
- **21**: `phase6(flver2): sf_flver2_decode_mesh extension implementation` - src/geom/flver2_decode.c
- **22**: `phase6(test): flver2 synthetic unit-cube round-trip` - tests/geom/test_flver2_synthetic.c
- **23**: `phase6(test): mtd synthetic round-trip` - tests/geom/test_mtd_synthetic.c
- **24**: `phase6(test): matbin synthetic round-trip with 8 ParamTypes` - tests/geom/test_matbin_synthetic.c
- **25**: `phase6(test): flver2 decode_mesh synthetic verification` - tests/geom/test_flver2_decode.c
- **26**: `phase6(test): flver2 e2e via c0000.flver` - tests/geom/test_flver2_e2e_er.c
- **27**: `phase6(test): matbin e2e via allmaterial.matbinbnd.dcx` - tests/geom/test_matbin_e2e_er.c
- **27b**: `phase6(test): mtd e2e via Sekiro (SKIP-allowed)` - tests/geom/test_mtd_e2e_sekiro.c
- **28**: `phase6(docs): refresh 4 mapping docs + finalize extensions.md` - docs/api-mapping/format-*.md, extensions.md
- **29**: `phase6(docs): PLAN.md §7 Phase 6 checkboxes complete` - .sisyphus/plans/PLAN.md
- **30**: `phase6(docs): roadmap + AGENTS.md final pass to ✅ done` - docs/roadmap/, AGENTS.md

---

## Success Criteria

### Verification Commands

```bash
# Build everything clean
rm -rf build-mingw
cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mingw  # Expected: 0 errors, 0 warnings (-Werror enabled)

# Unit + synthetic
ctest --test-dir build-mingw -L geom --output-on-failure  # Expected: all PASS

# ER e2e
ctest --test-dir build-mingw -L 'e2e_er' -R 'flver2|matbin' --output-on-failure  # Expected: all PASS

# Sekiro e2e (SKIP-allowed)
ctest --test-dir build-mingw -L 'e2e_sekiro' -R 'mtd' --output-on-failure  # Expected: PASS or SKIP

# Hygiene
grep -rn '"/home/' include/ src/ tests/  # Expected: 0 lines
grep -rE '(edge|spu|rsx)' include/souls_formats/sf_flver2.h  # Expected: only OUT-of-scope comments

# Mapping coverage
# DoD: ALL `未实现` rows must be cleared. Edge sub-tables (20 rows) move to `_skipped_`,
# in-scope rows (58) move to ✓ aligned. Net: `未实现` = 0 across all 4 docs.
grep -c '未实现' docs/api-mapping/format-flver-common.md  # Expected: 0
grep -c '未实现' docs/api-mapping/format-flver2.md       # Expected: 0
grep -c '_skipped_' docs/api-mapping/format-flver2.md    # Expected: 20 (Edge sub-tables)
grep -c '未实现' docs/api-mapping/format-mtd.md          # Expected: 0
grep -c '未实现' docs/api-mapping/format-matbin.md       # Expected: 0
```

### Final Checklist

- [x] All "Must Have" present
- [x] All "Must NOT Have" absent
- [x] All tests pass (Edge / BE / Unknown layout → graceful error, not crash)
- [x] 4 mapping docs aligned with implementation
- [ ] `docs/api-mapping/extensions.md` 含 3 条新增 entry
- [ ] PLAN.md / AGENTS.md / roadmap 三处状态表均 ✅ Phase 6
- [x] F1-F4 全部 APPROVE
- [x] User explicitly okay-ed Phase 6 完成
