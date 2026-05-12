# Phase 7 — 动画与特效（TAE + FXR3）

> **状态**：⏳ 待执行（v1.1 默认启用；v1.0 通过 `SF_ENABLE_PHASE7=OFF` 默认关闭） ·
> **预估**：~2 周 · **依赖**：Phase 1-6 全部完成（Phase 6 ✅ 2026-05-12）
>
> **策略**：先 Wave 0 切 Phase 6 状态 + 锁定 OUT-of-scope（Template subsystem / TAE legacy formats / AC6 TAE TBD）+ 跑 2 个 probe；Wave 1 落 2 个公共头 + CMake `SF_ENABLE_PHASE7` 接线 + `src/effects/` 骨架；Wave 2 单线 TAE（4 tasks）；Wave 3 5 路并行 FXR3 binary 子模块（StateMap / StateCondition / Field / Container-Effect-Action / Top-level writer）+ 合成 fixture；Wave 4 3 路 FXR3 XML（read / write / structural-equal test）；Wave 5 2 路 ER e2e；Wave 6 docs final pass；Wave Final 4 reviewer 并行。
>
> **绑定**：本计划严格遵守 [AGENTS.md](../../AGENTS.md) §5.x「STRICT UPSTREAM REFERENCE / API MIRRORS UPSTREAM」。
> 上游锁定提交：见 [`docs/api-mapping/UPSTREAM.md`](../../docs/api-mapping/UPSTREAM.md)（Phase 6 已 pin）。

---

## TL;DR

> **核心目标**：实现 TAE（SDT 格式，version 0x1000D，覆盖 Sekiro / Elden Ring / 可能 Nightreign）的动画事件时间线读 + 写；实现 FXR3（DS3 + Sekiro 版本，ER/AC6/Nightreign 复用 Sekiro 版本）粒子特效格式的**二进制 + XML 双向**读 + 写。整阶段通过 `SF_ENABLE_PHASE7=ON` 启用；OFF 时所有 Phase 7 source / test 不进 build（CMakeLists.txt:15 option 已存在）。
>
> **交付物**：
> - `include/souls_formats/sf_tae.h` + `src/effects/tae.c`：TAE 完整读写（SDT 格式；ParameterContainer 透传 opaque bytes）。
> - `include/souls_formats/sf_fxr3.h` + `src/effects/fxr3.c`：FXR3 二进制读 + 写；含 ConditionOperand 4 变体 + Field 2 变体 tagged union。
> - `src/effects/fxr3_xml_read.c` + `src/effects/fxr3_xml_write.c`：FXR3 XML 双向（mxml DOM）。
> - 单元测试：合成 TAE round-trip、合成 FXR3 binary round-trip、FXR3 XML structural-equal。
> - e2e 测试：c0000.anibnd.dcx → .tae 与 sfxbnd_commoneffects.ffxbnd.dcx → .fxr。
> - 文档：2 份 api-mapping md（format-tae.md / format-fxr3.md）全量刷新；extensions.md 增补 4 条新 entry；CHANGELOG.md 增 Phase 7 章；PLAN.md / AGENTS.md / roadmap 状态切换。
>
> **预估工作量**：~2 周（上游 in-scope ~4475 LOC C# → C；其中 TAE Template 801 LOC 推 v1.2 不计入）。
> **并行执行**：是 —— 7 个 wave（Wave 0-6 + Final），最宽 wave 6 个并行 task（Wave 3 — FXR3 binary 子模块矩阵）。**总计 28 个实施 task + 4 个 Final reviewer = 32 个 task-level checkbox**。
> **关键路径**：T7 (sf_tae.h) / T8 (sf_fxr3.h) → T9 (CMake 接线) → T10 (effects/ 骨架) → T15 (fxr3.c top-level) → T18 (Container/Effect/Action 递归) → T19 (writer) → T21 (XML read) → T22 (XML write) → T25 (FXR3 e2e) → F1-F4 → 用户验收。

---

## Context

### 原始请求

> 「编写阶段7的计划」

### 当前项目状态（2026-05-12）

- Phase 0-6 ✅ 已完成（4 + 5 + 13 + 32 + 20 + 38 + 15 测试 PASS；详 PLAN.md §7）。
- Phase 7 ⏳ pending —— 本 plan 范围。
- v1.0 GA：v1 后续是否在 v1.0 ship Phase 7 由用户决定（默认推 v1.1）；CMake option 已就位，零成本切换。

### 内部审查结论（Metis 备用：上游 agent 链路当日故障，由 Prometheus 主体直接做 gap 审计）

| 项 | 决策 |
|---|---|
| **TAE 格式覆盖** | v1.1 仅实现 **SDT** 格式（version `0x1000D`，覆盖 **Sekiro + Elden Ring + 可能 Nightreign**）。上游 `TAE.cs:206` 显示该 version code 对应 SDT/ER。**DS1 / SOTFS / DS3 / BB / DES / DESR 全部 `_skipped_`**（legacy games，v2 处理）。**AC6 TAE 暂列 TBD**（上游 `TAEFormat` enum 无 AC6 项；T4 probe 若发现 AC6 .tae 使用 0x1000D 即自动支持，否则推 v1.2）。 |
| **TAE Template subsystem**（801 LOC `Template.cs`） | **v1.1 OUT-of-scope**。`ApplyTemplate` / `BankTemplate` / `EventTemplate` / `ParameterTemplate` / `ParamType` 全部不实现。Event 参数维持上游 `byte[]`，C 端透传为 opaque `uint8_t*` + `size`。Typed parameter access 推 v1.2。 |
| **TAE EventGroup**（205 LOC `EventGroup.cs`） | **v1.1 IN-scope**（结构性元素，无 Template 也能 round-trip）。作为 opaque 包含 event index list 实现。 |
| **TAE AnimMiniHeader 多态**（Standard / ImportOtherAnim） | **v1.1 IN-scope**。使用 tagged union（`sf_tae_anim_mini_header_type_t` + 2-variant payload）。 |
| **FXR3 polymorphism** | 4 个 ConditionOperand 子类（Literal / External / StateTime / UnkMinus2）+ 2 个 Field 子类（FieldInt / FieldFloat），C 端用 **tagged union**：`sf_fxr3_operand_t` 与 `sf_fxr3_field_t` 各含 `_type` 字段 + variant payload。 |
| **FXR3 Field 类型启发式判定** | 上游 `FXR3.cs:1140-1146` 用 value-range 启发式（绝对值落在 [1e-4, 1e6) 视为 float 否则 int）。**逐字 mirror**，不优化。这是上游已知 TODO；C 端不擅自改进。 |
| **FXR3 Sekiro/ER 独有 sections**（11-14：ReferenceList / ExternalValueList / UnkBloodEnabler / UnkEmpty 哨兵） | 仅 `Version == Sekiro (5)` 时读/写。DS3 (`Version == 4`) 直接跳过。**ER + Nightreign + AC6 实测全部走 Sekiro 路径**（FXRVersion = 5，仅 PropertyInterpolationType 多出 `UnkAc6 = 7`）。 |
| **FXR3 XML 序列化** | 上游 `FXR3EnhancedSerialization` 通过 .NET `XmlSerializer` + class-level `[XmlType]` / `[XmlElement]` / `[XmlAttribute]` / `[XmlInclude]` 属性自动生成 XML schema。C 端用 **mxml** 手动 mirror schema。**测试不走 byte-equal raw XML**（whitespace / attribute 顺序不稳定），改用 **structural in-memory equality**（XML write → re-read → field-by-field 对比）。 |
| **FXR3 XML allocator** | mxml 不支持 thread-local allocator；**v1.1 决策**：mxml 使用其默认 `malloc/free`，C 端 `sf_allocator_t` 仅控制 fxr3 对象本身的分配；XML 字符串内部用 mxml 自带管理。记入 extensions.md。 |
| **DCX 外壳** | `.anibnd.dcx` 与 `.ffxbnd.dcx` 都是 DCX_KRAK wrapped BND4。Phase 3 `er_extract_from_data0` 仅打开 Data0；**c0000.anibnd.dcx 在生产 ER 中位于 Data3**（Phase 6 T4 probe 确认），sfxbnd 位置待 T5 probe 验证。Phase 7 e2e 模式：先试 `er_extract_from_data0`，失败 fallback 到手动 Data0..Data3 迭代（沿用 Phase 6 `tests/geom/test_flver2_e2e_er.c` 模式），或新增通用 `er_extract` helper（可选 sub-step）。 |
| **CMake gating** | `SF_ENABLE_PHASE7` option 已在 `CMakeLists.txt:15` 就位（默认 `OFF`）。本 plan 仅需在 `SF_SOURCES` / `SF_PUBLIC_HEADERS` 中加 `if(SF_ENABLE_PHASE7) ... endif()` 块。 |
| **Test label** | `anim`（合成 fixture）+ `e2e_er`（真实 ER 数据 e2e）。沿用 Phase 6 标签命名。 |
| **CHANGELOG.md** | 增补 Phase 7 章节，列举 2 个新公共头 + 4 个新源文件 + 5 个新测试 binary + 4 个 extension entry。 |

### 上游 .cs 文件清点（in-scope ~3674 LOC，不含 Template 801 LOC）

| 模块 | 文件 | LOC | 范围 |
|---|---|---|---|
| TAE 顶层 | `Formats/TAE/TAE.cs` | 700 | IN-scope（仅 SDT 分支，其他 format 路径 `_skipped_`） |
| TAE Animation + MiniHeader | `Formats/TAE/Animation.cs` | 720 | IN-scope（含 Standard / ImportOtherAnim tagged union） |
| TAE Event + ParameterContainer | `Formats/TAE/Event.cs` | 549 | IN-scope（ParameterContainer 作 opaque bytes） |
| TAE EventGroup | `Formats/TAE/EventGroup.cs` | 205 | IN-scope |
| TAE Template | `Formats/TAE/Template.cs` | 801 | **OUT-of-scope**（v1.2） |
| FXR3 | `Formats/FXR3.cs` | 1500 | IN-scope（单文件含 binary + XML serialization） |
| **TAE in-scope 小计** | | **2174** | |
| **FXR3 in-scope 小计** | | **1500** | |
| **Phase 7 in-scope 总计** | | **3674** | |

### Mapping doc 现状（2 份，41 mapping row，全部 `未实现`）

| Doc | Rows | 备注 |
|---|---|---|
| `format-tae.md` | 14 | 含 5 个 Template 相关 row → Wave 6 将其标 `_skipped_` |
| `format-fxr3.md` | 17 | 全部 in-scope |

---

## Work Objectives

### Core Objective

实现 TAE（SDT 格式）+ FXR3（DS3+Sekiro 二进制，含 ER/AC6/Nightreign 复用 Sekiro 版本）的双向读写；为 FXR3 提供 mxml-based XML round-trip；通过 `SF_ENABLE_PHASE7=ON` 启用整阶段；e2e 验证 Elden Ring c0000.anibnd.dcx + sfxbnd_commoneffects.ffxbnd.dcx 完整加载。

### Concrete Deliverables

**公共头**（2 个）：
- `include/souls_formats/sf_tae.h`
- `include/souls_formats/sf_fxr3.h`
- `umbrella` 头 `souls_formats.h` 同步含入（**无条件 include**；公共头始终声明 API，未 link 含 Phase 7 实现的库 → unresolved external 链接错）

**源码**（4 个）：
- `src/effects/tae.c`（top-level + Animation list + Event list + EventGroup + MiniHeader tagged union + ParameterContainer opaque）
- `src/effects/fxr3.c`（top-level + section table + StateMap/State/StateCondition/ConditionOperand tagged union + Container/Effect/Action 递归 + Property/PropertyModifier/UnkFieldList + Field tagged union + writer）
- `src/effects/fxr3_xml_read.c`（mxml DOM → fxr3 对象图）
- `src/effects/fxr3_xml_write.c`（fxr3 对象图 → mxml DOM → UTF-8 string）

**单元 / e2e 测试**（5 个）：
- `tests/anim/test_tae_synthetic.c`（1 anim × 1 event SDT 最小 fixture round-trip）
- `tests/anim/test_fxr3_synthetic.c`（最小 FXR3 binary round-trip + tagged union 全覆盖：2 个 Field 变体 × 4 个 Operand 变体）
- `tests/anim/test_fxr3_xml.c`（FXR3 → XML → FXR3 structural equality）
- `tests/anim/test_tae_e2e_er.c`（c0000.anibnd.dcx → BND4 → .tae）
- `tests/anim/test_fxr3_e2e_er.c`（sfxbnd_commoneffects.ffxbnd.dcx → BND4 → .fxr）

**文档**：
- `docs/api-mapping/format-tae.md` 全量刷新（14 行 → 9 in-scope 全 status ≠ "未实现"；5 Template-related row → `_skipped_`）
- `docs/api-mapping/format-fxr3.md` 全量刷新（17 行 → 100% 已完成）
- `docs/api-mapping/extensions.md` 增补 4 条新 entry：
  1. TAE Template subsystem deferral
  2. mxml default allocator（无 `sf_allocator_t` override）
  3. FXR3 XML round-trip equivalence policy（structural, not byte）
  4. TAE format coverage limited to SDT in v1.1
- `docs/roadmap/phase-7-animation-effects.md` 任务清单 + 状态行 sync 本 plan
- `docs/roadmap/README.md` Phase 7 行 `⏳ pending` → `🚧 in progress` → `✅ done`
- `AGENTS.md` §2 Current status 表 Phase 7 行更新
- `.sisyphus/plans/PLAN.md` §7 Phase 7 章节 checkbox 全勾 + §2.3 v1.x 路线图同步
- `CHANGELOG.md` 增 v0.4.0（或 next）章节列举 Phase 7 deliverables

### Definition of Done

- [ ] `cmake -B build-mingw -DSF_ENABLE_PHASE7=ON ...` 配置无 warning；`-DSF_ENABLE_PHASE7=OFF` 也无 warning（条件块语义正确）。
- [ ] `cmake --build build-mingw` 三种工具链（MSVC / clang-cl / MinGW-w64）全绿，`-Werror` 通过。
- [ ] `ctest --test-dir build-mingw -L anim --output-on-failure` 全绿（合成 fixture + XML round-trip）。
- [ ] `ctest --test-dir build-mingw -L 'e2e_er'` Phase 7 部分全绿（TAE + FXR3 ER e2e，**不允许 SKIP**——ER 副本已就位）。
- [ ] `ctest -L anim` 在 `SF_ENABLE_PHASE7=OFF` 时 0 tests run（条件 build 验证）。
- [ ] `docs/api-mapping/format-{tae,fxr3}.md` in-scope 行 status 全 ≠ "未实现"；Template-related 行 status = `_skipped_`。
- [ ] `docs/api-mapping/extensions.md` 含 4 条新增 entry（Template defer / mxml allocator / XML equivalence policy / TAE SDT-only）。
- [ ] AGENTS.md §2 表 Phase 7 行 = `✅ done` + 实测 `M/M PASS across N test binaries`。
- [ ] PLAN.md §7 Phase 7 章节 0 个未勾 checkbox。
- [ ] docs/roadmap/README.md Phase 7 行 = `✅ done`。
- [ ] CHANGELOG.md 含 Phase 7 deliverables 章节。
- [ ] F1-F4 全部 APPROVE，用户最终 okay。

### Must Have

- 上游 in-scope (~3674 LOC) 全部类 + 字段 + 公共方法均有 C 等价；TAE SDT 分支与 FXR3 完整对齐顺序与字段次序逐位 round-trip。
- TAE Animation / Event / EventGroup / AnimMiniHeader 全部支持读 + 写；ParameterContainer 作 opaque bytes 透传。
- FXR3 二进制 round-trip 字节级一致（合成 fixture）；ER 真实 .fxr e2e read 通过（结构性断言：node_count > 0 + 至少一个 Container 有 Effect 子节点）。
- FXR3 XML write → re-read → structural equality（4 个 Operand 变体 + 2 个 Field 变体 + 4 个 PropertyType 值 + 8 个 PropertyInterpolationType 值全部覆盖）。
- FXR3 Sekiro version sections 11-14（ReferenceList / ExternalValueList / UnkBloodEnabler / UnkEmpty 哨兵）正确读 + 写；DS3 version 跳过。
- 所有公共符号 `SF_API` 装饰；所有公共 enum 后置 `_Static_assert` drift 守护。
- `SF_ENABLE_PHASE7=OFF` 时 Phase 7 source / test 不进 build；公共头**始终 include**（声明 always-visible），但 `nm libsouls_formats.a | grep sf_(tae|fxr3)_` 必须空（无实现符号），且 OFF build 下若消费者尝试调用 sf_tae_* 函数 → 链接器报 unresolved external（预期）。
- mxml 第三方依赖通过 Phase 4 已建立的 CPM 引入（不另起炉灶）。

### Must NOT Have（Guardrails）

- ❌ **不实现 TAE Template subsystem**（801 LOC `Template.cs`）—— 推 v1.2。ParameterContainer 全程 opaque bytes。
- ❌ **不实现 TAE 任何非 SDT 格式**（DS1 / SOTFS / DS3 / BB / DES / DESR 路径全部 `_skipped_`）—— version 字段不在 `{0x1000D}` 时返回 `SF_ERR_UNSUPPORTED_VERSION`。
- ❌ **不暴露 TAE BigEndian 切换**到公共 API —— SDT 始终 LE。
- ❌ **不在 v1.1 内为 AC6 .tae 单独添加 TAEFormat enum 值** —— 等 T4 probe 结果；若 AC6 anibnd 不存在或使用未知 version，TBD 推 v1.2。
- ❌ **不优化 FXR3 Field 启发式判定** —— 严格 mirror 上游 `FXR3.cs:1140-1146` 的 value-range 启发式。
- ❌ **不引入新第三方依赖** —— XML 只用 Phase 4 已接的 mxml；不引入 libxml2 / Expat / yxml。
- ❌ **不为 mxml 提供 `sf_allocator_t` 桥** —— mxml 内部 malloc/free；仅 fxr3 对象本身受控（记入 extensions.md）。
- ❌ **不暴露 mxml / zstd / Win32 内部类型** 到公共头。
- ❌ **不让 `SF_ENABLE_PHASE7=OFF` build 包含任何 Phase 7 二进制符号** —— grep `nm libsouls_formats.a | grep sf_tae` 必须空。
- ❌ **不在 `_destroy` 之外调用 free**（沿用 Phase 1-6 一致性）。
- ❌ **不实现 FXR1 / FFXDLSE / ANI / MQB 任何遗留特效格式** —— 全部推 v2。
- ❌ **不实现 FXR3 XML write 的 byte-equal raw XML output 断言** —— 用 structural in-memory equality 替代。

---

## Verification Strategy（MANDATORY）

> **ZERO HUMAN INTERVENTION** —— 所有验收均由 agent 通过命令执行，禁止「用户手动确认」。
> 证据保存到 `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`。

### Test Decision

- **Infrastructure exists**：YES（Unity ThrowTheSwitch + ctest + `sf_add_test()` 已支持 label 路由；Phase 4 已 import mxml）。
- **Automated tests**：tests-after（沿用 Phase 4/5/6 策略；Phase 7 task 内含 implementation + 同 task 内附 test scenario，但执行顺序 implementation → test）。
- **Framework**：Unity（基础）+ ctest（驱动）+ `sf_add_test()`（路由）。
- **Labels**：`anim`（TAE + FXR3 合成 + XML round-trip 单元）、`e2e_er`（ER 真实数据 e2e，与 Phase 3/4/5/6 共用 label）。

### QA Policy

每个 task 都必须包含 agent-executed QA scenarios（见每个 TODO 的「QA Scenarios」段）。证据落到 `.sisyphus/evidence/`。

- **CLI / 构建工具**：`bash` + `cmake` + `ctest`（在 WSL2 中执行 MinGW cross 产物）。
- **二进制证据**：`xxd` / `objdump` / `nm` 对比 + 保存 hex dump 到 evidence；`diff input.bin output.bin` 字节比对。
- **Round-trip**：`bash` 跑测试 binary，对比 input/output 字节级一致；保留 input.bin 与 output.bin 到 evidence。
- **XML structural compare**：合成 FXR3 → XML 字符串 → re-parse → field-by-field 比较；mismatches 输出到 evidence。
- **静态检查**：`grep` 验证 API 命名前缀 `sf_tae_` / `sf_fxr3_` / `SF_FXR3_` / `SF_TAE_`、`_Static_assert` 存在性、`SF_ENABLE_PHASE7=OFF` build 不含 Phase 7 符号。

---

## Execution Strategy

### 并行执行 Wave 总览

```
Wave 0 — Preflight（清理 Phase 6 遗债 + 锁定 OUT-of-scope + 探测）:
├── T1: AGENTS.md / PLAN.md / roadmap/README.md 三处状态表 Phase 6 → ✅、Phase 7 → 🚧 [quick]
├── T2: PLAN.md §2.2 v1.x 路线图 + format-tae.md 五行 _skipped_ + format-fxr3.md status 准备  [quick]
├── T3: docs/api-mapping/extensions.md 起草 4 条新 entry stub                                  [writing]
├── T4: Empirical probe — c0000.anibnd.dcx → 任一 .tae → 实际 TAEFormat version 与 字段次序   [deep]
├── T5: Empirical probe — sfxbnd_commoneffects.ffxbnd.dcx → 任一 .fxr → FXRVersion + section 字节数 [deep]
└── T6: UPSTREAM.md game data snapshot 增补 c0000.anibnd.dcx + sfxbnd_commoneffects.ffxbnd.dcx sha256 [quick]

Wave 1 — Foundation（公共头 + CMake 接线 + 骨架）:
├── T7:  sf_tae.h —— opaque sf_tae_t / sf_tae_animation_t / sf_tae_event_t / sf_tae_event_group_t / sf_tae_anim_mini_header_t + TAEFormat / MiniHeaderType 枚举 + accessor API [quick]
├── T8:  sf_fxr3.h —— opaque sf_fxr3_t / sf_fxr3_state_map_t / sf_fxr3_state_t / sf_fxr3_state_condition_t / sf_fxr3_operand_t (tagged) / sf_fxr3_operator_t / sf_fxr3_container_t / sf_fxr3_effect_t / sf_fxr3_action_t / sf_fxr3_field_t (tagged) / sf_fxr3_property_t / sf_fxr3_property_modifier_t / sf_fxr3_unk_field_list_t + FXRVersion / FieldType / OperandType / OperatorType / PropertyType / PropertyInterpolationType 枚举 + accessor API + XML 函数原型 [quick]
├── T9:  CMakeLists.txt 接线 SF_ENABLE_PHASE7：把 4 个新源 + 2 个新头条件加入 SF_SOURCES / SF_PUBLIC_HEADERS [quick]
└── T10: src/effects/{tae,fxr3,fxr3_xml_read,fxr3_xml_write}.c 骨架（每文件含 minimal opaque struct + stub destroy；**无 #ifdef 守护** —— 由 CMake SF_SOURCES 条件块决定是否参与编译） [quick]

Wave 2 — TAE 实现（Wave 1 全绿后单线，本格式复杂度较低；可与 Wave 3 并行）:
├── T11: src/effects/tae.c read — TAE Header parse（version 0x1000D 守门 + SDT 路径）+ Animation list reader + AnimMiniHeader (Standard / ImportOtherAnim) tagged union 读 [deep]
├── T12: src/effects/tae.c read — Event list reader + EventGroup reader + ParameterContainer opaque bytes 持有 [unspecified-high]
├── T13: src/effects/tae.c write — Header / Animation / Event / EventGroup / MiniHeader / ParameterContainer 反向写 + Reserve/Fill offset 回填 + finish 校验 [deep]
└── T14: tests/anim/test_tae_synthetic.c —— 合成 SDT TAE：1 anim × 1 Standard mini-header × 1 event × 16 字节 opaque param + 1 event group（含该 event）round-trip 字节级一致 [unspecified-high]

Wave 3 — FXR3 二进制实现（Wave 1 全绿后 6 路并行，与 Wave 2 并行）:
├── T15: src/effects/fxr3.c read top-level —— FXR\0 magic + Version (4 或 5) + Id + 11 section table + Sekiro 段 ReferenceList / ExternalValueList / UnkBloodEnabler + StateMap / Container 入口 offset [deep]
├── T16: src/effects/fxr3.c —— StateCondition + ConditionOperand 4 变体 tagged union（Literal / External / StateTime / UnkMinus2）+ ConditionOperator + Field 读时调用 [deep]
├── T17: src/effects/fxr3.c —— Field 2 变体 tagged union（FieldInt / FieldFloat）+ value-range 启发式判定（mirror 上游 1140-1146 行）+ ReadAt / ReadMany [artistry]
├── T18: src/effects/fxr3.c —— Container / Effect / Action 递归 + Property + PropertyModifier + UnkFieldList + InterpolationType context 传递（含 UnkAc6 = 7 接受） [deep]
├── T19: src/effects/fxr3.c write —— 28 个 ReserveInt32 + FillInt32 + Sekiro section 11-14 条件写 + 各 list (Containers/Effects/Actions/Properties/Modifiers/...) flatten 写 [deep]
└── T20: tests/anim/test_fxr3_synthetic.c —— 合成 FXR3 binary：1 State × 4 Condition 变体 × 2 Field 变体（共覆盖 8 种组合）+ 1 Container × 1 Effect × 1 Action × 1 Property × 1 Modifier round-trip 字节级一致；DS3 与 Sekiro version 各一组 [unspecified-high]

Wave 4 — FXR3 XML 实现（Wave 3 T18 全绿后 3 路并行）:
├── T21: src/effects/fxr3_xml_read.c —— mxml DOM 解析 → sf_fxr3_t；schema 对照上游 XmlSerializer 标注（FXR3 / StateMap / State / StayCondition / Container / Effect / Action / Property / Field 元素 + Type / Value attribute 多态分支）[artistry]
├── T22: src/effects/fxr3_xml_write.c —— sf_fxr3_t → mxml DOM → UTF-8 string；XmlInclude 多态映射；attribute / element 选择遵守上游 schema [artistry]
└── T23: tests/anim/test_fxr3_xml.c —— FXR3 (T20 合成 fixture) → XML write → re-parse → structural equality（递归 field-by-field 比较，忽略 whitespace + attribute 顺序） [unspecified-high]

Wave 5 — ER e2e（Wave 2 + Wave 4 全绿后 2 路并行）:
├── T24: tests/anim/test_tae_e2e_er.c —— Data0..Data3 iter 提取 `/chr/c0000.anibnd.dcx`（c0000 在 Data3）→ BND4 → 任一 `.tae` entry → 解析 → anim_count > 0 + event_count > 0；记录文件名 / 字节数 / 解析数到 evidence [unspecified-high]
└── T25: tests/anim/test_fxr3_e2e_er.c —— 提取 `/sfx/sfxbnd_commoneffects.ffxbnd.dcx`（优先 Data0，fallback Data0..Data3）→ BND4 → 任一 `.fxr` entry → 解析 → version == Sekiro (5) + RootContainer non-null + 至少一个 Effect 子节点；记录文件名 / 字节数 / 节点统计到 evidence [unspecified-high]

Wave 6 — Docs + 状态表 final pass（Wave 5 全绿后 3 路并行）:
├── T26: 2 mapping doc 全量刷新（format-tae.md：9 in-scope row 标完成 + 5 Template-related row 标 _skipped_；format-fxr3.md：17 row 全标完成）+ extensions.md 4 条 final [writing]
├── T27: PLAN.md §7 Phase 7 章节 checkbox 全勾 + §1 状态表 final + §2.3 v1.x 路线图同步（移除 TAE / FXR3 from v1.1 backlog） [writing]
└── T28: docs/roadmap/phase-7-animation-effects.md 与本 plan 收尾对齐 + AGENTS.md §2 Phase 7 = ✅ + docs/roadmap/README.md Phase 7 行 = ✅ + CHANGELOG.md 增 Phase 7 章 [writing]

Wave FINAL（4 reviewer 并行 — 全部 wave 完成后启动；必须 ALL APPROVE 才向用户索取 okay）:
├── F1: 计划合规审计              [oracle]
├── F2: 代码质量审查              [unspecified-high]
├── F3: Real Manual QA            [unspecified-high]
└── F4: Scope fidelity check      [deep]
→ 4 reviewer 全 APPROVE → 向用户展示 → 等待用户显式 okay 才标记 Phase 7 完成。
```

### Dependency Matrix（关键路径）

- **T1-T6**：- (Wave 0 preflight，全 6 个独立) → Wave 1
- **T7**：T1, T2 → T10, T11, T14, T24
- **T8**：T1, T2 → T9, T10, T15-T22, T23, T25
- **T9**：T7, T8 → T10, T11, T15
- **T10**：T7, T8, T9 → T11-T22
- **T11**：T7, T9, T10 → T12, T13, T24
- **T12**：T11 → T13, T14, T24
- **T13**：T11, T12 → T14, T24
- **T14**：T13 → Wave 6
- **T15**：T8, T9, T10 → T16, T17, T18, T19
- **T16**：T15, T17 → T18, T19, T20, T21, T22
- **T17**：T15 → T16, T18, T19, T20, T21, T22
- **T18**：T15, T16, T17 → T19, T20, T21, T22, T25
- **T19**：T15-T18 → T20, T22, T25
- **T20**：T15-T19 → T23, Wave 5, Wave 6
- **T21**：T8, T18, T19 → T23
- **T22**：T8, T18, T19, T21 → T23
- **T23**：T20, T21, T22 → Wave 6
- **T24**：T13 + Phase 3 er_test_helper → Wave 6
- **T25**：T19 + Phase 3 er_test_helper → Wave 6
- **T26-T28**：Wave 5 全绿 → Wave Final
- **F1-F4**：全部 wave 完成 → user okay

### Agent Dispatch 总结

- **Wave 0**：6 tasks（3 × quick + 2 × deep + 1 × writing） —— 全独立
- **Wave 1**：4 tasks（4 × quick）
- **Wave 2**：4 tasks（2 × deep + 1 × unspecified-high + 1 × unspecified-high）
- **Wave 3**：6 tasks（4 × deep + 1 × artistry + 1 × unspecified-high） —— **与 Wave 2 并行；本 phase 最大并行度**
- **Wave 4**：3 tasks（2 × artistry + 1 × unspecified-high）
- **Wave 5**：2 tasks（2 × unspecified-high）
- **Wave 6**：3 tasks（3 × writing）
- **Wave Final**：4 tasks（oracle + unspecified-high × 2 + deep）

**总计**：28 实施 task + 4 reviewer = **32 task-level checkbox**。

---

## TODOs

### Wave 0 — Preflight & Cleanup（清理 Phase 6 遗债 + 锁定 OUT-of-scope + 探测）

- [x] 1. **AGENTS.md / PLAN.md / docs/roadmap/README.md 三处状态表 Phase 6 → ✅、Phase 7 → 🚧**

  **What to do**：
  - **Step A：实测 Phase 6 真实 ctest 数**（生成 evidence，不依赖陈旧 log）：
    1. `cmake --build build-mingw 2>&1 | tail -5`
    2. `ctest --test-dir build-mingw -L geom --output-on-failure 2>&1 | tee .sisyphus/evidence/task-1-phase6-geom-ctest.log`
    3. `ctest --test-dir build-mingw -L 'e2e_er' --output-on-failure 2>&1 | tee .sisyphus/evidence/task-1-phase6-e2e-ctest.log`
    4. 从两 log 末尾 `100% tests passed, 0 tests failed out of N` 抓 N；合计为 Phase 6 测试总数。
  - **Step B：AGENTS.md §2「Current status」表格**：
    - Phase 6 行：`🚧 in progress` → `✅ done`；Tests 列填 `N/N PASS across M test binaries`（沿用 Phase 5 / 4 / 3 风格）。
    - Phase 7 行：`⏳ optional / v1.1` → `🚧 in progress`；Tests 列保留 `—`（T28 完成时改）。
  - **Step C：PLAN.md §7 Phase 6 子标题**：保留现有 `✅ 完成 (2026-05-12) — 15/15 PASS across 8 test binaries`（已有）；Phase 7 子标题改为 `🚧 in progress`，estimate 校准为「2 周」。Phase 7 章节内 v1.0 / v1.1 决策保留。
  - **Step D：docs/roadmap/README.md Phase index 表**：
    - Phase 7 行：state = `🚧 in progress`、estimate = `2 wk`。

  **Must NOT do**：
  - ❌ 不动 PLAN.md §3-§6 架构 / 技术决策章节。
  - ❌ 不动 Phase 6 / 7 子标题里除 estimate / state 之外的内容。
  - ❌ 不发明 Tests 数；必须用 Step A 实测命令生成。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 3 文件 + 测试数实测。
  - **Skills**: `tech-doc-style-chinese`（PLAN.md 中文文档）。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T2-T6 并行）
  - **Parallel Group**: Wave 0
  - **Blocks**: T7-T8（Wave 1 起步前状态须明确）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - `.sisyphus/plans/phase-6-geometry-material.md:T1` —— Phase 6 起步同款状态切换 task；逐字对照。
  - `AGENTS.md:24-35` —— 状态表当前形态。
  - `PLAN.md:L638` Phase 6 子标题 `✅ 完成 (2026-05-12)` 范本。

  **Test References**：
  - `tests/CMakeLists.txt:269-291` —— Phase 6 注册的 8 个 geom + e2e_er test binary 列表。

  **External References**：
  - GNU awk —— 跨 mingw/WSL 都可用。

  **WHY Each Reference Matters**：
  - Phase 4/5/6 plan 建立了「先实测 ctest 再写文档」的金标准；避免凭印象写测试数。
  - 三处状态表（AGENTS.md / PLAN.md / roadmap/README.md）历史上出现过不同步漂移；Phase 7 起步就一起改。

  **Acceptance Criteria**：
  - [ ] `.sisyphus/evidence/task-1-phase6-geom-ctest.log` 存在且末尾含 `100% tests passed, 0 tests failed out of N1`。
  - [ ] `.sisyphus/evidence/task-1-phase6-e2e-ctest.log` 存在且末尾含 `100% tests passed, 0 tests failed out of N2`。
  - [ ] AGENTS.md §2 表 Phase 6 行 = `✅ done` + 实测 `N1+N2/N1+N2 PASS across M test binaries`。
  - [ ] AGENTS.md §2 表 Phase 7 行 = `🚧 in progress`。
  - [ ] docs/roadmap/README.md Phase 7 行 = `🚧 in progress` + 2 wk。

  **QA Scenarios**：

  ```
  Scenario: Phase 6 ctest 实测 0 failed
    Tool: Bash
    Preconditions: Phase 6 已完成；build-mingw 存在
    Steps:
      1. `cmake --build build-mingw 2>&1 | tail -5`
      2. `ctest --test-dir build-mingw -L geom --output-on-failure 2>&1 | tee .sisyphus/evidence/task-1-phase6-geom-ctest.log`
      3. `tail -3 .sisyphus/evidence/task-1-phase6-geom-ctest.log | grep -E '100% tests passed, 0 tests failed'`
    Expected Result: 步骤 3 命中（0 failed）
    Failure Indicators: 步骤 3 未命中
    Evidence: .sisyphus/evidence/task-1-phase6-geom-ctest.log

  Scenario: 状态表三处一致 Phase 7 = in-progress
    Tool: Bash
    Preconditions: T1 改动落地
    Steps:
      1. `grep -E '\| 7 \|' AGENTS.md | grep -c '🚧 in progress'`
      2. `grep -E '\| 7 \|' docs/roadmap/README.md | grep -c '🚧 in progress'`
    Expected Result: 两步全部输出 ≥ 1
    Failure Indicators: 任一步骤 = 0
    Evidence: .sisyphus/evidence/task-1-state-table-sync.log

  Scenario: 中文风格未漂移
    Tool: Bash
    Steps:
      1. `git diff AGENTS.md .sisyphus/plans/PLAN.md docs/roadmap/README.md | grep '^+' | grep -E '(你|您|让我们|快来|赶紧)' | tee .sisyphus/evidence/task-1-style.log`
    Expected Result: 输出空
    Failure Indicators: 命中
    Evidence: .sisyphus/evidence/task-1-style.log
  ```

  **Commit**: YES
  - Message: `phase7(state): switch Phase 6 to done, Phase 7 to in-progress in three status tables`
  - Files: `AGENTS.md`, `.sisyphus/plans/PLAN.md`, `docs/roadmap/README.md`, `.sisyphus/evidence/task-1-*`
  - Pre-commit: Phase 6 ctest 全绿（0 failed）

- [x] 2. **PLAN.md §2.2 / §2.3 + format-tae.md 五行标 `_skipped_`**

  **What to do**：
  - **Step A：PLAN.md §2.2 「v1 显式不实现」表格**（追加 TAE Template subsystem + TAE legacy formats 条目）：
    - 追加：「**TAE Template subsystem**（`Template.cs` 801 LOC，`ApplyTemplate` / `BankTemplate` / `EventTemplate` / `ParameterTemplate`）—— v1.1 不实现，参数容器维持 opaque bytes；typed access 推 v1.2」。
    - 追加：「**TAE 非 SDT 格式**（DS1 / SOTFS / DS3 / BB / DES / DESR）—— v1.1 仅实现 SDT (version 0x1000D)；其他 format byte 路径返回 `SF_ERR_UNSUPPORTED_VERSION`，推 v2 legacy」。
  - **Step B：PLAN.md §2.3 v1.x 路线图**：v1.1 段保留「TAE / FXR3 + PARAMDEF XML 写出」措辞，附注「TAE 仅 SDT 格式，Template subsystem 推 v1.2」。
  - **Step C：docs/api-mapping/format-tae.md** Status 段（文件顶部）补充：
    - 「**v1.1 scope**：仅 SDT 格式（version 0x1000D，covers Sekiro + Elden Ring）；其他 TAEFormat 路径 `_skipped_`。**Template 子系统全部 `_skipped_`**」。
  - **Step D：format-tae.md mapping 表 5 行改 status**：
    - `TAE.Template` 行 → `_skipped_`，Notes 列加「v1.2 推迟」。
    - `TAE.Template.BankTemplate` 行 → `_skipped_`。
    - `TAE.Template.EventTemplate` 行 → `_skipped_`。
    - `TAE.Template.ParameterTemplate` 行 → `_skipped_`。
    - `TAE.Template.ParamType` 行 → `_skipped_`。

  **Must NOT do**：
  - ❌ 不动 PLAN.md §3-§7（仅 §2.2 / §2.3）。
  - ❌ 不动 format-tae.md 其他 9 行（TAE / TAEFormat / Animation / AnimMiniHeader / Standard / ImportOtherAnim / Event / ParameterContainer / EventGroup）—— 这些是 in-scope，Wave 6 T26 才标完成。
  - ❌ 不创建 format-tae-template.md 独立文档；Template-related 行就留在 format-tae.md 下。

  **Recommended Agent Profile**：
  - **Category**: `quick`
  - **Skills**: `tech-doc-style-chinese`

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T7（sf_tae.h 起草时 Template OUT-of-scope 决策须已定）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - `.sisyphus/plans/phase-6-geometry-material.md:T2` —— Phase 6 同款「Edge geometry OUT-of-scope」操作；逐字对照。
  - `docs/api-mapping/format-flver2.md` 中 Edge 子表 status 标 `_skipped_` 后的形态。
  - `docs/api-mapping/POLICY.md` 中 `_skipped_` legend 项定义。

  **WHY Each Reference Matters**：
  - PLAN.md §2.2 是 canonical OUT-of-scope 清单；不在此处明确登记，后续 reviewer 容易判定 Template 缺失为「待补」而非「不做」。
  - format-tae.md 是 row-level alignment 状态源；标 `_skipped_` 后 F1 reviewer grep 时不会误判 Template 行未完成。

  **Acceptance Criteria**：
  - [ ] `grep -c 'TAE Template subsystem' .sisyphus/plans/PLAN.md` ≥ 1（§2.2 表中）。
  - [ ] `grep -c '_skipped_' docs/api-mapping/format-tae.md` ≥ 5。
  - [ ] `grep -c '未实现' docs/api-mapping/format-tae.md` ≥ 9（in-scope 9 行未动）。

  **QA Scenarios**：

  ```
  Scenario: format-tae.md Template 5 行全部标 _skipped_
    Tool: Bash
    Steps:
      1. `awk '/Template/{print}' docs/api-mapping/format-tae.md | grep -c '_skipped_'`
    Expected Result: 5
    Failure Indicators: ≠ 5
    Evidence: .sisyphus/evidence/task-2-tae-template-skipped.log

  Scenario: PLAN.md §2.2 注册 TAE Template + legacy formats 不实现
    Tool: Bash
    Steps:
      1. `awk '/### 2.2/,/### 2.3/' .sisyphus/plans/PLAN.md | grep -cE 'TAE Template|TAE 非 SDT'`
    Expected Result: ≥ 2
    Failure Indicators: < 2
    Evidence: .sisyphus/evidence/task-2-plan-tae-out.log
  ```

  **Commit**: YES
  - Message: `phase7(scope): lock TAE Template + legacy TAE formats OUT-of-scope in PLAN.md and format-tae.md`
  - Files: `.sisyphus/plans/PLAN.md`, `docs/api-mapping/format-tae.md`, `.sisyphus/evidence/task-2-*`
  - Pre-commit: 无（纯文档）

- [x] 3. **`docs/api-mapping/extensions.md` 起草 4 条新 entry stub**

  **What to do**：
  - 读 `docs/api-mapping/extensions.md` 当前 schema（Phase 4-6 已有若干 entry，保持一致）。
  - 添加 4 条新 entry stub（Wave 6 T26 final pass 才完整）：
    1. **TAE Template subsystem deferral**：
       - Type: Functional divergence（C 端 v1.1 不实现）。
       - Upstream Ref: `Formats/TAE/Template.cs` (801 LOC)；`TAE.cs:ApplyTemplate(Template, bool)`.
       - C API: 无（不暴露 Template / BankTemplate / EventTemplate / ParameterTemplate / ParamType）。
       - Rationale: Template 是 friendly param access；v1.1 范围仅做 round-trip，typed access 推 v1.2。
       - Impact: Event.Parameters 维持 opaque `uint8_t*` + size；消费者需自行解析。
    2. **mxml default allocator（无 `sf_allocator_t` override）**：
       - Type: C-style adaptation（已知 limitation）。
       - Upstream Ref: 上游用 .NET XmlSerializer，分配器由 GC 管理。
       - C API: `sf_fxr3_to_xml(...)` / `sf_fxr3_from_xml(...)` 内部使用 mxml 的默认 `malloc/free`；fxr3 对象本身受 `sf_allocator_t` 控制。
       - Rationale: mxml 4.0.4 不支持 thread-local allocator hook；v1.1 不接桥。
       - Impact: XML pipeline 内存不通过用户 allocator；XML 字符串完成后必须 mxml-style free。
    3. **FXR3 XML round-trip equivalence policy**：
       - Type: Functional adaptation。
       - Upstream Ref: `FXR3.cs:1488 FXR3ToXML / 1480 XMLToFXR3` 使用 .NET XmlSerializer。
       - C API: `sf_fxr3_to_xml` / `sf_fxr3_from_xml`。
       - Rationale: XmlSerializer 输出含不稳定 whitespace / attribute 顺序 / namespace；C 端无法 byte-equal 复制。
       - Impact: 测试用 structural in-memory equality（XML write → re-read → field-by-field 比对），不用 byte-equal raw XML。
    4. **TAE format coverage limited to SDT in v1.1**：
       - Type: Functional divergence。
       - Upstream Ref: `TAE.cs:202-260` 7 个 TAEFormat 分支。
       - C API: `sf_tae_read_from_memory` 在 version ≠ 0x1000D 时返回 `SF_ERR_UNSUPPORTED_VERSION`。
       - Rationale: v1.1 目标游戏（Sekiro / Elden Ring / Nightreign）全部使用 SDT；legacy 推 v2。AC6 TBD（probe 决定）。
       - Impact: DS1 / SOTFS / DS3 / BB / DES / DESR / AC6 (TBD) `.tae` 文件无法读取。

  **Must NOT do**：
  - ❌ 不实现任何 C 代码（本 task 仅文档）。
  - ❌ 不在 entries 里宣称 "TODO later"；Phase 7 必须拍板。
  - ❌ 不为 v1.2 计划单独建 entry（PLAN.md §2.3 已总括）。

  **Recommended Agent Profile**：
  - **Category**: `writing`
  - **Skills**: `tech-doc-style-chinese`（若 extensions.md 是中文）

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T26（final pass 引用本 stub）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - `docs/api-mapping/POLICY.md` —— C-style adaptation vs extension 区分定义。
  - `docs/api-mapping/extensions.md` 现状 —— Phase 4-6 已有 entry 的 schema。
  - `.sisyphus/plans/phase-6-geometry-material.md:T3` —— Phase 6 同款 stub-then-final-pass pattern。

  **API/Type References**：
  - `TAE.cs:202-260` —— 7 个 TAEFormat 分支的 ground truth。
  - `FXR3.cs:1140-1146` —— Field 启发式判定（**虽然不在 extensions.md，但相关；本 task 不录入，T17 task description 引用即可**）。
  - `FXR3.cs:1478-1500` —— FXR3EnhancedSerialization 入口。

  **External References**：
  - AGENTS.md §5.x rule 2(b) —— extension 必须录入 extensions.md 的硬约束。

  **WHY Each Reference Matters**：
  - POLICY.md 是判定 extension vs C-style adaptation 的依据；4 条 entry 都需按 schema 分类。
  - AGENTS.md §5.x 是硬约束；不录入 extensions.md → F1 reviewer 直接 REJECT。

  **Acceptance Criteria**：
  - [ ] `docs/api-mapping/extensions.md` 存在且含 4 条新 entry（关键字：`TAE Template`、`mxml.*allocator`、`XML.*equivalence`、`TAE.*SDT.*only`）。
  - [ ] 每条 entry 含 Type / Upstream Ref / C API / Rationale / Impact 5 段。
  - [ ] entry 内引用的 file:line 真实存在（Wave 0 验证）。

  **QA Scenarios**：

  ```
  Scenario: 4 条 entry 齐全
    Tool: Bash
    Steps:
      1. `grep -cE 'TAE Template|mxml.*allocator|XML.*equivalence|TAE.*SDT.*only' docs/api-mapping/extensions.md`
    Expected Result: ≥ 4
    Failure Indicators: < 4
    Evidence: .sisyphus/evidence/task-3-extensions-count.log

  Scenario: file:line 引用真实可达
    Tool: Bash
    Steps:
      1. `grep -oE '[A-Z][a-zA-Z0-9]+\.cs:[0-9]+' docs/api-mapping/extensions.md | sort -u | while read ref; do
            file=$(echo $ref | cut -d: -f1)
            line=$(echo $ref | cut -d: -f2)
            full=$(find /home/soar/src/SoulsFormatsNEXT -name $file | head -1)
            if [ -z "$full" ]; then echo "MISSING: $ref"; else
              total=$(wc -l < "$full")
              if [ $line -gt $total ]; then echo "OUT-OF-RANGE: $ref ($total lines)"; fi
            fi
          done | tee .sisyphus/evidence/task-3-refs.log`
    Expected Result: 输出空
    Failure Indicators: 任何 MISSING 或 OUT-OF-RANGE
    Evidence: .sisyphus/evidence/task-3-refs.log
  ```

  **Commit**: YES
  - Message: `phase7(docs): seed extensions.md entries for TAE Template defer + mxml allocator + XML equivalence + TAE SDT-only`
  - Files: `docs/api-mapping/extensions.md`, `.sisyphus/evidence/task-3-*`
  - Pre-commit: 无

- [x] 4. **Empirical probe — c0000.anibnd.dcx → 任一 `.tae` → 实际 TAEFormat version 与字段次序**

  **What to do**：
  - 写一次性 probe 程序 `tests/probes/probe_tae_format.c`：
    1. **关键**：c0000 系列资产生产 ER 版本中存放于 **Data3**，不在 Data0（已由 Phase 6 T4 probe 在 `tests/geom/test_flver2_e2e_er.c:5-8` 注释确认）。当前 `tests/e2e/er_test_helper.c:38-40,202-217` 中的 `er_extract_from_data0()` 仅打开 Data0。因此本 probe **必须**复用 Phase 6 `test_flver2_e2e_er.c` 的 Data0..Data3 顺序搜索模式：手动打开 Data0.bhd / Data1.bhd / Data2.bhd / Data3.bhd，对每个 BHD5 实例调 `sf_bhd5_find_by_path_hash(bhd, hash, &entry)`，第一个命中即用对应 .bdt 读取。**或者**（可选 sub-step）：扩展 `er_test_helper.c` 增 `er_extract(path, &out, &size)` 通用 helper（迭代 Data0..Data3）—— 若实施 sub-step，T24 / T25 直接复用且本 plan 后续 task 描述需同步更新。
    2. 用 BND4 reader 列出全部 `.tae` entry（c0000.anibnd 内通常含 ~1000 个 .tae）。
    3. 取前 5 个 `.tae` 作为样本。
    4. **手写最小 TAE header parser**（对照 `TAE.cs:179-260`）：
       - 读 magic `"TAE \0"` (4 字节，与上游 `AssertASCII("TAE ")` 一致；注意有空格)。
       - 读 BigEndian flag (byte at offset 0x04, AssertByte(0, 1))。
       - 读 IsBigEndian = 0（v1.1 SDT 必须 LE）。
       - 读 version (i32) at expected offset；断言 == `0x1000D`（SDT/ER）。
       - 记录 is64Bit 检测路径（version + 字段偏移结构推断）。
       - 读 Format byte（影响后续 Animation 解析路径）。
       - 读 Flags 数组 (byte[8])、SkeletonName / SibName offset、AnimCount、AnimOffset。
    5. 收集所有 5 个样本的：version 值、is64Bit 结果、Format 推断值、AnimCount 范围、ParameterContainer 字节数分布。
  - 输出 `.sisyphus/evidence/task-4-tae-probe.md`：
    - c0000.anibnd 内 .tae entry 总数。
    - 5 个样本路径 + 各自 version / Format / AnimCount。
    - 一个**最小** .tae 的字节级 hex dump (≤ 1 KB 那种)，作为 T14 合成 fixture 字节级参考。
    - 警告：若样本 version ≠ `0x1000D` → ALERT，plan 需 revise。
  - **不**实现完整 TAE 解析；本 task 是 read-only 探测。

  **Must NOT do**：
  - ❌ 不复用 `src/effects/`（Wave 1/2 才写）；独立实现 probe。
  - ❌ 不解码 Event ParameterContainer 内容（仅记字节数）。
  - ❌ 不为 Sekiro / AC6 / Nightreign 做同款 probe（v1.1 仅 ER）。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— 多 service 层组合 + 字节级解释。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T11（TAE Header parse 实现以 probe 字段次序为 ground truth）；T14（synthetic fixture 用 probe 出的最小 hex dump 作参考）；T24（e2e 测试覆盖 probe 列出的样本）
  - **Blocked By**: 无（ER 数据已就位，BHD5/DCX/BND4 已完成）

  **References**：

  **Pattern References**：
  - `.sisyphus/plans/phase-6-geometry-material.md:T4` —— Phase 6 同款 probe（probe_flver2_layouts.c）；逐字对照结构。
  - `tests/probes/probe_matbin_paramtypes.c`（Phase 6）—— BND4 entry 列举模式。
  - `tests/e2e/er_test_helper.c` —— BHD5 + DCX + BND4 提取链路。

  **API/Type References**：
  - `include/souls_formats/sf_bnd4.h` —— BND4 entry API。
  - `Formats/TAE/TAE.cs:Read()` —— 上游 header parse；仅参考 SDT 分支（其他分支 _skipped_）。

  **External References**：
  - AGENTS.md §8.4 —— ER 测试数据路径硬编码方式。

  **WHY Each Reference Matters**：
  - probe 必须独立于未来 T11 实现；不能 link 还没写的 src/effects/tae.c。
  - 复用 er_test_helper 避免重复实现 BHD5 提取。
  - 上游 SDT 分支字节布局是 probe 的核心 ground truth。

  **Acceptance Criteria**：
  - [ ] `tests/probes/probe_tae_format.c` 提交且通过 cmake 构建（`cmake --build build-mingw --target probe_tae_format`）。
  - [ ] `.sisyphus/evidence/task-4-tae-probe.md` 存在且含 5 个样本路径 + 各 version / Format / AnimCount + 1 个最小 hex dump（≤ 1 KB）。
  - [ ] 所有样本 version == `0x1000D`（若不一致 → ALERT，plan 需 revise）。

  **QA Scenarios**：

  ```
  Scenario: probe 跑通且抓到样本
    Tool: Bash
    Preconditions: ER 数据在；Phase 3 er_test_helper 可用
    Steps:
      1. `cmake --build build-mingw --target probe_tae_format`
      2. `./build-mingw/tests/probes/probe_tae_format.exe > .sisyphus/evidence/task-4-tae-probe.txt 2>&1; echo "exit=$?"`
      3. `grep -E 'TAE_VERSION: 0x[0-9A-F]+' .sisyphus/evidence/task-4-tae-probe.txt`
      4. `grep -E 'SAMPLE_COUNT: [0-9]+' .sisyphus/evidence/task-4-tae-probe.txt | head -1`
    Expected Result: 退出码 0；步骤 3 命中且全部 `0x1000D`；步骤 4 显示 SAMPLE_COUNT == 5
    Failure Indicators: 退出码 ≠ 0；version ≠ 0x1000D（plan 需 revise）；样本 < 5
    Evidence: .sisyphus/evidence/task-4-tae-probe.txt + .md

  Scenario: probe 不污染 src/effects/
    Tool: Bash
    Steps:
      1. `ls src/effects/ 2>/dev/null; echo "exit=$?"`
    Expected Result: 退出码 1（src/effects/ 尚不存在，Wave 1 才创建）
    Failure Indicators: src/effects/ 已有文件
    Evidence: .sisyphus/evidence/task-4-no-pollution.log
  ```

  **Commit**: YES
  - Message: `phase7(probe): empirical TAEFormat version in c0000.anibnd.dcx → .tae`
  - Files: `tests/probes/probe_tae_format.c`, `tests/probes/CMakeLists.txt`, `.sisyphus/evidence/task-4-*`
  - Pre-commit: probe 跑通且 evidence 写齐

- [x] 5. **Empirical probe — sfxbnd_commoneffects.ffxbnd.dcx → 任一 `.fxr` → FXRVersion + section 字节数分布**

  **What to do**：
  - 写一次性 probe 程序 `tests/probes/probe_fxr3_format.c`：
    1. 提取 `/sfx/sfxbnd_commoneffects.ffxbnd.dcx`：先调 `er_extract_from_data0(...)`；若 `SF_ERR_*`（未在 Data0）→ fallback Data0..Data3 顺序搜索（同 T4 probe Data3 fallback 模式）。**记录命中的 BHD index 到 evidence**（T24 / T25 e2e 可直接复用此结果）。
    2. 用 BND4 reader 列出全部 `.fxr` entry。
    3. 取前 10 个 `.fxr` 作为样本。
    4. **手写最小 FXR3 header parser**（对照 `FXR3.cs:62-122`）：
       - 读 magic `"FXR\0"` (4 字节)。
       - 读 short 0 + Version (u16) at offset 6 → 期望 4 (DS3) 或 5 (Sekiro)。
       - 读 Id (i32)。
       - 读 StateMapOffset (i32)。
       - 读 11 个 section offset + count 对（共 88 字节）。
       - 若 Version == Sekiro：读 4 个额外 offset+count（ReferenceList / ExternalValueList / UnkBloodEnabler / UnkEmpty 哨兵 = 32 字节）。
       - 验证 StateMap + Container offset 落入文件范围。
    5. 对每个样本收集：FXRVersion 值、Id、各 section count 直方图（State/Container/Effect/Action/Property/Modifier/ConditionalProperty/UnkFieldList/Field 各 count）。
  - 输出 `.sisyphus/evidence/task-5-fxr3-probe.md`：
    - sfxbnd_commoneffects.ffxbnd 内 .fxr entry 总数。
    - 10 个样本路径 + 各自 Version / Id / section counts。
    - Version 直方图（DS3 vs Sekiro 占比）—— 期望 ER 全 == Sekiro。
    - 一个**最小** .fxr 的字节级 hex dump（用于 T20 合成 fixture 字节级参考）。
    - ALERT 当 Version ∉ {4, 5} 时（不该发生但若发生需 revise plan）。
  - **不**实现完整 FXR3 解析；本 task 是 read-only 探测。

  **Must NOT do**：
  - ❌ 不复用 `src/effects/fxr3.c`（Wave 3 才写）；独立实现 probe。
  - ❌ 不解码 ConditionOperand / Field tagged union（仅 section count）。
  - ❌ 不为 AC6 做同款 probe。

  **Recommended Agent Profile**：
  - **Category**: `deep`
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T8（sf_fxr3.h FXRVersion 枚举值 probe 验证）；T15（top-level fxr3.c read 实现需 section 数验证）；T20（合成 fixture 应覆盖 probe 出的 section 分布）；T25（e2e 测试 hint）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - T4 probe 程序结构（同款一次性 probe）。
  - `.sisyphus/plans/phase-6-geometry-material.md:T5` —— Phase 6 同款 probe（probe_matbin_paramtypes.c）。

  **API/Type References**：
  - `include/souls_formats/sf_bnd4.h` —— BND4 reader。
  - `Formats/FXR3.cs:62-122` —— Is + Read header；逐字段对照。

  **External References**：
  - Metis 备用审查中 FXR3 Sekiro version sections 11-14 假设：probe 验证。

  **WHY Each Reference Matters**：
  - probe 主要价值是**验证** Metis 假设：ER .fxr 全部 Version == Sekiro (5)；若发现 Version == DS3 (4)，T15-T19 实现需双 version 全 cover。
  - 选定 10 个样本可让 T25 e2e 测试有具体目标。
  - 最小 .fxr hex dump 是 T20 合成 fixture 字节级 ground truth。

  **Acceptance Criteria**：
  - [ ] `tests/probes/probe_fxr3_format.c` 提交且通过 cmake 构建。
  - [ ] `.sisyphus/evidence/task-5-fxr3-probe.md` 含 entry 总数 + 10 样本 + Version 直方图 + 最小 hex dump。
  - [ ] 全部样本 Version ∈ {4, 5}；若 ALERT 触发 plan 需修正。

  **QA Scenarios**：

  ```
  Scenario: probe 跑通且 Version 全 ∈ {4, 5}
    Tool: Bash
    Steps:
      1. `cmake --build build-mingw --target probe_fxr3_format`
      2. `./build-mingw/tests/probes/probe_fxr3_format.exe > .sisyphus/evidence/task-5-fxr3-probe.txt 2>&1`
      3. `grep -E 'UNKNOWN_VERSION: [0-9]+' .sisyphus/evidence/task-5-fxr3-probe.txt; echo "exit=$?"`
      4. `grep -E 'FXR_VERSION_HIST:' .sisyphus/evidence/task-5-fxr3-probe.txt`
    Expected Result: 步骤 3 退出码 1（无 UNKNOWN_VERSION 命中）；步骤 4 显示分布（期望 Sekiro 主导）
    Failure Indicators: 步骤 3 退出码 0（出现未知 version）
    Evidence: .sisyphus/evidence/task-5-fxr3-probe.txt + .md
  ```

  **Commit**: YES
  - Message: `phase7(probe): empirical FXRVersion + section distribution in sfxbnd_commoneffects.ffxbnd.dcx → .fxr`
  - Files: `tests/probes/probe_fxr3_format.c`, `tests/probes/CMakeLists.txt`, `.sisyphus/evidence/task-5-*`
  - Pre-commit: probe 跑通且 evidence 写齐 + 0 UNKNOWN_VERSION

- [x] 6. **`docs/api-mapping/UPSTREAM.md` Game Data Snapshot 增补 c0000.anibnd.dcx + sfxbnd_commoneffects.ffxbnd.dcx sha256**

  **What to do**：
  - 读 Phase 5/6 在 `docs/api-mapping/UPSTREAM.md` 已建的「Game Data Snapshots」段。
  - 在 ER 子段下追加 2 行：
    - c0000.anibnd.dcx 提取路径 `/chr/c0000.anibnd.dcx`（**Data3**，由 Phase 6 c0000 e2e + 本 phase T4 probe 共同验证）+ 提取后 sha256 + 抓取日期。
    - sfxbnd_commoneffects.ffxbnd.dcx 提取路径 `/sfx/sfxbnd_commoneffects.ffxbnd.dcx`（具体所在 Data{N} 由 T5 probe 验证；T6 记录命中的 BHD index）+ 提取后 sha256 + 抓取日期。
  - 加注：「Phase 7 e2e 测试将 sha256 作 sanity check；游戏 patch 后 sha256 不匹配 → SKIP 并 log」。
  - 在 risk 列加一行：「Phase 7 e2e 锁定 c0000.anibnd.dcx 与 sfxbnd_commoneffects.ffxbnd.dcx snapshot；patch 后字段可能微变」。

  **Must NOT do**：
  - ❌ 不嵌入任何 game-derived 字节。
  - ❌ 不改 Phase 5/6 已建段落，仅 ER 段内追加 2 行。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— sha256sum + 文档插入。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T24 / T25（e2e 测试引用 snapshot hash 作 sanity check）
  - **Blocked By**: 无

  **References**：

  **Pattern References**：
  - `.sisyphus/plans/phase-6-geometry-material.md:T6` —— Phase 6 同款；逐字对照。
  - `docs/api-mapping/UPSTREAM.md` Phase 5/6 已建 schema。

  **External References**：
  - `sha256sum` CLI。

  **WHY Each Reference Matters**：
  - Phase 6 已在 UPSTREAM.md 建立 snapshot 政策；Phase 7 不另起炉灶。
  - e2e 测试加 sha256 sanity check 是防止「游戏升 patch → 测试误判 bug」的成熟手段。

  **Acceptance Criteria**：
  - [ ] `docs/api-mapping/UPSTREAM.md` 「Game Data Snapshots」段 ER 子段含新增 2 行（c0000.anibnd.dcx + sfxbnd_commoneffects.ffxbnd.dcx sha256）。
  - [ ] 每条新增含：BHD5 内路径 + sha256 + 抓取日期（YYYY-MM-DD）。

  **QA Scenarios**：

  ```
  Scenario: sha256 实测可重现
    Tool: Bash
    Steps:
      1. 用 er_test_helper 提取 /chr/c0000.anibnd.dcx 到 /tmp，记 sha256
      2. `grep c0000.anibnd.dcx docs/api-mapping/UPSTREAM.md` 抓出记录值
      3. diff 两者
    Expected Result: 一致
    Failure Indicators: 不一致 → 用户已升级游戏，需重抓 snapshot
    Evidence: .sisyphus/evidence/task-6-hash-verify.log

  Scenario: UPSTREAM.md 结构完整
    Tool: Bash
    Steps:
      1. `grep -A 5 'c0000.anibnd.dcx' docs/api-mapping/UPSTREAM.md | grep -c 'sha256'`
      2. `grep -A 5 'sfxbnd_commoneffects.ffxbnd.dcx' docs/api-mapping/UPSTREAM.md | grep -c 'sha256'`
    Expected Result: 两步均 ≥ 1
    Failure Indicators: 任一 = 0
    Evidence: .sisyphus/evidence/task-6-upstream-md.log
  ```

  **Commit**: YES
  - Message: `phase7(docs): pin c0000.anibnd.dcx + sfxbnd_commoneffects.ffxbnd.dcx sha256 in UPSTREAM.md`
  - Files: `docs/api-mapping/UPSTREAM.md`, `.sisyphus/evidence/task-6-*`
  - Pre-commit: 无

### Wave 1 — Foundation（公共头 + CMake 接线 + src/effects 骨架）

- [x] 7. **`sf_tae.h` —— opaque types + TAEFormat / MiniHeaderType 枚举 + accessor API**

  **What to do**：
  - 起草 `include/souls_formats/sf_tae.h`：
    - **opaque typedef**（forward declaration only，定义在 src/effects/tae.c）：
      - `sf_tae_t` —— root container
      - `sf_tae_animation_t` —— single animation entry
      - `sf_tae_event_t` —— single timed event
      - `sf_tae_event_group_t` —— event grouping
      - `sf_tae_anim_mini_header_t` —— polymorphic mini-header（Standard / ImportOtherAnim）
    - **公共 enum**（精确对照上游字节值）：
      - `sf_tae_format_t`（对照 `TAE.cs:20-51` 7 个值；C 端仅 SDT = 3 为活跃，其他记为 `_skipped_` 注释）：
        - `SF_TAE_FORMAT_DS1 = 0`（`_skipped_`，v2）
        - `SF_TAE_FORMAT_SOTFS = 1`（`_skipped_`，v2）
        - `SF_TAE_FORMAT_DS3 = 2`（`_skipped_`，v2，aka BB）
        - `SF_TAE_FORMAT_SDT = 3`（**v1.1 IN-scope**；covers Sekiro + Elden Ring）
        - `SF_TAE_FORMAT_DES = 4`（`_skipped_`，v2）
        - `SF_TAE_FORMAT_DESR = 5`（`_skipped_`，v2）
      - `sf_tae_anim_mini_header_type_t`（对照 `Animation.cs:15-26`）：
        - `SF_TAE_MINI_HEADER_STANDARD = 0`
        - `SF_TAE_MINI_HEADER_IMPORT_OTHER_ANIM = 1`
    - **POD 值类型**（结构体直接暴露，因字段简单且稳定）：
      - `sf_tae_anim_mini_header_standard_t`：is_loop_by_default (bool), allow_delay_load (bool), imports_hkx (bool), import_hkx_source_anim_id (int32_t)。
      - `sf_tae_anim_mini_header_import_t`：import_from_anim_id (int32_t), unknown (int32_t, 默认 -1)。**对照 `Animation.cs:172-198`：SDT 路径读两个 int32（ImportFromAnimID + Unknown），非 bool；上游 DESR 路径字段次序颠倒，DESR 推 v2 不实装**。
      - `sf_tae_anim_mini_header_t`（tagged union）：
        ```c
        struct sf_tae_anim_mini_header {
            sf_tae_anim_mini_header_type_t type;
            bool is_null_header;     /* upstream IsNullHeader */
            union {
                sf_tae_anim_mini_header_standard_t standard;
                sf_tae_anim_mini_header_import_t   import_other;
            } payload;
        };
        ```
    - **公共 accessor API**（对照 `TAE.cs` properties）：
      ```c
      SF_API sf_result_t sf_tae_read_from_memory(sf_tae_t **out,
                                                 const void *bytes, size_t size,
                                                 const sf_allocator_t *a);
      SF_API sf_result_t sf_tae_write_to_memory (const sf_tae_t *t,
                                                 void **out_bytes, size_t *out_size,
                                                 const sf_allocator_t *a);
      SF_API void          sf_tae_destroy(sf_tae_t *t);

      SF_API sf_tae_format_t sf_tae_format(const sf_tae_t *t);
      SF_API int32_t         sf_tae_id(const sf_tae_t *t);
      SF_API const char *    sf_tae_skeleton_name(const sf_tae_t *t);   /* UTF-8 */
      SF_API const char *    sf_tae_sib_name(const sf_tae_t *t);        /* UTF-8 */
      SF_API int64_t         sf_tae_event_bank(const sf_tae_t *t);
      SF_API size_t          sf_tae_animation_count(const sf_tae_t *t);
      SF_API const sf_tae_animation_t *
                            sf_tae_animation(const sf_tae_t *t, size_t i);

      SF_API int64_t         sf_tae_animation_id(const sf_tae_animation_t *a);
      SF_API const sf_tae_anim_mini_header_t *
                            sf_tae_animation_mini_header(const sf_tae_animation_t *a);
      SF_API size_t          sf_tae_animation_event_count(const sf_tae_animation_t *a);
      SF_API const sf_tae_event_t *
                            sf_tae_animation_event(const sf_tae_animation_t *a, size_t i);
      SF_API size_t          sf_tae_animation_event_group_count(const sf_tae_animation_t *a);
      SF_API const sf_tae_event_group_t *
                            sf_tae_animation_event_group(const sf_tae_animation_t *a, size_t i);

      SF_API float           sf_tae_event_start_time(const sf_tae_event_t *e);
      SF_API float           sf_tae_event_end_time  (const sf_tae_event_t *e);
      SF_API int32_t         sf_tae_event_type     (const sf_tae_event_t *e);
      SF_API const uint8_t * sf_tae_event_parameters(const sf_tae_event_t *e, size_t *out_size);

      SF_API int32_t         sf_tae_event_group_type(const sf_tae_event_group_t *g);
      SF_API size_t          sf_tae_event_group_member_count(const sf_tae_event_group_t *g);
      SF_API int32_t         sf_tae_event_group_member(const sf_tae_event_group_t *g, size_t i);
      ```
    - **`_Static_assert`** 后置：
      - `_Static_assert(SF_TAE_FORMAT_DESR == 5, "TAEFormat drift")`（哨兵）
      - `_Static_assert(SF_TAE_MINI_HEADER_IMPORT_OTHER_ANIM == 1, "MiniHeaderType drift")`
    - **所有公共符号 `SF_API` 装饰**。
    - **头部 `#include`** 限制为 `<stddef.h>` + `<stdint.h>` + `<stdbool.h>` + `souls_formats/sf_common.h`。

  **Must NOT do**：
  - ❌ 不暴露 Template / BankTemplate / EventTemplate / ParameterTemplate（OUT-of-scope）。
  - ❌ 不暴露 BigEndian 切换 API（SDT 始终 LE）。
  - ❌ 不暴露 AppliedTemplate / BankTemplate / ApplyTemplate API。
  - ❌ 不放 implementation。
  - ❌ 不在公共头 include `<stdio.h>` 等无关 system header。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 头文件起草。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T8, T9, T10 部分并行；T9 + T10 依赖 T7 + T8 完成）
  - **Parallel Group**: Wave 1
  - **Blocks**: T9, T10, T11-T13, T14, T24
  - **Blocked By**: T1, T2

  **References**：

  **Pattern References**：
  - `include/souls_formats/sf_paramdef.h:34-50` —— enum + `_Static_assert` 范本。
  - `include/souls_formats/sf_emevd.h` —— Phase 4 enum-heavy header 范本。
  - `include/souls_formats/sf_flver.h` —— Phase 6 公共类型分割模式（POD + opaque mix）。

  **API/Type References**：
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/TAE/TAE.cs:全文` —— properties (Format/ID/Flags/SkeletonName/SibName/Animations/EventBank) ground truth。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/TAE/Animation.cs:13-130` —— Animation properties + MiniHeader Standard / ImportOtherAnim 字段。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/TAE/Event.cs:全文` —— Event StartTime/EndTime/Type/Parameters ground truth。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/TAE/EventGroup.cs:全文` —— EventGroup.GroupType + Members（List<int>）。

  **Test References**：无（本 task 仅头文件）。

  **External References**：
  - AGENTS.md §5.x —— API 必须 mirror 上游属性的约束。

  **WHY Each Reference Matters**：
  - TAE.cs / Animation.cs / Event.cs / EventGroup.cs 字段顺序错位 = round-trip 失败。
  - PARAMDEF / EMEVD / FLVER 已建 `_Static_assert` 守护模式；复用。

  **Acceptance Criteria**：
  - [ ] `include/souls_formats/sf_tae.h` 提交且 `cmake -B build-on -DSF_ENABLE_PHASE7=ON ...` 后 `cmake --build build-on --target souls_formats_static` 通过（即便 T11 还没实现，dummy `.c` include 应编译）。
  - [ ] `grep '_Static_assert' include/souls_formats/sf_tae.h` ≥ 2。
  - [ ] `grep 'SF_API' include/souls_formats/sf_tae.h` ≥ 18（所有 accessor 函数）。
  - [ ] `grep -E 'enum sf_tae_(format|anim_mini_header_type)' include/souls_formats/sf_tae.h` 命中 2。

  **QA Scenarios**：

  ```
  Scenario: 头文件通过编译 sanity（OFF 时也不破）
    Tool: Bash
    Steps:
      1. `cmake -B build-mingw-off-test -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_ENABLE_PHASE7=OFF`
      2. `cmake --build build-mingw-off-test --target souls_formats_static 2>&1 | tee .sisyphus/evidence/task-7-build-off.log`
      3. `grep -c 'error' .sisyphus/evidence/task-7-build-off.log`
    Expected Result: 步骤 3 = 0
    Failure Indicators: > 0
    Evidence: .sisyphus/evidence/task-7-build-off.log

  Scenario: Template 类型不出现在公共头
    Tool: Bash
    Steps:
      1. `grep -cE 'Template|BankTemplate|EventTemplate|ParameterTemplate|ApplyTemplate' include/souls_formats/sf_tae.h`
    Expected Result: 0（或仅出现在 `_skipped_` 注释内 — 用 `-v` 排除）
    Failure Indicators: > 0
    Evidence: .sisyphus/evidence/task-7-no-template.log
  ```

  **Commit**: YES
  - Message: `phase7(tae): public header sf_tae.h with opaque types, TAEFormat enum, AnimMiniHeader tagged union, accessor API`
  - Files: `include/souls_formats/sf_tae.h`, `.sisyphus/evidence/task-7-*`
  - Pre-commit: `cmake --build build-on --target souls_formats_static` PASS

- [x] 8. **`sf_fxr3.h` —— opaque types + 6 枚举 + tagged unions + accessor + XML API 原型**

  **What to do**：
  - 起草 `include/souls_formats/sf_fxr3.h`：
    - **opaque typedef**：`sf_fxr3_t`, `sf_fxr3_state_map_t`, `sf_fxr3_state_t`, `sf_fxr3_state_condition_t`, `sf_fxr3_container_t`, `sf_fxr3_effect_t`, `sf_fxr3_action_t`, `sf_fxr3_property_t`, `sf_fxr3_property_modifier_t`, `sf_fxr3_unk_field_list_t`。
    - **公共 enum**（**精确对照** `FXR3.cs:285, 376, 386, 1054, 1231, 1239`）：
      - `sf_fxr3_version_t`（`FXRVersion : ushort`）：
        - `SF_FXR3_VERSION_DARK_SOULS_3 = 4`
        - `SF_FXR3_VERSION_SEKIRO = 5`
      - `sf_fxr3_operator_type_t`（`OperatorType`）：
        - `SF_FXR3_OPERATOR_NOT_EQUAL = 0`
        - `SF_FXR3_OPERATOR_EQUAL = 1`
        - `SF_FXR3_OPERATOR_GE = 2`
        - `SF_FXR3_OPERATOR_GT = 3`
        - `SF_FXR3_OPERATOR_LE = 4`
        - `SF_FXR3_OPERATOR_LT = 5`
      - `sf_fxr3_operand_type_t`（`OperandType`，注意负值）：
        - `SF_FXR3_OPERAND_LITERAL = -4`
        - `SF_FXR3_OPERAND_EXTERNAL = -3`
        - `SF_FXR3_OPERAND_TIME_OF_DAY = -2`
        - `SF_FXR3_OPERAND_STATE_TIME = -1`
      - `sf_fxr3_field_type_t`（`FieldType`）：
        - `SF_FXR3_FIELD_TYPE_INT = 0`
        - `SF_FXR3_FIELD_TYPE_FLOAT = 1`
      - `sf_fxr3_property_type_t`（`PropertyType`）：
        - `SF_FXR3_PROPERTY_TYPE_SCALAR = 0`
        - `SF_FXR3_PROPERTY_TYPE_VECTOR2 = 1`
        - `SF_FXR3_PROPERTY_TYPE_VECTOR3 = 2`
        - `SF_FXR3_PROPERTY_TYPE_COLOR = 3`
      - `sf_fxr3_property_interpolation_type_t`（`PropertyInterpolationType`，8 值）：
        - `SF_FXR3_INTERP_ZERO = 0`
        - `SF_FXR3_INTERP_ONE = 1`
        - `SF_FXR3_INTERP_CONSTANT = 2`
        - `SF_FXR3_INTERP_STEPPED = 3`
        - `SF_FXR3_INTERP_LINEAR = 4`
        - `SF_FXR3_INTERP_CURVE1 = 5`
        - `SF_FXR3_INTERP_CURVE2 = 6`
        - `SF_FXR3_INTERP_UNK_AC6 = 7`
    - **tagged union POD（直接暴露字段，因稳定）**：
      ```c
      typedef struct sf_fxr3_field {
          sf_fxr3_field_type_t type;
          union {
              int32_t  as_int;
              float    as_float;
          } value;
      } sf_fxr3_field_t;

      typedef struct sf_fxr3_operand {
          sf_fxr3_operand_type_t type;
          union {
              float    as_literal;     /* OperandType.Literal */
              int32_t  as_external;    /* OperandType.External */
              /* StateTime / TimeOfDay 无 payload */
          } value;
      } sf_fxr3_operand_t;
      ```
    - **`_Static_assert`** 守护：
      - `_Static_assert(SF_FXR3_VERSION_SEKIRO == 5, "FXRVersion drift")`
      - `_Static_assert(SF_FXR3_OPERATOR_LT == 5, "OperatorType drift")`
      - `_Static_assert(SF_FXR3_OPERAND_LITERAL == -4, "OperandType.Literal drift")`
      - `_Static_assert(SF_FXR3_OPERAND_STATE_TIME == -1, "OperandType.StateTime drift")`
      - `_Static_assert(SF_FXR3_FIELD_TYPE_FLOAT == 1, "FieldType drift")`
      - `_Static_assert(SF_FXR3_PROPERTY_TYPE_COLOR == 3, "PropertyType drift")`
      - `_Static_assert(SF_FXR3_INTERP_UNK_AC6 == 7, "InterpolationType drift")`
    - **公共 accessor API（binary）**：
      ```c
      SF_API sf_result_t sf_fxr3_read_from_memory(sf_fxr3_t **out,
                                                  const void *bytes, size_t size,
                                                  const sf_allocator_t *a);
      SF_API sf_result_t sf_fxr3_write_to_memory (const sf_fxr3_t *f,
                                                  void **out_bytes, size_t *out_size,
                                                  const sf_allocator_t *a);
      SF_API void          sf_fxr3_destroy(sf_fxr3_t *f);

      SF_API sf_fxr3_version_t sf_fxr3_version(const sf_fxr3_t *f);
      SF_API int32_t           sf_fxr3_id     (const sf_fxr3_t *f);

      SF_API const sf_fxr3_state_map_t *  sf_fxr3_root_state_map(const sf_fxr3_t *f);
      SF_API const sf_fxr3_container_t *  sf_fxr3_root_container(const sf_fxr3_t *f);

      SF_API size_t         sf_fxr3_reference_count   (const sf_fxr3_t *f);
      SF_API int32_t        sf_fxr3_reference(const sf_fxr3_t *f, size_t i);
      SF_API size_t         sf_fxr3_external_value_count(const sf_fxr3_t *f);
      SF_API int32_t        sf_fxr3_external_value (const sf_fxr3_t *f, size_t i);
      SF_API size_t         sf_fxr3_unk_blood_enabler_count(const sf_fxr3_t *f);
      SF_API int32_t        sf_fxr3_unk_blood_enabler  (const sf_fxr3_t *f, size_t i);

      SF_API size_t        sf_fxr3_state_map_state_count(const sf_fxr3_state_map_t *m);
      SF_API const sf_fxr3_state_t *
                          sf_fxr3_state_map_state(const sf_fxr3_state_map_t *m, size_t i);

      SF_API size_t                          sf_fxr3_state_condition_count(const sf_fxr3_state_t *s);
      SF_API const sf_fxr3_state_condition_t *
                          sf_fxr3_state_condition(const sf_fxr3_state_t *s, size_t i);

      SF_API sf_fxr3_operator_type_t sf_fxr3_condition_operator    (const sf_fxr3_state_condition_t *c);
      SF_API sf_fxr3_operand_t       sf_fxr3_condition_left_operand (const sf_fxr3_state_condition_t *c);
      SF_API sf_fxr3_operand_t       sf_fxr3_condition_right_operand(const sf_fxr3_state_condition_t *c);
      SF_API int32_t                 sf_fxr3_condition_next_state  (const sf_fxr3_state_condition_t *c);

      SF_API size_t                    sf_fxr3_container_id   (const sf_fxr3_container_t *c);
      SF_API size_t                    sf_fxr3_container_child_count(const sf_fxr3_container_t *c);
      SF_API const sf_fxr3_container_t *
                                      sf_fxr3_container_child(const sf_fxr3_container_t *c, size_t i);
      SF_API size_t                    sf_fxr3_container_effect_count(const sf_fxr3_container_t *c);
      SF_API const sf_fxr3_effect_t *
                                      sf_fxr3_container_effect(const sf_fxr3_container_t *c, size_t i);

      SF_API size_t                    sf_fxr3_effect_action_count(const sf_fxr3_effect_t *e);
      SF_API const sf_fxr3_action_t *
                                      sf_fxr3_effect_action(const sf_fxr3_effect_t *e, size_t i);

      SF_API size_t                    sf_fxr3_action_property_count(const sf_fxr3_action_t *a);
      SF_API const sf_fxr3_property_t *
                                      sf_fxr3_action_property(const sf_fxr3_action_t *a, size_t i);

      SF_API sf_fxr3_property_type_t  sf_fxr3_property_type(const sf_fxr3_property_t *p);
      SF_API sf_fxr3_property_interpolation_type_t
                                      sf_fxr3_property_interpolation(const sf_fxr3_property_t *p);
      SF_API bool                     sf_fxr3_property_is_loop(const sf_fxr3_property_t *p);
      SF_API size_t                   sf_fxr3_property_field_count(const sf_fxr3_property_t *p);
      SF_API sf_fxr3_field_t          sf_fxr3_property_field(const sf_fxr3_property_t *p, size_t i);
      SF_API size_t                   sf_fxr3_property_modifier_count(const sf_fxr3_property_t *p);
      SF_API const sf_fxr3_property_modifier_t *
                                      sf_fxr3_property_modifier(const sf_fxr3_property_t *p, size_t i);
      ```
    - **公共 XML API 原型**（实现在 T21/T22）：
      ```c
      SF_API sf_result_t sf_fxr3_from_xml(sf_fxr3_t **out,
                                          const char *xml_utf8, size_t xml_size,
                                          const sf_allocator_t *a);
      SF_API sf_result_t sf_fxr3_to_xml  (const sf_fxr3_t *f,
                                          char **out_xml_utf8, size_t *out_size,
                                          const sf_allocator_t *a);
      ```
    - **所有公共符号 `SF_API` 装饰**。
    - 头部 `#include` 限制为 `<stddef.h>` + `<stdint.h>` + `<stdbool.h>` + `souls_formats/sf_common.h`。

  **Must NOT do**：
  - ❌ 不暴露 PropertyModifier / UnkFieldList 内部字段（仅 count + opaque accessor，因 schema 复杂且 v1.1 不 typed）。
  - ❌ 不暴露 mxml 内部类型（mxml_node_t* 等）到公共头。
  - ❌ 不为 XML 错误添加新 `sf_result_t` enum（用现有 `SF_ERR_INTERNAL` 或 `SF_ERR_INVALID_ARG`）。
  - ❌ 不放 implementation。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 头文件起草。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T7 并行）
  - **Parallel Group**: Wave 1
  - **Blocks**: T9, T10, T15-T22, T23, T25
  - **Blocked By**: T1, T2

  **References**：

  **Pattern References**：
  - `include/souls_formats/sf_msb.h` —— Phase 5 中复杂 opaque + tagged accessor 模式。
  - `include/souls_formats/sf_flver2.h` —— Phase 6 中 layered opaque accessor 模式。

  **API/Type References**：
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FXR3.cs:全文`（特别是 19-1500）—— class hierarchy + XmlSerializer 注解 ground truth。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FXR3.cs:285-289` —— FXRVersion enum 字节值。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FXR3.cs:376-392` —— OperatorType / OperandType 字节值（含负值）。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FXR3.cs:1054-1058` —— FieldType。
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FXR3.cs:1231-1249` —— PropertyType / PropertyInterpolationType。

  **External References**：无。

  **WHY Each Reference Matters**：
  - 枚举值必须与上游 byte 值一致（file-format-defined）。
  - OperandType 含**负值**（-4 / -3 / -2 / -1），与 OperatorType (0..5) 不冲突 —— C 端用有符号 int，不要 cast 为 unsigned。
  - FXR3.cs:tagged union 是 polymorphic 类型，C 端用 type tag + union 严格 mirror。

  **Acceptance Criteria**：
  - [ ] `include/souls_formats/sf_fxr3.h` 提交且 `cmake -B build-on -DSF_ENABLE_PHASE7=ON ...` 后 `cmake --build build-on --target souls_formats_static` 通过。
  - [ ] `grep '_Static_assert' include/souls_formats/sf_fxr3.h` ≥ 7（6 枚举哨兵 + 1 union sentinel）。
  - [ ] `grep 'SF_API' include/souls_formats/sf_fxr3.h` ≥ 30（accessor + XML API）。
  - [ ] `grep -E 'enum sf_fxr3_(version|operator_type|operand_type|field_type|property_type|property_interpolation_type)' include/souls_formats/sf_fxr3.h` 命中 6。

  **QA Scenarios**：

  ```
  Scenario: 头文件通过编译 sanity
    Tool: Bash
    Steps:
      1. `cmake -B build-mingw-phase7 -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_ENABLE_PHASE7=ON`
      2. `cmake --build build-mingw-phase7 --target souls_formats_static 2>&1 | tee .sisyphus/evidence/task-8-build.log`
      3. `grep -c 'error' .sisyphus/evidence/task-8-build.log`
    Expected Result: 步骤 3 = 0
    Failure Indicators: > 0
    Evidence: .sisyphus/evidence/task-8-build.log

  Scenario: 枚举值与上游字节一致
    Tool: Bash
    Steps:
      1. `grep -E 'SF_FXR3_VERSION_(DARK_SOULS_3|SEKIRO)' include/souls_formats/sf_fxr3.h | grep -cE '= 4|= 5'`
      2. `grep -E 'SF_FXR3_OPERAND_(LITERAL|EXTERNAL|TIME_OF_DAY|STATE_TIME)' include/souls_formats/sf_fxr3.h | grep -cE '= -4|= -3|= -2|= -1'`
    Expected Result: 步骤 1 = 2；步骤 2 = 4
    Failure Indicators: 任一不一致 → 枚举 drift
    Evidence: .sisyphus/evidence/task-8-enum-check.log
  ```

  **Commit**: YES
  - Message: `phase7(fxr3): public header sf_fxr3.h with opaque types, 6 enums, ConditionOperand + Field tagged unions, XML API prototypes`
  - Files: `include/souls_formats/sf_fxr3.h`, `.sisyphus/evidence/task-8-*`
  - Pre-commit: build PASS

- [x] 9. **CMakeLists.txt 接线 `SF_ENABLE_PHASE7` + 4 个 placeholder 空源文件（避免 T10 之前 ON build 缺源）**

  **What to do**：
  - **Step 0：先创建 4 个 warning-clean placeholder `.c` 文件**（避免后续 CMake ON build step 在 T10 实装前因 source 缺失而失败）：
    - **关键**：项目 `cmake/compiler_warnings.cmake:16-20` 启用 `-Wpedantic -Werror`，空 translation unit 会触发 `ISO C forbids an empty translation unit` 而失败。placeholder 必须**非空且 warning-clean，但不产生任何 `sf_` 公共符号**。建议格式：
      ```c
      /* SPDX-License-Identifier: GPL-3.0-or-later */
      /* Phase 7 placeholder — T10 replaces with opaque struct + stub destroy. */
      typedef int phase7_placeholder_tae;  /* file-scope typedef; no symbol */
      ```
    - 4 个文件（每个唯一 typedef 名避免重名）：
      - `src/effects/tae.c`：`typedef int phase7_placeholder_tae;`
      - `src/effects/fxr3.c`：`typedef int phase7_placeholder_fxr3;`
      - `src/effects/fxr3_xml_read.c`：`typedef int phase7_placeholder_fxr3_xml_read;`
      - `src/effects/fxr3_xml_write.c`：`typedef int phase7_placeholder_fxr3_xml_write;`
    - **目的**：让 T9 的 CMake ON build 立刻 PASS（warning-clean `.c` 编为 0 公共符号 object）；T10 之后用 `Edit` 工具替换 typedef 行为完整 stub 内容（保留 SPDX header）。
  - 修改 `CMakeLists.txt`：
    - `SF_PUBLIC_HEADERS` 列表（约 47-81 行）：在末尾加 `if(SF_ENABLE_PHASE7) ... endif()` 块，列出：
      - `include/souls_formats/sf_tae.h`
      - `include/souls_formats/sf_fxr3.h`
    - `SF_SOURCES` 列表（约 82-160 行）：同样加 `if(SF_ENABLE_PHASE7) ... endif()` 块，列出：
      - `src/effects/tae.c`
      - `src/effects/fxr3.c`
      - `src/effects/fxr3_xml_read.c`
      - `src/effects/fxr3_xml_write.c`
    - **不动 `option(SF_ENABLE_PHASE7 ...)` 行**（line 15 已就位）。
    - **不引入** `target_compile_definitions(... SF_ENABLE_PHASE7=1)` 桥（**避免 PRIVATE 不传播到测试 / PUBLIC 让 OFF build 也暴露 ifdef 路径**）。改用**纯 CMake 源列表 gating**：SF_SOURCES 条件块决定 .c 文件是否参与编译；公共头 **始终被 umbrella include 且无 `#ifdef SF_ENABLE_PHASE7` 内部守护**（始终声明 API）。消费者用 `SF_ENABLE_PHASE7=OFF` build 的库 + include sf_tae.h → 编译期 OK，但链接 `sf_tae_*` 符号时报 unresolved external —— 这是预期行为，提示消费者需要 ON build。
    - 修改 `include/souls_formats/souls_formats.h`（umbrella）：末尾**无条件** include 两个新头（`#include "sf_tae.h"` / `#include "sf_fxr3.h"`）。
    - **不**为 Phase 7 单独 link mxml：`CMakeLists.txt:187` 已 link `mxml_static`（Phase 4 起即如此，Phase 7 fxr3_xml_*.c 直接复用同一 link 项）。
    - 顶层 CMakeLists.txt 修改范围：仅 SF_PUBLIC_HEADERS / SF_SOURCES 两个 list 内的 `if(SF_ENABLE_PHASE7) list(APPEND ...) endif()` 块。

  **Must NOT do**：
  - ❌ 不改 `option(SF_ENABLE_PHASE7 ...)` 默认值（保持 OFF）。
  - ❌ 不把 Phase 7 源加入 SF_SOURCES 主列表（必须在 `if(SF_ENABLE_PHASE7)` 块内）。
  - ❌ 不引入新第三方依赖。
  - ❌ 不修改 Phase 0-6 已稳定的 CMake 块（特别是 CMakeLists.txt:187 现有 `mxml_static` link —— 复用，不重复）。
  - ❌ **不在两个新公共头内部加 `#ifdef SF_ENABLE_PHASE7` 整文件守护**（避免 PRIVATE compile-def 不传播到测试目标）。
  - ❌ **不加 `target_compile_definitions(... SF_ENABLE_PHASE7=1)`**（gating 由 SF_SOURCES 条件块完成）。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— CMake 编辑。
  - **Skills**: 无（cmake.org 文档已熟）。

  **Parallelization**：
  - **Can Run In Parallel**: NO（T7 + T8 完成后才能引用新头；与 T10 串行）
  - **Parallel Group**: Wave 1（T9 在 T10 前）
  - **Blocks**: T10-T22
  - **Blocked By**: T7, T8

  **References**：

  **Pattern References**：
  - `CMakeLists.txt:15` —— `SF_ENABLE_PHASE7` option 已在；保留。
  - `cmake/deps/mxml.cmake` —— Phase 4 已引入 mxml。
  - `tests/CMakeLists.txt:3` —— Unity 接入模式。

  **API/Type References**：
  - CMake `if(...) endif()` 块语法 + `target_compile_definitions` 的 `$<...:...>` generator expression。

  **WHY Each Reference Matters**：
  - 条件块确保 OFF 时 Phase 7 二进制符号不进 library；F1 reviewer 会用 `nm` 验证。
  - mxml 已在依赖图但 Phase 4 仅 paramdef 用；Phase 7 需要显式 link 保险。

  **Acceptance Criteria**：
  - [ ] `src/effects/{tae,fxr3,fxr3_xml_read,fxr3_xml_write}.c` 4 个 placeholder 文件存在（SPDX header + 1 唯一 typedef 行 + 无 include 无函数；通过 `-Wpedantic -Werror`）。
  - [ ] `cmake -B build-on -DSF_ENABLE_PHASE7=ON ...` 配置无 warning。
  - [ ] `cmake -B build-off -DSF_ENABLE_PHASE7=OFF ...` 配置无 warning。
  - [ ] `cmake --build build-on --target souls_formats_static` 与 `cmake --build build-off --target souls_formats_static` 均成功（**`-Werror` 下也 PASS**）。
  - [ ] `nm build-off/libsouls_formats.a 2>/dev/null | grep -E 'sf_(tae|fxr3)_' | wc -l` == 0。
  - [ ] `nm build-on/libsouls_formats.a 2>/dev/null | grep -E 'sf_(tae|fxr3)_destroy' | wc -l` == 0（T9 仅 placeholder，T10 才加 destroy）。

  **QA Scenarios**：

  ```
  Scenario: 双向条件 build 干净
    Tool: Bash
    Steps:
      1. `rm -rf build-on build-off`
      2. `cmake -B build-on -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_ENABLE_PHASE7=ON 2>&1 | tee .sisyphus/evidence/task-9-cfg-on.log`
      3. `cmake -B build-off -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_ENABLE_PHASE7=OFF 2>&1 | tee .sisyphus/evidence/task-9-cfg-off.log`
      4. `grep -E 'warning|error' .sisyphus/evidence/task-9-cfg-on.log .sisyphus/evidence/task-9-cfg-off.log`
    Expected Result: 步骤 4 输出空（两 config 均无 warning / error）
    Failure Indicators: 命中 warning / error
    Evidence: .sisyphus/evidence/task-9-cfg-*.log

  Scenario: OFF build 不含 Phase 7 符号
    Tool: Bash
    Preconditions: T9 落地（含 placeholder .c）
    Steps:
      1. `cmake --build build-off --target souls_formats_static 2>&1 | tail -5`
      2. `nm build-off/libsouls_formats.a 2>/dev/null | grep -c 'sf_tae\|sf_fxr3' || true`
    Expected Result: 步骤 2 = 0（OFF build 不编 placeholder）
    Failure Indicators: > 0
    Evidence: .sisyphus/evidence/task-9-off-symbols.log

  Scenario: ON build 也 PASS（placeholder 空 .c 编为空 object）
    Tool: Bash
    Preconditions: T9 落地（含 placeholder .c）
    Steps:
      1. `cmake --build build-on --target souls_formats_static 2>&1 | tail -5`
      2. `nm build-on/libsouls_formats.a 2>/dev/null | grep -cE 'sf_(tae|fxr3)_destroy' || true`
    Expected Result: 步骤 1 退出码 0；步骤 2 = 0（placeholder 无 destroy 实装，destroy 由 T10 加入）
    Failure Indicators: 步骤 1 失败 = source 缺失或语法错误；或步骤 2 ≠ 0（说明 T9 不该实装 destroy）
    Evidence: .sisyphus/evidence/task-9-on-build.log
  ```

  **Commit**: YES
  - Message: `phase7(cmake): wire SF_ENABLE_PHASE7 to SF_SOURCES + SF_PUBLIC_HEADERS conditional block (with placeholder src/effects/*.c)`
  - Files: `CMakeLists.txt`, `include/souls_formats/souls_formats.h`, `src/effects/tae.c`, `src/effects/fxr3.c`, `src/effects/fxr3_xml_read.c`, `src/effects/fxr3_xml_write.c`, `.sisyphus/evidence/task-9-*`
  - Pre-commit: ON build + OFF build 均 PASS（warning-clean placeholder 编为 0 公共符号 object，`-Wpedantic -Werror` 下通过）

- [x] 10. **`src/effects/` 4 个 placeholder .c 文件填充为正式 stub（opaque struct + destroy + accessor 占位）**

  **What to do**：
  - 前置：T9 已在 `src/effects/` 创建 4 个 warning-clean placeholder（SPDX header + `typedef int phase7_placeholder_*;`）。本 task **`Edit` 这 4 个文件**：删除 placeholder typedef 行，添加完整 stub 内容（**不新建文件**）。
  - 4 个文件（**无 #ifdef 守护** —— 仅由 CMake SF_SOURCES 条件块决定是否参与编译，避免 PRIVATE compile-def 不传播测试目标的问题）：
    - `src/effects/tae.c`：定义 opaque `struct sf_tae { ... }` 与 `sf_tae_animation` / `sf_tae_event` / `sf_tae_event_group` / `sf_tae_anim_mini_header`（成员可暂为 minimal occupancy：`sf_allocator_t alloc;` + 各 list head 占位）；实现 `sf_tae_destroy(sf_tae_t *t)` 单一 free（释放 `t` 本身）；其他 accessor 函数返回 `0` / `NULL` / `SF_TAE_FORMAT_SDT` 等占位值（Wave 2 T11-T13 才实装）。
    - `src/effects/fxr3.c`：同上，定义所有 opaque struct + stub destroy；其他 accessor 占位返回。
    - `src/effects/fxr3_xml_read.c`：仅 `#include "souls_formats/sf_fxr3.h"` + `sf_fxr3_from_xml` stub 返回 `SF_ERR_INTERNAL`（T21 实装）。
    - `src/effects/fxr3_xml_write.c`：同上 `sf_fxr3_to_xml` stub 返回 `SF_ERR_INTERNAL`（T22 实装）。
  - 每个文件首行注释引用对应上游 `.cs` 文件（保留 STRICT UPSTREAM REFERENCE trace）。
  - 全部文件用 `clang-format -i` 格式化（沿用项目 `.clang-format`）。

  **Must NOT do**：
  - ❌ 不实装实际逻辑（仅 destroy + stubs；具体 read/write 由 Wave 2/3 实装）。
  - ❌ 不创建 `src/effects/CMakeLists.txt`（顶层 CMakeLists.txt 已通过 SF_SOURCES 直接含入）。
  - ❌ 不在 stub 中使用 `assert(false)` 阻止运行（用 `return SF_ERR_INTERNAL` / `return NULL`，便于 OFF build 验证）。
  - ❌ 不在 stub 中 include mxml（T21/T22 才用）。

  **Recommended Agent Profile**：
  - **Category**: `quick` —— 4 个 stub 文件起草。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（T9 后才能 build；与 T11+ 串行）
  - **Parallel Group**: Wave 1（最后一个 task）
  - **Blocks**: T11-T22
  - **Blocked By**: T7, T8, T9

  **References**：

  **Pattern References**：
  - `src/geom/mtd.c` —— Phase 6 单 module stub 起步模式（顶部上游 trace 注释）。
  - `src/script/esd.c` —— Phase 4/5 stub 起步模式。

  **API/Type References**：
  - `include/souls_formats/sf_common.h` —— `sf_allocator_t` + `sf_free()` API。
  - `include/souls_formats/sf_tae.h` / `sf_fxr3.h`（T7/T8 提交）。

  **External References**：
  - `.clang-format` —— 项目格式规范。

  **WHY Each Reference Matters**：
  - stub 文件让 Wave 2/3 task 各自接管对应文件而无 merge conflict。
  - destroy 实装确保 OFF/ON 双 build 都能 link（destroy 是必须的最小可用面）。

  **Acceptance Criteria**：
  - [ ] `src/effects/{tae,fxr3,fxr3_xml_read,fxr3_xml_write}.c` 4 文件存在。
  - [ ] **无** `#ifdef SF_ENABLE_PHASE7` 守护（OFF build 通过 CMake list 排除，非 preprocessor 排除）。
  - [ ] `cmake --build build-on --target souls_formats_static` PASS 无 warning。
  - [ ] `cmake --build build-mingw-off --target souls_formats_static` PASS（OFF 时 4 个 .c 编译为空 object）。
  - [ ] `clang-format --dry-run --Werror src/effects/*.c` 静默通过。

  **QA Scenarios**：

  ```
  Scenario: ON build 链接通过
    Tool: Bash
    Steps:
      1. `cmake --build build-on --target souls_formats_static 2>&1 | tee .sisyphus/evidence/task-10-build-on.log`
      2. `nm build-on/libsouls_formats.a | grep sf_tae_destroy`
      3. `nm build-on/libsouls_formats.a | grep sf_fxr3_destroy`
    Expected Result: 步骤 2 + 3 各有 1 行命中
    Failure Indicators: 任一未命中
    Evidence: .sisyphus/evidence/task-10-build-on.log

  Scenario: OFF build 无 Phase 7 符号
    Tool: Bash
    Steps:
      1. `cmake --build build-off --target souls_formats_static 2>&1 | tail -5`
      2. `nm build-off/libsouls_formats.a | grep -cE 'sf_(tae|fxr3)_'`
    Expected Result: 步骤 2 = 0
    Failure Indicators: > 0
    Evidence: .sisyphus/evidence/task-10-build-off.log

  Scenario: clang-format 静默
    Tool: Bash
    Steps:
      1. `clang-format --dry-run --Werror src/effects/*.c 2>&1 | tee .sisyphus/evidence/task-10-fmt.log`
    Expected Result: 输出空
    Failure Indicators: 任何 warning
    Evidence: .sisyphus/evidence/task-10-fmt.log
  ```

  **Commit**: YES
  - Message: `phase7(scaffold): src/effects/ stubs (tae.c, fxr3.c, fxr3_xml_read.c, fxr3_xml_write.c) with #ifdef gate and destroy stubs`
  - Files: `src/effects/tae.c`, `src/effects/fxr3.c`, `src/effects/fxr3_xml_read.c`, `src/effects/fxr3_xml_write.c`, `.sisyphus/evidence/task-10-*`
  - Pre-commit: ON + OFF build 均 PASS

### Wave 2 — TAE 实现（Wave 1 全绿后，与 Wave 3 并行）

- [x] 11. **`src/effects/tae.c` read — Header parse（SDT version 0x1000D 守门）+ Animation list + AnimMiniHeader tagged union**

  **What to do**：
  - 实现 `sf_tae_read_from_memory(...)`：
    - 创建 `sf_binary_reader_t` over input bytes。
    - 读 magic `AssertASCII("TAE ")`（注意末尾空格，4 字节）。
    - 读 BigEndian byte = `AssertByte(0)`（v1.1 SDT 仅 LE，其他 → `SF_ERR_UNSUPPORTED_VERSION`）。
    - 读 unk byte + AssertByte 序列（对照 `TAE.cs:181-198`）。
    - 读 version i32 = `AssertInt32(0x1000D)`（其他 version → `SF_ERR_UNSUPPORTED_VERSION`；Phase 7 不实装 legacy）。
    - 设 `Format = SDT`、`is64Bit = true`。
    - 读 `AssertVarint(0x40)` + `AssertVarint(0x50)`（对照 `TAE.cs:261-263` SDT 路径）。
    - 读 fileSize、`AssertVarint(0xA0)`、`AssertVarint(0x90)`、`AssertVarint(0x88)`。
    - 读 Flags byte[8]。
    - 读 ID + AnimCount + AnimOffset + EventBank。
    - 读 SkeletonName / SibName via offset（解码 UTF-16 LE → UTF-8 通过 sf_encoding）。
    - StepIn(AnimOffset)：循环 AnimCount 次创建 Animation：
      - 读 `Animation.ID`（i64 / i32 视 is64Bit）。
      - 读 MiniHeaderOffset、EventOffset、EventCount、EventGroupOffset、EventGroupCount、AnimFileOffset。
      - 切到 MiniHeaderOffset，读 4 字节 `MiniHeaderType` enum (uint32)：
        - == 0 (Standard) → 读 IsLoopByDefault / AllowDelayLoad / ImportsHKX / 4 padding + ImportHKXSourceAnimID (int32)。
        - == 1 (ImportOtherAnim) → 读 ImportFromAnimID (int32) + Unknown (int32，默认 -1)。SDT 格式无 padding；DES 格式才有 Pad(0x10)，DES 推 v2 不走此分支（对照 `Animation.cs:182-198`）。
        - 其他 → `SF_ERR_UNSUPPORTED_VERSION`。
      - 暂不读 Event list（T12 任务）；暂不读 EventGroup list（T12）。
  - 错误路径：所有 SF_ERR_* return 前必须 free 已分配的部分（避免泄漏）。
  - 内存模型：`sf_tae_t` 内部用 klib `kvec_t(sf_tae_animation_t *)` 储存 animation array。

  **Must NOT do**：
  - ❌ 不读取 Event / EventGroup（T12 任务）。
  - ❌ 不实装非 SDT 路径（version ≠ 0x1000D → 立即返回 `SF_ERR_UNSUPPORTED_VERSION`）。
  - ❌ 不解码 ParameterContainer（T12 仅记字节范围）。
  - ❌ 不写 `printf` 调试。
  - ❌ 不依赖 mxml（mxml 仅 fxr3 XML 用）。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— 字节级 header parse + tagged union 分支。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（T11 → T12 → T13 串行；与 Wave 3 并行）
  - **Parallel Group**: Wave 2
  - **Blocks**: T12, T13, T14, T24
  - **Blocked By**: T7, T9, T10

  **References**：

  **Pattern References**：
  - `src/script/esd.c` —— Phase 5 中类似 header + list-of-list parse 模式。
  - `src/script/emevd.c` —— Phase 4 同款 Reserve/Fill + StepIn/Out 风格。

  **API/Type References**：
  - `Formats/TAE/TAE.cs:179-300`（SDT 分支）—— header 字段次序 ground truth。
  - `Formats/TAE/Animation.cs:13-200` —— Animation read + MiniHeader 分支。
  - `include/souls_formats/sf_io.h` —— BinaryReaderEx API。

  **WHY Each Reference Matters**：
  - 上游 TAE.cs SDT 分支是字段次序唯一 ground truth；偏差 1 字节 = 全部 round-trip 失败。
  - klib kvec 是项目内默认动态数组（Phase 1-6 一致）。

  **Acceptance Criteria**：
  - [ ] `src/effects/tae.c` 含 `sf_tae_read_from_memory` 完整实装（Header + Animation list + MiniHeader tagged union）。
  - [ ] T4 probe 中第一个 .tae 样本调 `sf_tae_read_from_memory` 返回 `SF_OK` + AnimCount > 0。
  - [ ] 非 SDT version 样本（合成 byte stream）返回 `SF_ERR_UNSUPPORTED_VERSION`。

  **QA Scenarios**：

  ```
  Scenario: T4 probe 第一样本 header parse
    Tool: Bash
    Preconditions: T4 完成，evidence 中有 sample 路径
    Steps:
      1. `cmake --build build-on --target souls_formats_static`
      2. 写 tests/anim/test_tae_header_parse.c：调 er_extract → 取第一个 .tae → sf_tae_read_from_memory → 断言 AnimCount > 0
      3. ctest 跑该测试
    Expected Result: PASS
    Failure Indicators: SF_ERR_*；AnimCount == 0
    Evidence: .sisyphus/evidence/task-11-header-parse.log

  Scenario: 非 SDT version 拒绝
    Tool: Bash
    Steps:
      1. 合成 byte stream 含 version 0x1000C（DS3）
      2. sf_tae_read_from_memory(..., &t, NULL) → assert == SF_ERR_UNSUPPORTED_VERSION
    Expected Result: 正确拒绝
    Failure Indicators: SF_OK 或 crash
    Evidence: .sisyphus/evidence/task-11-non-sdt-reject.log
  ```

  **Commit**: 与 T12, T13 合并提交（共享 src/effects/tae.c）

- [x] 12. **`src/effects/tae.c` read — Event list + EventGroup + ParameterContainer opaque bytes**

  **What to do**：
  - 接续 T11 实装的 Animation list reader：对每个 Animation：
    - StepIn(EventOffset)：循环 EventCount 次创建 Event：
      - 读 EventStartTime offset / EventEndTime offset / EventDataOffset / Type uint32（对照 `Event.cs:Read` SDT 路径）。
      - 通过 offset 读 StartTime (float32) / EndTime (float32) at 各 offset。
      - 从 EventDataOffset 读 Parameters 区段 —— **作为 opaque bytes**：用 next Event 的 DataOffset 或 Animation 末尾推断长度（同上游 `Event.cs` 实现）；分配 `uint8_t *parameters` + `size_t parameters_size`。
    - StepIn(EventGroupOffset)：循环 EventGroupCount 次创建 EventGroup：
      - 读 GroupType (i32) + IndexCount (i32) + EventIndexOffset (i32)。
      - StepIn(EventIndexOffset)：读 IndexCount × int32 → member 数组。
  - **不实装** Template apply / parameter typed decode（OUT-of-scope）。
  - 错误路径：partial-allocated Animation/Event/EventGroup 在 `sf_tae_destroy` 中统一释放。

  **Must NOT do**：
  - ❌ 不解析 Parameters 字节为 typed 字段（保持 opaque）。
  - ❌ 不调用 `sf_tae_apply_template` 之类函数（不存在）。
  - ❌ 不在 read 后改 SkeletonName / SibName。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— Event list + EventGroup 中等复杂度。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（与 T11/T13 串行）
  - **Parallel Group**: Wave 2
  - **Blocks**: T13, T14, T24
  - **Blocked By**: T11

  **References**：

  **API/Type References**：
  - `Formats/TAE/Event.cs:全文`（重点 12-280）—— Event read 字段次序 + ParameterContainer 处理。
  - `Formats/TAE/EventGroup.cs:全文` —— EventGroup read。

  **WHY Each Reference Matters**：
  - ParameterContainer 大小推断需精确（取相邻 Event 的 DataOffset 差，或 Animation 末尾）；偏移错 1 字节 = round-trip 失败。

  **Acceptance Criteria**：
  - [ ] T4 probe 第一样本 read 后：`sf_tae_animation_event_count(anim_0) > 0`。
  - [ ] 任一 Event 的 `sf_tae_event_parameters(e, &size)` 返回非 NULL + size 与上游一致。
  - [ ] T4 probe 第一样本：`sf_tae_animation_event_group_count` 与上游 EventGroupCount 一致。

  **QA Scenarios**：

  ```
  Scenario: Event + EventGroup count 与上游一致
    Tool: Bash
    Steps:
      1. 写测试 binary：T4 第一样本 read → 与 .sisyphus/evidence/task-4-tae-probe.md 中记录的 expected 对比
    Expected Result: count 全部一致
    Failure Indicators: 不一致
    Evidence: .sisyphus/evidence/task-12-event-count.log
  ```

  **Commit**: 与 T11, T13 合并

- [x] 13. **`src/effects/tae.c` write — Header / Animation / Event / EventGroup / MiniHeader / ParameterContainer 反向写**

  **What to do**：
  - 实装 `sf_tae_write_to_memory(...)`：
    - 创建 `sf_binary_writer_t`（LE）。
    - 写 magic `"TAE "` + BigEndian = 0 + unk byte 序列。
    - 写 version 0x1000D + 各 Varint header（与上游 SDT 路径对称）。
    - 写 Flags + ID + AnimCount + 各 Reserve offset。
    - 写 SkeletonName / SibName 字符串（UTF-8 → UTF-16 LE via sf_encoding）。
    - 对每个 Animation：写 ID + Reserve MiniHeaderOffset / EventOffset / EventCount / EventGroupOffset / EventGroupCount / AnimFileOffset。
    - 第二轮 pass：FillInt32 MiniHeaderOffset → 写 MiniHeader（type-tagged 分支）。
    - 第三轮 pass：FillInt32 EventOffset → 写 Event 列表（StartTime / EndTime 通过 offset 间接，Parameters 字节透传）。
    - 第四轮 pass：FillInt32 EventGroupOffset → 写 EventGroup 列表。
    - 调 `sf_binary_writer_finish()` 验证所有 Reserve 都 Fill。
  - 返回堆缓冲（caller 用 `sf_free` 释放）。

  **Must NOT do**：
  - ❌ 不重写 Parameters 字节（直接透传 T12 read 时存的 opaque bytes）。
  - ❌ 不重算 StartTime / EndTime（保留 read 时的值）。
  - ❌ 不留任何未填的 Reserve（finish 必须返回 SF_OK）。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— Reserve/Fill 多轮 pass + 与 read 对称。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（与 T11, T12 串行）
  - **Parallel Group**: Wave 2
  - **Blocks**: T14, T24
  - **Blocked By**: T11, T12

  **References**：

  **API/Type References**：
  - `Formats/TAE/TAE.cs:Write()` SDT 路径 —— 写出字节次序 ground truth。
  - `include/souls_formats/sf_io.h` —— Reserve_* / Fill_* / finish API（Phase 1 已交付）。

  **WHY Each Reference Matters**：
  - 与 read 对称是 round-trip 字节级一致的前提；任何 byte 错位 = T14 测试失败。

  **Acceptance Criteria**：
  - [ ] T4 probe 第一样本：read → write → `memcmp(in, out, size) == 0`。
  - [ ] `sf_binary_writer_finish` 返回 SF_OK（无未填 Reserve）。

  **QA Scenarios**：

  ```
  Scenario: 真实 .tae round-trip
    Tool: Bash
    Steps:
      1. 写测试：T4 第一样本 → read → write → memcmp
    Expected Result: memcmp == 0
    Failure Indicators: 任何字节差异
    Evidence: .sisyphus/evidence/task-13-rt.log + input.bin + output.bin
  ```

  **Commit**: YES（合并 T11, T12, T13）
  - Message: `phase7(tae): SDT format read+write of Header, Animation, Event, EventGroup, MiniHeader (round-trip)`
  - Files: `src/effects/tae.c`, `.sisyphus/evidence/task-11-* task-12-* task-13-*`
  - Pre-commit: T13 round-trip 测试 PASS

- [x] 14. **`tests/anim/test_tae_synthetic.c` —— 合成最小 SDT TAE round-trip**

  **What to do**：
  - 创建 `tests/anim/test_tae_synthetic.c`：
    - 合成最小 SDT TAE 字节流：
      - magic `"TAE "` + BigEndian = 0 + version 0x1000D。
      - 1 Animation (ID=42)。
      - Animation 含 1 Standard MiniHeader (IsLoopByDefault=true, ImportHKXSourceAnimID=100)。
      - 1 Event (StartTime=0.0f, EndTime=1.0f, Type=300, Parameters = 16 字节 dummy `{0x00 ... 0x0F}`)。
      - 1 EventGroup (GroupType=10, Members=[0])。
      - SkeletonName = "c0000.hkt"、SibName = "c0000.sib"。
    - 调 `sf_tae_read_from_memory` → 断言所有字段值。
    - 调 `sf_tae_write_to_memory` → 与原合成字节流 `memcmp == 0`。
    - 测试也覆盖 ImportOtherAnim MiniHeader 变体（另一个 Animation ID=43，ImportFromAnimID=200, Unknown=-1）。
  - 注册到 tests/CMakeLists.txt（label `anim`）。

  **Must NOT do**：
  - ❌ 不依赖 ER 数据（合成 only）。
  - ❌ 不测 Template-related API。
  - ❌ 不写超过 1024 字节的合成 fixture（保持最小）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— 合成 fixture 字节级精确。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 Wave 3 任何 task 并行）
  - **Parallel Group**: Wave 2 末
  - **Blocks**: Wave 6
  - **Blocked By**: T13

  **References**：

  **Pattern References**：
  - `tests/geom/test_flver2_synthetic.c` —— Phase 6 合成 fixture 模式。
  - `tests/script/test_esd_synthetic_rt.c` —— Phase 5 合成 round-trip 模式。

  **API/Type References**：
  - `include/souls_formats/sf_tae.h`（T7）。
  - Unity ThrowTheSwitch `TEST_ASSERT_*` API。

  **WHY Each Reference Matters**：
  - 合成 fixture 是 CI 可跑（无游戏数据）的字节级 ground truth；保护 Phase 7 不回退。
  - Phase 6 已建合成 fixture 模式；复用避免重复造轮。

  **Acceptance Criteria**：
  - [ ] `tests/anim/test_tae_synthetic.c` 提交且注册到 `tests/CMakeLists.txt` label `anim`。
  - [ ] `ctest -L anim --output-on-failure` 中本测试 PASS。
  - [ ] 测试覆盖 Standard + ImportOtherAnim 两种 MiniHeader 变体。

  **QA Scenarios**：

  ```
  Scenario: 合成 TAE round-trip
    Tool: Bash
    Steps:
      1. `cmake --build build-on --target souls_formats_test_tae_synthetic`
      2. `ctest --test-dir build-mingw -R tae_synthetic --output-on-failure 2>&1 | tee .sisyphus/evidence/task-14-rt.log`
      3. `grep '100% tests passed' .sisyphus/evidence/task-14-rt.log`
    Expected Result: 步骤 3 命中
    Failure Indicators: 未命中
    Evidence: .sisyphus/evidence/task-14-rt.log
  ```

  **Commit**: YES
  - Message: `phase7(tests): tae_synthetic round-trip (1 anim × 1 event × 1 group, SDT, both MiniHeader variants)`
  - Files: `tests/anim/test_tae_synthetic.c`, `tests/CMakeLists.txt`, `.sisyphus/evidence/task-14-*`
  - Pre-commit: 测试 PASS

### Wave 3 — FXR3 二进制实现（Wave 1 全绿后，与 Wave 2 并行）

- [x] 15. **`src/effects/fxr3.c` read top-level —— FXR header + 11 section table + Sekiro 段 + StateMap / Container 入口 offset**

  **What to do**：
  - 实装 `sf_fxr3_read_from_memory(...)` 的顶层流程（对照 `FXR3.cs:62-136`）：
    - `AssertASCII("FXR\0")`、`AssertInt16(0)`、`ReadEnum16<FXRVersion>` (4 或 5，其他 → `SF_ERR_UNSUPPORTED_VERSION`)。
    - `AssertInt32(1)` + 读 `Id` (i32) + 读 `stateMapOffset` (i32) + `AssertInt32(1)` + 读 11 对 (offset, count) section entries。
    - **关键 section offsets** 暂存（用于 T16-T18 调用）：
      - Section 1 = State count（在 stateMapOffset 内）
      - Section 4 = Container offset/count
      - Section 5 = Effect offset/count
      - Section 6 = Action offset/count
      - Section 7 = Property offset/count
      - Section 8 = Modifier offset/count
      - Section 9 = ConditionalProperty offset/count
      - Section 10 = UnkFieldList offset/count
      - Section 11 = Field offset/count
    - 若 `Version == Sekiro (5)`：再读 referenceOffset/Count、externalValueOffset/Count、unkBloodEnablerOffset/Count、section15Offset (skip) + `AssertInt32(0)`；通过 `GetInt32s` 填充 ReferenceList / ExternalValueList / UnkBloodEnabler 三个 int 数组。
    - `br.Position = stateMapOffset` → 调 T16 入口 `read_state_map(br, &fxr->root_state_map)`。
    - `br.Position = containerOffset` → 调 T18 入口 `read_container(br, &fxr->root_container)`。
  - 内存模型：`sf_fxr3_t` 内部用 klib 数组 + opaque struct；T16-T18 共享同 .c 文件。

  **Must NOT do**：
  - ❌ 不读 State / Container 子树（T16 / T18 任务）。
  - ❌ 不优化为 single-pass（保持上游 multi-pass + offset/count 模式）。

  **Recommended Agent Profile**：
  - **Category**: `deep`
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T16, T17 并行；T18 依赖 T15+T16+T17）
  - **Parallel Group**: Wave 3
  - **Blocks**: T16, T17, T18, T19
  - **Blocked By**: T8, T9, T10

  **References**：

  **API/Type References**：
  - `Formats/FXR3.cs:62-136` —— top-level Is + Read（**逐字段对照**）。
  - `include/souls_formats/sf_io.h` —— `assert_*` / `read_*` / `step_in/out` API。

  **WHY Each Reference Matters**：
  - 28 个 ReserveInt32 / FillInt32（Write 时）对应 28 个 offset/count（Read 时）；序号错位 = 全部失败。

  **Acceptance Criteria**：
  - [ ] T5 probe 第一样本：调 `sf_fxr3_read_from_memory` → version ∈ {4, 5}，Id > 0。
  - [ ] T5 probe Sekiro 样本：`sf_fxr3_reference_count` / `_external_value_count` / `_unk_blood_enabler_count` 与 probe 报告一致。

  **QA Scenarios**：

  ```
  Scenario: 顶层 header parse 通过
    Tool: Bash
    Steps:
      1. 写测试 binary：T5 第一样本 read → 断言 version + Id
    Expected Result: PASS
    Failure Indicators: SF_ERR_*
    Evidence: .sisyphus/evidence/task-15-header.log
  ```

  **Commit**: 与 T16-T19 合并（共享 fxr3.c）

- [x] 16. **`src/effects/fxr3.c` —— StateMap / State / StateCondition + ConditionOperand 4 变体 tagged union + ConditionOperator**

  **What to do**：
  - 实装 `read_state_map(br, &out)` / `read_state(br, &out)` / `read_state_condition(br, &out)`：
    - StateMap：`AssertInt32(0)` + capacity (i32) + offset (i32) + `AssertInt32(0)` → StepIn(offset) → capacity 次 State。
    - State：同 StateMap 模式，capacity 次 StateCondition。
    - StateCondition（对照 `FXR3.cs:482-600`）：
      - 读 OperatorType + UnkFlag + LeftOperandType (i16) + LeftOperand fieldOffset (i32) + RightOperandType (i16) + RightOperand fieldOffset (i32) + NextState (i32) + 等。
      - 通过 LeftOperandType 分支调 `ConditionOperand.Create(type, value)`：
        - `OperandType.Literal (-4)` → 读 fieldOffset 处 float → tagged union with `as_literal`。
        - `OperandType.External (-3)` → 读 fieldOffset 处 int → tagged union with `as_external`。
        - `OperandType.TimeOfDay (-2)` → 无 payload（仅 type）。
        - `OperandType.StateTime (-1)` → 无 payload。
      - 同上对 RightOperand。
  - 内部 helper：`static sf_fxr3_operand_t read_operand_at(br, type, offset)` —— 处理 4 变体。

  **Must NOT do**：
  - ❌ 不暴露 ConditionOperand 子类型为独立 opaque（保持 tagged union value type）。
  - ❌ 不解析 `XmlInclude` 标签（XmlSerializer 是 T21/T22 才用）。

  **Recommended Agent Profile**：
  - **Category**: `deep`
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T15, T17 部分并行）
  - **Parallel Group**: Wave 3
  - **Blocks**: T18, T19, T20, T21, T22
  - **Blocked By**: T8, T15（依赖 stateMapOffset）, T17（依赖 Field reader for operand value）

  **References**：

  **API/Type References**：
  - `Formats/FXR3.cs:374-600`（StateCondition 完整段）—— 字段次序 + 多态 Create ground truth。

  **WHY Each Reference Matters**：
  - ConditionOperand 4 变体 是 polymorphic 类型；C tagged union 必须 mirror upstream 的 Create dispatch 逻辑。

  **Acceptance Criteria**：
  - [ ] T5 第一样本 read → 至少一个 State 含 ≥ 1 StateCondition。
  - [ ] 4 个 OperandType 至少在合成 fixture (T20) 中各覆盖一次。

  **QA Scenarios**：

  ```
  Scenario: 4 个 OperandType 变体 read 正确
    Tool: Bash
    Preconditions: T15 顶层 read 完成；T17 Field reader 可用
    Steps:
      1. 在 tests/anim/test_fxr3_synthetic.c（T20）中合成含 4 个 StateCondition 的最小 FXR3（OperandType 各 -4/-3/-2/-1）
      2. `cmake -B build-on -DSF_ENABLE_PHASE7=ON ...; cmake --build build-on --target souls_formats_test_fxr3_synthetic`
      3. `ctest --test-dir build-on -R fxr3_synthetic --output-on-failure 2>&1 | tee .sisyphus/evidence/task-16-operand.log`
      4. `grep -E '(Literal|External|StateTime|TimeOfDay)' .sisyphus/evidence/task-16-operand.log`
    Expected Result: 步骤 3 PASS；步骤 4 命中 4 个 variant 名
    Failure Indicators: 任一 variant 解析错误（Operand.type 与原值不符）
    Evidence: .sisyphus/evidence/task-16-operand.log

  Scenario: T5 probe 样本 StateCondition count 非零
    Tool: Bash
    Steps:
      1. 写测试 binary 调 T5 第一样本 read 后调 sf_fxr3_state_map_state_count + sf_fxr3_state_condition_count
      2. 断言至少一个 State 有 condition_count > 0
    Expected Result: PASS
    Failure Indicators: 全部 0（说明 StateCondition reader 未生效）
    Evidence: .sisyphus/evidence/task-16-real-condition.log
  ```

  **Commit**: 与 T15, T17-T19 合并

- [x] 17. **`src/effects/fxr3.c` —— Field tagged union (Int / Float) + value-range 启发式判定**

  **What to do**：
  - 实装 `read_field(br, context, index)` —— **严格 mirror `FXR3.cs:1114-1158`** 的启发式判定：
    - 若 `context` 是 Property：
      - 若 property->InterpolationType == UnkAc6 → index ∈ (0, propertyType+1] 视为 int。
      - 否则若 InterpolationType != Constant → index == 0 视为 int。
    - 若 `context` 是 OperandType → External 时视为 int。
    - 否则：peek float at position；若 |value| ∈ [1e-4, 1e6) → float，否则 int。
    - 实际 read 4 字节后 `br.Position += 4`。
  - 实装 `read_field_at(br, offset, context, index)` —— StepIn/Out wrapper。
  - 实装 `read_fields_many(br, count, context)` —— 循环。
  - 注意：`context` 是 `void *` 在 C 中（C# 是 object）；T16/T18 调用时显式传 Property* 或 OperandType（用 enum tag 区分）。
  - 实装 `write_field(bw, field)` —— FieldType 分支写 i32 / f32。

  **Must NOT do**：
  - ❌ 不改进启发式（不要扩大/收窄阈值；不要加 NaN/Inf 处理）。
  - ❌ 不为 `void *context` 加 type registry（保持轻量）。

  **Recommended Agent Profile**：
  - **Category**: `artistry` —— 启发式判定 + context-aware dispatch。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T15 / T16 并行）
  - **Parallel Group**: Wave 3
  - **Blocks**: T16, T18, T19, T20, T21, T22
  - **Blocked By**: T8

  **References**：

  **API/Type References**：
  - `Formats/FXR3.cs:1054-1229`（Field 全段）—— 启发式 + 多态 ground truth。

  **WHY Each Reference Matters**：
  - 启发式与上游 1:1 是字节级 round-trip 的前提；任何阈值改动 = 真实游戏数据解析失败。

  **Acceptance Criteria**：
  - [ ] T17 实装文件级单元测试（嵌在 test_fxr3_synthetic.c）：覆盖 |1.5e-3| / |999| / |1.5e-7| / 整数 0 的 4 个测试值，分别判 float / float / int / int（边界匹配上游）。

  **QA Scenarios**：

  ```
  Scenario: Field value-range 启发式 4 边界值
    Tool: Bash
    Preconditions: T17 Field reader 完成
    Steps:
      1. 合成 4 个 4-byte field bytes，分别按 float interpret 为 1.5e-3 / 999 / 1.5e-7 / int 0
      2. 写测试 binary 调 read_field(br, NULL, 0) 4 次（context = NULL 走启发式）
      3. 断言 type 分别 = FLOAT / FLOAT / INT / INT
    Expected Result: 全 PASS
    Failure Indicators: 任一边界判定与上游不一致
    Evidence: .sisyphus/evidence/task-17-heuristic.log

  Scenario: context-aware dispatch（Property + OperandType.External）
    Tool: Bash
    Steps:
      1. 合成 Property (InterpolationType=Linear) → 调 read_field(br, &property, 0) 期望 INT (stop count)
      2. 合成 Property (InterpolationType=Linear) → 调 read_field(br, &property, 1) 期望 FLOAT (interpolation point)
      3. 合成 OperandType.External 上下文 → 调 read_field(br, &op_type, 0) 期望 INT
    Expected Result: 三个 case 全 PASS
    Failure Indicators: 任一 dispatch 错误
    Evidence: .sisyphus/evidence/task-17-context.log
  ```

  **Commit**: 与 T15-T16, T18-T19 合并

- [x] 18. **`src/effects/fxr3.c` —— Container / Effect / Action 递归 + Property / PropertyModifier / UnkFieldList**

  **What to do**：
  - 实装 `read_container(br, &out)`（对照 `FXR3.cs:719-840`）：
    - 读 Container Id (i32) + ChildContainerCount + ChildContainerOffset + EffectCount + EffectOffset + ActionCount + ActionOffset。
    - StepIn(ChildContainerOffset)：递归 ChildContainerCount 次 → 子 Container 列表。
    - StepIn(EffectOffset)：循环 EffectCount 次 → Effect。
    - 不再继续读 Action（Action 节点的 read 在 Effect 内进行 —— 与上游一致）。
  - 实装 `read_effect(br, &out)`：读 Effect Id + ActionCount + ActionOffset → StepIn(ActionOffset) → 循环 Action。
  - 实装 `read_action(br, &out)`：读 Action Type + Field1Count + Field1Offset + Field2Count + Field2Offset + PropertyCount + PropertyOffset + UnkFieldListCount + UnkFieldListOffset。StepIn 各 → 读 Field 列表（调 T17 helper）、Property 列表、UnkFieldList。
  - 实装 `read_property(br, &out)`：读 PropertyType + InterpolationType + IsLoop + ModifierCount + ModifierOffset + FieldCount + FieldOffset。StepIn → 读 Modifier + Field。
  - 实装 `read_property_modifier(br, &out)`：读 ModifierType + PropertyType + ConditionalPropertyCount + ... + FieldCount + FieldOffset。
  - 实装 `read_unk_field_list(br, &out)`：opaque field count + bytes（v1.1 不深入 schema）。
  - **InterpolationType context 传递**：read_field 调用时显式传 `(void *)property` + index，使启发式正确。
  - 接受 `UnkAc6 = 7` 为合法 InterpolationType（不报错）。

  **Must NOT do**：
  - ❌ 不展平嵌套（保留 Container → Effect → Action → Property → Modifier 树）。
  - ❌ 不为 UnkFieldList 引入 typed schema（透传 opaque）。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— 多层递归 + context 传递。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（依赖 T15-T17）
  - **Parallel Group**: Wave 3
  - **Blocks**: T19, T20, T21, T22, T25
  - **Blocked By**: T15, T16, T17

  **References**：

  **API/Type References**：
  - `Formats/FXR3.cs:719-1052`（Container + Effect + Action + Property + PropertyModifier + UnkFieldList 全段）—— **大块代码，逐字段对照**。

  **WHY Each Reference Matters**：
  - Container 递归是 FXR3 树结构的核心；偏差 = 全部 .fxr 解析失败。

  **Acceptance Criteria**：
  - [ ] T5 第一样本 read → 至少一个 Container 有 Effect 子节点。
  - [ ] InterpolationType == UnkAc6 (7) 的 Property 不报错。

  **QA Scenarios**：

  ```
  Scenario: Container 树递归 read 正确
    Tool: Bash
    Preconditions: T15-T17 完成；ER 数据齐
    Steps:
      1. 写测试 binary 调 sf_fxr3_read_from_memory(T5 第一样本) → sf_fxr3_root_container → sf_fxr3_container_child_count + sf_fxr3_container_effect_count（递归）
      2. 累加全树 Effect / Action 数 → 与 T5 probe evidence 中记录的 EffectCount + ActionCount 对比
    Expected Result: 数量一致
    Failure Indicators: 数量不一致（说明递归 read 有漏）
    Evidence: .sisyphus/evidence/task-18-recursion.log

  Scenario: UnkAc6 InterpolationType 接受
    Tool: Bash
    Steps:
      1. 合成 Property（InterpolationType raw byte = 7）的最小 fxr3 字节流
      2. 调 sf_fxr3_read_from_memory → 期望 SF_OK + sf_fxr3_property_interpolation == SF_FXR3_INTERP_UNK_AC6
    Expected Result: PASS
    Failure Indicators: SF_ERR_UNSUPPORTED_VERSION 或类似错误
    Evidence: .sisyphus/evidence/task-18-unkac6.log
  ```

  **Commit**: 与 T15-T17, T19 合并

- [x] 19. **`src/effects/fxr3.c` write —— 28 ReserveInt32 + FillInt32 + Sekiro section 11-14 + 各 list flatten**

  **What to do**：
  - 实装 `sf_fxr3_write_to_memory(...)`（对照 `FXR3.cs:138-283`）：
    - 写 magic + Version + Id + 28 个 `ReserveInt32` 占位（StateMapOffset / StateOffset / TransitionOffset / TransitionCount / ContainerOffset / ContainerCount / EffectOffset / EffectCount / ActionOffset / ActionCount / PropertyOffset / PropertyCount / ModifierOffset / ModifierCount / ConditionalPropertyOffset / ConditionalPropertyCount / UnkFieldListOffset / UnkFieldListCount / FieldOffset / FieldCount + 9 个固定值或 reserved）。
    - 若 Version == Sekiro：额外 ReserveInt32 ReferenceOffset / ExternalValueOffset / UnkBloodEnablerOffset / UnkEmptyOffset。
    - 依次 `FillInt32` + 写各 list：StateMap → 各 State → 各 Transition (StateCondition) → 各 Container （flatten 树为 flat list，遵守 `RootContainer.Write` + `WriteContainers` 双 pass 模式）→ Effect → Action → Property → PropertyModifier → ConditionalProperty (来自 Modifier) → UnkFieldList → Field（最末，所有上层共用一个 Field pool）。
    - 每个 `FillInt32` 前对齐 `bw.Pad(16)`。
    - Sekiro：在 Field 段后写 ReferenceList / ExternalValueList / UnkBloodEnabler 三个 int 数组，各对齐 Pad(16)。
    - 调 `sf_binary_writer_finish` 验证 0 个 unfilled Reserve。

  **Must NOT do**：
  - ❌ 不省略任何 ReserveInt32（28 个全部需要，否则与 Read 对称失败）。
  - ❌ 不改 list 写出顺序（必须严格遵守上游 sequencing）。
  - ❌ 不写 DS3 的 Sekiro-only sections（version 守门）。

  **Recommended Agent Profile**：
  - **Category**: `deep` —— Reserve/Fill 大型对称 + 7 层 list flatten。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（依赖 T15-T18）
  - **Parallel Group**: Wave 3 末
  - **Blocks**: T20, T22, T25
  - **Blocked By**: T15, T16, T17, T18

  **References**：

  **API/Type References**：
  - `Formats/FXR3.cs:138-283`（Write 全段）—— **大段 Reserve/Fill，是写出对称的 ground truth**。

  **WHY Each Reference Matters**：
  - Write 段每行 ReserveInt32 / FillInt32 必须与 Read 对应；偏差 1 个 ReserveInt32 = `sf_binary_writer_finish` 失败。

  **Acceptance Criteria**：
  - [ ] T20 合成 fixture：read → write → memcmp == 0（DS3 + Sekiro 两版本各一组）。
  - [ ] `sf_binary_writer_finish` 返回 SF_OK（无 unfilled Reserve）。

  **QA Scenarios**：

  ```
  Scenario: writer finish 返回 SF_OK（0 unfilled Reserve）
    Tool: Bash
    Preconditions: T15-T18 完成
    Steps:
      1. 合成最小 fxr3（DS3 version）via T20 fixture
      2. 调 sf_fxr3_write_to_memory → 期望 SF_OK 且 out_bytes 非空
      3. 内部检查（添加 debug log 或 unit test）：sf_binary_writer_finish 返回 SF_OK
    Expected Result: SF_OK
    Failure Indicators: SF_ERR_INTERNAL（reserve 未填）
    Evidence: .sisyphus/evidence/task-19-finish.log

  Scenario: Sekiro section 11-14 仅 Sekiro version 写出
    Tool: Bash
    Steps:
      1. 合成最小 fxr3（DS3 version，empty ReferenceList）→ write → byte stream A
      2. 合成最小 fxr3（Sekiro version，empty ReferenceList）→ write → byte stream B
      3. byte stream B 在 header 区段后比 A 多 32 字节（4 个 offset+count 占位）
    Expected Result: B 长度 - A 长度 ≈ 32（考虑 padding 可 ±15）
    Failure Indicators: 长度差 == 0（说明 Sekiro section 未写）
    Evidence: .sisyphus/evidence/task-19-sekiro-sections.log
  ```

  **Commit**: YES（合并 T15-T19）
  - Message: `phase7(fxr3): binary read+write — header, sections, StateMap/Container recursion, tagged unions, writer`
  - Files: `src/effects/fxr3.c`, `.sisyphus/evidence/task-15-* task-16-* task-17-* task-18-* task-19-*`
  - Pre-commit: T20 round-trip 测试 PASS

- [x] 20. **`tests/anim/test_fxr3_synthetic.c` —— 合成 FXR3 binary round-trip（DS3 + Sekiro，全 tagged union 覆盖）**

  **What to do**：
  - 创建 `tests/anim/test_fxr3_synthetic.c`：
    - **Fixture A (DS3, version 4)**：
      - 1 State × 4 Conditions（各一 OperandType：Literal float=3.14 / External int=42 / StateTime / TimeOfDay；与 OperatorType EQUAL）。
      - 1 Container × 1 Effect × 1 Action × 1 Property（PropertyType=Scalar, InterpolationType=Constant, IsLoop=false, 1 Field=FieldFloat 1.0f）× 1 Modifier (无 ConditionalProperty)。
      - 1 UnkFieldList (count=0)。
      - No ReferenceList / ExternalValueList / UnkBloodEnabler（DS3 不写）。
      - read → write → memcmp == 0。
    - **Fixture B (Sekiro, version 5)**：
      - 同 A 结构，加 ReferenceList=[10, 20]、ExternalValueList=[100]、UnkBloodEnabler=[]。
      - 1 额外 Property 用 InterpolationType=UnkAc6 (验证 7 接受)。
      - 1 额外 Field 通过启发式判定为 float（值 1.5e-3）+ 1 通过启发式判定为 int（值 1e-7 abs 太小 → int）。
      - read → write → memcmp == 0。
    - 注册到 tests/CMakeLists.txt（label `anim`）。

  **Must NOT do**：
  - ❌ 不依赖 ER 数据。
  - ❌ 不超过 4 KB fixture per case（保持最小）。
  - ❌ 不测 XML（T23 任务）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high`
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 Wave 4 起步 task 并行）
  - **Parallel Group**: Wave 3 末
  - **Blocks**: T23, Wave 5
  - **Blocked By**: T15-T19

  **References**：

  **Pattern References**：
  - `tests/geom/test_flver2_synthetic.c` —— Phase 6 双 fixture 模式（cube + various）。
  - `tests/anim/test_tae_synthetic.c`（T14）—— 同 Wave 合成 fixture 模式。

  **API/Type References**：
  - `include/souls_formats/sf_fxr3.h`（T8）。

  **Acceptance Criteria**：
  - [ ] 测试覆盖 4 个 OperandType + 2 个 FieldType + 4 个 PropertyType + ≥ 2 个 InterpolationType（含 UnkAc6）。
  - [ ] DS3 + Sekiro fixture 各 round-trip memcmp == 0。
  - [ ] `ctest -L anim -R fxr3_synthetic` PASS。

  **QA Scenarios**：

  ```
  Scenario: 合成 FXR3 round-trip (DS3 + Sekiro)
    Tool: Bash
    Steps:
      1. `cmake --build build-on --target souls_formats_test_fxr3_synthetic`
      2. `ctest --test-dir build-mingw -R fxr3_synthetic --output-on-failure 2>&1 | tee .sisyphus/evidence/task-20-rt.log`
    Expected Result: 步骤 2 中 `100% tests passed`
    Failure Indicators: 未命中
    Evidence: .sisyphus/evidence/task-20-rt.log + input-{a,b}.bin + output-{a,b}.bin
  ```

  **Commit**: YES
  - Message: `phase7(tests): fxr3_synthetic binary round-trip (DS3 + Sekiro versions, all tagged union variants)`
  - Files: `tests/anim/test_fxr3_synthetic.c`, `tests/CMakeLists.txt`, `.sisyphus/evidence/task-20-*`
  - Pre-commit: 测试 PASS

### Wave 4 — FXR3 XML 实现（Wave 3 T18 全绿后 3 路并行）

- [x] 21. **`src/effects/fxr3_xml_read.c` —— mxml DOM 解析 → sf_fxr3_t（schema 对照上游 XmlSerializer 标注）**

  **What to do**：
  - 实装 `sf_fxr3_from_xml(out, xml_utf8, size, alloc)`：
    - `mxmlLoadString(NULL, xml_utf8, MXML_NO_CALLBACK)` → mxml_node_t *root。
    - 期望 root element name = `"FXR3"`（对照上游 `[XmlType(TypeName = "FXR3")]`）。
    - 读 root attribute：`Version` (FXRVersion enum 字符串 → 4 / 5)、`Id` (int)。
    - 读子元素 `StateMap` → 调 `parse_state_map(node, &out)`：
      - 遍历 `State` 子元素 → 各 State 含 `StayCondition` 子元素列表（对照 `FXR3.cs:333` `[XmlElement("StayCondition")]`）。
      - 每个 `StayCondition` 含 `LeftOperand` / `RightOperand` 子元素，其 element name 取决于子类型（XmlInclude 多态）：`<Literal>`, `<External>`, `<StateTime/>`, `<UnkMinus2/>`（TimeOfDay 上游误名 UnkMinus2，沿用）。`<Literal>` 含 `Value` attribute (float)，`<External>` 含 `Value` attribute (int)。
    - 读子元素 `Container` → 调 `parse_container(node, &out)`：递归子 Container、Effect、Action、Property。
    - Property 子元素 `Fields/Int` / `Fields/Float`（对照 `FXR3.cs:1194` `[XmlType("Float")]` / `[XmlType("Int")]`）。
    - **读 `<ReferenceList>` / `<ExternalValueList>` / `<UnkBloodEnabler>` 三个子元素**：每个含 `<int>...</int>` 子元素数组；解析为 int 数组填入 sf_fxr3_t。**DS3 与 Sekiro 都要解析这些元素**（与 T22 一致；XmlSerializer 默认序列化 public property 无 XmlIgnore）。DS3 这些列表通常为空（元素存在但子节点空）；Sekiro 可能非空。
  - 错误：mxml parse 失败或 schema 错配 → `SF_ERR_INTERNAL` + free 已分配。
  - **关键**：XML schema 必须**严格匹配** XmlSerializer 自动生成的 schema —— 测试时 T23 用合成 fxr3 → T22 write → T21 read 自验证。

  **Must NOT do**：
  - ❌ 不引入 namespace 处理（XmlSerializer 默认无 namespace）。
  - ❌ 不实装 mxml allocator 桥（已 extensions.md 记录）。
  - ❌ 不容忍 schema 偏差（不接受用户手写的非标准 XML —— v1.1 只接受 T22 自产的或上游 Rainbow Stone 工具产的）。

  **Recommended Agent Profile**：
  - **Category**: `artistry` —— mxml DOM traversal + polymorphic 子元素 dispatch。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T22 并行）
  - **Parallel Group**: Wave 4
  - **Blocks**: T23
  - **Blocked By**: T8, T18, T19

  **References**：

  **Pattern References**：
  - `src/param/paramdef_xml_read.c` —— Phase 4 mxml DOM read 模式（PARAMDEF 解析）；逐字对照 API 用法。

  **API/Type References**：
  - `Formats/FXR3.cs:18-1500` —— XmlSerializer 注解（`[XmlType]`, `[XmlElement]`, `[XmlAttribute]`, `[XmlInclude]`）= XML schema ground truth。
  - mxml 4.0.4 API docs（项目内 cmake/deps/mxml.cmake 已 pin）。

  **External References**：
  - mxml DOM API：`mxmlLoadString`, `mxmlFindElement`, `mxmlElementGetAttr`, `mxmlGetText`, `mxmlGetType`。

  **WHY Each Reference Matters**：
  - XmlSerializer 注解定义 XML element/attribute name + polymorphic dispatch；C 端必须 mirror。
  - Phase 4 paramdef_xml_read.c 已建 mxml usage 模式；复用避免重复造轮。

  **Acceptance Criteria**：
  - [ ] T22 完成后，T20 合成 fxr3 → write XML → read back → structural equality（T23 验证）。
  - [ ] mxml parse 错误正确返回 `SF_ERR_INTERNAL` + free。

  **QA Scenarios**：

  ```
  Scenario: 最小 hand-crafted XML 可被解析
    Tool: Bash
    Preconditions: T8 sf_fxr3.h 已起；T21 from_xml 实装完成
    Steps:
      1. 构造最小 XML 字符串：`<?xml version="1.0"?><FXR3 Version="Sekiro" Id="1"><StateMap><State><StayCondition><LeftOperand><StateTime/></LeftOperand><RightOperand><Literal Value="0.5"/></RightOperand></StayCondition></State></StateMap><Container/></FXR3>`
      2. 写测试 binary 调 sf_fxr3_from_xml(xml, len, &fxr, NULL) → 期望 SF_OK
      3. 调 sf_fxr3_state_map_state_count + sf_fxr3_state_condition_count + 验证 OperandType
    Expected Result: state_count == 1, condition_count == 1, RightOperand.type == LITERAL, value == 0.5
    Failure Indicators: SF_ERR_*; 任一断言失败
    Evidence: .sisyphus/evidence/task-21-handcraft.log

  Scenario: 损坏 XML 拒绝
    Tool: Bash
    Steps:
      1. 构造缺少闭合 tag 的 XML（如 `<FXR3 Version="Sekiro">`，无 `</FXR3>`）
      2. 调 sf_fxr3_from_xml → 期望 SF_ERR_INTERNAL，无内存泄漏（valgrind 或 ASan 验证）
    Expected Result: SF_ERR_INTERNAL
    Failure Indicators: SF_OK 或 crash
    Evidence: .sisyphus/evidence/task-21-malformed.log
  ```

  **Commit**: YES
  - Message: `phase7(fxr3-xml): XML read via mxml DOM (schema mirrors upstream XmlSerializer)`
  - Files: `src/effects/fxr3_xml_read.c`, `.sisyphus/evidence/task-21-*`
  - Pre-commit: T23 测试 PASS（T22 提前提交时本 commit 可同时）

- [x] 22. **`src/effects/fxr3_xml_write.c` —— sf_fxr3_t → mxml DOM → UTF-8 string**

  **What to do**：
  - 实装 `sf_fxr3_to_xml(f, out_xml, out_size, alloc)`：
    - 创建 root `<FXR3>` element via `mxmlNewElement(NULL, "FXR3")`。
    - 设 root attribute：`Version` (enum 名字符串 `"DarkSouls3"` / `"Sekiro"`)、`Id`（int → 字符串）。
    - 创建子 `<StateMap>`：
      - 遍历 States → 每个 `<State>` 含 `<StayCondition>` 列表。
      - 每个 `<StayCondition>` 创建 `<LeftOperand>` 子元素，其 element name 取决于 tagged union type：
        - Literal → `<LeftOperand>` 含 `<Literal Value="3.14"/>` (匹配 XmlInclude polymorphism)。
        - External → `<LeftOperand>` 含 `<External Value="42"/>`。
        - StateTime → `<LeftOperand>` 含 `<StateTime/>` (空)。
        - TimeOfDay (UnkMinus2) → `<LeftOperand>` 含 `<UnkMinus2/>`。
      - 同上 RightOperand。
    - 创建子 `<Container>`：递归子 Container / Effect / Action / Property。
    - Property 含 `<Fields>` 子元素，里面 `<Int Value="..."/>` / `<Float Value="..."/>` 混排。
    - **创建子 `<ReferenceList>` / `<ExternalValueList>` / `<UnkBloodEnabler>` 三个列表元素**：上游 `FXR3.cs:30-35` 这些 public property **无** `[XmlIgnore]`，XmlSerializer 默认序列化为 `<ReferenceList><int>10</int><int>20</int></ReferenceList>` 形态（XmlSerializer 默认对 `HashSet<int>` / `List<int>` 用 `<int>` 子元素，元素名取 type 名）。DS3 实际 FXR3 中这些列表为空 → 输出空元素 `<ReferenceList />`；Sekiro 可能非空 → 输出含 `<int>` 子元素。两个 version 都输出（保持与 XmlSerializer 行为一致）。
    - 调 `mxmlSaveString` 或 `mxmlSaveAllocString` → 返回 UTF-8 string。
  - 输出字符串通过 `sf_strbuf_t` 或直接 mxml malloc 返回；caller 负责释放（用 `sf_fxr3_free_xml(...)` helper，或 mxml-style `free()`）。

  **Must NOT do**：
  - ❌ 不输出 XML declaration `<?xml version="1.0"?>`（XmlSerializer 默认有 —— 实际上需要！查 mxml `MXML_TYPE_DECLARATION` 用法保持一致）。
  - ❌ 不 pretty-print 强制缩进（让 mxml 默认 whitespace 行为生效；测试用 structural equal）。
  - ❌ 不在 XML 中输出 mxml 内部 type 信息。

  **Recommended Agent Profile**：
  - **Category**: `artistry` —— polymorphic 写 + schema 严格匹配。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T21 并行）
  - **Parallel Group**: Wave 4
  - **Blocks**: T23
  - **Blocked By**: T8, T18, T19, T21（如果共享 helper）

  **References**：

  **Pattern References**：
  - `src/param/paramdef_xml_write.c`（如存在）—— Phase 4 mxml DOM write 模式。否则参考 mxml docs。

  **API/Type References**：
  - mxml DOM build API：`mxmlNewElement`, `mxmlElementSetAttr`, `mxmlNewText`, `mxmlSaveString` / `mxmlSaveAllocString`。
  - `Formats/FXR3.cs:1488 FXR3ToXML` —— 上游 `XmlSerializer.Serialize(writer, fxr)` 调用模式 + `XDocument.Parse(stream)` 结果作 ground truth。

  **WHY Each Reference Matters**：
  - mxml 不像 XmlSerializer 是 schema-driven 自动；必须手动 mirror 每个 element/attribute。

  **Acceptance Criteria**：
  - [ ] T23 测试：合成 fxr3 → to_xml → from_xml → structural equality。
  - [ ] XML output 含 `<FXR3 Version="..." Id="...">` root。

  **QA Scenarios**：

  ```
  Scenario: 合成 fxr3 → to_xml 输出含期望 token
    Tool: Bash
    Preconditions: T20 fixture B (Sekiro) 可读
    Steps:
      1. 写测试 binary：read fixture B → sf_fxr3_to_xml(&xml, &size, NULL) → 期望 SF_OK
      2. `grep -c '<FXR3 ' xml_output.xml`
      3. `grep -c 'Version="Sekiro"' xml_output.xml`
      4. `grep -c '<Container' xml_output.xml`
      5. `grep -cE '<(Literal|External|StateTime|UnkMinus2)' xml_output.xml` （4 个 OperandType 字段）
    Expected Result: 步骤 2 = 1；步骤 3 = 1；步骤 4 ≥ 1；步骤 5 ≥ 1
    Failure Indicators: 任一 = 0
    Evidence: .sisyphus/evidence/task-22-tokens.log + xml_output.xml

  Scenario: XML output 是 valid XML（mxml self-parse）
    Tool: Bash
    Steps:
      1. 调 sf_fxr3_to_xml 得 xml_str
      2. mxmlLoadString(NULL, xml_str, MXML_NO_CALLBACK) → 期望非 NULL
      3. mxmlDelete(root)
    Expected Result: 步骤 2 非 NULL
    Failure Indicators: NULL（XML 不 well-formed）
    Evidence: .sisyphus/evidence/task-22-valid-xml.log
  ```

  **Commit**: YES
  - Message: `phase7(fxr3-xml): XML write via mxml DOM (schema mirrors upstream XmlSerializer)`
  - Files: `src/effects/fxr3_xml_write.c`, `.sisyphus/evidence/task-22-*`
  - Pre-commit: T23 测试 PASS

- [x] 23. **`tests/anim/test_fxr3_xml.c` —— FXR3 → XML → re-parse → structural equality**

  **What to do**：
  - 创建 `tests/anim/test_fxr3_xml.c`：
    - 使用 T20 合成 Fixture B (Sekiro)：先 `sf_fxr3_read_from_memory` 得 fxr3_a。
    - `sf_fxr3_to_xml(fxr3_a, &xml_str, &xml_size, NULL)` → XML UTF-8 string。
    - 检查 XML string 不空、含 root `<FXR3` 等关键 token（基础 sanity）。
    - `sf_fxr3_from_xml(&fxr3_b, xml_str, xml_size, NULL)` → re-parsed fxr3。
    - **Structural equality check**（递归比较）：
      - Version、Id、ReferenceList / ExternalValueList / UnkBloodEnabler 内容。
      - RootStateMap 的 State count + 每 State 的 Condition count + 每 Condition 的 OperatorType + LeftOperand type + LeftOperand value (按 tagged union 分支)、RightOperand 同。
      - RootContainer 树：递归 ContainerId / ChildCount / EffectCount / ActionCount / ActionType / FieldType + Value / PropertyType + InterpolationType + IsLoop。
    - 任一字段不匹配 → 输出 diff 到 evidence 并 fail。
  - 注册到 tests/CMakeLists.txt（label `anim`）。

  **Must NOT do**：
  - ❌ 不做 byte-equal raw XML compare（whitespace / attribute order 不稳）。
  - ❌ 不测 ER 真实 .fxr 通过 XML（Wave 5 任务）。
  - ❌ 不允许任何字段 mismatch（structural equal 必须严格）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— 递归 structural 比较实现。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: NO（依赖 T20, T21, T22 全完成）
  - **Parallel Group**: Wave 4 末
  - **Blocks**: Wave 6
  - **Blocked By**: T20, T21, T22

  **References**：

  **Pattern References**：
  - `tests/param/test_paramdef_xml_read.c` —— Phase 4 XML round-trip 测试模式（部分）。

  **API/Type References**：
  - `include/souls_formats/sf_fxr3.h`（T8 提供 accessor）。

  **WHY Each Reference Matters**：
  - structural equality 是 XML 测试的标准 pattern（不依赖 XML 字符串完全相等）。

  **Acceptance Criteria**：
  - [ ] `tests/anim/test_fxr3_xml.c` 提交且注册 label `anim`。
  - [ ] 测试覆盖 4 OperandType + 2 FieldType + ≥ 2 InterpolationType（含 UnkAc6）。
  - [ ] `ctest -L anim -R fxr3_xml` PASS。

  **QA Scenarios**：

  ```
  Scenario: FXR3 XML round-trip structural equal
    Tool: Bash
    Steps:
      1. `cmake --build build-on --target souls_formats_test_fxr3_xml`
      2. `ctest --test-dir build-mingw -R fxr3_xml --output-on-failure 2>&1 | tee .sisyphus/evidence/task-23-rt.log`
    Expected Result: `100% tests passed`
    Failure Indicators: 未命中；evidence 中含 diff 输出
    Evidence: .sisyphus/evidence/task-23-rt.log + xml-out.xml + fxr3-a.txt + fxr3-b.txt
  ```

  **Commit**: YES
  - Message: `phase7(tests): fxr3 XML structural equality round-trip`
  - Files: `tests/anim/test_fxr3_xml.c`, `tests/CMakeLists.txt`, `.sisyphus/evidence/task-23-*`
  - Pre-commit: 测试 PASS

### Wave 5 — ER e2e（Wave 2 + Wave 4 全绿后 2 路并行）

- [x] 24. **`tests/anim/test_tae_e2e_er.c` —— c0000.anibnd.dcx → BND4 → .tae 真实数据 e2e**

  **What to do**：
  - 创建 `tests/anim/test_tae_e2e_er.c`：
    - **关键**：c0000.anibnd.dcx 与 c0000.chrbnd.dcx 同位置，**生产 ER 版本中存于 Data3**（非 Data0）。`er_extract_from_data0` (Phase 3) 仅打开 Data0 不够用。复用 Phase 6 `tests/geom/test_flver2_e2e_er.c:5-8` 注释中描述的 Data0..Data3 顺序搜索模式 —— 手动迭代 4 个 BHD5；或选择性扩展 `er_test_helper.c` 增 `er_extract(path, &bytes, &size)` 通用 helper（同 T4 备注，可与 T24 共享 1 个 sub-task）。若不扩展 helper，则直接 inline Data0..Data3 循环。
    - 检查 `sha256(bytes)` 与 `docs/api-mapping/UPSTREAM.md` 中 T6 pinned hash 一致 —— 不一致 → `TEST_IGNORE_MESSAGE("game patch changed snapshot, skipping")`（防止 patch 后误判 bug）。
    - 用 `sf_bnd4_read_from_memory` 解析 BND4。
    - 用 `sf_bnd4_find_by_name_suffix(".tae")` 或同等 helper 找第一个 `.tae` entry。
    - 调 `sf_tae_read_from_memory(entry->data, entry->size, &tae, NULL)`：
      - 断言 `SF_OK`。
      - 断言 `sf_tae_format(tae) == SF_TAE_FORMAT_SDT`。
      - 断言 `sf_tae_animation_count(tae) > 0`。
      - 断言每个 Animation 至少有 1 个 Event 或 1 个 EventGroup（c0000 是高活动角色）。
      - 记录 entry 名称 / 字节数 / anim count / event count 到 evidence。
    - 清理 + free。
  - 注册到 `tests/CMakeLists.txt`，label `e2e_er`（与 Phase 6 复用）。需要 `target_compile_definitions(SF_E2E_ELDEN_RING_DIR ... SF_E2E_OODLE_DIR ...)`（沿用 Phase 3+ 模式）。
  - 编译时通过 `tests/e2e/er_test_helper.c` 链接 helper。
  - **SKIP 条件**：`/mnt/c/Games/ELDEN RING/Game/Data0.bhd` 缺失 → 自动 SKIP（er_helper 已封装）。

  **Must NOT do**：
  - ❌ 不嵌入任何 game-derived bytes。
  - ❌ 不期望 ParameterContainer 可结构化解析（仅 opaque size check）。
  - ❌ 不测 round-trip 字节级一致（真实 .tae 可能有上游不显式处理的 padding；e2e 仅 read PASS）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high` —— e2e 测试 + helper 复用。
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T25 并行）
  - **Parallel Group**: Wave 5
  - **Blocks**: Wave 6
  - **Blocked By**: T13, T6

  **References**：

  **Pattern References**：
  - `tests/script/test_emevd_e2e_er.c` —— Phase 5 同款 BHD5 + BND4 + 内部格式 e2e 模式。
  - `tests/geom/test_flver2_e2e_er.c` —— Phase 6 c0000 资产 e2e 模式。
  - `tests/e2e/er_test_helper.c` —— `er_extract_from_data0` API。

  **API/Type References**：
  - `include/souls_formats/sf_tae.h` + `sf_bnd4.h`。

  **WHY Each Reference Matters**：
  - Phase 5/6 已建 ER e2e 模式（含 sha256 sanity / SKIP-on-missing-data / target_compile_definitions），复用。

  **Acceptance Criteria**：
  - [ ] `tests/anim/test_tae_e2e_er.c` 提交且注册 `e2e_er` label。
  - [ ] `ctest -L e2e_er -R tae_e2e_er` PASS（ER 数据齐时）。
  - [ ] Evidence 含 entry 名 + anim count + event count。

  **QA Scenarios**：

  ```
  Scenario: c0000.anibnd 中 .tae read 成功
    Tool: Bash
    Steps:
      1. `cmake --build build-on --target souls_formats_test_tae_e2e_er`
      2. `ctest --test-dir build-mingw -R tae_e2e_er --output-on-failure 2>&1 | tee .sisyphus/evidence/task-24-rt.log`
    Expected Result: PASS
    Failure Indicators: SF_ERR_*；anim_count == 0
    Evidence: .sisyphus/evidence/task-24-rt.log

  Scenario: sha256 patch sanity SKIP（注入失效 hash）
    Tool: Bash
    Steps:
      1. 临时改 UPSTREAM.md 中 c0000.anibnd.dcx hash 为伪值
      2. 跑测试 → 期望 SKIP 而非 FAIL
      3. 还原 hash
    Expected Result: 步骤 2 测试 SKIP 退出
    Failure Indicators: FAIL
    Evidence: .sisyphus/evidence/task-24-sha-skip.log
  ```

  **Commit**: YES
  - Message: `phase7(tests): TAE ER e2e (c0000.anibnd.dcx → BND4 → .tae)`
  - Files: `tests/anim/test_tae_e2e_er.c`, `tests/CMakeLists.txt`, `.sisyphus/evidence/task-24-*`
  - Pre-commit: 测试 PASS

- [x] 25. **`tests/anim/test_fxr3_e2e_er.c` —— sfxbnd_commoneffects.ffxbnd.dcx → BND4 → .fxr 真实数据 e2e**

  **What to do**：
  - 创建 `tests/anim/test_fxr3_e2e_er.c`：
    - 提取 `/sfx/sfxbnd_commoneffects.ffxbnd.dcx`：**先**调 `er_extract_from_data0(...)`；若返回 `SF_ERR_*`（路径不在 Data0），则 fallback 到 Data0..Data3 顺序搜索（同 T24 模式 / 同 T5 probe 模式）。**T5 probe 已实际验证**该路径所在 BHD，e2e 测试沿用同样的实际位置。
    - sha256 sanity check vs UPSTREAM.md（同 T24）。
    - `sf_bnd4_read_from_memory` → 找第一个 `.fxr` entry。
    - `sf_fxr3_read_from_memory(entry->data, entry->size, &fxr, NULL)`：
      - 断言 `SF_OK`。
      - 断言 `sf_fxr3_version(fxr) == SF_FXR3_VERSION_SEKIRO` (期望 ER 全用 Sekiro version)。
      - 断言 `sf_fxr3_root_container(fxr) != NULL`。
      - 断言 `sf_fxr3_container_effect_count(root) > 0` 或递归至少一个子 Container 有 Effect。
      - 记录 entry 名 / version / 各 section count 到 evidence。
    - 额外尝试 `sf_fxr3_to_xml`（结构性 sanity）：XML output 含 `<FXR3` token。
    - 清理。
  - 注册到 `tests/CMakeLists.txt`，label `e2e_er`。

  **Must NOT do**：
  - ❌ 不期望 round-trip 字节级一致（真实 .fxr 可能含 padding quirk；仅 read PASS）。
  - ❌ 不测 XML structural equality（XML 自回环已在 T23 测；e2e 仅 read + to_xml sanity）。

  **Recommended Agent Profile**：
  - **Category**: `unspecified-high`
  - **Skills**: 无。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T24 并行）
  - **Parallel Group**: Wave 5
  - **Blocks**: Wave 6
  - **Blocked By**: T19, T22, T6

  **References**：

  **Pattern References**：
  - `tests/geom/test_matbin_e2e_er.c` —— Phase 6 BND4-internal binary e2e 模式。

  **API/Type References**：
  - `include/souls_formats/sf_fxr3.h` + `sf_bnd4.h`。

  **WHY Each Reference Matters**：
  - sfxbnd_commoneffects 是 ER 最大 .fxr 容器之一，可测试递归 Container 解析。
  - to_xml sanity 验证 T22 在真实数据上不 crash。

  **Acceptance Criteria**：
  - [ ] `tests/anim/test_fxr3_e2e_er.c` 提交且 PASS（ER 数据齐时）。
  - [ ] Evidence 含 entry 名 + version + RootContainer effect count。

  **QA Scenarios**：

  ```
  Scenario: sfxbnd_commoneffects 中 .fxr read + to_xml 成功
    Tool: Bash
    Steps:
      1. `cmake --build build-on --target souls_formats_test_fxr3_e2e_er`
      2. `ctest --test-dir build-mingw -R fxr3_e2e_er --output-on-failure 2>&1 | tee .sisyphus/evidence/task-25-rt.log`
    Expected Result: PASS
    Failure Indicators: SF_ERR_*；Container == NULL
    Evidence: .sisyphus/evidence/task-25-rt.log
  ```

  **Commit**: YES
  - Message: `phase7(tests): FXR3 ER e2e (sfxbnd_commoneffects.ffxbnd.dcx → BND4 → .fxr)`
  - Files: `tests/anim/test_fxr3_e2e_er.c`, `tests/CMakeLists.txt`, `.sisyphus/evidence/task-25-*`
  - Pre-commit: 测试 PASS

### Wave 6 — Docs + 状态表 final pass（Wave 5 全绿后 3 路并行）

- [x] 26. **2 mapping doc 全量刷新（format-tae.md + format-fxr3.md）+ extensions.md 4 条 final**

  **What to do**：
  - **format-tae.md**：
    - 9 个 in-scope row（TAE / TAEFormat / Animation / AnimMiniHeader / Standard / ImportOtherAnim / Event / ParameterContainer / EventGroup）：status 列从 `未实现` → `✓ aligned`（或 `~ partial` 若有 caveat，如 ParameterContainer 是 opaque）。
    - 5 个 Template-related row（在 T2 已标 `_skipped_`）：保留 `_skipped_`，Notes 列校准为「v1.2 推迟，参见 extensions.md」。
    - 顶部 Status 段：「v1.1 implements TAE SDT format (version 0x1000D); covers Sekiro + Elden Ring. Template subsystem deferred to v1.2.」
  - **format-fxr3.md**：
    - 17 个 row 全部 status 从 `未实现` → `✓ aligned` 或 `~ partial`：
      - `FXR3.UnkFieldList` → `~ partial`（C 端 opaque，未结构化解码）。
      - `FXR3EnhancedSerialization` → `+ extension`（C 端 API 名不同：`sf_fxr3_to_xml` / `sf_fxr3_from_xml`，已记入 extensions.md）。
      - 其他 14 row → `✓ aligned`。
    - 顶部 Status：「v1.1 implements FXR3 binary + XML round-trip for DS3 (version 4) and Sekiro (version 5). ER/AC6/Nightreign use Sekiro version. UnkAc6 InterpolationType (7) accepted.」
  - **extensions.md**：将 T3 起草的 4 条 stub 完整化：
    - 每条加 file:line 真实链接（确认 Wave 3-4 实装后 file:line 不变）。
    - 加 Impact 段落具体描述哪些用户场景受影响。

  **Must NOT do**：
  - ❌ 不改 row 顺序（保持上游引用顺序）。
  - ❌ 不删除 row（即便不实现也用 `_skipped_` 标）。
  - ❌ 不引入新 status legend（用 README.md 已定义的 6 个）。

  **Recommended Agent Profile**：
  - **Category**: `writing`
  - **Skills**: `tech-doc-style-chinese`（如 doc 中文段落）。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T27, T28 并行）
  - **Parallel Group**: Wave 6
  - **Blocks**: Wave Final
  - **Blocked By**: T14, T20, T23, T24, T25

  **References**：

  **Pattern References**：
  - `docs/api-mapping/format-flver2.md` —— Phase 6 final pass 后的形态（含 `_skipped_` Edge 子表）。
  - `docs/api-mapping/format-flver-common.md` —— 简单 row 全 `✓ aligned` 形态。

  **WHY Each Reference Matters**：
  - mapping doc 是 F1 reviewer 检查 in-scope 完成度的 ground truth；任何 `未实现` 残留 = REJECT。

  **Acceptance Criteria**：
  - [ ] `grep -c '未实现' docs/api-mapping/format-tae.md` == 0（in-scope 9 行）。
  - [ ] `grep -c '_skipped_' docs/api-mapping/format-tae.md` >= 5（Template 5 行）。
  - [ ] `grep -c '未实现' docs/api-mapping/format-fxr3.md` == 0（17 行全清）。
  - [ ] extensions.md 4 条 entry 完整（含 Type / Upstream Ref / C API / Rationale / Impact 5 段）。

  **QA Scenarios**：

  ```
  Scenario: mapping doc 全量 in-scope 已完成
    Tool: Bash
    Steps:
      1. `grep -c '未实现' docs/api-mapping/format-tae.md`
      2. `grep -c '未实现' docs/api-mapping/format-fxr3.md`
    Expected Result: 两步均 = 0
    Failure Indicators: 任一 > 0
    Evidence: .sisyphus/evidence/task-26-mapping.log
  ```

  **Commit**: YES
  - Message: `phase7(docs): refresh format-tae.md + format-fxr3.md mapping status + finalize extensions.md`
  - Files: `docs/api-mapping/format-tae.md`, `docs/api-mapping/format-fxr3.md`, `docs/api-mapping/extensions.md`, `.sisyphus/evidence/task-26-*`
  - Pre-commit: grep 检查全过

- [x] 27. **`PLAN.md §7 Phase 7 章节 checkbox 全勾 + §1 状态表 final + §2.3 v1.x 路线图同步**

  **What to do**：
  - **PLAN.md §7 Phase 7 章节**：所有 `- [ ]` → `- [x]`；子标题加 `✅ 完成 (YYYY-MM-DD) — N/N PASS across M test binaries`（实测当时 ctest 数）。
  - **PLAN.md §1（如有「当前进度」类小节）**：Phase 7 状态切 `✅`。
  - **PLAN.md §2.3 v1.x 路线图**：v1.1 段从「**v1.1**：TAE / FXR3 + PARAMDEF XML 写出。」改为：
    - 移除已交付项目（如 TAE / FXR3 已 ship）。
    - 保留尚未交付项目（TAE Template subsystem → v1.2、PARAMDEF XML 写出 → v1.2 或单独章）。
  - **PLAN.md §11 风险表**：若 Phase 7 expose 了新风险，更新；否则 unchanged。

  **Must NOT do**：
  - ❌ 不动 §3-§6（除非 Phase 7 引入了新架构决策；本 phase 没有）。
  - ❌ 不发明测试数；用 T1 / 当前实测命令。

  **Recommended Agent Profile**：
  - **Category**: `writing`
  - **Skills**: `tech-doc-style-chinese`。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T26, T28 并行）
  - **Parallel Group**: Wave 6
  - **Blocks**: Wave Final
  - **Blocked By**: T14, T20, T23, T24, T25

  **References**：

  **Pattern References**：
  - `.sisyphus/plans/phase-6-geometry-material.md:T29` —— Phase 6 final PLAN.md sync。
  - `PLAN.md §7 Phase 6 子标题` —— 完成标记格式。

  **Acceptance Criteria**：
  - [ ] PLAN.md §7 Phase 7 章节 0 个未勾 checkbox。
  - [ ] §2.3 v1.1 段反映 TAE / FXR3 已交付。

  **QA Scenarios**：

  ```
  Scenario: Phase 7 章节 0 未勾
    Tool: Bash
    Steps:
      1. `awk '/^### Phase 7/,/^### Phase v2.0/' .sisyphus/plans/PLAN.md | grep -c '^- \[ \]'`
    Expected Result: 0
    Failure Indicators: > 0
    Evidence: .sisyphus/evidence/task-27-checkboxes.log
  ```

  **Commit**: YES
  - Message: `phase7(state): tick PLAN.md §7 Phase 7 checkboxes and sync v1.x roadmap`
  - Files: `.sisyphus/plans/PLAN.md`, `.sisyphus/evidence/task-27-*`
  - Pre-commit: grep 0 未勾

- [x] 28. **AGENTS.md + roadmap/README.md + roadmap/phase-7 + CHANGELOG.md final pass**

  **What to do**：
  - **AGENTS.md §2 Current status 表**：Phase 7 行从 `🚧 in progress` → `✅ done`；Tests 列填 `N/N PASS across M test binaries`（实测）。
  - **docs/roadmap/README.md Phase index 表**：Phase 7 行 state = `✅ done`、Tests 列填 `N/N PASS (YYYY-MM-DD)`。
  - **docs/roadmap/phase-7-animation-effects.md**：
    - 顶部 status 改 `✅ done`。
    - 「Exit criteria」段 4 个 checkbox 全勾。
    - 加最末段「Completion Retrospective」（参考 phase-1 / phase-6 形态）+ 链接到 `.sisyphus/plans/phase-7-animation-effects.md`。
  - **CHANGELOG.md**：增 `## [v0.4.0] - YYYY-MM-DD`（或 next version）章节：
    - Added: TAE format support (SDT version 0x1000D for Sekiro + Elden Ring)。
    - Added: FXR3 format support (DS3 + Sekiro binary + XML round-trip)。
    - Added: 2 new public headers (sf_tae.h, sf_fxr3.h)。
    - Added: 5 new tests under label `anim` + `e2e_er`。
    - Note: `SF_ENABLE_PHASE7` 默认 OFF（v1.0 ship）；v1.1 切 ON。
    - Note: TAE Template subsystem deferred to v1.2。
    - Note: Non-SDT TAE formats deferred to v2 (legacy games)。

  **Must NOT do**：
  - ❌ 不删除 Phase 6 / v1.x / v2 历史章节。
  - ❌ 不在 CHANGELOG 嵌入完整 plan 内容（链接即可）。
  - ❌ 不动 LICENSE / 第三方依赖列表。

  **Recommended Agent Profile**：
  - **Category**: `writing`
  - **Skills**: `tech-doc-style-chinese`（部分文档中文）。

  **Parallelization**：
  - **Can Run In Parallel**: YES（与 T26, T27 并行）
  - **Parallel Group**: Wave 6 末
  - **Blocks**: Wave Final
  - **Blocked By**: T14, T20, T23, T24, T25

  **References**：

  **Pattern References**：
  - `.sisyphus/plans/phase-6-geometry-material.md:T30` —— Phase 6 final roadmap + AGENTS 切换。
  - `CHANGELOG.md` 现有 v0.3.0 章节（Phase 3 完成时的格式）。
  - `docs/roadmap/phase-1-runtime.md:Completion Retrospective` 段 —— retrospective 形态范本。

  **Acceptance Criteria**：
  - [ ] AGENTS.md §2 表 Phase 7 行 = `✅ done` + 实测测试数。
  - [ ] docs/roadmap/README.md Phase 7 行 = `✅ done` + 日期。
  - [ ] docs/roadmap/phase-7-animation-effects.md Exit criteria 4 个全勾 + 有 Retrospective 段。
  - [ ] CHANGELOG.md 含 v0.4.0（或 next）章节列出 Phase 7 deliverables。

  **QA Scenarios**：

  ```
  Scenario: 三处状态表 + CHANGELOG 一致
    Tool: Bash
    Steps:
      1. `grep -E '\| 7 \|' AGENTS.md | grep -c '✅ done'`
      2. `grep -E '\| 7 \|' docs/roadmap/README.md | grep -c '✅ done'`
      3. `grep -c 'phase-7' docs/roadmap/phase-7-animation-effects.md` (含自我引用 retrospective)
      4. `grep -cE 'Phase 7|TAE|FXR3' CHANGELOG.md`
    Expected Result: 步骤 1-2 ≥ 1；步骤 3 ≥ 1；步骤 4 ≥ 3
    Failure Indicators: 任一为 0
    Evidence: .sisyphus/evidence/task-28-state-final.log
  ```

  **Commit**: YES
  - Message: `phase7(state): mark Phase 7 done in AGENTS.md + roadmap/README.md + roadmap doc + CHANGELOG.md`
  - Files: `AGENTS.md`, `docs/roadmap/README.md`, `docs/roadmap/phase-7-animation-effects.md`, `CHANGELOG.md`, `.sisyphus/evidence/task-28-*`
  - Pre-commit: 三处状态表 + CHANGELOG 一致性脚本通过

---

## Final Verification Wave（MANDATORY — after ALL implementation tasks）

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.
>
> **Do NOT auto-proceed after verification. Wait for user's explicit approval before marking work complete.**
> **Never mark F1-F4 as checked before getting user's okay.** Rejection or user feedback → fix → re-run → present again → wait for okay.

- [x] F1. **Plan Compliance Audit** — `oracle`

  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, run binary). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found (e.g. `grep -rn 'Template' include/souls_formats/sf_tae.h` 必须空；`grep 'ApplyTemplate' src/effects/tae.c` 必须空；`grep '0x1000B\|0x1000C' src/effects/tae.c` 必须空 except as `_skipped_` 注释；`grep -E 'FXR1|FFXDLSE|ANI|MQB' src/effects/` 必须空）。Check evidence files exist in `.sisyphus/evidence/`. Compare deliverables against plan TL;DR.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`

  Run `cmake --build build-on` 三次（ON Debug + RelWithDebInfo + sanitizers，配置时 `-B build-on -DSF_ENABLE_PHASE7=ON ...`）+ `cmake --build build-off`（OFF 配置时 `-B build-off -DSF_ENABLE_PHASE7=OFF ...`）验证条件 build 干净 + `ctest -L 'anim|e2e_er'`. Review all changed files for: `(void*)` 类型擦除滥用、空 catch (C 无 catch，关注 `if (rc != SF_OK)` 后无 cleanup)、`printf` debug 残留、commented-out code、unused includes / static functions. Check AI slop: 过度抽象（fxr3 节点不要建动态注册表）、generic names（避免 `data/result/item/temp`，应 `state/condition/operand/field`）。
  Output: `Build (ON) [PASS/FAIL] | Build (OFF) [PASS/FAIL] | Lint [PASS/FAIL] | Tests [N pass/N fail] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`

  Start from clean state（`git clean -fdx`，重新 `cmake -B build-mingw -DSF_ENABLE_PHASE7=ON`）. Execute EVERY QA scenario from EVERY task — follow exact steps, capture evidence. Test cross-task integration（FXR3 binary → XML → re-binary → 字节比对；TAE Animation reader → Event reader → EventGroup reader 集成）. Test edge cases: 空 TAE（0 animation）、空 FXR3（empty StateMap + empty Container）、DS3 vs Sekiro version 双路径、UnkAc6 InterpolationType. Save to `.sisyphus/evidence/final-qa/`.
  Output: `Scenarios [N/N pass] | Integration [N/N] | Edge Cases [N tested] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`

  For each task: read "What to do", read actual diff (`git log --oneline phase7-*`; `git diff main..HEAD`). Verify 1:1 — everything in spec was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance（特别注意：无 Template 实现 / 无 TAE 非 SDT 分支 / 无 mxml allocator override / 无 byte-equal XML 断言 / `SF_ENABLE_PHASE7=OFF` build 不含 Phase 7 符号）. Detect cross-task contamination: Task N touching Task M's files. Flag unaccounted changes.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

每个 task 末尾的 `Commit:` 段定义提交。原则：

- **粒度**：一个 task → 一个 commit；多个 task 共享提交（如 T15/T16/T17 共享 `fxr3.c` 时）合并到主 task 的 commit。
- **消息**：`phase7(<scope>): <description>` 风格；scope ∈ {`state`, `docs`, `scope`, `probe`, `tae`, `fxr3`, `fxr3-xml`, `cmake`, `tests`, `final`}。
- **Pre-commit**：每 commit 前跑 `cmake --build build-mingw` + 该 commit 涉及 label 的 `ctest -L <label>`；任一 FAIL 不许 commit。

Commit 列表（按 task 顺序）：

- **T1**：`phase7(state): switch Phase 6 to done, Phase 7 to in-progress in three status tables` — AGENTS.md, PLAN.md, docs/roadmap/README.md
- **T2**：`phase7(scope): lock TAE Template + legacy TAE formats OUT-of-scope in PLAN.md and format-tae.md` — PLAN.md, format-tae.md
- **T3**：`phase7(docs): seed extensions.md entries for TAE Template defer + mxml allocator + XML equivalence + TAE SDT-only`
- **T4**：`phase7(probe): empirical TAEFormat version in c0000.anibnd.dcx → .tae`
- **T5**：`phase7(probe): empirical FXRVersion + section distribution in sfxbnd_commoneffects.ffxbnd.dcx → .fxr`
- **T6**：`phase7(docs): pin c0000.anibnd.dcx + sfxbnd_commoneffects.ffxbnd.dcx sha256 in UPSTREAM.md`
- **T7**：`phase7(tae): public header sf_tae.h with opaque types, TAEFormat enum, AnimMiniHeader tagged union, accessor API`
- **T8**：`phase7(fxr3): public header sf_fxr3.h with opaque types, 6 enums, ConditionOperand + Field tagged unions, XML API prototypes`
- **T9**：`phase7(cmake): wire SF_ENABLE_PHASE7 to SF_SOURCES + SF_PUBLIC_HEADERS conditional block`
- **T10**：`phase7(scaffold): src/effects/ stubs (tae.c, fxr3.c, fxr3_xml_read.c, fxr3_xml_write.c) with #ifdef gate and destroy stubs`
- **T11-T13**：合并为 `phase7(tae): SDT format read+write of Header, Animation, Event, EventGroup, MiniHeader (round-trip)`（共享 src/effects/tae.c）
- **T14**：`phase7(tests): tae_synthetic round-trip (1 anim × 1 event × 1 group, SDT)`
- **T15-T19**：合并为 `phase7(fxr3): binary read+write — header, sections, StateMap/Container recursion, tagged unions, writer`（共享 src/effects/fxr3.c）
- **T20**：`phase7(tests): fxr3_synthetic binary round-trip (DS3 + Sekiro versions, all tagged union variants)`
- **T21**：`phase7(fxr3-xml): XML read via mxml DOM (schema mirrors upstream XmlSerializer)`
- **T22**：`phase7(fxr3-xml): XML write via mxml DOM (schema mirrors upstream XmlSerializer)`
- **T23**：`phase7(tests): fxr3 XML structural equality round-trip`
- **T24**：`phase7(tests): TAE ER e2e (c0000.anibnd.dcx → BND4 → .tae)`
- **T25**：`phase7(tests): FXR3 ER e2e (sfxbnd_commoneffects.ffxbnd.dcx → BND4 → .fxr)`
- **T26**：`phase7(docs): refresh format-tae.md + format-fxr3.md mapping status + finalize extensions.md`
- **T27**：`phase7(state): tick PLAN.md §7 Phase 7 checkboxes and sync v1.x roadmap`
- **T28**：`phase7(state): mark Phase 7 done in AGENTS.md + roadmap/README.md + roadmap doc + CHANGELOG.md`
- **Final**：`phase7(final): F1-F4 reviewer pass + user okay`

---

## Success Criteria

### Verification Commands

```bash
# 配置 + build (Phase 7 ON)
cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake \
    -DCMAKE_BUILD_TYPE=Debug -DSF_ENABLE_PHASE7=ON
cmake --build build-mingw                                       # Expected: 全绿无 warning

# 配置 + build (Phase 7 OFF — 确认条件 build 干净)
cmake -B build-mingw-off -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake \
    -DCMAKE_BUILD_TYPE=Debug -DSF_ENABLE_PHASE7=OFF
cmake --build build-mingw-off                                   # Expected: 全绿无 warning
nm build-mingw-off/libsouls_formats.a | grep -E 'sf_(tae|fxr3)_' # Expected: 空（无 Phase 7 符号）

# 测试
ctest --test-dir build-mingw -L anim --output-on-failure        # Expected: 全绿
ctest --test-dir build-mingw -L 'e2e_er' --output-on-failure    # Expected: 含 Phase 7 e2e 全绿
ctest --test-dir build-mingw-off -L anim --output-on-failure    # Expected: 0 tests run

# Mapping doc 完成度
grep -c '未实现' docs/api-mapping/format-tae.md                  # Expected: 0
grep -c '未实现' docs/api-mapping/format-fxr3.md                 # Expected: 0
grep -c '_skipped_' docs/api-mapping/format-tae.md               # Expected: ≥ 5 (Template-related)

# Extensions 4 条新 entry
grep -cE 'TAE Template|mxml.*allocator|XML.*equivalence|TAE.*SDT-only' docs/api-mapping/extensions.md
                                                                # Expected: ≥ 4

# 状态表三处一致
grep '✅ done.*Phase 7\|Phase 7.*✅ done' AGENTS.md              # Expected: ≥ 1
grep '✅ 完成.*Phase 7\|Phase 7.*✅ 完成' .sisyphus/plans/PLAN.md  # Expected: ≥ 1
grep '✅ done.*7\|7 .*✅ done' docs/roadmap/README.md             # Expected: ≥ 1
```

### Final Checklist

- [ ] All "Must Have" present (运行上述命令验证)
- [ ] All "Must NOT Have" absent (F1 reviewer grep 验证)
- [ ] All `anim` + Phase 7 `e2e_er` tests pass
- [ ] `SF_ENABLE_PHASE7=OFF` build 不含 Phase 7 符号
- [ ] 2 mapping doc 0 个 "未实现"
- [ ] 4 extension entry 全部录入
- [ ] 3 处状态表 Phase 7 = ✅
- [ ] CHANGELOG.md 含 Phase 7 章节
- [ ] F1-F4 全 APPROVE
- [ ] 用户最终 okay
