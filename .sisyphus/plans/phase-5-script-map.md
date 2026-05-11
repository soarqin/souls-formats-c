# Phase 5 — 脚本与地图（ESD + MSB 家族）

> **状态**：⏳ 待执行 · **预估**：~3 周 · **依赖**：Phase 1-4 全部完成
>
> **策略**：先把 Phase 4 遗留的 6 处绝对路径 include bug 与文档状态表对齐到现实（Wave 0），再奠定 MSB 公共骨架与 ESD reader（Wave 1），随后并行展开三个 variant × 五个 sub-param 的实现（Wave 2），最后跑全部四款游戏的 e2e（Wave 3），4 个 reviewer 并行收尾（Wave Final）。
>
> **绑定**：本计划严格遵守 [AGENTS.md](../../AGENTS.md) §5.x「STRICT UPSTREAM REFERENCE / API MIRRORS UPSTREAM」。
> 上游锁定提交：`9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a`（见 `docs/api-mapping/UPSTREAM.md`）。

---

## TL;DR

> **核心目标**：实现 ESD（状态机）与 MSB 三 variant（MSBS / MSBE / MSBVI）的读 + 写双向支持，对齐上游 net9.0 分支语义；同时清理 Phase 4 残留 bug 并刷新项目状态表。
>
> **交付物**：
> - `sf_esd.{h,c}`：ESD 读 + 写，含 Condition bytecode 编解码。
> - `sf_msb.h` + `src/map/msb_common.c`：list-of-lists 公共骨架。
> - `sf_msbs.{h,c}` / `sf_msbe.{h,c}` / `sf_msbvi.{h,c}`：三 variant 读 + 写。
> - `tests/e2e/{sekiro,nightreign,ac6}_test_helper.{h,c}`：新增三个游戏的 e2e 数据访问 helper。
> - Phase 4 bug 修复 + project-wide include 路径守护测试。
> - 文档：5 份 api-mapping md 全量刷新；PLAN.md 状态表更新。
>
> **预估工作量**：~3 周（含 Metis 修正后的实际 MSB subtype 规模 ≈ 195 个 subtype × 2 方向 ≈ 390 method pair，~20-30K LOC）。
> **并行执行**：是 —— 6 个 wave（Wave 0-4 + Final），最宽 wave 17 个并行 task。总计 46 个实施 task + 4 个 Final reviewer = **50 个 task-level checkbox**。
> **关键路径**：T1 (preflight bug fix) → T2 (status table) → T7 (sf_msb.h) → T8 (msb_common skeleton) → T14a/b/c (variant root dispatcher) → T18/T23/T28 (PartsParam, LOC 最大) → T34/T37/T38 (MSBE e2e ER + NR) → F1-F4 → 用户验收。

---

## Context

### 原始请求

> 「继续编写下一阶段的计划」

### 当前项目状态

- Phase 0-3 ✅ 完成（PLAN.md 已锁定）。
- Phase 4 ✅ 完成（含 PARAM / PARAMDEF / PARAMTDF / FMG / **EMEVD** 全部读写）—— git log `4ab075e`、`1af885d`、`d8b649f` 记录了 F1-F4 APPROVE。但 PLAN.md §1 状态表仍把 Phase 4 显示为「⏳ pending」；本 plan Wave 0 修正之。
- Phase 5 原范围（EMEVD + ESD + MSB），用户已在 Phase 4 阶段把 EMEVD 提前合并；本 plan 范围 = **ESD + MSB 家族**。

### 访谈结论

| 决策点 | 用户确认值 |
|---|---|
| 下一阶段范围 | Phase 5 = ESD + MSB 家族；并附带修复 Phase 4 遗留的 6 处绝对路径 include bug |
| ESD 范围 | 读 + 写都做（含 Condition bytecode 评估器编解码） |
| MSB 三 variant 交付 | 公共骨架先行，三 variant 并行 wave |
| Phase 4 bug 处理 | 并入 Phase 5 Wave 0 第一个 task；加 grep 守护测试防回归 |
| 测试策略 | Tests-after + 每个 task 自带 Agent QA scenarios；Final wave 沿用 4 reviewer parallel pattern |
| e2e 必过 | 四款游戏（ER / Sekiro / Nightreign / AC6）e2e 全部 hard requirement，不允许 SKIP |

### 游戏数据布局（已实地确认 2026-05-11）

| 游戏 | 路径 | 主索引 | 备注 |
|---|---|---|---|
| ER | `/mnt/c/Games/ELDEN RING/Game/` | `Data0.bhd` + `Data0.bdt`（4 个同名 pair：Data0..Data3） | 已有 `er_test_helper.{c,h}`（Phase 3）只开 Data0 pair；ER 测试路径都集中在 Data0 |
| Sekiro | `/mnt/c/Games/Sekiro/` | 5 个同名 pair：`Data1.{bhd,bdt}` … `Data5.{bhd,bdt}` | **无 Data0**；新增 `sekiro_test_helper`，遍历 5 个 pair（同名 bhd↔bdt 配对），命中即返回 |
| Nightreign | `/mnt/c/Games/ELDEN RING NIGHTREIGN/Game/` | `data0.bhd` + `data0.bdt`（4 个同名 pair：data0..data3，**全小写**） | 与 ER 同结构但小写文件名；新增 `nightreign_test_helper` 仅开 data0 pair（NR 测试路径预期集中在 data0，与 ER 一致） |
| AC6 | `/mnt/c/Games/ARMORED CORE VI FIRES OF RUBICON/Game/` | 待用户从 Steam 下载就位 | start-work 前必须确认到位；新增 `ac6_test_helper` |

### Metis 复核要点（已纳入下方设计）

1. **MSB 子表规模远超原估**：三 variant 合计 ~195 个 subtype（含 Parts/Events/Regions/Models/Routes/Layers），双向约 390 个 method pair；Wave 2 必须按「variant × sub-param」二维拆分，单 agent 不可能完成一个完整 variant。
2. **Nightreign MSB 兼容性零证据**：上游 MSBE 源码 `grep -i nightreign` 零命中；社区暗示 MSBE 同字段可读 NR，但没有上游官方确认。Wave 0 必须做 probe。
3. **MSB 版本对齐**：ER 多次补丁后 MSB 版本号变动；plan 中需明确锁定的 ER game patch level。
4. **绝对路径 include bug 居然过了 Phase 4 F1-F4 APPROVE**：说明 reviewer 没跑跨机编译；本 plan 必须落地 project-wide grep guard CI step 防止再犯。
5. **MSB Shape / Region subtype 易过度实现**：ER 不用的某些 Shape 子类型上游有占位实现，C 端必须明确 OUT-of-scope。

---

## Work Objectives

### Core Objective

实现 ESD 与 MSB 三 variant 的双向读写，对齐上游语义；同时偿还 Phase 4 遗留技术债，使项目状态文档与代码实际状态完全一致。

### Concrete Deliverables

- 公共头：`include/souls_formats/sf_esd.h`、`sf_msb.h`、`sf_msbs.h`、`sf_msbe.h`、`sf_msbvi.h`。
- 源码：`src/script/esd.c`、`src/script/esd_bytecode.c`、`src/map/msb_common.c`、`src/map/msbs/*.c`（6 文件）、`src/map/msbe/*.c`（6 文件）、`src/map/msbvi/*.c`（7 文件，多 LayerParam）。
- 测试 helper：`tests/e2e/sekiro_test_helper.{c,h}`、`nightreign_test_helper.{c,h}`、`ac6_test_helper.{c,h}`。
- 单元测试：每个 sub-param 一个 reader + 一个 writer + 一个 round-trip 合成 fixture。
- e2e 测试：ESD via ER；MSBS via Sekiro；MSBE via ER；MSBE via Nightreign；MSBVI via AC6。
- 文档：5 份 api-mapping md（esd / msb-common / msbs / msbe / msbvi）全量刷新到对应行级覆盖率；PLAN.md §1 状态表更新（Phase 4 ✅、Phase 5 状态切换）；`docs/roadmap/phase-5-script-map.md` 同步实际任务清单。
- 工程债：Phase 4 遗留 6 处绝对路径 include 修复；新增 grep guard 测试。

### Definition of Done

- [ ] `ctest --test-dir build-mingw -L 'script|map' --output-on-failure` 全绿。
- [ ] `ctest --test-dir build-mingw -L 'e2e_er|e2e_sekiro|e2e_nightreign|e2e_ac6' --output-on-failure` 全绿（**不允许 SKIP**）。
- [ ] `grep -rn '"/home/' include/ src/ tests/` 零命中。
- [ ] `docs/api-mapping/format-{esd,msb-common,msbs,msbe,msbvi}.md` 所有行 status ≠ "未实现"。
- [ ] PLAN.md §1 表格内 Phase 4 状态 = ✅，Phase 5 状态 = ✅。
- [ ] F1-F4 全部 APPROVE，用户最终 okay。

### Must Have

- 三 variant 的 5 个 sub-param（MSBVI 6 个）逐一对齐上游 Sekiro / ER / AC6 已知 subtype 表；上游有的，C 端必须有等价 reader + writer。
- ESD Condition bytecode 结构化解码（known opcode 表 ≥ 10 个）+ OP_UNKNOWN 字节级保真 round-trip。允许未识别 opcode fallback 到 OP_UNKNOWN + raw bytes 原样保留；**不允许把所有 evaluator 都 dump 为不透明字节数组**——必须解出 known opcode 子集形成操作数树。
- 四款游戏 e2e 都 PASS（hard requirement）。
- Phase 4 6 处绝对路径 include bug 全部清除，且加 CI 级别守护。
- 所有公共符号必须 `SF_API` 装饰；所有公共 enum 后置 `_Static_assert`。

### Must NOT Have（Guardrails）

- ❌ 不实现 MSB1 / MSB2 / MSB3 / MSBAC4 / MSBB / MSBD / MSBDR / MSBFA / MSBN / MSBV / MSBVD（推到 v2 legacy）。
- ❌ 不实现 MSBVI / MSBE 中 ER 不使用的 Shape 子类型（如 navmesh / cone shape）—— 仅实现上游对应 game 的 EXISTING reader 中实际用到的 shape kind；用 `SF_ERR_UNSUPPORTED_VERSION` 显式拒绝其他。
- ❌ 不引入新的第三方依赖；ESD 与 MSB 都是纯解析问题，复用 `sf_io` / `sf_encoding` / `sf_math` 已有的 service 层。
- ❌ 不在公共头中暴露任何 `mxml`、`zstd`、`zlib-ng`、Win32 句柄；统一用 opaque pointer。
- ❌ 不复用 `er_test_helper` 的 Data0 假设来访问 Sekiro / Nightreign / AC6；每个 helper 必须有独立的 layout 抽象，**不允许跨游戏交叉污染数据访问代码**。
- ❌ 不在 `_destroy` 之外调用任何 `free`；所有分配器一致性沿用 Phase 1-4 约定。
- ❌ 不在 ESD bytecode 解释中引入完整的 VM 执行能力—— v1 只做 **结构化字节码 → 操作数树**的纯句法转换，不做语义求值。
- ❌ 不在 Phase 5 内做 MSBE Nightreign 专属字段扩展；如果 probe 显示 NR 与 ER 字节兼容，复用即可；如果不兼容，仅记录差异并把扩展推到 v1.1。

---

## Verification Strategy（MANDATORY）

> **ZERO HUMAN INTERVENTION** —— 所有验收均由 agent 通过命令执行，禁止「用户手动确认」。
> 证据保存到 `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`。

### Test Decision

- **Infrastructure exists**：YES（Unity ThrowTheSwitch；CMake 通过 `sf_add_test()` helper 已支持 label 路由）。
- **Automated tests**：tests-after（Phase 4 沿用同款策略）。
- **Framework**：Unity（基础）+ ctest（驱动）+ `sf_add_test()`（路由）。
- **Labels**：`script`（ESD）、`map`（MSB common + 三 variant）、`e2e_er` / `e2e_sekiro` / `e2e_nightreign` / `e2e_ac6`、`phase-4-debt`（针对 include bug 修复及守护）。

### QA Policy

每个 task 都必须包含 agent-executed QA scenarios（见每个 TODO 的「QA Scenarios」段）。证据落到 `.sisyphus/evidence/`。

- **CLI / 构建工具**：用 `bash` + `cmake` + `ctest`（在 WSL2 中执行 MinGW cross 产物）。
- **二进制证据**：`xxd` / `objdump` / `nm` 对比 + 保存 hex dump 到 evidence。
- **Round-trip**：`bash` 跑测试 binary，对比 input/output 字节级一致；保留 input.bin 与 output.bin 到 evidence。
- **静态检查**：`grep` 直接验证 include 路径污染、API 命名前缀、`_Static_assert` 存在性。

---

## Execution Strategy

### 并行执行 Wave 总览

```
Wave 0（preflight — 必须最先，全部完成才能进入 Wave 1）:
├── T1: 修 Phase 4 绝对路径 include bug + project-wide grep guard test  [quick]
├── T2: PLAN.md 状态表对齐现实（Phase 4 ✅，Phase 5 in progress）       [quick]
├── T3: docs/api-mapping/format-esd.md 路径修正 + UPSTREAM.md 对照     [quick]
├── T4: Nightreign MSB ↔ MSBE 兼容性 empirical probe                    [deep]
├── T5: 三游戏 patch version 锁定（写到 docs/api-mapping/UPSTREAM.md）  [quick]
└── T6: docs/roadmap/phase-5-script-map.md 任务清单同步本 plan          [writing]

Wave 1（foundation — Wave 0 全绿后启动）:
├── T7:   sf_msb.h 公共类型 + IMsbEntry / Point3 / Shape opaque        [quick]
├── T8:   src/map/msb_common.c list-of-lists 骨架 reader+writer         [unspecified-high]
├── T9:   sf_esd.h 公共类型 + State/Condition/CommandCall opaque        [quick]
├── T10:  src/script/esd.c 二进制 reader（含 long/short format 检测）   [deep]
├── T11:  src/script/esd_bytecode.c Condition bytecode 解码器            [artistry]
├── T12:  tests/e2e/sekiro_test_helper.{c,h} —— 多 bhd 遍历访问          [unspecified-high]
├── T13:  tests/e2e/nightreign_test_helper.{c,h} —— 小写 path 适配        [quick]
├── T14:  tests/e2e/ac6_test_helper.{c,h} —— gated 等 AC6 数据就位       [quick]
├── T14a: sf_msbs.h + msbs.c 根 dispatcher（容器壳，subtype 字段空）    [unspecified-high]
├── T14b: sf_msbe.h + msbe.c 根 dispatcher                              [unspecified-high]
└── T14c: sf_msbvi.h + msbvi.c 根 dispatcher（含 LayerParam dispatch）  [unspecified-high]

Wave 2（variant × sub-param 矩阵 — Wave 1 全绿后 16 路并行）:
[MSBS — Sekiro]
├── T15: src/map/msbs/model_param.c read+write             [deep]
├── T16: src/map/msbs/event_param.c read+write             [deep]
├── T17: src/map/msbs/point_param.c read+write             [deep]
├── T18: src/map/msbs/parts_param.c read+write             [deep]
└── T19: src/map/msbs/route_param.c read+write             [unspecified-high]
[MSBE — ER + Nightreign]
├── T20: src/map/msbe/model_param.c read+write             [deep]
├── T21: src/map/msbe/event_param.c read+write             [deep]
├── T22: src/map/msbe/point_param.c read+write             [deep]
├── T23: src/map/msbe/parts_param.c read+write             [deep]
└── T24: src/map/msbe/route_param.c read+write             [unspecified-high]
[MSBVI — AC6]
├── T25: src/map/msbvi/model_param.c read+write            [deep]
├── T26: src/map/msbvi/event_param.c read+write            [deep]
├── T27: src/map/msbvi/point_param.c read+write            [deep]
├── T28: src/map/msbvi/parts_param.c read+write            [deep]
├── T29: src/map/msbvi/route_param.c read+write            [unspecified-high]
└── T30: src/map/msbvi/layer_param.c read+write (unique)   [unspecified-high]
[ESD]
└── T31: src/script/esd_write.c writer（含 bytecode encode）[deep]

Wave 3（integration + e2e — Wave 2 全绿后 9 路并行）:
├── T32: 合成 round-trip — ESD                            [quick]
├── T33: 合成 round-trip — MSBS                           [quick]
├── T34: 合成 round-trip — MSBE                           [quick]
├── T35: 合成 round-trip — MSBVI                          [quick]
├── T36: ESD e2e via ER `/script/talk/m10_00_00_00.talkesdbnd.dcx`     [unspecified-high]
├── T37: MSBE e2e via ER `/map/mapstudio/m60_42_36_00.msb.dcx`         [unspecified-high]
├── T38: MSBE e2e via Nightreign `/map/mapstudio/<m??>.msb.dcx`        [unspecified-high]
├── T39: MSBS e2e via Sekiro `/map/mapstudio/<m??>.msb.dcx`            [unspecified-high]
└── T40: MSBVI e2e via AC6 `/map/mapstudio/<m??>.msb.dcx`              [unspecified-high]

Wave 4（docs + state table — Wave 3 全绿后 3 路并行）:
├── T41: PLAN.md §1 表格 final pass + Phase 5 章节 checkbox 全勾  [writing]
├── T42: 5 份 api-mapping md 全量刷新 + coverage check 报告         [writing]
└── T43: docs/roadmap/phase-5-script-map.md 与本 plan 收尾对齐      [writing]

Wave FINAL（4 reviewer 并行 — 全部 wave 完成后启动；必须 ALL APPROVE 才向用户索取 okay）:
├── F1: 计划合规审计              [oracle]
├── F2: 代码质量审查              [unspecified-high]
├── F3: Real Manual QA            [unspecified-high]
└── F4: Scope fidelity check      [deep]
→ 4 reviewer 全 APPROVE → 向用户展示 → 等待用户显式 okay 才标记 Phase 5 完成。
```

### Dependency Matrix（关键路径）

- **T1-T6**：- (preflight) → Wave 1 全部
- **T7**: T2 → T8, T14a, T14b, T14c, T15-T30
- **T8**: T7 → T14a, T14b, T14c, T15-T30
- **T9**: T3 → T10, T11
- **T10**: T9 → T11, T31, T36
- **T11**: T10 → T31, T36
- **T12**: T5 → T39
- **T13**: T5 → T38
- **T14**: T5, AC6-data-ready → T40
- **T14a**: T7, T8 → T15-T19, T33, T39
- **T14b**: T7, T8 → T20-T24, T34, T37, T38
- **T14c**: T7, T8 → T25-T30, T35, T40
- **T15-T19** (MSBS sub-params): T7, T8, **T14a** → T33, T39
- **T20-T24** (MSBE sub-params): T7, T8, **T14b** → T34, T37, T38
- **T25-T30** (MSBVI sub-params): T7, T8, **T14c** → T35, T40
- **T31** (ESD writer): T10, T11 → T32, T36
- **T32-T35**: 对应 variant 的 sub-param tasks + root dispatcher → Wave Final
- **T36-T40**: 对应 helper + variant 完成 → Wave Final
- **T41-T43**: Wave 3 完成 → Wave Final
- **F1-F4**: 全部 wave 完成 → user okay

### Agent Dispatch 总结

- **Wave 0**：6 tasks（5 × quick + 1 × deep）
- **Wave 1**：11 tasks（4 × quick + 5 × unspecified-high + 1 × deep + 1 × artistry）
- **Wave 2**：17 tasks（14 × deep + 3 × unspecified-high）—— **最大并行度，建议每 5 个一组分批分发避免 agent 池饱和**
- **Wave 3**：9 tasks（4 × quick + 5 × unspecified-high）
- **Wave 4**：3 tasks（writing）
- **Wave Final**：4 tasks（oracle + unspecified-high × 2 + deep）

---

## TODOs

### Wave 0 — Preflight & Cleanup（清理 Phase 4 债务、对齐文档与现实）

- [x] 1. **修复 Phase 4 遗留 6 处绝对路径 include + 加 grep guard**

  **What to do**：
  - 把 `include/souls_formats/sf_param.h:14-15` 的 `#include "/home/soar/src/souls-formats-c/include/souls_formats/sf_common.h"` 改为 `#include "souls_formats/sf_common.h"`（io.h 同处理）。
  - 把 `include/souls_formats/sf_emevd.h:25/28/35/38` 的 4 处 `#include "/home/..."` 改为相对包含路径（`"souls_formats/sf_common.h"` 与 `"souls_formats/sf_io.h"`，按现有 ifdef 结构保留）。
  - 新增 `tests/core/test_no_absolute_paths.c`：运行时调用 `system()` 跑 `grep -rln '"/home/' include/ src/ tests/`，命中即 FAIL。注册到 `tests/CMakeLists.txt` label `"hygiene"`。
  - 跑 `cmake --build build-mingw` 验证编译通过。

  **Must NOT do**：
  - ❌ 不改任何其他 include；只动这 6 处确认 bug。
  - ❌ 不通过 `-I` flag 强加 include 路径补丁；必须改源文件本身。
  - ❌ 不静默 grep guard test 的失败；CMake 必须把它列为 hygiene label 强制项。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 6 处文本替换 + 1 个测试加注册。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T2-T6 并行）
  - **Parallel Group**: Wave 0（与 T2, T3, T4, T5, T6 同组）
  - **Blocks**: T7 起所有 Wave 1 任务（修完才能安心 add 新 include）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - `include/souls_formats/sf_paramdef.h:14-15` —— 正确的相对包含示例（`"souls_formats/sf_common.h"`）

  **API/Type References**：无（纯路径修复）。

  **Test References**：
  - `tests/CMakeLists.txt:全文` —— 学习 `sf_add_test(name path label)` 注册模式。

  **External References**：
  - `grep --version` —— GNU grep 在 MinGW msys2 / WSL2 / MSVC vcpkg 上都可用。

  **WHY Each Reference Matters**：
  - 正确的相对包含写法在 `sf_paramdef.h` 里就有，照搬即可，避免漏改 ifdef 块。
  - hygiene label 是 Phase 5 新设的类目，T1 是第一个测试，建立模式给后续 task 复用。

  **Acceptance Criteria**：
  - [ ] `grep -rn '"/home/' include/ src/ tests/` 输出空。
  - [ ] `cmake --build build-mingw --target souls_formats_test_no_absolute_paths` 通过。
  - [ ] `ctest --test-dir build-mingw -L hygiene` PASS。
  - [ ] `sf_param.h` 与 `sf_emevd.h` 的 git diff 仅包含 include 行修改，无其他变动。

  **QA Scenarios**：

  ```
  Scenario: include 修复后跨机编译通过
    Tool: Bash (cmake + objdump)
    Preconditions: 干净 build dir，sf_param.h 与 sf_emevd.h 已修
    Steps:
      1. `rm -rf build-mingw && cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug`
      2. `cmake --build build-mingw --target souls_formats 2>&1 | tee .sisyphus/evidence/task-1-build.log`
      3. `grep -E 'error:|warning:' .sisyphus/evidence/task-1-build.log | grep -v 'third_party/'`
    Expected Result: 步骤 3 输出为空（无 error 无 warning）
    Failure Indicators: 任何 `error: .* /home/soar/` 报错
    Evidence: .sisyphus/evidence/task-1-build.log

  Scenario: grep guard 在故意污染时正确失败
    Tool: Bash
    Preconditions: T1 主修改已落地，guard test 已注册
    Steps:
      1. `echo '#include "/home/test"' >> tests/core/test_no_absolute_paths.c`（临时污染）
      2. `cmake --build build-mingw --target souls_formats_test_no_absolute_paths`
      3. `ctest --test-dir build-mingw -R no_absolute_paths --output-on-failure; echo "exit=$?"`
      4. `git checkout tests/core/test_no_absolute_paths.c`（恢复）
    Expected Result: 步骤 3 退出码 ≠ 0，stderr 包含 "absolute path detected"
    Failure Indicators: 退出码 = 0（guard 没触发）
    Evidence: .sisyphus/evidence/task-1-guard-fail.log
  ```

  **Commit**: YES
  - Message: `phase5(bug-cleanup): fix 6 absolute-path includes and add hygiene grep guard`
  - Files: `include/souls_formats/sf_param.h`, `include/souls_formats/sf_emevd.h`, `tests/core/test_no_absolute_paths.c`, `tests/CMakeLists.txt`
  - Pre-commit: `cmake --build build-mingw && ctest -L hygiene`

- [x] 2. **状态表对齐现实：AGENTS.md §2 + PLAN.md §7 + roadmap/README.md**

  **What to do**：
  - **Step A：实地跑 ctest 取 Phase 4 真实测试数**（不依赖任何已有 evidence 文件，自己生成）：
    1. `cmake --build build-mingw`（全量构建，确保 Phase 4 所有 target 链接成功）。
    2. **ctest label 选择必须用 regex 并集**（`-L`（大写）多次给 = 交集语义，会过滤掉只有单一 label 的测试；用 `-L 'param|script'` 或单 label 分别跑后合并 log）：
       ```bash
       ctest --test-dir build-mingw -L 'param|script' --output-on-failure 2>&1 | tee .sisyphus/evidence/task-2-phase4-ctest.log
       ```
    3. 从 log 末尾 `100% tests passed, X tests failed out of Y` 行抓出 `Y/Y PASS`；同时 `awk '/^test [0-9]+/{c++} END{print c}' .sisyphus/evidence/task-2-phase4-ctest.log` 得到 test binary 数 M。
    4. 把 Phase 4 实际测试数填入下方文档（格式：`Y/Y PASS across M test binaries`）。
    - **若 build / ctest 失败**（例如 Phase 4 遗留 include bug 未修）：T2 阻塞，等 T1（修 absolute-path include）通过；T1 + T2 顺序依赖。
  - **Step B：AGENTS.md §2「Current status」表格**（行 28-37 附近）：
    - 把 Phase 4 行的 `⏳ pending` 改为 `✅ done`，Tests 列填 Step A 抓的 `Y/Y PASS across M test binaries`。
    - 把 Phase 5 行的 `⏳ pending` 改为 `🚧 in progress`，Tests 列保留 `—`（T41 完成时再改）。
  - **Step C：PLAN.md §7「阶段化里程碑」**（PLAN.md L416 开始）：
    - Phase 4 子标题 `### Phase 4 — 参数与文本（预估 1.5 周）`（L579 附近）：在标题尾追加 `✅ 完成 (2026-05-11) — Y/Y PASS across M test binaries`，对齐 Phase 0-3 的标记格式。
    - Phase 4 章节内所有 `- [ ]` checkbox 改为 `- [x]`。
    - Phase 5 子标题 `### Phase 5 — 脚本与地图（预估 2.5 周）`（L608 附近）：把估时从「2.5 周」改为「3 周」（Metis 修正），但 checkbox 不动（T41 收尾）。
  - **Step D：PLAN.md §2.1 v1 必交付表格**（L36-47）：EMEVD 行（脚本类目）追加备注「（已在 Phase 4 完成，2026-05-11）」。
  - **Step E：docs/roadmap/README.md** Phase index 表格：Phase 4 行 state = `✅ done`、Tests 列填 `Y/Y PASS`；Phase 5 行 estimate 从 2.5 wk 改为 3 wk。

  **Must NOT do**：
  - ❌ 不动 PLAN.md §3-§6 的任何架构 / 技术决策章节。
  - ❌ 不动 Phase 6 / 7 子标题或 checkbox。
  - ❌ 不删除已 stale 的 checkbox 历史；保留 git 可追溯。
  - ❌ 不发明 Tests 数；必须用 Step A 实测命令生成。

  **Recommended Agent Profile**：
  - **Category**: `quick`
  - **Skills**: `tech-doc-style-chinese`（PLAN.md 是中文文档，须保持风格统一）。

  **Parallelization**：
  - **Can Run In Parallel**: NO（依赖 T1 让构建可编译）
  - **Parallel Group**: Wave 0（与 T3-T6 并行，但本身依赖 T1 完成）
  - **Blocks**: T7
  - **Blocked By**: T1

  **References**：

  **Pattern References**：
  - `AGENTS.md:18-29` —— 状态表当前形态（Phase/Title/State/Tests 四列；Phase 3 行 `32/32 PASS across 12 test binaries` 是测试数填法范本）。
  - `PLAN.md:L429` (Phase 0 子标题) —— `### Phase 0 — 工程脚手架（预估 0.5 周）✅ 完成 (2026-05-10)` 是完成标记格式范本。
  - `PLAN.md:L532` (Phase 3 子标题) —— `✅ **DONE 2026-05-10 — 32/32 PASS across 12 test binaries**` 是带测试数的范本。
  - git log `4ab075e`, `1af885d`, `d8b649f` —— Phase 4 完成证据链。

  **Test References**：
  - `tests/CMakeLists.txt` —— Phase 4 注册的 test binary 全集（搜 `sf_add_test` 关键字）。

  **External References**：无。

  **WHY Each Reference Matters**：
  - 状态表的 Tests 列必须给具体数字，不能空；Phase 3 范本明确告知格式「N/N PASS across M test binaries」。
  - AGENTS.md 才是 canonical 状态表（agents 启动时首读），不是 PLAN.md；不要写错位置。
  - 由于 `.sisyphus/evidence/phase4-*` 只有 pre-flight evidence 而没有 final test count，本 task 必须自己跑 ctest 重新取数据；这也是 Phase 5 一开始的 sanity check（顺便验证 Phase 4 在 T1 修完 include bug 后仍可跑通）。
  - 中文文档风格 skill 防止改动时不知不觉用了第二人称或宣传腔。

  **Acceptance Criteria**：
  - [ ] `.sisyphus/evidence/task-2-phase4-ctest.log` 存在且末尾包含 `100% tests passed, 0 tests failed out of Y`。
  - [ ] AGENTS.md §2 表格 Phase 4 行 = `✅ done` + 实测 `Y/Y PASS across M test binaries`（Y 来自 Step A）。
  - [ ] AGENTS.md §2 表格 Phase 5 行 = `🚧 in progress`。
  - [ ] PLAN.md §7 Phase 4 子标题已附 `✅ 完成 (2026-05-11) — Y/Y PASS across M test binaries`；章节内 0 个未勾 checkbox。
  - [ ] PLAN.md §7 Phase 5 子标题估时 = 3 周。
  - [ ] PLAN.md §2.1 EMEVD 行含「已在 Phase 4 完成」备注。
  - [ ] docs/roadmap/README.md Phase index 表 Phase 4 = `✅ done` + Y/Y PASS；Phase 5 estimate = 3 wk。

  **QA Scenarios**：

  ```
  Scenario: Phase 4 ctest 实测通过且抓到测试数（label 用 regex 并集）
    Tool: Bash
    Preconditions: T1 完成（include bug 已修，build 可编译）
    Steps:
      1. `cmake --build build-mingw 2>&1 | tail -5`
      2. `ctest --test-dir build-mingw -L 'param|script' --output-on-failure 2>&1 | tee .sisyphus/evidence/task-2-phase4-ctest.log`
      3. `tail -3 .sisyphus/evidence/task-2-phase4-ctest.log | grep -E 'tests passed, 0 tests failed'`
      4. `tail -3 .sisyphus/evidence/task-2-phase4-ctest.log | grep -oE 'out of [0-9]+'`
    Expected Result: 步骤 3 命中（0 failed）；步骤 4 输出 `out of Y` 其中 Y > 0（这个 Y 就是要填进文档的 Phase 4 测试总数）。**`-L 'param|script'` 用 regex 并集语义，包含任一 label；如改成 `-L param -L script` 会按交集筛选导致只有同时标了两个 label 的测试被选中（Phase 4 测试只标其一），数值会偏低**。
    Failure Indicators: 步骤 3 未命中（有失败的测试）；或步骤 4 输出空；或测试数明显 < Phase 4 实际写入数（表明 label 语义错）
    Evidence: .sisyphus/evidence/task-2-phase4-ctest.log

  Scenario: AGENTS.md 状态表全自洽
    Tool: Bash
    Preconditions: T2 改动落地
    Steps:
      1. `grep -E '^\| [0-9] \|' AGENTS.md | tee .sisyphus/evidence/task-2-agents-table.log`
      2. `grep -c '✅ done' .sisyphus/evidence/task-2-agents-table.log`
      3. `grep -c '🚧 in progress' .sisyphus/evidence/task-2-agents-table.log`
      4. `grep -c '⏳ pending' .sisyphus/evidence/task-2-agents-table.log`
    Expected Result: 步骤 2 = 5（Phase 0/1/2/3/4）；步骤 3 = 1（Phase 5）；步骤 4 = 2（Phase 6/7）
    Failure Indicators: 任一数量不一致
    Evidence: .sisyphus/evidence/task-2-agents-table.log

  Scenario: PLAN.md §7 Phase 4 章节 0 未勾 checkbox
    Tool: Bash
    Preconditions: T2 落地
    Steps:
      1. `awk '/^### Phase 4/,/^### Phase 5/' .sisyphus/plans/PLAN.md | grep -c '^- \[ \]' | tee .sisyphus/evidence/task-2-phase4-unchecked.log`
    Expected Result: 输出 0
    Failure Indicators: > 0
    Evidence: .sisyphus/evidence/task-2-phase4-unchecked.log

  Scenario: 中文风格未漂移
    Tool: Bash
    Preconditions: T2 改动落地
    Steps:
      1. `git diff AGENTS.md .sisyphus/plans/PLAN.md docs/roadmap/README.md | grep '^+' | grep -E '(你|您|让我们|快来|赶紧)' | tee .sisyphus/evidence/task-2-style.log`
    Expected Result: 输出为空
    Failure Indicators: 命中
    Evidence: .sisyphus/evidence/task-2-style.log
  ```

  **Commit**: YES
  - Message: `phase5(docs): align status tables (AGENTS.md + PLAN.md + roadmap) with reality`
  - Files: `AGENTS.md`, `.sisyphus/plans/PLAN.md`, `docs/roadmap/README.md`, `.sisyphus/evidence/task-2-*`
  - Pre-commit: `ctest -L param -L script` PASS（0 failed）

- [x] 3. **api-mapping/format-esd.md 路径修正 + UPSTREAM.md 对照**

  **What to do**：
  - 修改 `docs/api-mapping/format-esd.md`：把上游引用 `SoulsFormats/Formats/ESD/ESD.cs` 修正为 `SoulsFormats/Formats/ESD.cs`（实际位置）。
  - 在 mapping 表 Notes 列补「inner classes 在同一文件」一句。
  - 在 `docs/api-mapping/UPSTREAM.md` ESD 段落对照上游 `Formats/ESD.cs:875` 行数与 pinned commit。
  - 增补 ESD-class 内置 `State`（line 435）、`Condition`（line 584）、`CommandCall`（line 742）的 mapping 行（这些原 doc 里可能漏掉）。

  **Must NOT do**：
  - ❌ 不动 MSB 系列 mapping doc；那些归 T42 统一收尾。
  - ❌ 不改任何 status 列；本 task 仅对齐上游路径，实现状态留给 Wave 2-3 实际工作完成后再更新。

  **Recommended Agent Profile**：
  - **Category**: `quick`
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T9（sf_esd.h 起草时要照 mapping 取信号）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - `docs/api-mapping/format-emevd.md` —— Phase 4 已刷新的 mapping doc，作为 schema 范本。
  - `docs/api-mapping/UPSTREAM.md:全文` —— pinned commit 与文件清单。

  **API/Type References**：
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/ESD.cs:10` —— ESD class 起点。
  - `Formats/ESD.cs:435` —— State 类。
  - `Formats/ESD.cs:584` —— Condition 类。
  - `Formats/ESD.cs:742` —— CommandCall 类。

  **Test References**：无。

  **External References**：上游 GitHub blob 链接（pinned commit）。

  **WHY Each Reference Matters**：
  - 错误的路径 `Formats/ESD/ESD.cs` 是 mapping doc 复制 phase-5 roadmap 时引入的；UPSTREAM.md 是单一事实源，必须同步修正避免 T10 又被误导。

  **Acceptance Criteria**：
  - [ ] `grep -n 'Formats/ESD/ESD.cs' docs/api-mapping/format-esd.md` 输出 0 行。
  - [ ] `grep -n 'Formats/ESD.cs' docs/api-mapping/format-esd.md` 输出 ≥ 4 行（class 头 + 3 inner class）。
  - [ ] `docs/api-mapping/UPSTREAM.md` 中 ESD 行的行数与 line offset 与 ESD.cs 实测一致。

  **QA Scenarios**：

  ```
  Scenario: mapping doc 路径一致性
    Tool: Bash
    Preconditions: T3 改动落地
    Steps:
      1. `grep -rn 'Formats/ESD/' docs/api-mapping/ | tee .sisyphus/evidence/task-3-stale-paths.log; echo "exit=${PIPESTATUS[0]}"`
      2. `wc -l /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/ESD.cs | tee -a .sisyphus/evidence/task-3-stale-paths.log`
      3. `grep -c '^|' docs/api-mapping/format-esd.md | tee -a .sisyphus/evidence/task-3-stale-paths.log`
    Expected Result: 步骤 1 grep `exit=1`（无命中即 grep 退出码 1）；步骤 2 第一列 = 875；步骤 3 ≥ 30（mapping rows 充足）
    Failure Indicators: 步骤 1 `exit=0`（有命中，即 stale path 仍在）
    Evidence: .sisyphus/evidence/task-3-stale-paths.log
  ```

  **Commit**: YES
  - Message: `phase5(docs): fix format-esd.md upstream path + add inner class rows`
  - Files: `docs/api-mapping/format-esd.md`, `docs/api-mapping/UPSTREAM.md`
  - Pre-commit: 无

- [x] 4. **Nightreign MSB ↔ MSBE empirical 兼容性 probe**

  **What to do**：
  - 写一个一次性 probe 程序 `tests/probes/probe_nightreign_msb.c`：
    1. 用现有 BHD5 + DCX 工具从 `/mnt/c/Games/ELDEN RING NIGHTREIGN/Game/data0.bhd` 提取 `/map/mapstudio/m??.msb.dcx`（任选一个）。
    2. 读 MSB 头：magic、entry list 数量、每个 entry list 的 name 字符串。
    3. 与 ER `/mnt/c/Games/ELDEN RING/Game/Data0.bhd` 同名 entry 的头比对（同字段 byte-by-byte / 同 entry list 名）。
  - 输出 `.sisyphus/evidence/task-4-nightreign-probe.md`，结论分三档：
    - **A (compatible)**：头与 entry list 名完全一致 → MSBE C 实现可直接复用。
    - **B (mostly compatible)**：头一致但某些 subtype 数量不同 → MSBE 复用 + 少量扩展。
    - **C (diverged)**：头不一致 → 需要独立 `MSBNR` 模块；本 plan 范围调整。
  - 把结论同步到 `docs/api-mapping/format-msbe.md` Notes 列。

  **Must NOT do**：
  - ❌ 不修改 MSB 实现代码本身；本 task 是 read-only 探测，仅生成报告。
  - ❌ 不在 probe 里实现完整 MSB parser；只读 header / entry list name。
  - ❌ 不跨入 Sekiro / AC6 兼容性问题；那两个游戏有独立 variant。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— 探测涉及多个 service 层 helper 组合 + 字节级比较 + 写决策报告。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T20-T24（MSBE 系列：probe 结论 = A 才能按当前 plan 走，B 需要小补丁，C 需要 plan revision）
  - **Blocked By**: 无（ER 数据已就位，Nightreign 数据已就位）

  **References**：

  **Pattern References**：
  - `tests/e2e/er_test_helper.c` —— BHD5 + DCX 提取链路示例（Sekiro/Nightreign helper 会沿用同样的 pattern）。
  - `src/archive/bnd4.c:read_header` —— 多 entry list 头读取的逐字段模式。

  **API/Type References**：
  - `include/souls_formats/sf_bhd5.h` —— BHD5 reader API。
  - `include/souls_formats/sf_dcx.h` —— DCX decompress API。
  - `include/souls_formats/sf_io.h` —— binary reader API。

  **Test References**：
  - `tests/e2e/test_bhd5_e2e_er.c:全文` —— BHD5 + DCX 协同使用的 e2e 写法。

  **External References**：
  - 上游 `Formats/MSB/MSBE/MSBE.cs:Read()` —— header parse 的 reference 实现，本 probe 只读到 entry-list name 这一层即可。

  **WHY Each Reference Matters**：
  - probe 不需要完整 MSB parser；只要能像 BND4 一样读「N 个命名子段」就够了，所以参考 BND4 reader 而非未来的 MSB reader。

  **Acceptance Criteria**：
  - [ ] `.sisyphus/evidence/task-4-nightreign-probe.md` 存在，含 3 段：「ER MSB 头 hex dump」+「NR MSB 头 hex dump」+「diff 与决策档」。
  - [ ] 决策档明确为 A / B / C 之一；若 B/C 则附跟进 action item。
  - [ ] `docs/api-mapping/format-msbe.md` Notes 列含「Nightreign compatibility: <decision>, see task-4 probe」。

  **QA Scenarios**：

  ```
  Scenario: probe 实际跑成功
    Tool: Bash + ctest
    Preconditions: ER + NR 数据都在；Phase 3 er_test_helper 可用
    Steps:
      1. `cmake --build build-mingw --target probe_nightreign_msb`
      2. `./build-mingw/tests/probes/probe_nightreign_msb.exe > .sisyphus/evidence/task-4-nightreign-probe.txt 2>&1`
      3. `grep -E 'DECISION: [ABC]' .sisyphus/evidence/task-4-nightreign-probe.txt`
    Expected Result: 步骤 3 命中 1 行（明确决策档）
    Failure Indicators: 退出码 ≠ 0；或决策档缺失
    Evidence: .sisyphus/evidence/task-4-nightreign-probe.txt + .md

  Scenario: 决策档 = C 时升级阻塞 Wave 2
    Tool: Bash
    Preconditions: probe 已跑且 evidence 写好
    Steps:
      1. `grep 'DECISION: C' .sisyphus/evidence/task-4-nightreign-probe.txt && echo "BLOCKED" || echo "PROCEED"`
    Expected Result: 输出 PROCEED（A/B 档），否则 plan 必须 user 二次确认
    Failure Indicators: 输出 BLOCKED 但 Wave 2 仍启动
    Evidence: .sisyphus/evidence/task-4-decision.log
  ```

  **Commit**: YES
  - Message: `phase5(probe): empirical Nightreign MSB compatibility check`
  - Files: `tests/probes/probe_nightreign_msb.c`, `tests/probes/CMakeLists.txt`, `.sisyphus/evidence/task-4-*`, `docs/api-mapping/format-msbe.md`
  - Pre-commit: probe 跑通且 evidence 写齐

- [x] 5. **锁定 4 款游戏的 patch version + 写入 UPSTREAM.md**

  **What to do**：
  - 用各游戏自带方式抓 patch / build 号（ER: `regulation.bin` 头 + `eldenring.exe` version resource；Sekiro: 同；NR: 同；AC6 待补）。
  - 在 `docs/api-mapping/UPSTREAM.md` 增加 §「Game Data Snapshots」段，记录每款游戏：
    - 安装路径
    - 主索引文件（Data0/data0/Data1）与 sha256
    - `regulation.bin` 或对应版本文件的 build 号（若可读）
    - 抓取日期
  - 加注 risk：「若用户后续更新游戏到新 patch，MSB 字段可能变动，本 plan 测试 fixture 仅承诺锁定 snapshot」。

  **Must NOT do**：
  - ❌ 不读取 / 哈希游戏 EXE / DLL 主体；只读 small index files。
  - ❌ 不把任何游戏字节嵌入 repo；只记录 sha256 + 路径。
  - ❌ 不在 git 提交里包含 hash 之外的任何 game-derived 内容。

  **Recommended Agent Profile**：
  - **Category**: `quick`
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T12, T13, T14（test helper 起草时要引用 patch 锁定信息）
  - **Blocked By**: 无（数据已实地确认）

  **References**：

  **Pattern References**：
  - `docs/api-mapping/UPSTREAM.md:全文` —— 现有 schema 范本（pinned commit 信息）。

  **External References**：
  - sha256sum CLI（mingw / WSL2 都自带）。

  **WHY Each Reference Matters**：
  - UPSTREAM.md 是 plan 与代码之间的「外部状态契约」；把游戏数据 snapshot 加进去保证 e2e 测试可重现。

  **Acceptance Criteria**：
  - [ ] `docs/api-mapping/UPSTREAM.md` 新增 §「Game Data Snapshots」，覆盖 ER / Sekiro / NR 三款（AC6 标 TBD）。
  - [ ] 每款游戏至少 1 个 sha256 + 路径 + 抓取日期。
  - [ ] AC6 行包含明确 TODO「start-work 前由用户补齐」。

  **QA Scenarios**：

  ```
  Scenario: UPSTREAM.md 结构完整
    Tool: Bash
    Preconditions: T5 落地
    Steps:
      1. `grep -A 50 'Game Data Snapshots' docs/api-mapping/UPSTREAM.md | grep -c 'sha256'`
      2. `grep -A 50 'Game Data Snapshots' docs/api-mapping/UPSTREAM.md | grep -c 'AC6\|ARMORED CORE'`
    Expected Result: 步骤 1 ≥ 3；步骤 2 ≥ 1
    Failure Indicators: 缺 sha256 或缺 AC6 行
    Evidence: .sisyphus/evidence/task-5-snapshot.log

  Scenario: hash 实测可重现
    Tool: Bash
    Preconditions: ER 数据在
    Steps:
      1. `sha256sum '/mnt/c/Games/ELDEN RING/Game/Data0.bhd'` 与 UPSTREAM.md 中记录值对比
    Expected Result: 一致
    Failure Indicators: 不一致 → 用户已升级游戏，需要重抓 snapshot
    Evidence: .sisyphus/evidence/task-5-hash-verify.log
  ```

  **Commit**: YES
  - Message: `phase5(docs): pin ER/Sekiro/Nightreign/AC6 game data snapshots`
  - Files: `docs/api-mapping/UPSTREAM.md`
  - Pre-commit: 无

- [x] 6. **同步 `docs/roadmap/phase-5-script-map.md` 到本 plan**

  **What to do**：
  - 修改 `docs/roadmap/phase-5-script-map.md`：
    - 把范围段从「EMEVD + ESD + MSB」缩减为「ESD + MSB」并注脚指向 Phase 4 EMEVD 完成纪要。
    - 在「Deliverables」段下补 Wave 0-4 概述与 task 对照。
    - 在「File structure」段补 `tests/probes/`、`tests/e2e/{sekiro,nightreign,ac6}_test_helper.{c,h}`、`src/map/{msbs,msbe,msbvi}/*.c`（每个 5-6 文件）。
    - 在「QA Scenarios」段补 4 游戏 e2e 矩阵。
    - 「Exit criteria」段对齐本 plan 的 Definition of Done。
  - 风格保持原文档英文，与现有 phase-1 ~ phase-4 roadmap 一致。

  **Must NOT do**：
  - ❌ 不动 phase-6 / phase-7 roadmap；本 task 只摸 phase-5。
  - ❌ 不替换原文档的整体 schema；增量更新即可。

  **Recommended Agent Profile**：
  - **Category**: `writing` —— 纯文档同步。
  - **Skills**: 无（roadmap 是英文）。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: 无（文档同步与代码 task 互不阻塞）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - `docs/roadmap/phase-4-param-text.md` —— Phase 4 完成后的 roadmap 形态。
  - `docs/roadmap/phase-3-archive-containers.md` —— 多 e2e 矩阵的文档化模式。

  **External References**：无。

  **WHY Each Reference Matters**：
  - Phase 4 roadmap 是最新校准过的，结构最贴合本 plan 需求；直接复制 schema 改内容即可。

  **Acceptance Criteria**：
  - [ ] phase-5-script-map.md 中「Goal」段不再提 EMEVD。
  - [ ] 「File structure」段含 `src/map/msbs/`, `src/map/msbe/`, `src/map/msbvi/` 三目录。
  - [ ] 「QA Scenarios」段含 4 款游戏 e2e。
  - [ ] `docs/roadmap/README.md` Phase 5 行的 estimate 从 2.5 wk 改为 3 wk（反映 Metis 修正）。

  **QA Scenarios**：

  ```
  Scenario: roadmap 与 plan 一致
    Tool: Bash
    Preconditions: T6 落地
    Steps:
      1. `grep -c 'MSBVI\|MSBE\|MSBS\|ESD' docs/roadmap/phase-5-script-map.md`
      2. `grep 'EMEVD' docs/roadmap/phase-5-script-map.md | grep -v 'Phase 4'`
    Expected Result: 步骤 1 ≥ 10；步骤 2 输出空（EMEVD 仅以「Phase 4 完成」上下文出现）
    Failure Indicators: EMEVD 仍作为 Phase 5 deliverable 列出
    Evidence: .sisyphus/evidence/task-6-roadmap-check.log
  ```

  **Commit**: YES
  - Message: `phase5(docs): roadmap reflects post-Phase-4 scope (ESD + MSB family)`
  - Files: `docs/roadmap/phase-5-script-map.md`, `docs/roadmap/README.md`
  - Pre-commit: 无

### Wave 1 — Foundation（MSB 公共骨架 + ESD reader + 3 个 test helper）

- [x] 7. **`sf_msb.h` 公共类型 + IMsbEntry / Point3 / Shape opaque**

  **What to do**：
  - 起草 `include/souls_formats/sf_msb.h`：
    - `sf_msb_part_kind_t` 枚举（覆盖 ER/Sekiro/AC6 全部 part 类型并集，参考 `IMsb.cs` 与 3 个 variant 的 `PartsParam.cs` enum）。
    - 同款 `sf_msb_region_kind_t`、`sf_msb_event_kind_t`、`sf_msb_model_kind_t`。
    - 共享 POD：`sf_msb_point3_t`（复用 `sf_vec3_t`）、`sf_msb_transform_t`（Position + Rotation + Scale）。
    - opaque：`typedef struct sf_msb_part sf_msb_part_t;` 等。
    - 通用 accessor：`sf_msb_part_name`、`sf_msb_part_transform`、`sf_msb_part_model_index`、`sf_msb_part_layout_kind`。
    - 所有 enum 后置 `_Static_assert(MAX_VALUE == N, ...)`。
  - 所有公共符号 `SF_API` 装饰。

  **Must NOT do**：
  - ❌ 不暴露任何 variant-specific 字段；那些在 sf_msbs.h / sf_msbe.h / sf_msbvi.h 里。
  - ❌ 不在头文件里放任何具体 struct 定义；全 opaque。
  - ❌ 不引入新依赖。

  **Recommended Agent Profile**：
  - **Category**: `quick`
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T9, T10, T11, T12, T13, T14 并行）
  - **Parallel Group**: Wave 1
  - **Blocks**: T8, T15-T30
  - **Blocked By**: T1, T2

  **References**：

  **Pattern References**：
  - `include/souls_formats/sf_bnd4.h` —— opaque + accessor 公共模式。
  - `include/souls_formats/sf_paramdef.h:34-50` —— enum + `_Static_assert` 写法。

  **API/Type References**：
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MSB/IMsb.cs` —— 公共 interface。
  - `Formats/MSB/MSB.cs` —— 上游公共基类。
  - `Formats/MSB/MSBS/PartsParam.cs:enum PartType` / `MSBE/PartsParam.cs:enum PartType` / `MSBVI/PartsParam.cs:enum PartType` —— part 枚举三合一。

  **External References**：无。

  **WHY Each Reference Matters**：
  - 三 variant 的 part kind 有重叠也有差异；sf_msb.h 需要给一个超集枚举，让具体 variant header 通过 typed accessor 暴露各自子集。
  - opaque + accessor 是项目一致的 ABI 稳定策略。

  **Acceptance Criteria**：
  - [ ] `sf_msb.h` 编译通过（被一个 dummy `.c` include 测试）。
  - [ ] `grep '_Static_assert' include/souls_formats/sf_msb.h` ≥ 4 行（每个 enum 一个）。
  - [ ] `grep 'SF_API' include/souls_formats/sf_msb.h` ≥ 10 行（accessor 数量）。
  - [ ] 头文件中没有任何非 opaque struct 定义（只 typedef forward）。

  **QA Scenarios**：

  ```
  Scenario: 头文件可被 C 与 C++ 双向 include
    Tool: Bash
    Preconditions: T7 完成
    Steps:
      1. 临时写 `tests/core/_msb_header_smoke.c`: `#include "souls_formats/sf_msb.h"\nint main(){return 0;}`
      2. 临时写 `tests/core/_msb_header_smoke.cpp`: 同内容
      3. `cmake --build build-mingw --target ...smoke` 两个都跑
    Expected Result: 两个编译都通过；linker 不抱怨 unresolved symbol（accessor 还没实现但 header 应该编译）
    Failure Indicators: extern "C" 包装漏；或 typedef 冲突
    Evidence: .sisyphus/evidence/task-7-header-smoke.log
  ```

  **Commit**: YES
  - Message: `phase5(msb-common): introduce sf_msb.h shared types (opaque + accessors)`
  - Files: `include/souls_formats/sf_msb.h`, CMakeLists.txt 的 `SF_PUBLIC_HEADERS`
  - Pre-commit: header-only smoke build

- [x] 8. **`src/map/msb_common.c` list-of-lists 骨架 reader + writer**

  **What to do**：
  - 实现 `msb_common.c`：
    - `msb_common_read_header(reader, out_layout)` —— 读 MSB 头（version、entry count、list offsets）。
    - `msb_common_iter_lists(reader, cb, ctx)` —— 遍历 list-of-lists，调 callback 处理每个命名子段。
    - `msb_common_read_entry_list(reader, name_offset, list_offset, cb)` —— 读一个命名子段并迭代 entries。
    - 对应 writer side：`msb_common_reserve_*` / `msb_common_fill_*`。
  - 同时提供 internal header `src/map/msb_internal.h` 把这些 helper 暴露给 variant 模块（不暴露到公共 API）。

  **Must NOT do**：
  - ❌ 不实现任何具体的 entry kind（part / event / region / model / route / layer 都不动）；本 task 只搞「框架」。
  - ❌ 不假设 entry 数量上限。
  - ❌ 不暴露任何符号到 `include/souls_formats/`。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— 公共 IO 抽象 + 多 variant 复用入口设计。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（T7 阻塞）
  - **Parallel Group**: Wave 1（与 T9-T14 并行，但本身依赖 T7）
  - **Blocks**: T15-T30
  - **Blocked By**: T7

  **References**：

  **Pattern References**：
  - `src/archive/bnd4.c:read_header` 与 `bnd4.c:read_entries` —— 完全同款 list-of-lists 模式（BND4 也是 N 命名子段）。
  - `src/core/binary_reader.c` 与 `binary_writer.c` —— reserve/fill 模式。

  **API/Type References**：
  - `Formats/MSB/MSB.cs:Read()` —— 上游公共 read 实现。
  - `Formats/MSB/MSBS/MSBS.cs:Read()` —— 第一个 variant 调用 base.Read 后的 dispatch 范式。

  **Test References**：
  - `tests/archive/test_bnd4_synthetic.c` —— list-of-lists round-trip 测试模式。

  **External References**：无。

  **WHY Each Reference Matters**：
  - BND4 与 MSB 在「N 个命名子段」层面同构；复用 BND4 实现经验可以避免重复造轮。
  - reserve/fill pattern 是项目对 multi-pass 二进制写出的约定，必须沿用。

  **Acceptance Criteria**：
  - [ ] `msb_common.c` + `msb_internal.h` 提交且编译通过。
  - [ ] `nm build-mingw/libsouls_formats.a | grep msb_common_` 至少 6 个符号（read_header / iter_lists / read_entry_list / 对应 writer 三个）。
  - [ ] 所有 reserve_* 配对了 fill_*（用 grep 验证：`grep msb_common_reserve` 与 `grep msb_common_fill` 行数相等）。

  **QA Scenarios**：

  ```
  Scenario: 骨架级 round-trip（无具体 entry）
    Tool: Bash + Unity
    Preconditions: T8 完成；写一个最小测试构造 N=0 的 MSB 骨架
    Steps:
      1. 测试代码：用 `msb_common_*` 写出一个 0 entry 的 MSB → 字节流；再读回；assert N=0、entry list 名列表一致。
      2. `cmake --build build-mingw --target souls_formats_test_msb_common_skeleton`
      3. `ctest -R msb_common_skeleton --output-on-failure`
    Expected Result: PASS
    Failure Indicators: reserve/fill 不平衡报错；或 offset 计算偏 1
    Evidence: .sisyphus/evidence/task-8-skeleton-rt.log

  Scenario: writer finish 检查 reserve 全 fill
    Tool: Bash
    Preconditions: 写一个故意漏 fill 的负面测试
    Steps:
      1. 临时改测试代码：reserve 之后跳过一次 fill；调用 finish
      2. assert finish 返回 SF_ERR_INVALID_STATE 或对应错误码
    Expected Result: 故意失败被 writer 拦截
    Failure Indicators: writer 沉默通过
    Evidence: .sisyphus/evidence/task-8-reserve-leak.log
  ```

  **Commit**: YES
  - Message: `phase5(msb-common): list-of-lists skeleton reader+writer`
  - Files: `src/map/msb_common.c`, `src/map/msb_internal.h`, `tests/map/test_msb_common_skeleton.c`, CMakeLists.txt
  - Pre-commit: 骨架级 round-trip 测试通过

- [x] 9. **`sf_esd.h` 公共类型 + State/Condition/CommandCall opaque**

  **What to do**：
  - 起草 `include/souls_formats/sf_esd.h`：
    - opaque：`sf_esd_t`, `sf_esd_state_t`, `sf_esd_condition_t`, `sf_esd_command_call_t`。
    - 字段 accessor 对齐 `Formats/ESD.cs` public 字段：`LongFormat`, `DarkSoulsCount`, `Name`, `Unk70`/`74`/`78`/`7C`, `StateGroups`。
    - `Condition` 子结构 accessor：`Subconditions`, `PassCommands`, `TargetState`, `Evaluator`(bytecode bytes)。
    - `CommandCall` accessor：`CommandBank`, `CommandID`, `Arguments`（bytecode arg blob 列表）。
    - public read/write API（含 `_from_memory` 与 `_to_memory` 对）。
  - 所有公共符号 `SF_API`；所有 enum 后置 `_Static_assert`。

  **Must NOT do**：
  - ❌ 不暴露 bytecode 内部结构；用 opaque `sf_esd_bytecode_t` 或 `const uint8_t*` + size。
  - ❌ 不引入 dict / hashmap 抽象到公共 API；用 group-by-id 的迭代 accessor 即可。

  **Recommended Agent Profile**：
  - **Category**: `quick`
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: T10, T11, T31
  - **Blocked By**: T3

  **References**：

  **Pattern References**：
  - `include/souls_formats/sf_emevd.h` —— Phase 4 完成的 script header，accessor schema 完全可复用。
  - `include/souls_formats/sf_param.h:opaque + accessor` —— 大对象 + 子对象迭代的成熟范式。

  **API/Type References**：
  - `Formats/ESD.cs:10-150` —— ESD class fields。
  - `Formats/ESD.cs:435-580` —— State class。
  - `Formats/ESD.cs:584-740` —— Condition class。
  - `Formats/ESD.cs:742-870` —— CommandCall class。

  **External References**：
  - `docs/api-mapping/format-esd.md`（T3 修正后） —— 行级 mapping。

  **WHY Each Reference Matters**：
  - sf_emevd.h 是同类（script / bytecode）格式的最近邻范本，schema 直接照搬即可保持项目内一致性。

  **Acceptance Criteria**：
  - [ ] `sf_esd.h` header-only smoke build 通过（C + C++）。
  - [ ] `grep '_Static_assert' include/souls_formats/sf_esd.h` ≥ 2 行。
  - [ ] `grep 'SF_API' include/souls_formats/sf_esd.h` ≥ 15 行（accessor + read/write API）。
  - [ ] 没有 absolute-path include（T1 的 guard 通过）。

  **QA Scenarios**：

  ```
  Scenario: 头文件 smoke build
    Tool: Bash
    Preconditions: T9 完成
    Steps:
      1. 写 `tests/core/_esd_header_smoke.{c,cpp}` 各 include sf_esd.h
      2. `cmake --build build-mingw --target ...smoke`
    Expected Result: 两个编译通过
    Failure Indicators: typedef 冲突 / extern "C" 漏
    Evidence: .sisyphus/evidence/task-9-header-smoke.log
  ```

  **Commit**: YES
  - Message: `phase5(esd): introduce sf_esd.h public API (opaque + accessors)`
  - Files: `include/souls_formats/sf_esd.h`, CMakeLists.txt
  - Pre-commit: header smoke build

- [x] 10. **`src/script/esd.c` 二进制 reader（含 long/short format 检测）**

  **What to do**：
  - 实现 ESD reader：
    - 检测 `LongFormat`（32 vs 64 位 offset 表）。
    - 读 `DarkSoulsCount`, `Name`（可选）, `Unk70/74/78/7C`。
    - 读 `StateGroups`：外层 dict (groupId → inner dict)；内层 dict (stateId → State)。
    - 每个 State：读 `Conditions` 数组（递归读 Subconditions）、`EntryCommands`、`ExitCommands`、`WhileCommands`。
    - Condition 的 `Evaluator` 字段：作为不透明 `bytecode_bytes` 缓存下来（T11 在另一 task 中解码）。
  - 所有内部分配走 caller 传入的 allocator；caller 通过 `sf_esd_destroy` 一次性释放。

  **Must NOT do**：
  - ❌ 不实现 writer（留给 T31）。
  - ❌ 不解码 Condition bytecode（留给 T11）。
  - ❌ 不读 EDD（companion 格式，OUT of scope）。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— 复杂嵌套结构 + double dict + 4 种 command list + format flag dispatch。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（依赖 T9）
  - **Parallel Group**: Wave 1
  - **Blocks**: T11, T31, T32, T36
  - **Blocked By**: T9

  **References**：

  **Pattern References**：
  - `src/script/emevd.c` —— Phase 4 完成的同类格式 reader，dict/list 读取套路一致。
  - `src/param/param.c:read_rows` —— 大量重复结构的逐项 read 模式。

  **API/Type References**：
  - `Formats/ESD.cs:Read()` —— 上游入口。
  - `Formats/ESD.cs:172-433`（ESD.Read 主体）—— double-dict 读取流程。

  **Test References**：
  - `tests/script/test_emevd_read.c` —— Phase 4 EMEVD reader 测试，本 task 仿照其结构写 ESD reader 测试。

  **External References**：无。

  **WHY Each Reference Matters**：
  - EMEVD reader 在 Phase 4 经过 F1-F4 APPROVE 验证，是项目内最权威的 script-format reader 范本。
  - param.c 处理了「读 N 个不定长子结构」这一项目特色场景的所有边界 case。

  **Acceptance Criteria**：
  - [ ] `sf_esd_read_from_memory` 实现。
  - [ ] `sf_esd_state_group_count` / `sf_esd_state_group_get` / `sf_esd_state` accessor 全实现。
  - [ ] `tests/script/test_esd_read.c` 用 fixture 验证：2 group × 3 state × 2 condition 的合成 ESD 读取后字段值一致。
  - [ ] `tests/script/test_esd_read.c` 同时跑 long format 与 short format 各一次。
  - [ ] 内存 leak free：`ctest -L 'script'` 与 `--build-type Asan` 都不报泄漏。

  **QA Scenarios**：

  ```
  Scenario: ESD reader 跑合成 fixture
    Tool: Bash + Unity
    Preconditions: T10 完成；fixture 通过 Python helper 或 C helper 构造
    Steps:
      1. fixture 构造（在测试代码内）：2 group / 3 state / 各 2 condition
      2. `cmake --build build-mingw --target souls_formats_test_esd_read`
      3. `ctest -R esd_read --output-on-failure`
    Expected Result: PASS；assert StateGroupCount == 2, State[0][0].Conditions.size == 2
    Failure Indicators: count 不一致；任一字段值偏差
    Evidence: .sisyphus/evidence/task-10-esd-read.log

  Scenario: 损坏 header 时优雅拒绝
    Tool: Bash + Unity
    Preconditions: T10 完成
    Steps:
      1. 构造一个 magic 错误 / version 错误的 fixture
      2. 调用 sf_esd_read_from_memory
      3. assert 返回 SF_ERR_INVALID_MAGIC 或对应错误码
    Expected Result: reader 不 crash，错误码明确
    Failure Indicators: segfault；或返回 SF_OK
    Evidence: .sisyphus/evidence/task-10-esd-corrupt.log
  ```

  **Commit**: YES
  - Message: `phase5(esd): binary reader (long/short format, 4 command lists)`
  - Files: `src/script/esd.c`, `src/script/esd_internal.h`, `tests/script/test_esd_read.c`, CMakeLists.txt
  - Pre-commit: `ctest -R esd_read` PASS

- [x] 11. **`src/script/esd_bytecode.c` Condition bytecode 解码器**

  **What to do**：
  - 实现 ESD Condition bytecode 解码：把 `Condition.Evaluator` 的字节流转为操作数树（结构化）。
  - 提供 `sf_esd_condition_bytecode_decode(const uint8_t *bytes, size_t size, sf_esd_bytecode_tree_t **out, alloc)` 与对应 destroy。
  - bytecode 命令集参考上游：常量 push、寄存器 read、二元运算、调用 helper、跳转。具体 opcode 表在 `Formats/ESD.cs` 中没有；需要从社区参考（Paramdex / DSAS reference）补全。**若上游 opcode 表不完整，明确 mark unknown opcode 为 `SF_ESD_OP_UNKNOWN` + 保留原字节，**而非 fail**。
  - 节点树暴露 accessor：opcode、operand list、children。

  **Must NOT do**：
  - ❌ 不做语义求值（VM 执行）；只做句法树构造。
  - ❌ 不为每个 opcode 写 documentation；只在头文件枚举里放简短一行注释。
  - ❌ unknown opcode 不要 abort；fallback 到 OP_UNKNOWN + raw bytes 保留。

  **Recommended Agent Profile**：
  - **Category**: `artistry` —— 上游未文档化的 bytecode 表需要从 community + 经验推导，是不规范的、创造性的任务。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（依赖 T10 拿到 evaluator bytes）
  - **Parallel Group**: Wave 1
  - **Blocks**: T31, T32, T36
  - **Blocked By**: T10

  **References**：

  **Pattern References**：
  - `src/script/emevd_instruction.c` —— Phase 4 同类 instruction bytecode 读取实现。

  **API/Type References**：
  - `Formats/ESD.cs:Condition` —— 评估器字段。
  - 上游 community 参考：Paramdex `paramdex/Common/ESD/` 与 DSAS（DarkSoulsAnimStudio）的 ESD 解码器（若可访问）。
  - 上游 net9.0 分支 `Formats/ESD.cs` 没有 opcode 表，需在 Wave 1 内 timebox 4 小时调研，找不到就 fallback 到 OP_UNKNOWN 策略。

  **Test References**：
  - `tests/script/test_emevd_read.c:instruction 验证段` —— bytecode 结构化测试方法。

  **External References**：
  - `https://github.com/JKAnderson/DSAnimStudio`（若可访问）—— ESD 评估器参考实现。

  **WHY Each Reference Matters**：
  - EMEVD instruction 解码器是项目内最近邻；同样从「字节流 → 结构化树」的思路。
  - ESD 评估器没有官方文档，community decoder 是唯一来源；timebox 调研防止陷入无底洞。

  **Acceptance Criteria**：
  - [ ] `sf_esd_condition_bytecode_decode` 实现。
  - [ ] opcode 表 ≥ 10 个 known opcode（push const、register read、+/-/*/、call helper、jump）。
  - [ ] unknown opcode 走 fallback：`opcode = SF_ESD_OP_UNKNOWN, raw_bytes_len > 0`。
  - [ ] 测试覆盖：构造合成 evaluator（含至少 2 known + 1 unknown opcode），解码后树形结构正确。

  **QA Scenarios**：

  ```
  Scenario: 合成 evaluator 解码正确
    Tool: Bash + Unity
    Preconditions: T11 完成
    Steps:
      1. 构造合成 evaluator: `push_const(0x42) push_const(0x10) add`
      2. 调用 decode
      3. assert tree: root.opcode = ADD, root.operands.size == 2, operands[0].opcode = PUSH_CONST, .value == 0x42
    Expected Result: 树形匹配
    Failure Indicators: opcode 错；或 operand 数错
    Evidence: .sisyphus/evidence/task-11-evaluator-decode.log

  Scenario: unknown opcode 走 fallback 而非 fail
    Tool: Bash + Unity
    Preconditions: T11 完成
    Steps:
      1. 构造 evaluator 中插入 0xFF（已知 unknown）
      2. decode
      3. assert: 返回 SF_OK；某节点 .opcode == SF_ESD_OP_UNKNOWN；.raw_bytes_len > 0
    Expected Result: 不 abort
    Failure Indicators: 返回非 SF_OK；或 segfault
    Evidence: .sisyphus/evidence/task-11-unknown-opcode.log
  ```

  **Commit**: YES
  - Message: `phase5(esd): Condition bytecode decoder with OP_UNKNOWN fallback`
  - Files: `src/script/esd_bytecode.c`, `src/script/esd_internal.h` (扩展), `tests/script/test_esd_bytecode.c`
  - Pre-commit: `ctest -R esd_bytecode` PASS

- [x] 12. **`tests/e2e/sekiro_test_helper.{c,h}` —— 多 bhd 遍历访问**

  **What to do**：
  - 实现 `sekiro_test_helper`：
    - `sekiro_helper_init(void)`：检测 `/mnt/c/Games/Sekiro/Data{1..5}.bhd` 全部存在；逐个 open（每个 BHD5 + BDT pair 单独管理）。
    - `sekiro_extract_from_anybhd(const char *path, void **out, size_t *out_size)`：对每个 BHD5 算 path hash，命中即停；返回堆缓冲（caller 释放）。
    - `sekiro_helper_shutdown(void)`：释放所有 BHD5 句柄。
    - `sekiro_helper_is_available(void)`：所有 5 个 bhd 都在才返回 true。
  - 不允许 SKIP：用户已确认 Sekiro 数据就位，这个 helper 在 start-work 时必须可用；若 init 失败应该 PASS=NO 让 e2e fail 而非 skip。

  **Must NOT do**：
  - ❌ 不复制 ER helper 的 Data0 假设；Sekiro 没有 Data0。
  - ❌ 不在 helper 里实现 DCX 提取；那是 `er_extract_from_data0` 已经提供的服务，调相同的 `sf_dcx_*` API 即可。
  - ❌ 不写跨进程缓存；helper 是 per-test-process 单例。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— BHD5/DCX 集成 + 多 archive 路由设计。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: T39
  - **Blocked By**: T5

  **References**：

  **Pattern References**：
  - `tests/e2e/er_test_helper.c:全文` —— 同类 helper 的完整实现，结构、API、生命周期都可复用。
  - `tests/e2e/er_test_helper.h` —— public API 命名风格。

  **API/Type References**：
  - `include/souls_formats/sf_bhd5.h` —— BHD5 reader API。
  - `include/souls_formats/sf_dcx.h` —— DCX decompressor。
  - `include/souls_formats/sf_hash.h:sf_filename_hash` —— path hash 算法（FromPath hash）。

  **Test References**：
  - `tests/e2e/test_er_helper_smoke.c` —— helper 自身的 smoke test 模板。

  **External References**：无。

  **WHY Each Reference Matters**：
  - er_test_helper 是项目内唯一 reference；sekiro_test_helper 应该是同款 API shape 加上「多 BHD 遍历」扩展，不要造新轮子。

  **Acceptance Criteria**：
  - [ ] `sekiro_helper_init` / `sekiro_extract_from_anybhd` / `sekiro_helper_shutdown` / `sekiro_helper_is_available` 全实现。
  - [ ] `tests/e2e/test_sekiro_helper_smoke.c`：init → 取一个已知存在的 entry（用 Sekiro 已知 path，如 `/chr/c0000.chrbnd.dcx`）→ size > 0 → shutdown → PASS。
  - [ ] helper 失败时返回明确错误码（SF_ERR_NOT_FOUND 等），不 abort。

  **QA Scenarios**：

  ```
  Scenario: Sekiro helper 提取已知 entry
    Tool: Bash + Unity
    Preconditions: Sekiro 数据在 /mnt/c/Games/Sekiro/
    Steps:
      1. `cmake --build build-mingw --target souls_formats_test_sekiro_helper_smoke`
      2. `ctest -R sekiro_helper_smoke --output-on-failure`
    Expected Result: PASS；提取的 bytes 前 4 字节 = DCX magic 或 BND magic
    Failure Indicators: helper init 失败；或 entry 未找到
    Evidence: .sisyphus/evidence/task-12-sekiro-smoke.log

  Scenario: 不存在的 path 优雅返回
    Tool: Bash + Unity
    Preconditions: helper 已 init
    Steps:
      1. 调用 sekiro_extract_from_anybhd("/不存在/路径") → assert 返回 SF_ERR_NOT_FOUND
    Expected Result: 错误码明确，不 crash
    Failure Indicators: segfault；或返回 SF_OK
    Evidence: .sisyphus/evidence/task-12-sekiro-not-found.log
  ```

  **Commit**: YES
  - Message: `phase5(helpers): sekiro_test_helper with multi-bhd traversal`
  - Files: `tests/e2e/sekiro_test_helper.{c,h}`, `tests/e2e/test_sekiro_helper_smoke.c`, `tests/CMakeLists.txt`
  - Pre-commit: smoke test PASS

- [x] 13. **`tests/e2e/nightreign_test_helper.{c,h}` —— 小写 path 适配**

  **What to do**：
  - 复用 er_test_helper 思路，只改两点：
    - 路径前缀：`/mnt/c/Games/ELDEN RING NIGHTREIGN/Game/`
    - 索引文件：`data0.bhd` + `data0.bdt`（**全小写、同名配对**，与 er_test_helper 的 `Data0.bhd` + `Data0.bdt` 同款 pair 模式，仅大小写不同）
  - API：`nightreign_helper_init`、`nightreign_extract_from_data0`、`nightreign_helper_shutdown`、`nightreign_helper_is_available`。
  - 考虑：直接重构 er_test_helper 成参数化版本 + 两个 thin wrapper（推荐做法），还是各自独立实现？**本 task 选独立实现路线**——避免给 ER e2e 引入风险；后续 v1.1 可重构。

  **Must NOT do**：
  - ❌ 不动 er_test_helper（保持 ER e2e 稳定）。
  - ❌ 不假设 NR 与 ER 路径完全相同；NR 用 `data0.bdt`（小写），大小写敏感的 fs 上必须精确匹配。
  - ❌ 不假设 BHD 与 BDT 跨 shard 配对：必须同名（`dataN.bhd` ↔ `dataN.bdt`，不是 `data0.bhd` ↔ `data1.bdt`）。
  - ❌ 不 SKIP；NR 数据已确认就位。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 几乎是 er_test_helper 的镜像。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: T38
  - **Blocked By**: T5

  **References**：

  **Pattern References**：
  - `tests/e2e/er_test_helper.c:全文` —— 直接镜像。

  **API/Type References**：同 T12。

  **External References**：无。

  **WHY Each Reference Matters**：
  - NR helper 90% 是 ER helper 的 copy；只换路径常量。把它写成独立文件而非参数化是为了让 Phase 5 不引入 ER e2e 的回归风险。

  **Acceptance Criteria**：
  - [ ] `nightreign_test_helper.{c,h}` 提交。
  - [ ] `tests/e2e/test_nightreign_helper_smoke.c`：init → 取已知 entry（NR `/map/mapstudio/m??.msb.dcx` 任一）→ size > 0 → PASS。

  **QA Scenarios**：

  ```
  Scenario: NR helper 提取 MSB
    Tool: Bash + Unity
    Preconditions: NR 数据在 /mnt/c/Games/ELDEN RING NIGHTREIGN/Game/
    Steps:
      1. 先用一次性 list 工具列出 NR 中 mapstudio 第一个 msb.dcx
      2. helper 提取 → DCX magic OK
    Expected Result: PASS
    Failure Indicators: 路径未找到（说明 NR 索引解析有问题）
    Evidence: .sisyphus/evidence/task-13-nr-smoke.log
  ```

  **Commit**: YES
  - Message: `phase5(helpers): nightreign_test_helper (lowercase data0 paths)`
  - Files: `tests/e2e/nightreign_test_helper.{c,h}`, `tests/e2e/test_nightreign_helper_smoke.c`, `tests/CMakeLists.txt`
  - Pre-commit: smoke test PASS

- [x] 14. **`tests/e2e/ac6_test_helper.{c,h}` —— gated 等 AC6 数据就位**

  **What to do**：
  - 起草 AC6 helper：API 同 ER/Sekiro/NR pattern。
  - **gating**：start-work 时检测 `/mnt/c/Games/ARMORED CORE VI FIRES OF RUBICON/Game/` 内 `Data0.bhd` 或对应索引文件是否存在；缺失就**直接 fail Wave 1**（不允许进 Wave 2）；用户必须先把 AC6 装好。
  - helper 实现完成后跑 smoke：提取 AC6 `/map/mapstudio/m??.msb.dcx` 任一。

  **Must NOT do**：
  - ❌ 不 SKIP；用户明确要求 e2e 必过，gate 要硬。
  - ❌ 不臆测 AC6 layout；如果 Data0/Data1 命名跟其他游戏不同，按实际命名 hard-code。
  - ❌ 不绕过 AC6 缺数据情况；fail loud。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 同 T13 但额外加 gate。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: T40
  - **Blocked By**: T5；**用户在 start-work 前必须就位 AC6 数据**

  **References**：

  **Pattern References**：
  - `tests/e2e/er_test_helper.c:全文` 与 T13 nightreign helper。

  **API/Type References**：同 T12。

  **External References**：无。

  **WHY Each Reference Matters**：
  - AC6 helper 现在是「按假设写」的状态；用户上线 AC6 后第一时间 verify 实际 layout 与本 task 的假设是否一致。

  **Acceptance Criteria**：
  - [ ] `ac6_test_helper.{c,h}` 提交。
  - [ ] gate logic：缺数据时编译期 / 测试 init 期明确 fail 并打印 actionable 错误信息。
  - [ ] smoke test 通过 AC6 第一个 mapstudio MSB 验证。

  **QA Scenarios**：

  ```
  Scenario: AC6 数据已就位时 helper 正常工作
    Tool: Bash + Unity
    Preconditions: 用户已把 AC6 安装到 `/mnt/c/Games/ARMORED CORE VI FIRES OF RUBICON/Game/`
    Steps:
      1. `ls '/mnt/c/Games/ARMORED CORE VI FIRES OF RUBICON/Game/' | grep -iE 'data0\.bhd|Data0\.bhd'`
      2. `ctest -R ac6_helper_smoke --output-on-failure`
    Expected Result: 步骤 1 命中；步骤 2 PASS
    Failure Indicators: 步骤 1 空 → 用户未就位，gate 必须触发，Wave 1 失败
    Evidence: .sisyphus/evidence/task-14-ac6-smoke.log

  Scenario: AC6 数据缺失时 gate 正确触发
    Tool: Bash
    Preconditions: 临时 rename AC6 目录验证 gate
    Steps:
      1. `mv '/mnt/c/Games/ARMORED CORE VI FIRES OF RUBICON' '/mnt/c/Games/AC6.bak'`
      2. `ctest -R ac6_helper_smoke --output-on-failure; echo "exit=$?"`
      3. 恢复目录
    Expected Result: 步骤 2 退出码 ≠ 0；错误信息含「AC6 game data not found at expected path」
    Failure Indicators: SKIP；或沉默 PASS
    Evidence: .sisyphus/evidence/task-14-ac6-gate.log
  ```

  **Commit**: YES
  - Message: `phase5(helpers): ac6_test_helper with hard-fail gate on missing data`
  - Files: `tests/e2e/ac6_test_helper.{c,h}`, `tests/e2e/test_ac6_helper_smoke.c`, `tests/CMakeLists.txt`
  - Pre-commit: smoke test PASS（在 AC6 数据就位的前提下）

- [x] 14a. **`sf_msbs.h` + `src/map/msbs/msbs.c` 根 dispatcher（8 segment 容器壳）**

  **What to do**：
  - **上游段顺序（参考 `MSBS.cs:91-107`，共 8 个 list segment，必须严格按此顺序读写）**：
    1. `Models` (typed ModelParam，T15 实现) → magic id 见上游
    2. `Events` (typed EventParam，T16)
    3. `Regions` (typed PointParam，T17)
    4. `Routes` (typed RouteParam，T19)
    5. `Layers` (**`EmptyParam(0x23, "LAYER_PARAM_ST")` ——MSBS 没有 typed Layer**，空段头有 magic + 0 entries)
    6. `Parts` (typed PartsParam，T18)
    7. `PartsPoses` (**`EmptyParam(0, "MAPSTUDIO_PARTS_POSE_ST")` ——空段头**)
    8. `BoneNames` (**`EmptyParam(0, "MAPSTUDIO_BONE_NAME_STRING")` ——空段头**)
  - 起草 `include/souls_formats/sf_msbs.h`：
    - opaque `sf_msbs_t`、`sf_msbs_model_t`、`sf_msbs_event_t`、`sf_msbs_region_t`、`sf_msbs_part_t`、`sf_msbs_route_t`（具体字段 accessor 由 T15-T19 在各 sub-param task 中扩展）。
    - 根 API：`sf_msbs_read_from_memory(out, bytes, size, alloc)`、`sf_msbs_write_to_memory(in, out_bytes, out_size, alloc)`、`sf_msbs_destroy(m)`、`sf_msbs_{model,event,region,part,route}_count(m)` 与 `_at(m, idx)` accessor 系列。
    - **不暴露 Layer / PartsPoses / BoneNames 给公共 API**——这 3 段是占位 EmptyParam，仅在 root dispatcher 内部读写空头保持字节兼容。
    - 所有公共符号 `SF_API`；所有 enum 后置 `_Static_assert`。
  - 起草 `src/map/msbs/msbs.c`：
    - `sf_msbs_read_from_memory`：用 `msb_common_iter_lists`（T8）按上述 **8 段顺序** dispatch；typed 段调 sub-param read（T15-T19），EmptyParam 段调 `msb_common_read_empty_param`（T8 helper，验证 entry count = 0、magic id 与 name 一致）。
    - `sf_msbs_write_to_memory`：对应 dispatch；EmptyParam 段写空头 + magic id + name + 0 entries。
    - `sf_msbs_destroy`：递归释放各 typed sub-param 列表。
    - 同步起草 `src/map/msbs/msbs_internal.h`，声明 5 个 typed `msbs_<name>_param_read` / `_write` 内部函数（实现由 T15-T19 落地）+ EmptyParam dispatcher 调用。
  - 单元测试 `tests/map/msbs/test_msbs_root_smoke.c`：构造一个 0-entry MSBS（**8 段全空，含 3 个 EmptyParam 占位**）→ write → read → write → cmp 0。

  **Must NOT do**：
  - ❌ 不实现任何具体 subtype 的字段映射（那是 T15-T19 的范围）。
  - ❌ 不在 `sf_msbs.h` 暴露 MSBE / MSBVI 概念。
  - ❌ 不复用 MSBE / MSBVI 内部代码（每 variant 独立）。
  - ❌ 不省略 EmptyParam 段（Layer / PartsPoses / BoneNames）——上游 reader 在缺段时抛 `InvalidDataException`，C 端必须 round-trip 保字节级一致。

  **Recommended Agent Profile**：Category=`unspecified-high`（API 设计 + dispatcher 框架 + EmptyParam 占位语义）；Skills=无。

  **Parallelization**：YES（与 T14b, T14c 并行）；Wave 1；Blocks=T15-T19, T33, T39；Blocked By=T7, T8。

  **References**：
  - Pattern：`src/archive/bnd4.c:全文`（BND4 也是 list-of-lists 根 dispatcher）；`include/souls_formats/sf_bnd4.h`（opaque + accessor）。
  - API：`Formats/MSB/MSBS/MSBS.cs:91-115`（Read()，明确 8 段顺序与 EmptyParam 写法）、`MSBS.cs:Write()` 对应段。
  - WHY：MSBS 段数 8 而非 5；漏掉 EmptyParam 段会让 round-trip 偏移全部错位。**Layer / PartsPoses / BoneNames 是 MSBS 特有的占位段，MSBE / MSBVI 没有这两段**。

  **Acceptance Criteria**：
  - [ ] `sf_msbs.h` + `msbs.c` + `msbs_internal.h` 提交并编译通过。
  - [ ] 5 个 typed sub-param internal 函数 forward-declared（实现由 T15-T19 链入）；3 个 EmptyParam 段在 dispatcher 内部处理。
  - [ ] `tests/map/msbs/test_msbs_root_smoke.c` round-trip cmp 0 PASS（**0-entry MSBS 含 8 段，3 个 EmptyParam 段头保留**）。
  - [ ] `grep 'SF_API' include/souls_formats/sf_msbs.h` ≥ 12 行（read/write/destroy + 5×count + 5×at）。
  - [ ] sf_msbs.h 中无 layer / partspose / bonename 任何字眼（仅在 internal 处理）。

  **QA Scenarios**：
  ```
  Scenario: 0-entry MSBS root 8-段 round-trip
    Tool: Bash + Unity
    Preconditions: T7, T8 完成
    Steps:
      1. fixture：构造空 MSBS（8 段全空：5 typed + Layer/PartsPoses/BoneNames 3 EmptyParam）
      2. `cmake --build build-mingw --target souls_formats_test_msbs_root_smoke`
      3. `ctest --test-dir build-mingw -R msbs_root_smoke --output-on-failure`
      4. 测试内部：write A → read A → write A' → cmp A A'
      5. 测试断言：write 后字节流应含 8 个段头（`MODEL_PARAM_ST` / `EVENT_PARAM_ST` / `POINT_PARAM_ST` / `ROUTE_PARAM_ST` / `LAYER_PARAM_ST` / `PARTS_PARAM_ST` / `MAPSTUDIO_PARTS_POSE_ST` / `MAPSTUDIO_BONE_NAME_STRING` 字符串）
    Expected Result: 步骤 3 退出码 0；cmp 退出码 0；步骤 5 全部段名都出现在 hex dump
    Failure Indicators: 段头数 ≠ 8；或 EmptyParam 段被略写
    Evidence: .sisyphus/evidence/task-14a-msbs-root-smoke.log + bins + hex_dump.txt

  Scenario: sf_msbs.h header 双向 include smoke
    Tool: Bash
    Preconditions: T14a 完成
    Steps:
      1. 临时 `tests/core/_msbs_header_smoke.c` 和 `.cpp`：`#include "souls_formats/sf_msbs.h"\nint main(){return 0;}`
      2. `cmake --build build-mingw --target ...smoke`
    Expected Result: C + C++ 两个 smoke 都编译通过
    Failure Indicators: typedef 冲突；extern "C" 漏；absolute path include 复发
    Evidence: .sisyphus/evidence/task-14a-msbs-header-smoke.log

  Scenario: 公共头不泄漏 EmptyParam 占位段概念
    Tool: Bash
    Preconditions: T14a 完成
    Steps:
      1. `grep -iE 'layer|partspose|bonename|partspose|empty.param' include/souls_formats/sf_msbs.h | tee .sisyphus/evidence/task-14a-public-leak.log`
    Expected Result: 输出为空
    Failure Indicators: 命中
    Evidence: .sisyphus/evidence/task-14a-public-leak.log
  ```

  **Commit**: `phase5(msbs): root dispatcher + sf_msbs.h opaque API (8 segments incl. EmptyParam placeholders)`；files: `include/souls_formats/sf_msbs.h`, `src/map/msbs/msbs.c`, `src/map/msbs/msbs_internal.h`, `tests/map/msbs/test_msbs_root_smoke.c`, CMakeLists.txt；Pre-commit: `ctest -R msbs_root` PASS.

- [x] 14b. **`sf_msbe.h` + `src/map/msbe/msbe.c` 根 dispatcher（6 segment 容器壳）**

  **What to do**：
  - **上游段顺序（参考 `MSBE.cs:80-92`，共 6 个 list segment，必须严格按此顺序读写）**：
    1. `Models` (typed ModelParam，T20)
    2. `Events` (typed EventParam，T21)
    3. `Regions` (typed PointParam，T22)
    4. `Routes` (typed RouteParam，T24)
    5. `Layers` (**`EmptyParam(0x49, "LAYER_PARAM_ST")` ——MSBE 没有 typed Layer**，空段头)
    6. `Parts` (typed PartsParam，T23)
  - 起草 `include/souls_formats/sf_msbe.h`、`src/map/msbe/msbe.c`、`src/map/msbe/msbe_internal.h`，与 T14a 完全同款 schema：
    - opaque `sf_msbe_t` + 5 个 typed sub-param opaque + 5 个 typed internal read/write forward decl + 1 个 EmptyParam dispatcher 调用。
    - root API：`sf_msbe_read_from_memory` / `sf_msbe_write_to_memory` / `sf_msbe_destroy` + 5×count + 5×at（**EmptyParam 段不暴露**）。
  - 单元测试 `tests/map/msbe/test_msbe_root_smoke.c`：0-entry MSBE（6 段全空，含 1 个 EmptyParam）round-trip cmp 0。

  **Must NOT do**：与 T14a 同；额外❌ 不复用 MSBS root 代码（每 variant 独立）；❌ 不引入 Nightreign-specific 分支（probe 结论 = A 时直接复用即可）；❌ 不省略 LayerParam EmptyParam 段（漏掉会偏移）。

  **Recommended Agent Profile**：Category=`unspecified-high`；Skills=无。

  **Parallelization**：YES（与 T14a, T14c 并行）；Wave 1；Blocks=T20-T24, T34, T37, T38；Blocked By=T7, T8。

  **References**：
  - Pattern：T14a；`Formats/MSB/MSBE/MSBE.cs:80-100`（Read，6 段顺序与 EmptyParam 写法）。
  - API：`Formats/MSB/MSBE/MSBE.cs:Read()` / `Write()`。
  - WHY：MSBE 段数 6 而非 5；Layer 段是 EmptyParam 占位但**必须保留**否则 round-trip 字节偏移错位。

  **Acceptance Criteria**：
  - [ ] `sf_msbe.h` + `msbe.c` + `msbe_internal.h` 提交并编译通过。
  - [ ] 5 个 typed sub-param internal 函数 forward-declared；1 个 EmptyParam（Layer）段由 dispatcher 内部处理。
  - [ ] `tests/map/msbe/test_msbe_root_smoke.c` 0-entry round-trip cmp 0 PASS。
  - [ ] `grep 'SF_API' include/souls_formats/sf_msbe.h` ≥ 12 行。
  - [ ] sf_msbe.h 中无 layer / empty.param 字眼。

  **QA Scenarios**：
  ```
  Scenario: 0-entry MSBE root 6-段 round-trip
    Tool: Bash + Unity
    Preconditions: T7, T8 完成
    Steps:
      1. fixture：空 MSBE（6 段全空：5 typed + 1 EmptyParam(Layer)）
      2. `cmake --build build-mingw --target souls_formats_test_msbe_root_smoke`
      3. `ctest --test-dir build-mingw -R msbe_root_smoke --output-on-failure`
      4. 测试内部：write → read → write → cmp
      5. 测试断言：hex dump 应含 `MODEL_PARAM_ST` / `EVENT_PARAM_ST` / `POINT_PARAM_ST` / `ROUTE_PARAM_ST` / `LAYER_PARAM_ST` / `PARTS_PARAM_ST` 6 个段名
    Expected Result: 步骤 3 退出码 0；cmp 退出码 0；步骤 5 全部 6 段名出现
    Failure Indicators: 段数 ≠ 6；LayerParam 被略写
    Evidence: .sisyphus/evidence/task-14b-msbe-root-smoke.log + bins + hex_dump.txt

  Scenario: sf_msbe.h header smoke
    Tool: Bash
    Preconditions: T14b 完成
    Steps:
      1. `tests/core/_msbe_header_smoke.{c,cpp}` include sf_msbe.h
      2. `cmake --build build-mingw --target ...smoke`
    Expected Result: 编译通过
    Failure Indicators: include 错；extern "C" 漏
    Evidence: .sisyphus/evidence/task-14b-msbe-header-smoke.log
  ```

  **Commit**: `phase5(msbe): root dispatcher + sf_msbe.h opaque API (6 segments incl. Layer EmptyParam placeholder)`；files: `include/souls_formats/sf_msbe.h`, `src/map/msbe/msbe.c`, `src/map/msbe/msbe_internal.h`, `tests/map/msbe/test_msbe_root_smoke.c`, CMakeLists.txt；Pre-commit: `ctest -R msbe_root` PASS.

- [x] 14c. **`sf_msbvi.h` + `src/map/msbvi/msbvi.c` 根 dispatcher（6 segment 容器壳，含 typed LayerParam）**

  **What to do**：
  - **上游段顺序（参考 `MSBVI.cs:80-100`，共 6 个 list segment，必须严格按此顺序读写）**：
    1. `Models` (typed ModelParam，T25)
    2. `Events` (typed EventParam，T26)
    3. `Regions` (typed PointParam，T27)
    4. `Routes` (typed RouteParam，T29)
    5. `Layers` (**typed LayerParam，T30** —— MSBVI 与 MSBS/MSBE 的关键区别：**MSBVI 这一段是 typed param，非 EmptyParam**)
    6. `Parts` (typed PartsParam，T28)
  - 起草 `include/souls_formats/sf_msbvi.h`、`src/map/msbvi/msbvi.c`、`src/map/msbvi/msbvi_internal.h`：
    - opaque `sf_msbvi_t` + 6 个 typed sub-param opaque（多 `sf_msbvi_layer_t`）+ 6 个 typed internal read/write forward decl。
    - root API：`sf_msbvi_read_from_memory` / `sf_msbvi_write_to_memory` / `sf_msbvi_destroy` + 6×count + 6×at（含 layer typed accessor）。
  - 单元测试 `tests/map/msbvi/test_msbvi_root_smoke.c`：0-entry MSBVI（6 段全空，**含 typed LayerParam 0 entries 而非 EmptyParam**）round-trip cmp 0。

  **Must NOT do**：与 T14a/T14b 同；额外❌ 不在 sf_msb.h 公共头中暴露 layer 概念（T30 也强调过）；❌ 不把 MSBVI Layer 当 EmptyParam 处理 —— 它是有 typed `Layer` 类型的真实 param；❌ 不假设 LayerParam 与 RouteParam 段顺序 —— 上游 MSBVI.cs 顺序固定为 Models/Events/Regions/Routes/**Layers**/Parts。

  **Recommended Agent Profile**：Category=`unspecified-high`；Skills=无。

  **Parallelization**：YES（与 T14a, T14b 并行）；Wave 1；Blocks=T25-T30, T35, T40；Blocked By=T7, T8。

  **References**：
  - Pattern：T14a, T14b；`Formats/MSB/MSBVI/MSBVI.cs:Read()` / `Write()`（6-sub-param dispatch 顺序）。
  - API：`Formats/MSB/MSBVI/MSBVI.cs`。
  - WHY：MSBVI 比 MSBE / MSBS 多 LayerParam；list 顺序需严格按上游来 —— 上游顺序若错位，整文件偏移全报废。

  **Acceptance Criteria**：
  - [ ] `sf_msbvi.h` + `msbvi.c` + `msbvi_internal.h` 提交并编译通过。
  - [ ] 6 个 sub-param internal 函数 forward-declared（含 layer）。
  - [ ] `tests/map/msbvi/test_msbvi_root_smoke.c` 0-entry round-trip cmp 0 PASS。
  - [ ] `grep 'SF_API' include/souls_formats/sf_msbvi.h` ≥ 14 行（read/write/destroy + 6×count + 6×at）。
  - [ ] 公共头 sf_msb.h / sf_msbs.h / sf_msbe.h 中 `grep -i 'layer'` 0 命中。

  **QA Scenarios**：
  ```
  Scenario: 0-entry MSBVI root round-trip（6 sub-params）
    Tool: Bash + Unity
    Preconditions: T7, T8 完成
    Steps:
      1. fixture：空 MSBVI（6 sub-param 全 0 entries，含 layer 0）
      2. `cmake --build build-mingw --target souls_formats_test_msbvi_root_smoke`
      3. `ctest --test-dir build-mingw -R msbvi_root_smoke --output-on-failure`
      4. 测试内部：write → read → write → cmp
    Expected Result: 步骤 3 退出码 0；cmp 退出码 0；layer 段头 = 0 entries
    Failure Indicators: layer 段被略写；或 list dispatch 顺序错位
    Evidence: .sisyphus/evidence/task-14c-msbvi-root-smoke.log + bins

  Scenario: layer 概念未泄漏到公共头
    Tool: Bash
    Preconditions: T14c 完成
    Steps:
      1. `grep -i 'layer' include/souls_formats/sf_msb.h include/souls_formats/sf_msbs.h include/souls_formats/sf_msbe.h | tee .sisyphus/evidence/task-14c-public-leak.log`
    Expected Result: 输出为空
    Failure Indicators: 命中
    Evidence: .sisyphus/evidence/task-14c-public-leak.log
  ```

  **Commit**: `phase5(msbvi): root dispatcher + sf_msbvi.h opaque API (incl. layer)`；files: `include/souls_formats/sf_msbvi.h`, `src/map/msbvi/msbvi.c`, `src/map/msbvi/msbvi_internal.h`, `tests/map/msbvi/test_msbvi_root_smoke.c`, CMakeLists.txt；Pre-commit: `ctest -R msbvi_root` PASS.

### Wave 2 — Variant × Sub-Param 矩阵（17 路并行，Wave 1 全绿后启动）

> Wave 1 完成后，每个 variant 的根 header + dispatcher（T14a/T14b/T14c）就位；Wave 2 各 sub-param task 实现 internal `msbX_<name>_param_read` / `_write` 函数（在 T14a-c 中 forward-declared），由 root dispatcher 链入。
>
> **Wave 2 额外依赖**（在每 task 的 "Blocked By=T7, T8" 之外，下列依赖**全部 implicit 自动叠加**，执行端调度时必须遵守）：
> - **MSBS sub-params（T15-T19）**：额外 Blocked By **T14a**（sf_msbs.h + msbs.c 根 dispatcher）。
> - **MSBE sub-params（T20-T24）**：额外 Blocked By **T14b**。
> - **MSBVI sub-params（T25-T30）**：额外 Blocked By **T14c**。
> 各 sub-param task 内文档化的 "Blocked By=T7, T8" 视为简写；root dispatcher 是隐含前置条件。

> **执行原则**：每个 task 实现一个 variant 的一个 sub-param 的 read + write + synthetic round-trip 单元测试。每个 task 都遵循同样的模板，但 reference 与具体 subtype 列表按 variant 分别取上游对应 `.cs` 文件。Variant 间彼此独立，**严格禁止跨 variant 共享 internal state**。

#### MSBS（Sekiro）

- [x] 15. **`src/map/msbs/model_param.c` read + write**

  **What to do**：实现 MSBS ModelParam（5 个 model subtype：MapPiece / Object / Enemy / Player / Collision）的 read + write，对齐 `Formats/MSB/MSBS/ModelParam.cs` 与同目录下各 Model 子类。所有字段逐一映射；每个 subtype 一个 inner switch。

  **Must NOT do**：❌ 不动 MSBE / MSBVI 模块；❌ 不在公共头里暴露 MSBS-specific 字段；❌ 不跳过 subtype（上游枚举里出现的都必须实现）。

  **Recommended Agent Profile**：Category=`deep`（多 subtype 字段精确映射）；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T33, T39；Blocked By=T7, T8。

  **References**：
  - Pattern：`src/script/emevd_event.c`（Phase 4 多子类 read+write 模式）。
  - API：`Formats/MSB/MSBS/ModelParam.cs`、`Formats/MSB/MSBS/MSBS.cs:Read()`。
  - Test：`tests/script/test_emevd_synthetic.c`（per-subtype fixture 模式）。
  - WHY：EMEVD 在 Phase 4 用同款 subtype switch 模式 F1-F4 APPROVE 过，本 task 完全沿用。

  **Acceptance Criteria**：
  - [ ] 5 个 model subtype 全实现 reader + writer。
  - [ ] `tests/map/msbs/test_model_param_synthetic.c` 1 个 fixture / subtype 字节级 round-trip PASS。
  - [ ] `ctest -L map -R msbs_model_param` PASS。

  **QA Scenarios**：
  ```
  Scenario: 5 个 subtype 字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture 构造：每个 subtype 一行 entry，各填 ≥ 3 个字段非默认值
      2. `ctest -R msbs_model_param_synthetic --output-on-failure`
    Expected Result: 5 subtype round-trip 全 PASS
    Failure Indicators: 任一 subtype output ≠ input
    Evidence: .sisyphus/evidence/task-15-msbs-model-rt.log

  Scenario: 未知 subtype kind 返回 SF_ERR_UNSUPPORTED
    Tool: Bash
    Preconditions: T15 完成
    Steps:
      1. fixture 中插入一个 invalid model kind = 99
      2. assert reader 返回 SF_ERR_UNSUPPORTED_VERSION 或 _UNSUPPORTED_KIND
    Expected Result: 错误码明确，不 crash
    Evidence: .sisyphus/evidence/task-15-msbs-model-unknown.log
  ```

  **Commit**: `phase5(msbs): ModelParam read+write (5 subtypes)`；files: `src/map/msbs/model_param.c`, `src/map/msbs/msbs_internal.h`, `tests/map/msbs/test_model_param_synthetic.c`, CMakeLists.txt；Pre-commit: ctest PASS.

- [x] 16. **`src/map/msbs/event_param.c` read + write**

  **What to do**：实现 MSBS EventParam（14 个 event subtype，含 Sound / SFX / Treasure / Generator / ObjAct / MapOffset / WalkRoute / GroupTour / Other 等），对齐 `Formats/MSB/MSBS/EventParam.cs`。

  **Must NOT do**：❌ 不实现 ER / AC6 中独有的 event kind；❌ 不在 event subtype 之间共享可变状态。

  **Recommended Agent Profile**：Category=`deep`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T33, T39；Blocked By=T7, T8。

  **References**：
  - Pattern：T15 + `src/script/emevd_event.c`。
  - API：`Formats/MSB/MSBS/EventParam.cs`（14 个 inner class）。
  - WHY：Sekiro Event subtype 数量最多，需要严格按上游 enum 顺序映射 kind 编号。

  **Acceptance Criteria**：
  - [ ] 14 个 event subtype 全实现。
  - [ ] `tests/map/msbs/test_event_param_synthetic.c` 14 fixture round-trip PASS。

  **QA Scenarios**：
  ```
  Scenario: 14 个 subtype round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture：14 个 event entry，每个一个 subtype
      2. `ctest -R msbs_event_param_synthetic --output-on-failure`
    Expected Result: 14 subtype 全 PASS
    Evidence: .sisyphus/evidence/task-16-msbs-event-rt.log

  Scenario: Event with empty parameters
    Tool: Bash + Unity
    Steps:
      1. fixture：event 的 parameter 数组长度 0
      2. round-trip
    Expected Result: PASS，empty 数组保留为 0 长度
    Evidence: .sisyphus/evidence/task-16-msbs-event-empty.log
  ```

  **Commit**: `phase5(msbs): EventParam read+write (14 subtypes)`；files: `src/map/msbs/event_param.c`, `tests/map/msbs/test_event_param_synthetic.c`, CMakeLists.txt.

- [x] 17. **`src/map/msbs/point_param.c` read + write**

  **What to do**：实现 MSBS PointParam（Sekiro region kind 20 个，含 InvasionPoint / EnvironmentMapPoint / Sound / SFX / WindSFX / SpawnPoint / WalkRoute / WarpPoint / ActivationArea / Event / Logic / EnvironmentMapEffectBox / WindArea / MufflingBox / MufflingPortal / SoundSpaceOverride / Patrol / FastTravelRestriction / Other 等），对齐 `Formats/MSB/MSBS/PointParam.cs`。**Point 实际是 region**——上游用 PointParam 命名但内部都是 region kind。

  **Must NOT do**：❌ 不实现 Shape 全集（仅 Sekiro 用到的 shape kind）；❌ 不在 region 上下文外引用 Shape。

  **Recommended Agent Profile**：Category=`deep`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T33, T39；Blocked By=T7, T8。

  **References**：
  - Pattern：T15-T16。
  - API：`Formats/MSB/MSBS/PointParam.cs`、`Formats/MSB/Shape.cs`（用到的 shape kind 子集）。
  - WHY：Sekiro 是 MSB 中 region 数量最多的 variant，shape 子集必须严格限定。

  **Acceptance Criteria**：
  - [ ] 20 个 region subtype 全实现。
  - [ ] Shape kind 只支持 Sekiro 实际使用的子集（Point / Box / Sphere / Cylinder / Composite），其他 kind reader 返回 SF_ERR_UNSUPPORTED_VERSION。
  - [ ] `tests/map/msbs/test_point_param_synthetic.c` 20 fixture round-trip PASS。

  **QA Scenarios**：
  ```
  Scenario: 20 个 region subtype round-trip
    Tool: Bash + Unity
    Steps:
      1. fixture：20 region entries
      2. `ctest -R msbs_point_param_synthetic --output-on-failure`
    Expected Result: PASS
    Evidence: .sisyphus/evidence/task-17-msbs-point-rt.log

  Scenario: 未支持的 Shape kind 优雅拒绝
    Tool: Bash + Unity
    Steps:
      1. fixture：region 用 Shape.NavMesh（Sekiro 不用）
      2. reader → assert SF_ERR_UNSUPPORTED_VERSION
    Expected Result: 明确错误码
    Evidence: .sisyphus/evidence/task-17-msbs-shape-unsupported.log
  ```

  **Commit**: `phase5(msbs): PointParam read+write (20 region subtypes)`；files: `src/map/msbs/point_param.c`, `tests/map/msbs/test_point_param_synthetic.c`, CMakeLists.txt.

- [x] 18. **`src/map/msbs/parts_param.c` read + write**

  **What to do**：实现 MSBS PartsParam（8 个 part subtype：MapPiece / Object / Enemy / Player / Collision / DummyObject / DummyEnemy / ConnectCollision）的 read + write，对齐 `Formats/MSB/MSBS/PartsParam.cs`。这是 MSB 中 **最复杂** 的子表（字段最多，包含 Transform / GparamConfig / SceneGparamConfig / DrawGroups / DispGroups / 等）。

  **Must NOT do**：❌ 不引入 ER/AC6 中的额外 part 字段；❌ 不缩减字段集（上游有的都映射，包括 unknown bytes 字段）。

  **Recommended Agent Profile**：Category=`deep`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T33, T39；Blocked By=T7, T8。

  **References**：
  - Pattern：T15-T17；上游 `MSBS/PartsParam.cs` 的 `Read()` / `Write()` 实现。
  - API：`Formats/MSB/MSBS/PartsParam.cs`（8 个 inner class，每个 ~50-100 字段）。
  - WHY：PartsParam 是 plan 中 LOC 最大的一个 task，预估 2000+ LOC；执行 agent 必须先 grep 上游 .cs 一遍统计字段再开工。

  **Acceptance Criteria**：
  - [ ] 8 个 part subtype 全实现。
  - [ ] 每个 subtype 至少 90% 字段（含 unknown）出现在 C 端 `parts_param.c`。
  - [ ] `tests/map/msbs/test_parts_param_synthetic.c` 8 fixture round-trip PASS。

  **QA Scenarios**：
  ```
  Scenario: 8 个 part subtype round-trip + 字段完整性
    Tool: Bash + Unity
    Steps:
      1. fixture：8 个 part entries，每个填 ≥ 20 个字段
      2. `ctest -R msbs_parts_param_synthetic --output-on-failure`
      3. `grep -c 'sf_msbs_parts_param_field_' src/map/msbs/parts_param.c`（应 ≥ 上游字段数 × 90%）
    Expected Result: 全 PASS
    Evidence: .sisyphus/evidence/task-18-msbs-parts-rt.log

  Scenario: 引用 unknown model index 时优雅处理
    Tool: Bash + Unity
    Steps:
      1. fixture：part.model_index = 9999（超过 model list 长度）
      2. round-trip
    Expected Result: 写出读回字段一致；不做引用合法性校验（上游也不做）
    Evidence: .sisyphus/evidence/task-18-msbs-parts-bad-ref.log
  ```

  **Commit**: `phase5(msbs): PartsParam read+write (8 subtypes, largest sub-param)`；files: `src/map/msbs/parts_param.c`, `tests/map/msbs/test_parts_param_synthetic.c`, CMakeLists.txt.

- [x] 19. **`src/map/msbs/route_param.c` read + write**

  **What to do**：实现 MSBS RouteParam（2 个 route subtype：MufflingPortalLink / MufflingBoxLink），对齐 `Formats/MSB/MSBS/RouteParam.cs`。这是 MSBS 中最简单的 sub-param。

  **Must NOT do**：❌ 不引入 ER/AC6 route 子类。

  **Recommended Agent Profile**：Category=`unspecified-high`（任务规模较小但 schema 与其他 sub-param 一致）；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T33, T39；Blocked By=T7, T8。

  **References**：
  - Pattern：T15-T18。
  - API：`Formats/MSB/MSBS/RouteParam.cs`。
  - WHY：route 是最简单 sub-param，留 catch-up wave。

  **Acceptance Criteria**：
  - [ ] 2 个 route subtype 全实现 reader + writer。
  - [ ] `tests/map/msbs/test_route_param_synthetic.c` round-trip cmp 0 PASS。

  **QA Scenarios**：
  ```
  Scenario: MSBS Route 2 subtype 字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture：2 个 Route entry，1 个 MufflingPortalLink + 1 个 MufflingBoxLink，每个填 Name + ≥ 2 字段非默认值
      2. `cmake --build build-mingw --target souls_formats_test_msbs_route_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbs_route_param_synthetic --output-on-failure`
      4. `cmp fixture_input.bin fixture_output.bin`
    Expected Result: 步骤 3 退出码 0；步骤 4 cmp 退出码 0
    Failure Indicators: 任一字段未保留；cmp 报偏差
    Evidence: .sisyphus/evidence/task-19-msbs-route-rt.log + fixture_input.bin + fixture_output.bin

  Scenario: 空 RouteParam 段保留
    Tool: Bash + Unity
    Preconditions: T19 完成
    Steps:
      1. fixture：RouteParam 0 entries
      2. `ctest --test-dir build-mingw -R msbs_route_param_empty --output-on-failure`
      3. `cmp fixture_input.bin fixture_output.bin`
    Expected Result: cmp 0；空段头保留为 0 entries
    Failure Indicators: 段被略写或填充字节出现
    Evidence: .sisyphus/evidence/task-19-msbs-route-empty.log + bins
  ```

  **Commit**: `phase5(msbs): RouteParam read+write (2 subtypes)`；files: `src/map/msbs/route_param.c`, `tests/map/msbs/test_route_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbs_route_param` PASS.

#### MSBE（Elden Ring + Nightreign）

- [x] 20. **`src/map/msbe/model_param.c` read + write**

  **What to do**：实现 MSBE ModelParam（5 个 model subtype）对齐 `Formats/MSB/MSBE/ModelParam.cs`。结构与 T15 同款，字段表按 ER 对齐。

  **Must NOT do**：❌ 不动 MSBS / MSBVI；❌ 不为 Nightreign 加 NR-specific 字段（probe 结论 = A 时直接复用，结论 = B 时单独建一个 follow-up task；本 plan 假设 A）。

  **Recommended Agent Profile**：Category=`deep`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T34, T37, T38；Blocked By=T4, T7, T8。

  **References**：
  - Pattern：T15 + `Formats/MSB/MSBE/MSBE.cs:Read()`。
  - API：`Formats/MSB/MSBE/ModelParam.cs`、`/MSBE/MSBE.cs`。
  - WHY：MSBE Model 与 MSBS Model 字段对齐度高，但 ER 多了 InstanceID 等新字段；需要按 MSBE.cs 全量映射。

  **Acceptance Criteria**：
  - [ ] 5 model subtype（MapPiece / Asset / Enemy / Player / Collision）全实现 reader + writer。
  - [ ] `tests/map/msbe/test_model_param_synthetic.c` 5 fixture round-trip cmp 0 PASS。

  **QA Scenarios**：
  ```
  Scenario: ER Model 5 subtype 字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture：5 个 model entry，每个一种 subtype（MapPiece/Asset/Enemy/Player/Collision），每个填 ≥ 4 字段非默认值（含 InstanceID）
      2. `cmake --build build-mingw --target souls_formats_test_msbe_model_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbe_model_param_synthetic --output-on-failure`
      4. `cmp fixture_input.bin fixture_output.bin`
    Expected Result: 步骤 3、4 退出码 0
    Failure Indicators: 任一字段不保留；cmp 报偏差
    Evidence: .sisyphus/evidence/task-20-msbe-model-rt.log + fixture bins

  Scenario: InstanceID 字段保真（ER 1.07 特有）
    Tool: Bash + Unity
    Preconditions: T20 完成
    Steps:
      1. fixture：Asset 子类 entry 中 InstanceID = 0x12345678
      2. `ctest --test-dir build-mingw -R msbe_model_param_instance_id --output-on-failure`
      3. 测试断言：写出后再读，InstanceID == 0x12345678
    Expected Result: assert PASS
    Failure Indicators: 字段被覆盖或截断
    Evidence: .sisyphus/evidence/task-20-msbe-model-instanceid.log
  ```

  **Commit**: `phase5(msbe): ModelParam read+write (5 subtypes)`；files: `src/map/msbe/model_param.c`, `src/map/msbe/msbe_internal.h`, `tests/map/msbe/test_model_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbe_model_param` PASS.

- [x] 21. **`src/map/msbe/event_param.c` read + write**

  **What to do**：实现 MSBE EventParam（12 个 event subtype）对齐 `Formats/MSB/MSBE/EventParam.cs`。

  **Must NOT do**：❌ 不实现 MSBS 独有的 event kind；❌ 不实现 AC6 独有的 event kind。

  **Recommended Agent Profile**：Category=`deep`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T34, T37, T38；Blocked By=T7, T8。

  **References**：
  - Pattern：T16, T20。
  - API：`Formats/MSB/MSBE/EventParam.cs`。
  - WHY：MSBE event subtype 数量适中，pattern 已熟。

  **Acceptance Criteria**：
  - [ ] 12 event subtype 全实现 reader + writer。
  - [ ] `tests/map/msbe/test_event_param_synthetic.c` 12 fixture round-trip cmp 0 PASS。

  **QA Scenarios**：
  ```
  Scenario: ER Event 12 subtype 字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture：12 个 event entry，每个一种 ER event subtype（按上游 MSBE/EventParam.cs 的 enum 顺序），每个填 Name + ≥ 3 字段非默认值
      2. `cmake --build build-mingw --target souls_formats_test_msbe_event_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbe_event_param_synthetic --output-on-failure`
      4. `cmp fixture_input.bin fixture_output.bin`
    Expected Result: 12 subtype 全 PASS；cmp 0
    Failure Indicators: 任一 subtype output ≠ input
    Evidence: .sisyphus/evidence/task-21-msbe-event-rt.log + fixture bins

  Scenario: Event 引用 unknown part name 时字符串原样保留
    Tool: Bash + Unity
    Preconditions: T21 完成
    Steps:
      1. fixture：event entry 的 target_part_name = "DOES_NOT_EXIST"
      2. `ctest --test-dir build-mingw -R msbe_event_param_bad_ref --output-on-failure`
      3. 测试断言：写出后再读，target_part_name == "DOES_NOT_EXIST"
    Expected Result: 字符串原样保留；不做 reference 合法性校验（上游也不做）
    Failure Indicators: 字符串被清空或替换
    Evidence: .sisyphus/evidence/task-21-msbe-event-badref.log
  ```

  **Commit**: `phase5(msbe): EventParam read+write (12 subtypes)`；files: `src/map/msbe/event_param.c`, `tests/map/msbe/test_event_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbe_event_param` PASS.

- [x] 22. **`src/map/msbe/point_param.c` read + write**

  **What to do**：实现 MSBE PointParam（**39 个 region subtype** —— 三 variant 中最多的），对齐 `Formats/MSB/MSBE/PointParam.cs`。包括 ER 独有的 Asset / OperationalArea / NavmeshGroup / MapPoint 等。

  **Must NOT do**：❌ 不限制 Shape kind 集合（ER 用 shape 比 Sekiro 多）；❌ 不为未来 Nightreign-specific kind 留 stub。

  **Recommended Agent Profile**：Category=`deep`（subtype 数量最多，是单 task LOC 第二大）；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T34, T37, T38；Blocked By=T7, T8。

  **References**：
  - Pattern：T17, T20。
  - API：`Formats/MSB/MSBE/PointParam.cs`（39 inner classes）、`Formats/MSB/Shape.cs`。
  - WHY：MSBE point param 是 plan 中 single task 工作量第二大（仅次于 T23 PartsParam）；执行 agent 必须分块 commit（按每 10 个 subtype 一个 sub-commit 也允许，但最终 squash 成一个 task commit）。

  **Acceptance Criteria**：
  - [ ] 39 region subtype 全实现 reader + writer。
  - [ ] `tests/map/msbe/test_point_param_synthetic.c` ≥ 30 fixture round-trip cmp 0 PASS（剩余 ≤ 9 个 subtype 通过 e2e T37 覆盖）。
  - [ ] Shape kind 集合 = 上游 `Shape.cs:10-20` 实际的 7 个 ShapeType 值 = **Point(0) / Circle(1) / Sphere(2) / Cylinder(3) / Rectangle(4) / Box(5) / Composite(6)** 全支持（外加 None = 0xFFFFFFFF 用于无 shape 的 region）。

  **QA Scenarios**：
  ```
  Scenario: ER Point 39 subtype 中至少 30 个字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture：30 个 region entry，覆盖上游 PointParam.cs enum 中前 30 个 subtype（含 InvasionPoint / EnvMapPoint / Sound / SFX / WarpPoint / Asset / OperationalArea / NavmeshGroup / MapPoint 等），每个填 Name + Shape + ≥ 3 字段
      2. `cmake --build build-mingw --target souls_formats_test_msbe_point_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbe_point_param_synthetic --output-on-failure`
      4. `cmp fixture_input.bin fixture_output.bin`
    Expected Result: 30 subtype round-trip 全 PASS；cmp 0
    Failure Indicators: 任一 subtype 字段错位；reader 在 known kind 上报 unsupported
    Evidence: .sisyphus/evidence/task-22-msbe-point-rt.log + fixture bins

  Scenario: Shape 全集 7 kind 覆盖（对齐 Shape.cs:10-20）
    Tool: Bash + Unity
    Preconditions: T22 完成
    Steps:
      1. fixture：7 个 region entry，每个 shape 为 `Point` / `Circle` / `Sphere` / `Cylinder` / `Rectangle` / `Box` / `Composite` 之一（**严格按上游 ShapeType enum，None 不在覆盖内**）
      2. `cmake --build build-mingw --target souls_formats_test_msbe_point_param_shape_coverage`
      3. `ctest --test-dir build-mingw -R msbe_point_param_shape_coverage --output-on-failure`
      4. `cmp fixture_input.bin fixture_output.bin`
    Expected Result: 7 shape 全 round-trip PASS；cmp 0
    Failure Indicators: 任一 shape 报 SF_ERR_UNSUPPORTED_VERSION
    Evidence: .sisyphus/evidence/task-22-msbe-shape-coverage.log + bins

  Scenario: 上游不存在的 Shape kind（如 NavMesh / Triangle）触发 unsupported
    Tool: Bash + Unity
    Preconditions: T22 完成
    Steps:
      1. fixture：region.shape_kind = 99（上游 ShapeType enum 不存在的值）
      2. `ctest --test-dir build-mingw -R msbe_point_param_invalid_shape --output-on-failure`
      3. 测试断言：reader 返回 SF_ERR_UNSUPPORTED_VERSION 或 SF_ERR_UNSUPPORTED_KIND
    Expected Result: 明确错误码
    Failure Indicators: segfault；或静默 round-trip
    Evidence: .sisyphus/evidence/task-22-msbe-shape-invalid.log
  ```

  **Commit**: `phase5(msbe): PointParam read+write (39 region subtypes, 7 shape kinds aligned to Shape.cs)`；files: `src/map/msbe/point_param.c`, `tests/map/msbe/test_point_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbe_point_param` PASS.

- [x] 23. **`src/map/msbe/parts_param.c` read + write**

  **What to do**：实现 MSBE PartsParam（8 个 part subtype，但每个 part 字段比 MSBS 多 50%）对齐 `Formats/MSB/MSBE/PartsParam.cs`。这是整 plan 中 **single task LOC 最大** 的（预估 2500-3000 LOC）。包括 ER 新增的 Asset 字段、SceneGparamConfig 扩展。

  **Must NOT do**：❌ 不缩减字段（包括 unknown bytes）；❌ 不为 Nightreign 加扩展字段（probe 决策外）；❌ 不实现 ER 1.10+ patch 新加的字段（锁定到 1.07 patch level，T5 已记录）。

  **Recommended Agent Profile**：Category=`deep`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T34, T37, T38；Blocked By=T7, T8。

  **References**：
  - Pattern：T18, T20。
  - API：`Formats/MSB/MSBE/PartsParam.cs`（8 inner class 共 ~3000 LOC C#）。
  - Test：`tests/map/msbs/test_parts_param_synthetic.c`（T18 完成后作为范本）。
  - WHY：是 plan 中最大单 task，执行 agent 应该用 sub-commit per subtype 控制进度。

  **Acceptance Criteria**：
  - [ ] 8 part subtype 全实现。
  - [ ] 每个 subtype 至少 90% 字段映射。
  - [ ] `tests/map/msbe/test_parts_param_synthetic.c` PASS。

  **QA Scenarios**：
  ```
  Scenario: ER Part 8 subtype 字段保真
    Tool: Bash + Unity
    Steps: fixture：8 part entries 各填 ≥ 30 字段
    Expected Result: PASS
    Evidence: .sisyphus/evidence/task-23-msbe-parts-rt.log

  Scenario: Asset.AssetSfxParamRelativeIDs 数组保真
    Tool: Bash + Unity
    Steps: Asset 子类的 AssetSfxParamRelativeIDs 数组填 5 个非零值，round-trip
    Expected Result: 全 5 值不变
    Evidence: .sisyphus/evidence/task-23-msbe-asset-array.log
  ```

  **Commit**: `phase5(msbe): PartsParam read+write (8 subtypes, LOC heaviest task)`；files: `src/map/msbe/parts_param.c`, `tests/map/msbe/test_parts_param_synthetic.c`, CMakeLists.txt.

- [x] 24. **`src/map/msbe/route_param.c` read + write**

  **What to do**：实现 MSBE RouteParam（3 个 route subtype：MufflingPortalLink / MufflingBoxLink / Other）对齐 `Formats/MSB/MSBE/RouteParam.cs`。

  **Must NOT do**：❌ 不动其他 variant。

  **Recommended Agent Profile**：Category=`unspecified-high`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T34, T37, T38；Blocked By=T7, T8。

  **References**：T19, T20；`Formats/MSB/MSBE/RouteParam.cs`。WHY：route 是 MSB 中最简单 sub-param，complete the matrix。

  **Acceptance Criteria**：
  - [ ] 3 route subtype 全实现 reader + writer。
  - [ ] `tests/map/msbe/test_route_param_synthetic.c` round-trip cmp 0 PASS。

  **QA Scenarios**：
  ```
  Scenario: ER Route 3 subtype 字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture：3 个 Route entry，每个一种 ER route subtype（按上游 MSBE/RouteParam.cs 的 enum 顺序）
      2. `cmake --build build-mingw --target souls_formats_test_msbe_route_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbe_route_param_synthetic --output-on-failure`
      4. `cmp fixture_input.bin fixture_output.bin`
    Expected Result: 3 subtype round-trip PASS；cmp 0
    Failure Indicators: 任一字段未保留；cmp 报偏差
    Evidence: .sisyphus/evidence/task-24-msbe-route-rt.log + fixture bins

  Scenario: 0 entries RouteParam 段保留
    Tool: Bash + Unity
    Preconditions: T24 完成
    Steps:
      1. fixture：RouteParam 0 entries
      2. `ctest --test-dir build-mingw -R msbe_route_param_empty --output-on-failure`
      3. `cmp fixture_input.bin fixture_output.bin`
    Expected Result: cmp 0；空段头保留为 0 entries
    Failure Indicators: 段被略写
    Evidence: .sisyphus/evidence/task-24-msbe-route-empty.log
  ```

  **Commit**: `phase5(msbe): RouteParam read+write (3 subtypes)`；files: `src/map/msbe/route_param.c`, `tests/map/msbe/test_route_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbe_route_param` PASS.

#### MSBVI（Armored Core VI）

- [x] 25. **`src/map/msbvi/model_param.c` read + write**

  **What to do**：实现 MSBVI ModelParam 对齐 `Formats/MSB/MSBVI/ModelParam.cs`。上游实测 **5 个 concrete model subtype**：`MapPiece`(L227) / `Asset`(L242) / `Enemy`(L257) / `Player`(L272) / `Collision`(L287)。AC6 与 ER 在 Model param 上是同款 5 subtype，但字段不同（如 InstanceID 等 ER-specific 字段在 AC6 没有 / 反之）。

  **Must NOT do**：❌ 不实现上游标记为 NOT IMPLEMENTED 的 enum 值（仅做 unsupported stub，reader 遇到时返回 SF_ERR_UNSUPPORTED_VERSION）；❌ 不动其他 variant；❌ 不通过 #ifdef 切换 game-specific 字段。

  **Recommended Agent Profile**：Category=`deep`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T35, T40；Blocked By=T7, T8。

  **References**：
  - Pattern：T15（MSBS）, T20（MSBE） —— 同款 5-subtype 模式。
  - API：`Formats/MSB/MSBVI/ModelParam.cs` L227-300（5 inner class）、`MSBVI/MSBVI.cs:Read()`。
  - WHY：MSBVI Model 与 MSBS / MSBE 的 5 subtype 同名，但字段表不同；不要复用代码，按 MSBVI 上游 schema 重写。

  **Acceptance Criteria**：
  - [ ] 5 model subtype 全实现 reader + writer。
  - [ ] `tests/map/msbvi/test_model_param_synthetic.c` 5 fixture round-trip PASS。
  - [ ] enum 中标记 `NOT IMPLEMENTED` 的 model kind 走 unsupported 分支，不 abort。

  **QA Scenarios**：
  ```
  Scenario: AC6 Model 5 subtype 字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成（msb_common 骨架）
    Steps:
      1. fixture 在 test 文件中构造：5 个 model entry，每个一种 subtype；每个填 ≥ 3 字段非默认值
      2. `cmake --build build-mingw --target souls_formats_test_msbvi_model_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbvi_model_param_synthetic --output-on-failure`
    Expected Result: 5 subtype round-trip 全 PASS；步骤 3 退出码 0
    Failure Indicators: 任一 subtype output ≠ input；reader 在 known kind 上报 unsupported
    Evidence: .sisyphus/evidence/task-25-msbvi-model-rt.log + fixture_input.bin + fixture_output.bin

  Scenario: NOT IMPLEMENTED enum 值走 unsupported 分支
    Tool: Bash + Unity
    Preconditions: T25 完成
    Steps:
      1. 在测试代码中构造 fixture：在 model param 区段插入一个标记为 NOT IMPLEMENTED 的 kind（例如上游 enum 中存在但 class 未定义的值）
      2. 调用 sf_msbvi_read_from_memory
      3. assert 返回 SF_ERR_UNSUPPORTED_VERSION
    Expected Result: 明确错误码，不 crash
    Failure Indicators: segfault；或静默通过
    Evidence: .sisyphus/evidence/task-25-msbvi-model-unsupported.log
  ```

  **Commit**: `phase5(msbvi): ModelParam read+write (5 subtypes)`；files: `src/map/msbvi/model_param.c`, `src/map/msbvi/msbvi_internal.h`, `tests/map/msbvi/test_model_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbvi_model_param` PASS.

- [x] 26. **`src/map/msbvi/event_param.c` read + write**

  **What to do**：实现 MSBVI EventParam 对齐 `Formats/MSB/MSBVI/EventParam.cs`。上游实测 **7 个 concrete event subtype**：`Treasure`(L316) / `Generator`(L434) / `MapOffset`(L587) / `PlatoonInfo`(L625) / `PatrolRoute`(L708) / `MapGimmick`(L791) / `Other`(L907)。文件内额外有 22 个 enum 值被标 `NOT IMPLEMENTED`，仅做 unsupported stub 不实现。

  **Must NOT do**：❌ 不实现 `NOT IMPLEMENTED` 标记的 enum 值；❌ 不混入 ER / MSBS event kind；❌ 不缩减字段。

  **Recommended Agent Profile**：Category=`deep`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T35, T40；Blocked By=T7, T8。

  **References**：
  - Pattern：T16（MSBS 14 subtype）, T21（MSBE 12 subtype） —— 同款多 subtype 模式。
  - API：`Formats/MSB/MSBVI/EventParam.cs` L42-1000（EventParam 容器 + 7 concrete class）；上游 enum 区段标记的 NOT IMPLEMENTED 行。
  - Test：`tests/map/msbs/test_event_param_synthetic.c`（T16 完成后作为范本）。
  - WHY：AC6 event 实际可实现的只有 7 个 concrete 子类型；早期 Metis 报告里的「26」是 enum 值数量（含 NOT IMPLEMENTED）的误读。

  **Acceptance Criteria**：
  - [ ] 7 concrete event subtype 全实现 reader + writer。
  - [ ] enum 中 NOT IMPLEMENTED 值出现时 reader 返回 SF_ERR_UNSUPPORTED_VERSION。
  - [ ] `tests/map/msbvi/test_event_param_synthetic.c` 7 fixture round-trip PASS。

  **QA Scenarios**：
  ```
  Scenario: AC6 Event 7 subtype 字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture 在 test 文件中构造：7 个 event entry，每个一种 subtype（Treasure/Generator/MapOffset/PlatoonInfo/PatrolRoute/MapGimmick/Other）
      2. `cmake --build build-mingw --target souls_formats_test_msbvi_event_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbvi_event_param_synthetic --output-on-failure`
    Expected Result: 7 subtype round-trip 全 PASS
    Failure Indicators: 任一 subtype output ≠ input
    Evidence: .sisyphus/evidence/task-26-msbvi-event-rt.log + fixture bins

  Scenario: NOT IMPLEMENTED enum 值返回 unsupported
    Tool: Bash + Unity
    Preconditions: T26 完成
    Steps:
      1. fixture 中插入一个 NOT IMPLEMENTED 标记的 event kind 值
      2. 调用 sf_msbvi_read_from_memory
      3. assert 返回 SF_ERR_UNSUPPORTED_VERSION
    Expected Result: 明确错误码
    Failure Indicators: segfault；或静默通过
    Evidence: .sisyphus/evidence/task-26-msbvi-event-unsupported.log
  ```

  **Commit**: `phase5(msbvi): EventParam read+write (7 concrete subtypes, NOT IMPLEMENTED enum values as unsupported stub)`；files: `src/map/msbvi/event_param.c`, `tests/map/msbvi/test_event_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbvi_event_param` PASS.

- [x] 27. **`src/map/msbvi/point_param.c` read + write**

  **What to do**：实现 MSBVI PointParam（28 个 region subtype）对齐 `Formats/MSB/MSBVI/PointParam.cs`。

  **Must NOT do**：❌ 不混入 ER region kind；❌ Shape 子集按 AC6 实际使用限定。

  **Recommended Agent Profile**：Category=`deep`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T35, T40；Blocked By=T7, T8。

  **References**：T17, T22；`Formats/MSB/MSBVI/PointParam.cs`。WHY：与 T22 同款实现策略。

  **Acceptance Criteria**：
  - [ ] 28 region subtype 全实现 reader + writer。
  - [ ] `tests/map/msbvi/test_point_param_synthetic.c` ≥ 20 fixture round-trip cmp 0 PASS（剩余 ≤ 8 个 subtype 通过 e2e T40 覆盖）。

  **QA Scenarios**：
  ```
  Scenario: AC6 Point 28 subtype 中至少 20 个字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture：20 个 region entry，覆盖上游 MSBVI/PointParam.cs enum 中前 20 个 subtype，每个填 Name + Shape + ≥ 3 字段非默认值
      2. `cmake --build build-mingw --target souls_formats_test_msbvi_point_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbvi_point_param_synthetic --output-on-failure`
      4. `cmp fixture_input.bin fixture_output.bin`
    Expected Result: 20 subtype round-trip PASS；cmp 0
    Failure Indicators: 任一 subtype 字段不保留
    Evidence: .sisyphus/evidence/task-27-msbvi-point-rt.log + fixture bins

  Scenario: AC6 独有 region 字段（如 GroundAdjustOverride）保真
    Tool: Bash + Unity
    Preconditions: T27 完成
    Steps:
      1. fixture：region 子类 entry 带 AC6 独有字段（GroundAdjustOverride 或类似）
      2. `ctest --test-dir build-mingw -R msbvi_point_param_ac6_only --output-on-failure`
      3. 测试断言：写出后再读，AC6 独有字段值不变
    Expected Result: 字段保真
    Failure Indicators: 字段被默认化
    Evidence: .sisyphus/evidence/task-27-msbvi-ac6-only.log
  ```

  **Commit**: `phase5(msbvi): PointParam read+write (28 region subtypes)`；files: `src/map/msbvi/point_param.c`, `tests/map/msbvi/test_point_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbvi_point_param` PASS.

- [x] 28. **`src/map/msbvi/parts_param.c` read + write**

  **What to do**：实现 MSBVI PartsParam（**14 个 part subtype** —— 三 variant 中最多），对齐 `Formats/MSB/MSBVI/PartsParam.cs`。AC6 有大量 MA-specific（Mechanical Assembly）part kind。

  **Must NOT do**：❌ 不缩减字段；❌ 不限制 ER 中已有的 8 个 part kind（AC6 super-set 包含它们）；❌ 不引入 #ifdef。

  **Recommended Agent Profile**：Category=`deep`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T35, T40；Blocked By=T7, T8。

  **References**：
  - Pattern：T18, T23。
  - API：`Formats/MSB/MSBVI/PartsParam.cs`（14 inner class）。
  - WHY：AC6 part subtype 数量最多，工作量与 T23 同档。

  **Acceptance Criteria**：
  - [ ] 14 part subtype 全实现 reader + writer（MapPiece / Enemy / Player / Collision / DummyAsset / DummyEnemy / ConnectCollision / Asset / Object / Item / NPCWander / Protoboss / Navmesh / Invalid，对应上游 MSBVI/PartsParam.cs L1403-L3890）。
  - [ ] 每个 subtype 至少 90% 字段（含 unknown）映射。
  - [ ] `tests/map/msbvi/test_parts_param_synthetic.c` 14 fixture round-trip cmp 0 PASS。

  **QA Scenarios**：
  ```
  Scenario: AC6 Part 14 subtype 字节级 round-trip + 字段完整性
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture：14 个 part entry，每个一种 AC6 subtype（MapPiece/Enemy/Player/Collision/DummyAsset/DummyEnemy/ConnectCollision/Asset/Object/Item/NPCWander/Protoboss/Navmesh/Invalid），每个填 ≥ 20 字段非默认值
      2. `cmake --build build-mingw --target souls_formats_test_msbvi_parts_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbvi_parts_param_synthetic --output-on-failure`
      4. `cmp fixture_input.bin fixture_output.bin`
      5. `grep -c 'sf_msbvi_parts_param_field_' src/map/msbvi/parts_param.c`（应 ≥ 上游字段数 × 90%）
    Expected Result: 14 subtype 全 PASS；cmp 0；步骤 5 字段数 ≥ 90% 上游
    Failure Indicators: 任一 subtype 字段错位；字段统计不足
    Evidence: .sisyphus/evidence/task-28-msbvi-parts-rt.log + fixture bins + field count

  Scenario: AC6 MA-specific（Mechanical Assembly）独有字段保真
    Tool: Bash + Unity
    Preconditions: T28 完成
    Steps:
      1. fixture：Asset 或 Object 子类 entry 带 AC6 独有 MA 字段（对照 MSBVI/PartsParam.cs 的 AC6-only 字段集）
      2. `ctest --test-dir build-mingw -R msbvi_parts_param_ma_specific --output-on-failure`
      3. 测试断言：MA 独有字段值不变
    Expected Result: 字段保真
    Failure Indicators: 字段被覆盖
    Evidence: .sisyphus/evidence/task-28-msbvi-ma-parts.log
  ```

  **Commit**: `phase5(msbvi): PartsParam read+write (14 subtypes)`；files: `src/map/msbvi/parts_param.c`, `tests/map/msbvi/test_parts_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbvi_parts_param` PASS.

- [x] 29. **`src/map/msbvi/route_param.c` read + write**

  **What to do**：实现 MSBVI RouteParam 对齐 `Formats/MSB/MSBVI/RouteParam.cs`（112 行）。上游实测 **没有 subtype**，只有单一 `Route : NamedEntry` 类（字段：`Name` / `Unk08` / `Unk0C` 及若干 unknown int 字段）。结构最简单，对齐 MSBS RouteParam 的 2 subtype 模式但去掉 subtype 分支即可。

  **Must NOT do**：❌ 不假设 subtype；❌ 不动其他 variant；❌ 不引入 MSBS / MSBE Route 字段（AC6 Route 字段集不同）。

  **Recommended Agent Profile**：Category=`unspecified-high`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T35, T40；Blocked By=T7, T8。

  **References**：
  - Pattern：T19（MSBS RouteParam, 2 subtype）, T24（MSBE RouteParam, 3 subtype） —— 同款逐字段实现，但 MSBVI 是单类。
  - API：`Formats/MSB/MSBVI/RouteParam.cs:Route` class（含 Name + 多个 Unk int 字段）。
  - WHY：MSBVI Route 在三 variant 中字段最少、结构最简单；适合用作 Wave 2 的「校准」task。

  **Acceptance Criteria**：
  - [ ] 单一 `Route` 类型 reader + writer 实现。
  - [ ] Route 的所有 Unk 字段（包括 Unk08/Unk0C 等）逐一映射并 round-trip 保留。
  - [ ] `tests/map/msbvi/test_route_param_synthetic.c` round-trip PASS。

  **QA Scenarios**：
  ```
  Scenario: AC6 Route 字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture：3 个 Route entry，每个填 Name + 不同 Unk 字段
      2. `cmake --build build-mingw --target souls_formats_test_msbvi_route_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbvi_route_param_synthetic --output-on-failure`
    Expected Result: round-trip cmp 0
    Failure Indicators: Unk 字段被默认化（写出零）
    Evidence: .sisyphus/evidence/task-29-msbvi-route-rt.log + fixture bins

  Scenario: 空 RouteParam 段保留
    Tool: Bash + Unity
    Preconditions: T29 完成
    Steps:
      1. fixture：RouteParam 0 entries
      2. round-trip
      3. `cmp fixture_input.bin fixture_output.bin`
    Expected Result: cmp 0；空段头保留为 0 entries
    Evidence: .sisyphus/evidence/task-29-msbvi-route-empty.log
  ```

  **Commit**: `phase5(msbvi): RouteParam read+write (single Route class, no subtypes)`；files: `src/map/msbvi/route_param.c`, `tests/map/msbvi/test_route_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbvi_route_param` PASS.

- [x] 30. **`src/map/msbvi/layer_param.c` read + write（AC6 独有）**

  **What to do**：实现 MSBVI LayerParam 对齐 `Formats/MSB/MSBVI/LayerParam.cs`（114 行）。上游实测 **没有 subtype**，只有单一 `Layer : NamedEntry` 类（字段：`Name` / `Unk08` / `Unk10` / `Unk14`）。结构与 T29 Route 同款简单。layer 是 MSBE / MSBS 都没有的、AC6 独有的子表。

  **Must NOT do**：❌ 不在 sf_msb.h 公共头中暴露 layer 概念（其他 variant 无 layer）；❌ 不向后兼容到 MSBS / MSBE（layer 是 AC6 独有）；❌ 不假设 subtype。

  **Recommended Agent Profile**：Category=`unspecified-high`；Skills=无。

  **Parallelization**：YES；Wave 2；Blocks=T35, T40；Blocked By=T7, T8。

  **References**：
  - Pattern：T29（MSBVI RouteParam，单 NamedEntry 类同款）。
  - API：`Formats/MSB/MSBVI/LayerParam.cs:38-150` —— `LayerParam : Param<Layer>` 容器 + `Layer : NamedEntry` 单类（Name + Unk08/Unk10/Unk14）。
  - WHY：layer 是 AC6 独有特征，需要在 sf_msbvi.h 中通过 typed accessor 暴露但绝不渗透公共头 sf_msb.h。

  **Acceptance Criteria**：
  - [ ] LayerParam 容器 + 单 `Layer` 类型 reader + writer 实现。
  - [ ] `Name` + `Unk08` + `Unk10` + `Unk14` 4 个字段全映射并 round-trip 保留。
  - [ ] `tests/map/msbvi/test_layer_param_synthetic.c` round-trip PASS。
  - [ ] `grep 'layer' include/souls_formats/sf_msb.h include/souls_formats/sf_msbs.h include/souls_formats/sf_msbe.h` 0 命中。
  - [ ] `grep 'sf_msbvi_layer' include/souls_formats/sf_msbvi.h` ≥ 3 行（layer typed accessor 暴露）。

  **QA Scenarios**：
  ```
  Scenario: AC6 LayerParam 字节级 round-trip
    Tool: Bash + Unity
    Preconditions: T8 完成
    Steps:
      1. fixture：3 个 Layer entry，每个填 Name + 不同 Unk08/Unk10/Unk14
      2. `cmake --build build-mingw --target souls_formats_test_msbvi_layer_param_synthetic`
      3. `ctest --test-dir build-mingw -R msbvi_layer_param_synthetic --output-on-failure`
    Expected Result: cmp 0；三个 Unk 字段全保留
    Failure Indicators: 字段被默认化；或 Name 编码错误
    Evidence: .sisyphus/evidence/task-30-msbvi-layer-rt.log + fixture bins

  Scenario: 公共头未泄漏 layer 概念
    Tool: Bash
    Preconditions: T30 完成
    Steps:
      1. `grep -i 'layer' include/souls_formats/sf_msb.h include/souls_formats/sf_msbs.h include/souls_formats/sf_msbe.h | tee .sisyphus/evidence/task-30-public-leak.log`
    Expected Result: 文件输出空
    Failure Indicators: 命中
    Evidence: .sisyphus/evidence/task-30-public-leak.log

  Scenario: sf_msbvi.h 中 layer accessor 完整
    Tool: Bash
    Preconditions: T30 完成
    Steps:
      1. `grep -E '^SF_API.*sf_msbvi_layer' include/souls_formats/sf_msbvi.h | tee .sisyphus/evidence/task-30-layer-api.log`
    Expected Result: ≥ 3 行（count / get_by_index / Name accessor 等）
    Failure Indicators: < 3
    Evidence: .sisyphus/evidence/task-30-layer-api.log
  ```

  **Commit**: `phase5(msbvi): LayerParam (AC6-only sub-param, single Layer class)`；files: `src/map/msbvi/layer_param.c`, `include/souls_formats/sf_msbvi.h`（layer typed accessor）, `tests/map/msbvi/test_layer_param_synthetic.c`, CMakeLists.txt；Pre-commit: `ctest -R msbvi_layer_param` PASS.

#### ESD writer

- [x] 31. **`src/script/esd_write.c` writer（含 bytecode encode）**

  **What to do**：实现 ESD writer：
  - `sf_esd_write_to_memory(esd, out_bytes, out_size, alloc)`：把 in-memory `sf_esd_t` 序列化回二进制。
  - 反向调用 `esd_bytecode_encode(tree, out_bytes, out_size, alloc)`：把 T11 的操作数树编回字节流；OP_UNKNOWN 节点直接 emit `raw_bytes`。
  - reserve/fill pattern：所有 offset 字段用 `sf_binary_writer_reserve_*` + `_fill_*` 配对。

  **Must NOT do**：❌ 不在 writer 里改 ESD 内部结构（read-only consume）；❌ 不优化 bytecode（OP_UNKNOWN 必须 byte-by-byte 还原）；❌ 不允许任何 reserve_* 漏 fill。

  **Recommended Agent Profile**：Category=`deep`（writer 是 reader 的镜像 + bytecode 编码 + reserve/fill 严格配对）；Skills=无。

  **Parallelization**：NO（依赖 T10, T11）；Wave 2；Blocks=T32, T36；Blocked By=T10, T11。

  **References**：
  - Pattern：`src/script/emevd.c:write` 段（Phase 4 EMEVD writer）。
  - API：`Formats/ESD.cs:Write()`、`Condition.Write()`、`CommandCall.Write()`。
  - Test：`tests/script/test_emevd_write.c`（writer 测试范本）。
  - WHY：EMEVD writer 在 Phase 4 通过 F1-F4 APPROVE，reserve/fill 模式与本 task 完全同构。

  **Acceptance Criteria**：
  - [ ] `sf_esd_write_to_memory` 实现。
  - [ ] bytecode encoder 处理 known + OP_UNKNOWN。
  - [ ] `tests/script/test_esd_write.c` 通过合成 fixture write 后 byte-compare PASS。
  - [ ] `sf_binary_writer_finish` 不报 reserve_unfilled。

  **QA Scenarios**：
  ```
  Scenario: ESD round-trip byte-equal
    Tool: Bash + Unity
    Steps:
      1. fixture：2 group / 3 state / 2 condition
      2. write → bytes A
      3. read A → 同构 esd_t
      4. write 同构 esd_t → bytes B
      5. `cmp bytes_a.bin bytes_b.bin`
    Expected Result: cmp 退出码 0
    Evidence: .sisyphus/evidence/task-31-esd-rt.log + bytes_a.bin / bytes_b.bin

  Scenario: OP_UNKNOWN bytecode 节点保真
    Tool: Bash + Unity
    Steps: fixture 中 evaluator 含 OP_UNKNOWN + raw_bytes = [0xFF, 0xAB, 0xCD]
    Steps: read → write → assert evaluator byte stream 完全一致
    Expected Result: PASS
    Evidence: .sisyphus/evidence/task-31-unknown-roundtrip.log
  ```

  **Commit**: `phase5(esd): binary writer with bytecode encoder + OP_UNKNOWN preservation`；files: `src/script/esd_write.c`, `src/script/esd_internal.h`（扩展）, `tests/script/test_esd_write.c`, CMakeLists.txt；Pre-commit: `ctest -R 'esd_write|esd_read'` PASS.

### Wave 3 — Integration / e2e（9 路并行，Wave 2 全绿后启动）

- [x] 32. **合成 round-trip integration — ESD**

  **What to do**：合并 T10/T11/T31 的 reader + bytecode + writer，构造一个完整的复杂 fixture（5 group × 10 state × 3 condition × condition 含 evaluator bytecode 含 OP_UNKNOWN），跑端到端 write → read → write → byte-equal。注册 label `script`。

  **Must NOT do**：❌ 不引入新 ESD 字段；❌ 不在 integration test 里复制 unit test 已覆盖的边界。

  **Recommended Agent Profile**：Category=`quick`；Skills=无。

  **Parallelization**：YES；Wave 3；Blocks=Wave Final；Blocked By=T31。

  **References**：`tests/script/test_emevd_synthetic.c`（Phase 4 EMEVD integration test 范本）；T10/T11/T31 实现。WHY：integration test 是 Final Wave 之前最后一道结构化关卡。

  **Acceptance Criteria**：fixture write → read → write → cmp 字节级一致 PASS；`ctest -R esd_synthetic_integration` PASS。

  **QA Scenarios**：
  ```
  Scenario: ESD 复杂 fixture 双向 round-trip
    Tool: Bash + Unity
    Steps:
      1. 构造 fixture（5 group × 10 state × 3 cond）
      2. write A → read A → write A' → cmp A A'
    Expected Result: cmp 0
    Evidence: .sisyphus/evidence/task-32-esd-integration.log + bytes.bin × 2

  Scenario: 同 fixture 在 long 与 short format 都 round-trip
    Tool: Bash + Unity
    Steps: fixture 切换 LongFormat=true / false 各跑一次
    Expected Result: 两次都 PASS
    Evidence: .sisyphus/evidence/task-32-esd-long-short.log
  ```

  **Commit**: `phase5(esd): synthetic integration round-trip`；files: `tests/script/test_esd_synthetic.c`, CMakeLists.txt.

- [x] 33. **合成 round-trip integration — MSBS**

  **What to do**：组合 T15-T19，构造 MSBS 完整最小 fixture（每个 sub-param ≥ 1 entry），整 MSBS round-trip byte-equal。

  **Must NOT do**：❌ 不为 fixture 引入未在 T15-T19 中支持的 subtype。

  **Recommended Agent Profile**：Category=`quick`；Skills=无。

  **Parallelization**：YES；Wave 3；Blocks=Wave Final；Blocked By=T15-T19。

  **References**：T32 + 各 sub-param 测试；`Formats/MSB/MSBS/MSBS.cs:Write()`。WHY：integration 验证 5 个 sub-param 在 msb_common 框架中协同正确。

  **Acceptance Criteria**：MSBS 完整 round-trip cmp 0；ctest PASS。

  **QA Scenarios**：
  ```
  Scenario: MSBS 完整 round-trip
    Tool: Bash + Unity
    Steps: fixture: 1 model + 1 event + 1 region + 1 part + 1 route → write → read → write → cmp
    Expected Result: cmp 0
    Evidence: .sisyphus/evidence/task-33-msbs-integration.log

  Scenario: 跨 sub-param 引用一致
    Tool: Bash + Unity
    Steps: fixture: part 引用 model[0]、event 引用 part[0]；round-trip 后引用关系不变
    Expected Result: PASS
    Evidence: .sisyphus/evidence/task-33-msbs-xref.log
  ```

  **Commit**: `phase5(msbs): synthetic integration round-trip (5 sub-params)`；files: `tests/map/msbs/test_synthetic_integration.c`, CMakeLists.txt.

- [x] 34. **合成 round-trip integration — MSBE**

  **What to do**：组合 T20-T24，整 MSBE round-trip byte-equal。

  **Must NOT do**：❌ 不使用 Nightreign-specific fixture；❌ 不引入未在 T20-T24 支持的 subtype。

  **Recommended Agent Profile**：Category=`quick`；Skills=无。

  **Parallelization**：YES；Wave 3；Blocks=Wave Final；Blocked By=T20-T24。

  **References**：T33；`Formats/MSB/MSBE/MSBE.cs:Write()`。WHY：与 T33 同款。

  **Acceptance Criteria**：MSBE 完整 round-trip cmp 0；`ctest -R msbe_synthetic_integration` PASS。

  **QA Scenarios**：
  ```
  Scenario: MSBE 完整 round-trip 字节级一致
    Tool: Bash + Unity
    Preconditions: T20-T24 全部完成
    Steps:
      1. fixture 在测试代码中构造：1 model（MapPiece）+ 1 event（Generator）+ 1 region（InvasionPoint）+ 1 part（Asset 引用 model[0]）+ 1 route（MufflingPortalLink）
      2. `cmake --build build-mingw --target souls_formats_test_msbe_synthetic_integration`
      3. `ctest --test-dir build-mingw -R msbe_synthetic_integration --output-on-failure`
      4. 测试代码内：fixture → write A → read A → write A' → `cmp A A'`
    Expected Result: 步骤 3 退出码 0；步骤 4 内部 cmp 退出码 0
    Failure Indicators: 任一字段不保留；或字节流偏差
    Evidence: .sisyphus/evidence/task-34-msbe-integration.log + msbe_a.bin + msbe_aprime.bin

  Scenario: ER 独有 Asset 字段保真（含 AssetSfxParamRelativeIDs 数组）
    Tool: Bash + Unity
    Preconditions: T34 第一个 scenario 通过
    Steps:
      1. fixture：part 类型 = Asset，填 5 个 Asset 独有字段（含 AssetSfxParamRelativeIDs 5 元素数组）
      2. `ctest --test-dir build-mingw -R msbe_synthetic_asset --output-on-failure`
      3. 测试断言：round-trip 后 Asset 5 字段值 + 数组 5 元素值全保留
    Expected Result: 步骤 3 全 assert PASS
    Failure Indicators: 字段被默认化；或数组元素丢失
    Evidence: .sisyphus/evidence/task-34-msbe-asset.log
  ```

  **Commit**: `phase5(msbe): synthetic integration round-trip (5 sub-params)`；files: `tests/map/msbe/test_synthetic_integration.c`, CMakeLists.txt；Pre-commit: `ctest -R msbe_synthetic` PASS.

- [x] 35. **合成 round-trip integration — MSBVI**

  **What to do**：组合 T25-T30（含 LayerParam），整 MSBVI round-trip byte-equal。

  **Must NOT do**：❌ 不使用 ER-specific subtype。

  **Recommended Agent Profile**：Category=`quick`；Skills=无。

  **Parallelization**：YES；Wave 3；Blocks=Wave Final；Blocked By=T25-T30。

  **References**：T33；`Formats/MSB/MSBVI/MSBVI.cs:Write()`。

  **Acceptance Criteria**：MSBVI 完整 round-trip cmp 0；`ctest -R msbvi_synthetic_integration` PASS；fixture 必含至少 1 个 LayerParam entry。

  **QA Scenarios**：
  ```
  Scenario: MSBVI 完整 round-trip（含 layer）字节级一致
    Tool: Bash + Unity
    Preconditions: T25-T30 全部完成
    Steps:
      1. fixture 在测试代码中构造：1 model（MapPiece）+ 1 event（Generator）+ 1 region（InvasionPoint）+ 1 part（Asset）+ 1 route（单 Route 类，Name + Unk08/Unk0C）+ **1 layer（Name + Unk08/Unk10/Unk14）**
      2. `cmake --build build-mingw --target souls_formats_test_msbvi_synthetic_integration`
      3. `ctest --test-dir build-mingw -R msbvi_synthetic_integration --output-on-failure`
      4. 测试代码内：fixture → write A → read A → write A' → `cmp A A'`
    Expected Result: 步骤 3 退出码 0；步骤 4 内部 cmp 退出码 0；layer entry 字段全保留
    Failure Indicators: layer 字段被默认化；或字节流偏差
    Evidence: .sisyphus/evidence/task-35-msbvi-integration.log + msbvi_a.bin + msbvi_aprime.bin

  Scenario: 6 sub-param（含 layer）协同字段保真
    Tool: Bash + Unity
    Preconditions: T35 第一个 scenario 通过
    Steps:
      1. fixture：layer[0].Unk14 = 0xABCDEF；part[0].name 引用 layer[0].name 字符串
      2. `ctest --test-dir build-mingw -R msbvi_synthetic_xref --output-on-failure`
      3. 测试断言：round-trip 后 layer[0].Unk14 == 0xABCDEF；name 字符串一致
    Expected Result: 步骤 3 全 assert PASS
    Failure Indicators: layer Unk 字段丢失；或 name pool 被错误压缩
    Evidence: .sisyphus/evidence/task-35-msbvi-layer-xref.log
  ```

  **Commit**: `phase5(msbvi): synthetic integration round-trip (6 sub-params incl. layer)`；files: `tests/map/msbvi/test_synthetic_integration.c`, CMakeLists.txt；Pre-commit: `ctest -R msbvi_synthetic` PASS.

- [x] 36. **ESD e2e via ER `/script/talk/m10_00_00_00.talkesdbnd.dcx`**

  **What to do**：
  - 用 `er_extract_from_data0("/script/talk/m10_00_00_00.talkesdbnd.dcx")` 提取 BND4。
  - 解析 BND4 找到任一 `.esd` entry。
  - 调用 `sf_esd_read_from_memory` 解析。
  - 断言：`state_group_count > 0`；任一 state 的 condition count ≥ 1；bytecode 解码后树高 ≥ 1。
  - 二次 round-trip：把同一 ESD 写回 bytes → 再读 → 字段值一致（注意：byte-equal 不要求，因为字符串池可能差异；但语义级 round-trip 必过）。

  **Must NOT do**：❌ 不 SKIP；❌ 不接受 byte-equal 要求降级（语义 round-trip 必须 100% 字段一致）。

  **Recommended Agent Profile**：Category=`unspecified-high`（多服务集成 + 真实数据）；Skills=无。

  **Parallelization**：YES；Wave 3；Blocks=Wave Final；Blocked By=T10, T11, T31。

  **References**：
  - Pattern：`tests/param/test_fmg_e2e_er.c`、`tests/script/test_emevd_e2e_er.c`（Phase 4 e2e 范本）。
  - API：`tests/e2e/er_test_helper.h`（已有）+ T10/T11/T31。
  - WHY：talkesdbnd.dcx 是 ER ESD 主入口；m10_00_00_00 是 Stormveil（Limgrave 入口区），社区已知存在。

  **Acceptance Criteria**：
  - [ ] `tests/e2e/test_esd_e2e_er.c` 跑通 PASS。
  - [ ] 语义级 round-trip：所有 state / condition / command 字段值不变。
  - [ ] ctest label `e2e_er` 包含本 task；不允许 SKIP。

  **QA Scenarios**：
  ```
  Scenario: ER talkesdbnd 真实 ESD 解析 + 语义 round-trip
    Tool: Bash + Unity
    Preconditions: ER 数据就位；Phase 3 er_helper 可用
    Steps:
      1. `ctest -R esd_e2e_er --output-on-failure`
      2. 检查 evidence 中的 state count、condition count、bytecode tree depth 都非零
    Expected Result: PASS
    Failure Indicators: SKIP；任一字段不匹配
    Evidence: .sisyphus/evidence/task-36-esd-er.log

  Scenario: 字节级 round-trip 差异分析（容忍但记录）
    Tool: Bash + Unity
    Steps: 二次写出与原始字节 cmp；若差异，diff 位置须在「字符串池排序」范畴
    Expected Result: 差异限定在 string pool 段；不影响读取
    Evidence: .sisyphus/evidence/task-36-byte-diff.log
  ```

  **Commit**: `phase5(esd): ER e2e via talkesdbnd extraction`；files: `tests/e2e/test_esd_e2e_er.c`, CMakeLists.txt；Pre-commit: PASS（非 SKIP）.

- [x] 37. **MSBE e2e via ER `/map/mapstudio/m60_42_36_00.msb.dcx`**

  **What to do**：
  - 用 `er_extract_from_data0("/map/mapstudio/m60_42_36_00.msb.dcx")` 提取 MSB。
  - 调用 `sf_msbe_read_from_memory` 解析。
  - 断言：`part_count > 0` && `region_count > 0` && `event_count > 0` && `model_count > 0` && `route_count >= 0`；任一 part 的 transform 非全零；任一 region 的 shape kind 在已实现集合内。
  - 二次语义 round-trip。

  **Must NOT do**：❌ 不 SKIP；❌ 不对未知 shape kind 静默通过（必须 unsupported error 显式）。

  **Recommended Agent Profile**：Category=`unspecified-high`；Skills=无。

  **Parallelization**：YES；Wave 3；Blocks=Wave Final；Blocked By=T20-T24。

  **References**：
  - Pattern：`tests/script/test_emevd_e2e_er.c`、`tests/param/test_param_apply_paramdef_e2e.c`。
  - API：T20-T24 + er_test_helper。
  - WHY：m60_42_36_00 是 Limgrave 标志性地图，社区文档完善，可用作回归 baseline。

  **Acceptance Criteria**：
  - [ ] `tests/e2e/test_msbe_e2e_er.c` PASS。
  - [ ] 5 个 sub-param count 全非零（route 允许 0）。
  - [ ] ctest label `e2e_er`；不允许 SKIP。

  **QA Scenarios**：
  ```
  Scenario: ER mapstudio m60_42_36_00 真实 MSB
    Tool: Bash + Unity
    Preconditions: ER 数据就位
    Steps: `ctest -R msbe_e2e_er --output-on-failure`
    Expected Result: PASS
    Evidence: .sisyphus/evidence/task-37-msbe-er.log

  Scenario: 真实 ER MSB 中所有 region shape kind 都被识别
    Tool: Bash
    Steps: 测试中收集所有 region.shape_kind，输出到 evidence；不允许出现 SF_ERR_UNSUPPORTED_VERSION
    Expected Result: 所有 kind 在已实现集合内
    Evidence: .sisyphus/evidence/task-37-shape-coverage.log
  ```

  **Commit**: `phase5(msbe): ER e2e via Limgrave m60_42_36_00`；files: `tests/e2e/test_msbe_e2e_er.c`, CMakeLists.txt；Pre-commit: PASS.

- [x] 38. **MSBE e2e via Nightreign**

  **What to do**：
  - 用 `nightreign_extract_from_data0(任一 mapstudio MSB 路径)` 提取（T13 完成后实测一个真实 NR map path）。
  - 调用 `sf_msbe_read_from_memory` 解析（如 T4 probe 结论 = A）。
  - 断言：与 T37 同款（5 sub-param count 非零）。
  - 二次语义 round-trip。

  **Must NOT do**：❌ 不 SKIP；❌ 若 T4 probe 结论 = B/C，本 task 阻塞且 plan 需要修订（不要静默通过）。

  **Recommended Agent Profile**：Category=`unspecified-high`；Skills=无。

  **Parallelization**：YES；Wave 3；Blocks=Wave Final；Blocked By=T13, T20-T24, T4 (probe=A)。

  **References**：
  - Pattern：T37。
  - API：T13 + T20-T24。
  - WHY：Nightreign 与 ER 共用 MSBE 在上游零证据，T4 probe 决策档直接影响本 task 的执行；本 task 是 plan 中风险最高的 e2e。

  **Acceptance Criteria**：
  - [ ] `tests/e2e/test_msbe_e2e_nightreign.c` PASS。
  - [ ] T4 probe 决策档 = A 是前置条件；若 = B/C，本 task 阻塞 → 触发 plan 修订流程。

  **QA Scenarios**：
  ```
  Scenario: Nightreign mapstudio 真实 MSB
    Tool: Bash + Unity
    Preconditions: NR 数据就位；T4 probe = A
    Steps: `ctest -R msbe_e2e_nightreign --output-on-failure`
    Expected Result: PASS
    Failure Indicators: SKIP；任一字段不匹配；reader 返回 UNSUPPORTED_VERSION
    Evidence: .sisyphus/evidence/task-38-nightreign-msbe.log

  Scenario: probe 结论 = B/C 时本 task 阻塞
    Tool: Bash
    Steps: `grep 'DECISION: [BC]' .sisyphus/evidence/task-4-nightreign-probe.txt && echo "BLOCKED"`
    Expected Result: 命中 BLOCKED → 本 task 不应执行；plan 需修订
    Evidence: .sisyphus/evidence/task-38-gating.log
  ```

  **Commit**: `phase5(msbe): Nightreign e2e via mapstudio MSB`；files: `tests/e2e/test_msbe_e2e_nightreign.c`, CMakeLists.txt；Pre-commit: PASS.

- [x] 39. **MSBS e2e via Sekiro**

  **What to do**：
  - 用 T12 sekiro_helper 提取 Sekiro `/map/mapstudio/m??.msb.dcx`（实测时选第一个存在的）。
  - 调用 `sf_msbs_read_from_memory` 解析。
  - 断言：与 T37 同款（5 sub-param count 非零）+ shape kind 在 MSBS 支持集合内。
  - 二次语义 round-trip。

  **Must NOT do**：❌ 不 SKIP；❌ 不混用 MSBE reader。

  **Recommended Agent Profile**：Category=`unspecified-high`；Skills=无。

  **Parallelization**：YES；Wave 3；Blocks=Wave Final；Blocked By=T12, T15-T19。

  **References**：
  - Pattern：T37。
  - API：T12 + T15-T19。
  - WHY：Sekiro 是 v1 用户明确指定的第三款游戏，e2e 必过保证 MSBS 实现正确。

  **Acceptance Criteria**：
  - [ ] `tests/e2e/test_msbs_e2e_sekiro.c` PASS。
  - [ ] Sekiro mapstudio 任一 MSB count 全非零。

  **QA Scenarios**：
  ```
  Scenario: Sekiro mapstudio 真实 MSB
    Tool: Bash + Unity
    Preconditions: Sekiro 数据就位
    Steps: `ctest -R msbs_e2e_sekiro --output-on-failure`
    Expected Result: PASS
    Evidence: .sisyphus/evidence/task-39-msbs-sekiro.log

  Scenario: Sekiro MSB 所有 shape kind 已实现
    Tool: Bash
    Steps: 收集 region.shape_kind 直方图，输出到 evidence
    Expected Result: 全部 kind 在 T17 实现的子集内
    Evidence: .sisyphus/evidence/task-39-sekiro-shape-coverage.log
  ```

  **Commit**: `phase5(msbs): Sekiro e2e via mapstudio MSB`；files: `tests/e2e/test_msbs_e2e_sekiro.c`, CMakeLists.txt；Pre-commit: PASS.

- [x] 40. **MSBVI e2e via AC6**

  **What to do**：
  - 用 T14 ac6_helper 提取 AC6 `/map/mapstudio/m??.msb.dcx`（实测时选第一个存在）。
  - 调用 `sf_msbvi_read_from_memory` 解析。
  - 断言：6 sub-param count 全非零（包括 layer）。
  - 二次语义 round-trip。

  **Must NOT do**：❌ 不 SKIP；❌ 不在 AC6 数据未就位时静默通过（T14 gate 强制 fail）。

  **Recommended Agent Profile**：Category=`unspecified-high`；Skills=无。

  **Parallelization**：YES；Wave 3；Blocks=Wave Final；Blocked By=T14, T25-T30；**用户 AC6 数据必须就位**。

  **References**：
  - Pattern：T37, T39。
  - API：T14 + T25-T30。
  - WHY：AC6 是 v1 4 游戏中最后到位、风险最高的一个；e2e 必过保证 MSBVI 实现稳。

  **Acceptance Criteria**：
  - [ ] `tests/e2e/test_msbvi_e2e_ac6.c` PASS。
  - [ ] LayerParam count > 0（AC6 实际地图都用 layer）。

  **QA Scenarios**：
  ```
  Scenario: AC6 mapstudio 真实 MSB
    Tool: Bash + Unity
    Preconditions: AC6 数据就位
    Steps: `ctest -R msbvi_e2e_ac6 --output-on-failure`
    Expected Result: PASS
    Failure Indicators: SKIP；reader 报 UNSUPPORTED；LayerParam count = 0
    Evidence: .sisyphus/evidence/task-40-msbvi-ac6.log

  Scenario: AC6 字段差异性回归
    Tool: Bash + Unity
    Steps: 取 ≥ 3 个不同 AC6 mapstudio MSB，跑 reader，evidence 中输出每个的 sub-param count 直方图
    Expected Result: 3 MSB 都 PASS；count 直方图记录
    Evidence: .sisyphus/evidence/task-40-ac6-coverage.log
  ```

  **Commit**: `phase5(msbvi): AC6 e2e via mapstudio MSB`；files: `tests/e2e/test_msbvi_e2e_ac6.c`, CMakeLists.txt；Pre-commit: PASS.

### Wave 4 — 文档收尾（3 路并行，Wave 3 全绿后启动）

- [x] 41. **PLAN.md §1 表格 final pass + Phase 5 章节 checkbox 全勾**

  **What to do**：
  - 把 PLAN.md §1 表格 Phase 5 行从 `🚧 in progress` 改为 `✅ done`，Tests 列填具体通过数（按 ctest 实测 N/N PASS）。
  - 把 PLAN.md §X Phase 5 详细章节（L608-635 附近）所有 `- [ ]` checkbox 改为 `- [x]`。
  - 在表格下方加一行 Phase 5 完成纪要（一句话指向本 plan + git commits）。

  **Must NOT do**：❌ 不动 Phase 6/7 内容。

  **Recommended Agent Profile**：Category=`writing`；Skills=`tech-doc-style-chinese`。

  **Parallelization**：YES；Wave 4；Blocks=Wave Final；Blocked By=Wave 3 全绿。

  **References**：T2 与 PLAN.md §1。WHY：状态表是项目唯一对外契约的一部分，必须最终对齐。

  **Acceptance Criteria**：
  - [ ] `grep '✅ done' .sisyphus/plans/PLAN.md` ≥ 6（Phase 0-5）。
  - [ ] PLAN.md §Phase 5 章节 0 个未勾 checkbox。

  **QA Scenarios**：
  ```
  Scenario: PLAN.md 一致性
    Tool: Bash
    Steps: `grep -c '- \[ \]' .sisyphus/plans/PLAN.md`（Phase 5 章节范围）
    Expected Result: 0
    Evidence: .sisyphus/evidence/task-41-plan-checkbox.log

  Scenario: Phase 5 测试数填实
    Tool: Bash
    Steps: `grep -A 1 'Phase 5' .sisyphus/plans/PLAN.md | grep '✅ done.*[0-9]\+/[0-9]\+'`
    Expected Result: 命中
    Evidence: .sisyphus/evidence/task-41-tests-count.log
  ```

  **Commit**: `phase5(docs): mark Phase 5 done in PLAN.md status table`；files: `.sisyphus/plans/PLAN.md`.

- [x] 42. **5 份 api-mapping md 全量刷新 + coverage check 报告**

  **What to do**：
  - 逐一刷新 `docs/api-mapping/format-{esd,msb-common,msbs,msbe,msbvi}.md`：
    - 所有 mapping row 的 Status 列从「未实现」/「TODO」改为「✓ aligned」（确实实现的）或「+ extension」（折叠到 sf_param_apply_mode 那种 C 风格调整）或「_skipped_」（标注推迟到 v1.1 或 OUT of scope）。
    - 增补 row：上游有但前期 mapping 漏掉的（如 ESD 的 inner classes 等）。
  - 写一个 `.sisyphus/evidence/phase5-mapping-coverage.md`：列出每份 mapping doc 的 row 总数、各 Status 分布，确认无「未实现」剩余。

  **Must NOT do**：❌ 不改 PARAM / FMG / EMEVD / BND 等 Phase 0-4 的 mapping。

  **Recommended Agent Profile**：Category=`writing`；Skills=无。

  **Parallelization**：YES；Wave 4；Blocks=Wave Final；Blocked By=Wave 3 全绿。

  **References**：5 份 api-mapping md 自身 + 上游 .cs 行级对照。WHY：mapping coverage 是 plan「Definition of Done」之一。

  **Acceptance Criteria**：
  - [ ] `grep -l '未实现' docs/api-mapping/format-{esd,msb-common,msbs,msbe,msbvi}.md` 空。
  - [ ] coverage 报告输出 5 份 doc 的 row 数与 Status 分布。

  **QA Scenarios**：
  ```
  Scenario: mapping doc 未实现零剩余
    Tool: Bash
    Steps: `grep -c '未实现' docs/api-mapping/format-{esd,msb-common,msbs,msbe,msbvi}.md`
    Expected Result: 全 0
    Evidence: .sisyphus/evidence/task-42-mapping-coverage.log

  Scenario: coverage 报告自洽
    Tool: Bash
    Steps: 把 .sisyphus/evidence/phase5-mapping-coverage.md 与 mapping doc 真实 row 数对比
    Expected Result: 一致
    Evidence: .sisyphus/evidence/task-42-report-consistency.log
  ```

  **Commit**: `phase5(docs): refresh 5 api-mapping md to ✓ aligned`；files: `docs/api-mapping/format-{esd,msb-common,msbs,msbe,msbvi}.md`, `.sisyphus/evidence/phase5-mapping-coverage.md`.

- [x] 43. **`docs/roadmap/phase-5-script-map.md` 与本 plan 收尾对齐**

  **What to do**：
  - 把 roadmap 中所有 deliverable 段标记为 done（用 `- [x]`）。
  - 「Exit criteria」段勾选全部。
  - 末尾追加一段「Completion Notes」：指向本 plan、F1-F4 evidence、用户最终 okay 时间戳。
  - 更新 `docs/roadmap/README.md` Phase 5 行状态 = ✅ done，Tests 列填实测数。

  **Must NOT do**：❌ 不动 Phase 6 / 7 doc。

  **Recommended Agent Profile**：Category=`writing`；Skills=无。

  **Parallelization**：YES；Wave 4；Blocks=Wave Final；Blocked By=Wave 3 全绿。

  **References**：T6（Wave 0 的 roadmap 同步）+ 本 plan。WHY：roadmap 是 Phase doc 入口，必须与 PLAN.md 状态一致。

  **Acceptance Criteria**：
  - [ ] `docs/roadmap/phase-5-script-map.md` 全部 checkbox 勾上。
  - [ ] `docs/roadmap/README.md` Phase 5 行 = ✅ done。
  - [ ] Completion Notes 段含 N/N tests PASS 数。

  **QA Scenarios**：
  ```
  Scenario: roadmap checkbox 全勾
    Tool: Bash
    Steps: `grep -c '- \[ \]' docs/roadmap/phase-5-script-map.md`
    Expected Result: 0
    Evidence: .sisyphus/evidence/task-43-roadmap.log

  Scenario: README phase 表对齐
    Tool: Bash
    Steps: `grep 'Phase 5' docs/roadmap/README.md | grep '✅ done'`
    Expected Result: 命中
    Evidence: .sisyphus/evidence/task-43-readme-table.log
  ```

  **Commit**: `phase5(docs): mark phase-5 roadmap done + completion notes`；files: `docs/roadmap/phase-5-script-map.md`, `docs/roadmap/README.md`.

---

## Final Verification Wave（MANDATORY — 全部实施 task 完成后启动）

> 4 个 reviewer agent 并行执行。任一 REJECT 都阻塞用户验收；全 APPROVE 后向用户展示汇总，**等待用户显式 okay 才标记 Phase 5 完成**。

- [x] **F1. 计划合规审计** —— `oracle`
  通读本 plan + `.sisyphus/plans/PLAN.md` Phase 5 章节；逐条比对「Must Have」与代码现实（read file + run commands）；逐条搜「Must NOT Have」在 codebase 是否出现禁用 pattern。检查 `.sisyphus/evidence/` 是否每个 task 都有证据文件；对照交付物清单与 git diff。
  Output：`Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | Evidence [N/N] | VERDICT: APPROVE/REJECT`

- [x] **F2. 代码质量审查** —— `unspecified-high`
  跨编译目标矩阵跑 `cmake --build build-{mingw,msvc,asan}` + `ctest -L 'script|map|e2e_*'`；review 所有 Wave 0-3 改动文件，扫 `as any` / `@ts-ignore` 类标记、空 catch、commented-out code、未使用 import、AI slop（过度注释、过度抽象、`data`/`result`/`item`/`temp` 之类的泛名）；验证所有公共符号 `SF_API` 装饰、所有 enum 有 `_Static_assert`。
  Output：`Build [PASS/FAIL × 3 targets] | Tests [N pass/N fail per label] | Files [N clean/N issues] | VERDICT`

- [x] **F3. Real Manual QA** —— `unspecified-high`
  干净状态启动（删 build dir 重 configure）；逐条执行 Wave 0-4 每个 task 的 QA scenarios；跑 4 款游戏的 e2e 矩阵实测；跨 task 集成测试（如 MSBE round-trip 完后用 `er_extract_from_data0` 取真实 ER MSB 喂回去）；测边界（空 list、未知 subtype、损坏 header）。证据存 `.sisyphus/evidence/final-qa/`。
  Output：`Scenarios [N/N pass] | E2E Matrix [4/4 games] | Integration [N/N] | Edge Cases [N tested] | VERDICT`

- [x] **F4. Scope Fidelity Check** —— `deep`
  对每个 task：读「What to do」与「Must NOT do」、读实际 git diff（`git log --since=phase5-start`）；逐条核对 1:1—— spec 内的都做了且没多做；扫跨 task 污染（如 T22 改了 T20 范围的文件）；列出所有未在 plan 内描述的变更（unaccounted changes）。
  Output：`Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

- 每个 task 一个 commit；commit message 前缀 `phase5({scope}):`，scope 取 `bug-cleanup` / `msb-common` / `msbs` / `msbe` / `msbvi` / `esd` / `helpers` / `tests` / `docs` 之一。
- 实施 + 单元测试 + 文档更新在**同一个 commit**（不允许跨 commit 拆开）。
- 每个 commit 必须 `-Werror` 全编译矩阵绿；不允许带 warning 入库。
- Wave 切换处（W0→W1, W1→W2, ...）单独一个 `phase5(checkpoint): wave-N complete` commit，由 Sisyphus 自动生成。

---

## Success Criteria

### Verification Commands

```bash
# 1. 编译矩阵全绿
cmake --build build-mingw 2>&1 | tee .sisyphus/evidence/final-build.log
ctest --test-dir build-mingw -L 'script|map' --output-on-failure
ctest --test-dir build-mingw -L 'e2e_er|e2e_sekiro|e2e_nightreign|e2e_ac6' --output-on-failure

# 2. include 路径污染零命中
grep -rn '"/home/' include/ src/ tests/ && echo "FAIL: absolute paths" || echo "PASS"

# 3. api-mapping 覆盖率
grep -c "未实现" docs/api-mapping/format-{esd,msb-common,msbs,msbe,msbvi}.md
# 期望：全 0

# 4. 公共符号前缀
nm build-mingw/libsouls_formats.a | grep ' T ' | awk '{print $3}' | grep -v '^sf_' && echo "FAIL: missing sf_ prefix" || echo "PASS"
```

### 最终 checklist

- [ ] 所有 Must Have 项目可在 codebase 找到对应实现。
- [ ] 所有 Must NOT Have 在 codebase 中不出现。
- [ ] `ctest -L 'script|map'` 全绿。
- [ ] 四款游戏 e2e 矩阵全绿（**不允许 SKIP**）。
- [ ] Phase 4 遗留 6 处绝对路径 include 全部清除 + grep guard 测试稳定。
- [ ] PLAN.md §1 表格 Phase 4 ✅ / Phase 5 ✅。
- [ ] F1-F4 全 APPROVE。
- [ ] 用户最终显式 okay。
