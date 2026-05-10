# Phase 4 — Param + Text + EMEVD 工作计划

> **Status**: ⏳ Pending · **Estimate**: 2.5 weeks · **Depends on**: Phase 2 (regulation AES + DCX), Phase 3 (BND4 reader, `er_extract_from_data0` test helper)
>
> Strict upstream alignment policy applies — see [AGENTS.md](../../AGENTS.md) §5.x and [`.sisyphus/plans/PLAN.md`](./PLAN.md).
>
> **Pinned upstream commit**: `9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a` (verified `git -C /home/soar/src/SoulsFormatsNEXT log -1`).
>
> **Scope adjustment (2026-05-11)**: User moved EMEVD from Phase 5 to Phase 4 because EMEVD is the most frequently modded format. Phase 5 reduced to ESD + MSB(s/e/vi).

---

## TL;DR

> **Quick Summary**: Implement the 5 most mod-relevant FromSoftware data formats — PARAM (parameter tables), PARAMDEF (schemas, binary + XML read), PARAMTDF (enum names), FMG (localized text), and EMEVD (event scripts). After this phase, the library can decrypt `regulation.bin`, parse the inner BND4, apply Paramdex schemas to specific .param entries, read in-game text from MSGBND archives, and parse event scripts from Data0.
>
> **Deliverables**:
> - 5 public headers (`sf_param.h`, `sf_paramdef.h`, `sf_paramtdf.h`, `sf_fmg.h`, `sf_emevd.h`)
> - 11 source files (`src/param/{5}`, `src/text/{1}`, `src/script/{5}`)
> - 11 test files (8 in `tests/param/`, 3 in `tests/script/`)
> - 1-2 example programs (`examples/sf_param_dump.c`, optional `sf_emevd_dump.c`)
> - Updated api-mapping rows (5 docs flipped from `未实现` → `✓ aligned` / `+ extension`)
> - +75 to +95 new `sf_*` DLL exports (current: **469** → target: **544-564**)
>
> **Estimated Effort**: Medium (2.5 weeks, 30-32 atomic tasks across 5 waves + 1 final review wave)
> **Parallel Execution**: YES — 5 waves (Wave 0 probe → Wave 1 foundation → Wave 2 readers → Wave 3 writers/XML/apply → Wave 4 e2e+examples) + Final 4-parallel review
> **Critical Path**: T0.1 → T1.1-T1.5 → T2.1 → T3.3 → T4.4 → F1-F4 → user okay
> **Max Concurrent**: 7 (Wave 3)

---

## Context

### Original Request

> "编写第四阶段的plan" — 用户要求生成 Phase 4 工作计划。

### Scope Adjustment

> "我需要稍微修改一下阶段内容,把emevd加入第四阶段,因为这是mod中比较经常需要修改的格式" — 用户在访谈中要求把 EMEVD 从 Phase 5 提前到 Phase 4。Phase 5 因此缩减为 ESD + MSB(s/e/vi)。Phase 4 时间估计从 1.5 周上调到 2.5 周;Phase 5 从 2.5 周下调到 2 周;项目总时间不变。

### Interview Summary

**Confirmed Decisions** (2026-05-11):

| # | Decision | Choice | Rationale |
|---|---|---|---|
| Q1 | 任务粒度 | 细粒度 4-Wave 并行 (~22-28 → 实际 32 tasks) | 最大化并行,匹配 Phase 3 模式 |
| Q2 | PARAM Cell API | Tagged union + 类型化 getter (both) | 既支持内省,又支持热路径 typed access |
| Q8 | ESD 范围 | 留在 Phase 5 与 MSB 一起 | ESD/MSB Talk system 耦合度高;mod 修改频率显著低 |
| Q10 | 时间估计 | Phase 4 = 2.5 wk, Phase 5 = 2 wk | 总和不变 |

**Defaults Applied** (用户可推翻):

| # | Decision | Default | Rationale |
|---|---|---|---|
| Q3 | regulation 流水线 helper | `er_load_param(name, **out)` 添加到 `tests/e2e/er_test_helper.h` | 简化 e2e 测试代码 |
| Q4 | PARAMDEF 二进制写出 | 在 Phase 4 内,完整 round-trip | 与 PLAN.md §7 + Phase 3 模式一致 |
| Q5 | PARAMTDF 解析器 | 自写 ~200 LoC state machine | 上游同样 naive,无新依赖 |
| Q7 | Bit-packed 字段 | little-endian bit order, `paramdef_apply.c` helper | 与上游 `Cell.cs:236-244` 1:1 |
| Q9 | EMEVD 版本覆盖 | Wave 0 探针实测 → 默认 ER/AC6/Nightreign 别名 Sekiro;novel 时扩展 enum | 风险驱动决策 |
| Q11 | EMEVD ArgData | 原始字节透传,不做 EMEDF JSON 加载 | 与上游 `Instruction.cs` 1:1 |

### Research Findings

- ER `regulation.bin` (`/mnt/c/Games/ELDEN RING/Game/regulation.bin`, 2.0 MB) — AES-256-CBC,16 字节 IV 前缀
- Paramdex `/home/soar/dev/paramdex/ER/Defs/SpEffect.xml`: ParamType="SP_EFFECT_PARAM_ST", Index=86, DataVersion=4, Unicode=True, **FormatVersion=203** (verified)
- ER `/Game/event/` 目录**不存在** — `.emevd.dcx` 全部在 `Data0.bhd/bdt`,需 `er_extract_from_data0`
- 当前 DLL 导出: **469** `sf_*` 符号
- Upstream 真实复杂度比 Phase 4 doc 揭示的更深 (见 §Metis Review 下方)

### Metis Review

> Metis (Plan Consultant) 在计划生成前进行了完整 gap 分析,识别出 36 条边界条件与 15 条 scope creep 风险。完整报告见 `.sisyphus/evidence/phase4-metis-review.md` (会在 Wave 0 写出)。

**Metis 强制指令 (已纳入本计划)**:
1. **Wave 0 经验探针**先于 Wave 1 (验证 EMEVD flag 组合 / item id / event 路径 / BND4 entry 名)
2. PARAM Apply API 用 `sf_param_apply_mode_t { UNCONDITIONAL, SOMEWHAT_CAREFUL, CAREFUL }` 枚举 (不是 bool) — 与上游 8 个 Apply 变体的 3 个核心模式对齐
3. **PARAMDEF 9 个 FormatVersion** 全部读取支持 (0/101/102/103/104/106/201/202/203),写出限定 v104/v106/v201/v202/v203
4. **VersionAware PARAMDEF 不可写**二进制 (与上游 `PARAMDEF.cs:191-192` 一致,返回 `SF_ERR_INVALID_ARG`)
5. `_Static_assert` 配对每个新 type:**enum 类型用计数/常量值断言** (`sf_paramdef_def_type_t` count==13, `sf_emevd_format_t` count==N, `sf_fmg_version_t` count==3, `sf_param_apply_mode_t` 常量值稳定, `sf_param_cell_kind_t` count==15);**byte-storage flags 类型用 sizeof 断言** (`sf_param_format_flags1_t` 与 `sf_param_format_flags2_t` 是 `typedef uint8_t` + 常量集,`sizeof == 1`)。**禁止对 C11 enum 类型本身做 sizeof==1 断言** (默认 int 4 字节,不可移植)。
6. **Wave 4.5** 任务: 翻新 `docs/api-mapping/format-{param,paramdef,paramtdf,fmg,emevd}.md` 所有 "未实现" 行 → `✓ aligned` / `+ extension`
7. 文件命名 `.sisyphus/plans/phase-4-param-text.md` (单文件,~30+ tasks)
8. 每 TODO 必须有 ≥1 happy QA + ≥1 failure QA,全部 agent-executable

**Momus Review 修正 (2026-05-11 round 1)**:
- 文件路径修正: `tests/archive/test_bhd5_e2e_er.c` → `tests/e2e/test_bhd5_e2e_er.c` (Phase 3 实际目录);`examples/sf_dcx_unwrap.c` → `examples/sf_bnd_extract.c` (Phase 3 唯一 example)
- T3.4 PARAMDEF XML 结构重写: Field 编码用 `Def="<type> <name> [= <default>]"` 属性 + 3 个 regex 解析,而非"显式 sub-element per property";子元素仅 DisplayName/Description/Minimum/Maximum/Increment/SortID/EditFlag/DisplayFormat/ParamRef1-5
- `sf_paramdef_get_index` 添加为 `+ extension` (Paramdex `<Index>` 元素上游 PARAMDEF 不读,但 Paramdex 工具用);`sf_paramdef_field_get_sort_id` 同理 (`<SortID>` 元素);两者均 XML-only,binary 时返回 sentinel

**Metis 强制禁止 (Must NOT)**:
1. EMEDF JSON loader (即使桩代码)
2. PARAMDEF XML writer (推迟 v1.1)
3. PARAM 库内 CSV/JSON/TSV 导出 (`sf_param_dump.c` 是 EXAMPLE,不是 API)
4. UnnamedRows / HeaderlessRows 处理 (Chromehounds/AC Formula Front,v1 范围外)
5. `RegulationVersioned*` Apply 变体 (推迟 v1.1)
6. Paramdex 自动发现 (`glob("*.xml")`)
7. Bit-packing 数学"美化" — `Row.cs:236-244` 的 `(64 - bitSize - bitOffset)` shift 逻辑需逐字镜像
8. FMG MD5 验证 (上游故意 skip,只读不验)
9. 修复 v103 字段大小"bug" (上游已注释承认,需保留以维持 round-trip 对等)

---

## Work Objectives

### Core Objective

完成 Phase 4 全部 5 个格式 (PARAM/PARAMDEF/PARAMTDF/FMG/EMEVD) 的 Pure C 实现,严格镜像上游 `SoulsFormatsNEXT` (commit `9f5848f5f`) 的 wire format 与 API 语义,通过合成 fixture round-trip + ER e2e 双重验证,所有验证完全 agent-executable 无人工干预。

### Concrete Deliverables

#### 公共头文件 (5)

```
include/souls_formats/sf_param.h
include/souls_formats/sf_paramdef.h
include/souls_formats/sf_paramtdf.h
include/souls_formats/sf_fmg.h
include/souls_formats/sf_emevd.h
```

#### 源文件 (11)

```
src/param/param.c                  ← PARAM 二进制读+写 + 头部解析
src/param/paramdef.c               ← PARAMDEF 二进制读+写 (9 versions read; 5 versions write)
src/param/paramdef_xml_read.c      ← mxml DOM → sf_paramdef_t (无写出)
src/param/paramdef_apply.c         ← 3-mode apply + bitstream extract/insert helpers
src/param/paramtdf.c               ← PARAMTDF text parser + writer

src/text/fmg.c                     ← FMG v0/v1/v2 读+写,MD5 prefix,group merging,ReuseOffsets

src/script/emevd.c                 ← EMEVD 主入口 + Game format probe
src/script/emevd_event.c           ← Event 子结构 (含 RestBehavior, Parameters)
src/script/emevd_instruction.c     ← Instruction (Bank/ID/ArgData/Layer)
src/script/emevd_layer.c           ← Layer mask (5 字段读写)
src/script/emevd_parameter.c       ← Parameter (指令参数引用)
```

#### 测试 (11 + 1 evidence)

```
tests/param/test_param_synthetic.c               ← 3 行 × 5 字段 round-trip
tests/param/test_paramdef_binary.c               ← 9 versions round-trip (v0/v101/v102/v103/v104/v106/v201/v202/v203)
tests/param/test_paramdef_xml.c                  ← mxml 反序列化 + Paramdex SpEffect.xml e2e
tests/param/test_paramtdf_synthetic.c            ← 3-entry text round-trip + edge cases
tests/param/test_fmg_synthetic.c                 ← v0/v1/v2 + MD5 + ReuseOffsets
tests/param/test_paramdef_xml_e2e.c              ← /home/soar/dev/paramdex/ER/Defs/SpEffect.xml
tests/param/test_param_apply_paramdef_e2e.c      ← regulation.bin → BND4 → SpEffectParam → apply SpEffect.xml
tests/param/test_fmg_e2e_er.c                    ← er_extract_from_data0 → ItemName.fmg → query item

tests/script/test_emevd_synthetic.c              ← 1 event × 1 instruction round-trip + Layer
tests/script/test_emevd_format_probe.c           ← Wave 0 probe runner + Wave 2 follow-up
tests/script/test_emevd_e2e_er.c                 ← er_extract_from_data0 → emevd.dcx → 验证 + flag 报告

.sisyphus/evidence/phase4-pre-flight.md          ← Wave 0 探针输出 (持久 evidence)
```

#### 测试辅助库扩展 (1)

```
tests/e2e/er_test_helper.h    (extend)  ← + er_load_param(name, **out, *out_size)
tests/e2e/er_test_helper.c    (extend)  ← regulation AES + BND4 + entry lookup 一站式
```

#### 示例 (1 必需 + 1 可选)

```
examples/sf_param_dump.c    ← regulation.bin + SpEffect.xml → TSV (id\tname\ticon_id...)
examples/sf_emevd_dump.c    ← (可选) emevd.dcx → 事件列表 + 指令计数
```

#### API mapping 文档刷新 (5)

```
docs/api-mapping/format-param.md       ← 全部 "未实现" → "✓ aligned" / "+ extension"
docs/api-mapping/format-paramdef.md    ← 同上
docs/api-mapping/format-paramtdf.md    ← 同上
docs/api-mapping/format-fmg.md         ← 同上
docs/api-mapping/format-emevd.md       ← 同上 (并加注 ER/AC6/Nightreign 探针结果)
docs/api-mapping/extensions.md         ← 新增: er_load_param, format probe, 3-mode apply
docs/api-mapping/POLICY.md             ← 新增: PARAMTDF Trim('"') 镜像, 8→3 Apply 折叠
```

### Definition of Done

每条都是 agent-executable 验证条件 (无人工干预):

- [ ] `cmake --build build-mingw` 全绿,零 warning,`-Werror` 通过
- [ ] `ctest --test-dir build-mingw -L param --output-on-failure` 全绿 (8 测试 PASS,0 FAIL)
- [ ] `ctest --test-dir build-mingw -L script --output-on-failure` 全绿 (3 测试 PASS,0 FAIL)
- [ ] `objdump -p libsouls_formats.dll | grep -c '^\s*\[\s*[0-9]\+\]\s*sf_'` 输出 ∈ [544, 564]
- [ ] `examples/sf_param_dump.exe` 跑过 ER `regulation.bin` + `SpEffect.xml`,产生非空 TSV (≥100 行 + 表头)
- [ ] Wave 0 probe evidence `.sisyphus/evidence/phase4-pre-flight.md` 存在并包含 EMEVD 头部 16 字节、SpEffectParam BND4 entry 名、ItemName.fmg item 1030000 文本
- [ ] 所有 api-mapping `format-*.md` 中 Phase 4 涉及行 status 不为 "未实现"
- [ ] `extensions.md` + `POLICY.md` 包含本阶段 6 处 divergence 文档
- [ ] PLAN.md Phase 4 box ticked,记录测试通过数 + 时间戳

### Must Have

- 严格镜像上游 wire format,合成 fixture round-trip 字节级一致 (5 格式)
- ER e2e (regulation pipeline + msgbnd FMG + emevd) 全部 PASS,SKIP 仅在前置数据缺失时合法
- PARAM 应用 PARAMDEF 后 cell 值正确 (含 bit-packed 字段)
- 9 个 PARAMDEF FormatVersion 全部读取通过
- EMEVD 5 个上游 Game 变体 + ER 探针结果文档化
- FMG MD5 prefix 检测 + group merging 算法与上游一致
- 每个公共 enum 配 `_Static_assert` 守护
- 每个 TODO 含 ≥1 Happy + ≥1 Failure QA scenario,全部 agent-executable

### Must NOT Have (Guardrails)

> 这些是 Metis 识别的高风险 AI slop / scope creep 模式,执行时**禁止**。

#### 代码层禁止

- ❌ EMEDF JSON loader (即使空桩)
- ❌ PARAMDEF XML 写出 (推迟 v1.1)
- ❌ PARAM 库内 CSV/TSV/JSON 导出函数 (示例程序可有,头文件不可有)
- ❌ UnnamedRows / HeaderlessRows 处理 (v1 不支持 Chromehounds/AC Formula Front)
- ❌ `ApplyRegulationVersioned*` 8 个变体 (推迟 v1.1)
- ❌ Paramdex 自动发现 (`glob`/`opendir`),测试必须取显式路径
- ❌ Bit-packing 数学逻辑"美化" — 严格镜像 `Row.cs:236-244` 的 `(64 - bitSize - bitOffset)` shift
- ❌ FMG MD5 hash 校验 — 上游只读不验,镜像
- ❌ 修复 v103 field size 上游 bug — 保留以维持 round-trip 对等
- ❌ Cell.Value 缓存到 PARAM 内 — 每次 apply 重计算
- ❌ 自动检测 endianness 不匹配 — 上游 ApplyParamdefCarefully **不**检查,镜像
- ❌ 自动忽略 orphaned bits — 上游 `Row.cs:138` throws,镜像 (返回 `SF_ERR_BAD_DATA`)
- ❌ PARAMTDF "宽容" 解析 (escape sequences, BOM, comments) — 上游 `Trim('"')` 简单粗暴,镜像
- ❌ FMG `byte[0] == 0x00` 但实际是 MD5 的情况 — 镜像上游限制 (无法区分)
- ❌ 自动推导 PARAM 游戏类型 — 消费者关注点
- ❌ `sf_*_clone` 函数 (PARAM/PARAMDEF/FMG/EMEVD) — 上游有 copy ctor 不等于 C 必须有
- ❌ Cell typed-getter 超出 13 个 DefType — 一类型一 getter,不多不少
- ❌ Unicode 检测启发式 — flag 错就错,信任上游

#### 架构层禁止

- ❌ PARAM 类型层依赖 PARAMDEF — `sf_param_t *` 不持有 PARAMDEF 引用
- ❌ PARAMTDF 二进制读写 — 上游纯文本,镜像
- ❌ EMEVD 自动解析 LinkedFileOffsets 字符串 — 暴露 offset list + StringData blob,消费者切片
- ❌ `sf_paramdef_get_field_at_byte_offset` 类反向查询 helper

#### 进程层禁止

- ❌ 跨任务部分提交 (一个任务的代码分多个 commit) — 一任务一 commit (除非任务说明明确允许)
- ❌ 手编辑 fixture 二进制 — `tests/fixtures/synthetic/` 全部由 C `build_fixture()` 在 test-time 生成
- ❌ 跳过 `_Static_assert` 配对枚举与查表 — 这是项目 Phase 0 起的硬约定

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** - ALL verification is agent-executed. No exceptions.
> Acceptance criteria requiring "user manually tests/confirms" are FORBIDDEN.

### Test Decision

- **Infrastructure exists**: ✅ YES — Unity (ThrowTheSwitch) + ctest 在 Phase 0 接入,Phase 1-3 已用 32+ 测试验证
- **Automated tests**: **YES (Tests-after,与 Phase 1-3 一致)** — 每任务实现 + 对应 `test_*.c` 文件;Wave 4 添加 e2e 整合测试
- **Framework**: Unity (单头文件 MIT)
- **Test runner**: `ctest --test-dir build-mingw -L <label>`,labels: `param`, `script`
- **Wiring**: `tests/CMakeLists.txt` 通过 `sf_add_test(<name> <source> LABELS <labels>)` helper

### QA Policy

每个任务 MUST 包含 agent-executable QA scenarios (见 TODO 模板)。Evidence 保存至 `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`。

- **库/模块** (本阶段主要类型): Bash + ctest + objdump
  - `ctest --test-dir build-mingw -L <label>` 跑测试,stdout 重定向至 evidence log
  - `objdump -p libsouls_formats.dll | grep '^\s*\[\s*[0-9]\+\]\s*sf_'` 验证导出符号增长
  - `cmake --build build-mingw 2>&1 | tee build.log` 验证零 warning
- **TUI/CLI** (示例程序): `interactive_bash` (tmux) — 跑 `examples/sf_param_dump.exe`,捕获 stdout,grep 表头与行计数
- **不适用**: Playwright (无 UI)、curl (无 API)

### Evidence 命名约定

```
.sisyphus/evidence/
├── phase4-pre-flight.md                       ← Wave 0 探针报告 (持久)
├── phase4-metis-review.md                     ← Metis gap 分析 (持久)
├── task-{N}-{scenario-slug}.log               ← ctest output
├── task-{N}-{scenario-slug}-build.log         ← cmake build output
├── task-{N}-{scenario-slug}-objdump.txt       ← DLL 符号 diff
└── final-qa/                                  ← Final Wave F3 输出
    ├── F1-plan-compliance.md
    ├── F2-code-quality.md
    ├── F3-real-qa.md
    └── F4-scope-fidelity.md
```

---

## Execution Strategy

### Parallel Execution Waves

> 5 个工作 Wave + 1 个最终 review Wave。最大化并行度,Phase 4 估计 2.5 周内完成。

```
Wave 0 (Pre-Flight Probe — MUST run before Wave 1):
└── T0.1: ER 经验探针 — 验证 EMEVD flag/item id/event path/BND4 entry name [unspecified-high]

Wave 1 (Foundation/Scaffolding — 7 parallel after Wave 0):
├── T1.1: sf_param.h 公共类型 + enum (FormatFlags1/2, Cell value tagged union, apply mode) [quick]
├── T1.2: sf_paramdef.h 公共类型 + DefType enum + EditFlags [quick]
├── T1.3: sf_paramtdf.h 公共类型 [quick]
├── T1.4: sf_fmg.h 公共类型 + Version enum [quick]
├── T1.5: sf_emevd.h 公共类型 + Format enum + Event/Instruction/Layer/Parameter [quick]
├── T1.6: src/param/paramdef_apply.c bitstream helpers (extract_bits / insert_bits) [unspecified-high]
└── T1.7: CMakeLists 更新 (新增 src/param /text /script,tests/param /script,examples) [quick]

Wave 2 (Binary Readers + Probe Validator — 5 parallel after Wave 1):
├── T2.1: PARAMDEF 二进制读取器 (9 versions, all field encoding branches) [deep]
├── T2.2: PARAM 二进制读取器 (header + rows + names,无 apply) [deep]
├── T2.3: PARAMTDF 文本解析器 (state machine ~200 LoC) [unspecified-high]
├── T2.4: FMG 读取器 (v0/v1/v2 + MD5 + groups + 双 string offset width) [unspecified-high]
└── T2.5: EMEVD 读取器 (5 Game variants + Layer + Parameter + LinkedFile) [deep]

Wave 3 (Writers + XML + Apply — 7 parallel after Wave 2):
├── T3.1: PARAMDEF 二进制写出 (5 versions: v104/v106/v201/v202/v203;v0/v101/v102/v103 read-only) [deep]
├── T3.2: PARAM 二进制写出 (header + rows + names + strings) [deep]
├── T3.3: PARAM apply paramdef (3-mode + cell tagged union 填充 + bit-packing 解码) [ultrabrain]
├── T3.4: PARAMDEF XML 反序列化 (mxml DOM walk,9 + VersionAware fields) [unspecified-high]
├── T3.5: PARAMTDF 文本写出 (entries → naive text format) [quick]
├── T3.6: FMG 写出 (group merging + MD5 计算 + ReuseOffsets) [unspecified-high]
└── T3.7: EMEVD 写出 (5 Game variants + Layer/Parameter/Instruction/LinkedFile) [deep]

Wave 4 (e2e + Examples — 6 parallel after Wave 3):
├── T4.1: er_test_helper 扩展 (er_load_param 一站式辅助) [quick]
├── T4.2: PARAM/PARAMDEF/PARAMTDF/FMG 合成 fixture 单测 (5 测试) [unspecified-high]
├── T4.3: PARAMDEF XML 真实 Paramdex e2e 单测 [quick]
├── T4.4: PARAM apply paramdef 真实 regulation.bin e2e 单测 (含 bit-packing 边界) [unspecified-high]
├── T4.5: FMG 真实 ER msgbnd e2e 单测 [quick]
├── T4.6: EMEVD 合成 + 真实 ER e2e 单测 (含 format probe 后续验证) [unspecified-high]
└── T4.7: examples/sf_param_dump.c (+ 可选 sf_emevd_dump.c) [quick]

Wave 4.5 (Documentation Polish — 1 task after Wave 4 全绿):
└── T4.8: 翻新 5 个 api-mapping format-*.md + extensions.md + POLICY.md [writing]

Wave FINAL (4 parallel reviews — after T4.8 完成):
├── F1: Plan compliance audit [oracle]
├── F2: Code quality review [unspecified-high]
├── F3: Real manual QA [unspecified-high]
└── F4: Scope fidelity check [deep]
→ 用户 explicit okay → 标记 Phase 4 完成

Critical Path: T0.1 → T1.1 → T2.1 → T3.3 → T4.4 → T4.8 → F1-F4 → user okay
Parallel Speedup: ~65% faster than sequential (估算)
Max Concurrent: 7 (Wave 3)
```

### Dependency Matrix

| Task | Depends On | Blocks |
|---|---|---|
| T0.1 | — | T1.* (强约束:Wave 0 报告 informs Wave 2 EMEVD enum 设计) |
| T1.1-T1.5 | T0.1 | T2.* |
| T1.6 | T1.1, T1.2 | T3.3 |
| T1.7 | T1.1-T1.6 | All builds |
| T2.1 | T1.2, T1.6, T1.7 | T3.1, T3.3 |
| T2.2 | T1.1, T1.7 | T3.2, T3.3 |
| T2.3 | T1.3, T1.7 | T3.5 |
| T2.4 | T1.4, T1.7 | T3.6 |
| T2.5 | T1.5, T1.7 | T3.7 |
| T3.1 | T2.1 | T4.2, T4.3 |
| T3.2 | T2.2 | T4.2, T4.4 |
| T3.3 | T1.6, T2.1, T2.2 | T4.4 |
| T3.4 | T1.2, T2.1 | T4.3, T4.4 |
| T3.5 | T2.3 | T4.2 |
| T3.6 | T2.4 | T4.2, T4.5 |
| T3.7 | T2.5 | T4.2, T4.6 |
| T4.1 | Phase 3 helper | T4.4, T4.5, T4.6 |
| T4.2 | T3.1-T3.7 | T4.8 |
| T4.3 | T3.4 | T4.8 |
| T4.4 | T3.2, T3.3, T4.1 | T4.8 |
| T4.5 | T3.6, T4.1 | T4.8 |
| T4.6 | T3.7, T4.1, T0.1 | T4.8 |
| T4.7 | T4.4 | F1-F4 |
| T4.8 | T4.2-T4.7 | F1-F4 |
| F1-F4 | T4.8 | user okay |

### Agent Dispatch Summary

| Wave | Tasks | Agent assignment |
|---|---|---|
| 0 | 1 | T0.1 → `unspecified-high` |
| 1 | 7 | T1.1-T1.5 → `quick` (header 文件); T1.6 → `unspecified-high` (bitstream 数学); T1.7 → `quick` |
| 2 | 5 | T2.1, T2.2, T2.5 → `deep` (复杂版本/格式分支); T2.3, T2.4 → `unspecified-high` |
| 3 | 7 | T3.1, T3.2, T3.7 → `deep`; T3.3 → `ultrabrain` (bit-packing + 3 mode 最复杂); T3.4, T3.6 → `unspecified-high`; T3.5 → `quick` |
| 4 | 7 | T4.1, T4.3, T4.7 → `quick`; T4.2, T4.4, T4.5, T4.6 → `unspecified-high` |
| 4.5 | 1 | T4.8 → `writing` |
| Final | 4 | F1 → `oracle`; F2 → `unspecified-high`; F3 → `unspecified-high`; F4 → `deep` |

---

## TODOs

> Implementation + Test = ONE Task (除非任务标注 "test-only")。
> EVERY task MUST have: Recommended Agent Profile + Parallelization info + References + Acceptance Criteria + ≥1 Happy QA + ≥1 Failure QA。
> **A task WITHOUT QA Scenarios is INCOMPLETE. No exceptions.**

### Wave 0 — Pre-Flight Empirical Probe (MUST run first)

- [x] **T0.1. ER 经验探针: 验证 EMEVD flags / event 路径 / item id / BND4 entry name**

  **What to do**:
  写一个临时 C 程序 (或 ctest 中的 setup-only 测试) 跑下列探测:
  1. 调 `er_extract_from_data0` 尝试解出至少一个 ER `.emevd.dcx` (枚举候选路径列表: `/event/m60_42_36_00.emevd.dcx`, `/event/common.emevd.dcx`, `/event/m11_00_00_00.emevd.dcx`, `/event/m60_44_52_00.emevd.dcx`)。第一个成功者:打印 DCX 解压后前 16 字节十六进制,验证 magic == `EVD\0`,记录 `bigEndian/is64Bit/unk06/unk07/version` 5 字节。
  2. 调 Phase 2 `regulation_decrypt` 解密 ER `regulation.bin` → 内部 BND4 → 列出 entry name 中含 `SpEffectParam.param` 的精确路径 (期望 `param/GameParam/SpEffectParam.param` 但实测验证)。
  3. 同上,extracr `/msg/engus/item.msgbnd.dcx` (或备用 `/msg/engUS/`、`/msg/en-US/`)→ BND4 → 找 `ItemName.fmg` → 因 FMG reader 还未实现,只验证 entry 存在 + size > 1KB。
  4. 全部结果写入 `.sisyphus/evidence/phase4-pre-flight.md`,作为后续 Wave 决策的 ground truth。

  **Must NOT do**:
  - 不实现 FMG/EMEVD 解析逻辑 (本任务仅探测 wire-level)
  - 不修改任何 src/ 文件 (探针程序临时存于 tests/ 或 examples/probe/,本任务后可删)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 涉及 Phase 2/3 多个 helper 调用 + 字节级分析,需理解 BHD5 + AES + DCX + BND4 全链路
  - **Skills**: 无 (现有 Bash + interactive_bash 足够)

  **Parallelization**:
  - **Can Run In Parallel**: NO (Wave 0 单任务,阻塞 Wave 1)
  - **Wave**: 0 (序列化的第一步)
  - **Blocks**: T1.5 (sf_emevd.h Format enum 设计依赖探针结果),T4.5 (FMG msgbnd 路径),T4.6 (EMEVD e2e 路径)
  - **Blocked By**: 无 (Phase 3 helper 已就绪)

  **References**:

  **Pattern References** (existing code to follow):
  - `tests/e2e/er_test_helper.h` — 现有 `er_extract_from_data0` 接口
  - `tests/e2e/test_bhd5_e2e_er.c` — 路径哈希 + DCX 解压示例 (Phase 3 已就绪)

  **Upstream References** (for context only,本任务不实现):
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/EMEVD.cs:95-117` — magic + 5 flag combinations
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:68-74` — MD5 byte 0 detection

  **Test Data**:
  - `/mnt/c/Games/ELDEN RING/Game/regulation.bin` (verified 2.0 MB)
  - ER Data0.bhd/bdt (Phase 3 已可读)

  **Acceptance Criteria**:
  - [ ] `.sisyphus/evidence/phase4-pre-flight.md` 文件存在
  - [ ] 文件包含字段: "EMEVD magic (hex):", "EMEVD flags (5 bytes):", "EMEVD format match: {Sekiro|Novel}", "SpEffectParam BND4 entry name:", "ItemName.fmg msgbnd path:", "ItemName.fmg entry size:"
  - [ ] EMEVD flag 字节实测值与 `EMEVD.cs:114` Sekiro 变体 (bigEndian=0, is64Bit=1, unk06=1, unk07=1, version=0xCD) 比对结果记录 (匹配 → 后续 T1.5 用 Sekiro 别名;novel → 后续 T1.5 必须扩展 enum)
  - [ ] 探针程序源代码可重复运行 (commit 至 `tests/script/test_emevd_format_probe.c` 或临时 `examples/probe_phase4.c`)

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — ER full data set 可达):
    Tool: interactive_bash (tmux)
    Preconditions: 
      - /mnt/c/Games/ELDEN RING/Game/{Data0.bhd,Data0.bdt,regulation.bin} 可达
      - ~/dev/oodle/oo2core_6_win64.dll 可达
      - Phase 3 build-mingw 工件存在
    Steps:
      1. cd /home/soar/src/souls-formats-c
      2. cmake --build build-mingw --target souls_formats_test_emevd_format_probe
      3. ./build-mingw/tests/script/souls_formats_test_emevd_format_probe.exe 2>&1 | tee /tmp/probe.log
      4. cat .sisyphus/evidence/phase4-pre-flight.md | grep -E 'EMEVD magic|format match|SpEffectParam BND4|ItemName.fmg'
    Expected Result: 
      - Step 3 退出码 0
      - Step 4 输出 6+ 行,每行匹配上面字段 grep 模式
      - "EMEVD magic (hex):" 行包含 "45 56 44 00" (= "EVD\0")
      - "EMEVD format match:" 行包含 "Sekiro" 或 "Novel: <flags>"
    Failure Indicators: 
      - "EMEVD magic" 字段缺失或 hex 值不是 45 56 44 00
      - 探针程序 segfault / abort
    Evidence: .sisyphus/evidence/phase4-pre-flight.md (持久) + .sisyphus/evidence/task-T0.1-happy.log (probe stdout)

  Scenario B (FAILURE — Data0 缺失,SKIP graceful):
    Tool: interactive_bash
    Preconditions:
      - 临时重命名 /mnt/c/Games/ELDEN RING/Game/Data0.bhd 为 .bhd.bak (test-only)
    Steps:
      1. ./build-mingw/tests/script/souls_formats_test_emevd_format_probe.exe 2>&1 | tee /tmp/probe-skip.log
      2. mv /mnt/c/Games/ELDEN RING/Game/Data0.bhd.bak /mnt/c/Games/ELDEN RING/Game/Data0.bhd  # 恢复
    Expected Result:
      - 探针返回退出码 0 (Unity TEST_IGNORE,不算失败)
      - /tmp/probe-skip.log 含 "IGNORE" 或 "data0 not found"
      - .sisyphus/evidence/phase4-pre-flight.md 写入 "SKIP: ER data unavailable" 标记
    Evidence: .sisyphus/evidence/task-T0.1-skip.log
  ```

  **Commit**: `phase4(probe): empirical pre-flight verification of EMEVD/PARAM/FMG paths`
  - Files: `tests/script/test_emevd_format_probe.c` (新增,Wave 0 stub;Wave 2 重用), `tests/script/CMakeLists.txt`, `.sisyphus/evidence/phase4-pre-flight.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -R emevd_format_probe`

---

### Wave 1 — Foundation / Scaffolding (parallel after T0.1)

- [x] **T1.1. `sf_param.h` 公共类型 + 枚举 (含 Cell tagged union, FormatFlags, apply mode)**

  **What to do**:
  在 `include/souls_formats/sf_param.h` 创建公共 API 表面,严格镜像上游 `PARAM.cs` + `Row.cs` + `Cell.cs`:
  1. 不透明类型: `typedef struct sf_param sf_param_t; typedef struct sf_param_row sf_param_row_t; typedef struct sf_param_cell sf_param_cell_t;`
  2. **Byte-sized flags 类型** `sf_param_format_flags1_t` (PARAM.cs:467-513,上游 `enum : byte` — C 中改为 `typedef uint8_t` + 常量集以保证 sizeof==1,C11 enum 默认 int=4 字节不保证可移植性):
     ```c
     typedef uint8_t sf_param_format_flags1_t;
     #define SF_PARAM_FORMAT_FLAGS1_NONE             ((sf_param_format_flags1_t)0x00)
     #define SF_PARAM_FORMAT_FLAGS1_FLAG01           ((sf_param_format_flags1_t)0x01)
     #define SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET  ((sf_param_format_flags1_t)0x02)
     #define SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET ((sf_param_format_flags1_t)0x04)
     /* ... 其余 bits 镜像上游 */
     #define SF_PARAM_FORMAT_FLAGS1_OFFSET_PARAM_TYPE ((sf_param_format_flags1_t)0x80)
     _Static_assert(sizeof(sf_param_format_flags1_t) == 1, "FormatFlags1 must be 1 byte");
     ```
  3. **Byte-sized flags 类型** `sf_param_format_flags2_t` (PARAM.cs:519-565,同样 typedef + 常量):
     ```c
     typedef uint8_t sf_param_format_flags2_t;
     #define SF_PARAM_FORMAT_FLAGS2_NONE                ((sf_param_format_flags2_t)0x00)
     #define SF_PARAM_FORMAT_FLAGS2_UNICODE_ROW_NAMES   ((sf_param_format_flags2_t)0x01)
     /* ... */
     _Static_assert(sizeof(sf_param_format_flags2_t) == 1, "FormatFlags2 must be 1 byte");
     ```
  4. Enum `sf_param_apply_mode_t { SF_PARAM_APPLY_UNCONDITIONAL, SF_PARAM_APPLY_SOMEWHAT_CAREFUL, SF_PARAM_APPLY_CAREFUL }` (Metis 强制:取代上游 8 个 method 中的 3 个核心模式;`RegulationVersioned*` 推迟 v1.1)。配 `_Static_assert(SF_PARAM_APPLY_CAREFUL == 2, ...)` 验证常量值稳定 (sizeof 上游 C# 不约束)。
  5. Enum `sf_param_cell_kind_t { U8, S8, U16, S16, U32, S32, B32, F32, ANGLE32, F64, DUMMY8_BIT, DUMMY8_ARRAY, U8_ARRAY, FIXSTR, FIXSTR_W }` — 镜像 `Cell.cs:28-57` 13 个 DefType + 拆分 u8/dummy8 双模式。配 `_Static_assert` 验证 enum **计数** (count == 15) 而非 sizeof。
  5.1. **C11 enum size 提醒**: C11 `enum` 底层类型由编译器选择 (`int` 或更紧凑;MinGW-w64 默认 `int`),禁止对 enum 类型本身做 `sizeof == N` 断言。对需要 byte/u16 storage 的 flags 用 `typedef uint8_t/uint16_t` + 常量集。
  6. Tagged union `sf_param_cell_value_t { sf_param_cell_kind_t kind; union { uint8_t u8, int8_t s8, uint16_t u16, int16_t s16, uint32_t u32, int32_t s32, uint32_t b32, float f32, float angle32, double f64, struct { const uint8_t *data; size_t size; } bytes, char *str_utf8, /* fixstr_w 也是 utf8 显示 */ } v; }`。
  7. 类型化 getter 声明 (13 个,一类型一个): `sf_result_t sf_param_cell_get_u8(const sf_param_row_t *r, const char *name, uint8_t *out)` ... `sf_param_cell_get_string(const sf_param_row_t *r, const char *name, const char **out_utf8)`。
  8. PARAM 主 API 声明: `sf_param_read_from_memory`, `sf_param_read_from_stream`, `sf_param_read_from_path`, `sf_param_write_to_memory`, `sf_param_write_to_stream`, `sf_param_write_to_path`, `sf_param_apply_paramdef`, `sf_param_apply_paramdef_multi`, `sf_param_destroy`, getters (param_type, big_endian, format2d, format2e, paramdef_format_version, paramdef_data_version, row_count, get_row, find_row_by_id)。
  9. Row API: `sf_param_row_get_id`, `sf_param_row_get_name`, `sf_param_row_get_cell_count`, `sf_param_row_get_cell`, `sf_param_row_find_cell` (按 internal name)。
  10. Cell API: `sf_param_cell_get_value` (返回 tagged union), 13 typed getters。
  11. 全部公共符号用 `SF_API` 装饰。
  12. 文件头: GPL-3.0 license header + ASCII art divider + 上游 commit hash 引用。

  **Must NOT do**:
  - 不实现任何 .c 内容 (本任务仅头文件)
  - 不添加 RegulationVersioned* API
  - 不添加 sf_param_clone / 其它上游没有的 helper
  - 不暴露 StringOffsetDictionary 等 writer-internal 状态

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 纯头文件声明,无逻辑实现,但需细致镜像 13 个 enum + tagged union
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 1)
  - **Wave**: 1 (与 T1.2-T1.7 并行)
  - **Blocks**: T1.6, T2.2, T3.2, T3.3
  - **Blocked By**: T0.1

  **References**:

  **Pattern References** (existing code to follow):
  - `include/souls_formats/sf_bnd4.h` — 不透明类型 + getter + SF_API 模式
  - `include/souls_formats/sf_dcx.h:sf_dcx_type_t` — enum + `_Static_assert` 配对模式
  - `src/core/error.c:_Static_assert(sizeof(g_result_strs) / sizeof(...))` — 静态断言守护查表

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/PARAM.cs:1-65` — 类声明 + properties
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/PARAM.cs:467-565` — FormatFlags1/2 enums
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/Row.cs:13-50` — Row 类 + properties
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/Cell.cs:1-60` — Cell 类 + DefType 13 类型

  **API Mapping Reference**:
  - `docs/api-mapping/format-param.md` — 35 行 row-level mapping (Wave 4.5 翻新此文件)

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_param.h` 文件存在 + GPL header
  - [ ] 编译为 C11 干净 (`gcc -fsyntax-only -Wall -Wextra -Werror -std=c11 -I include sf_param.h`) → 退出码 0
  - [ ] 至少 4 个 `_Static_assert`: FormatFlags1 sizeof==1, FormatFlags2 sizeof==1, apply_mode 常量值稳定, cell_kind count==15
  - [ ] FormatFlags1/2 是 `typedef uint8_t` + 常量集 (**不是** C enum)
  - [ ] 全部公共符号有 `SF_API` 前缀,无 `static` 函数声明
  - [ ] `grep -c '^SF_API' include/souls_formats/sf_param.h` ≥ 30
  - [ ] tagged union 含 15 个 cell kinds (含 dummy8/u8 双模式拆分)

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — header 编译通过):
    Tool: Bash
    Preconditions: 仅 sf_common.h + sf_io.h 已就绪 (Phase 0/1 完成)
    Steps:
      1. cd /home/soar/src/souls-formats-c
      2. x86_64-w64-mingw32-gcc -fsyntax-only -Wall -Wextra -Wpedantic -Werror -std=c11 \
           -I include -include souls_formats/sf_common.h \
           include/souls_formats/sf_param.h 2>&1 | tee /tmp/T1.1-syntax.log
      3. echo "Exit: $?"
      4. grep -c '^SF_API' include/souls_formats/sf_param.h
    Expected Result:
      - Step 2 输出为空 (零 warning + 零 error)
      - Step 3 输出 "Exit: 0"
      - Step 4 输出 ≥ 30
    Failure Indicators:
      - 任何 warning/error
      - 退出码非 0
    Evidence: .sisyphus/evidence/task-T1.1-syntax.log

  Scenario B (FAILURE — _Static_assert 完整性检测):
    Tool: Bash
    Preconditions: T1.1 实现完成
    Steps:
      1. grep -c '_Static_assert' include/souls_formats/sf_param.h
      2. grep -E '_Static_assert.*sizeof.*sf_param_format_flags1_t.*==.*1' include/souls_formats/sf_param.h
      3. grep -E '_Static_assert.*sizeof.*sf_param_format_flags2_t.*==.*1' include/souls_formats/sf_param.h
      4. grep -E 'typedef\s+uint8_t\s+sf_param_format_flags[12]_t' include/souls_formats/sf_param.h
    Expected Result:
      - Step 1 ≥ 4
      - Step 2 至少 1 行 (FormatFlags1 是 uint8_t 别名)
      - Step 3 至少 1 行 (FormatFlags2 是 uint8_t 别名)
      - Step 4 至少 2 行 (两个 typedef 都存在)
    Failure Indicators:
      - Step 1 < 4 → _Static_assert 不完整
      - Step 2-3 空 → flags sizeof 守护缺失
      - Step 4 < 2 → 错误地把 flags 实现为 C enum (会破坏 sizeof 假设)
    Evidence: .sisyphus/evidence/task-T1.1-static-assert.log
  ```

  **Commit**: `phase4(param): add sf_param.h public types, enums, cell tagged union`
  - Files: `include/souls_formats/sf_param.h`
  - Pre-commit: `bash -c 'x86_64-w64-mingw32-gcc -fsyntax-only -Wall -Werror -std=c11 -I include include/souls_formats/sf_param.h'`

- [x] **T1.2. `sf_paramdef.h` 公共类型 + DefType + EditFlags + 9 FormatVersion 常量**

  **What to do**:
  在 `include/souls_formats/sf_paramdef.h` 创建公共 API 表面,严格镜像上游 `PARAMDEF.cs` + `Field.cs`:
  1. 不透明类型: `typedef struct sf_paramdef sf_paramdef_t; typedef struct sf_paramdef_field sf_paramdef_field_t;`
  2. Enum `sf_paramdef_def_type_t` 13 个值: s8/u8/s16/u16/s32/u32/b32/f32/angle32/f64/dummy8/fixstr/fixstrW (Field.cs:14)。配 `_Static_assert(SF_PARAMDEF_DEF_TYPE_FIXSTR_W + 1 == 13, "DefType count")` (用计数断言,**不**用 sizeof,因 C11 enum 默认 int)。
  3. Enum `sf_paramdef_edit_flags_t` (Field.cs:86): None=0, Wrap=1, Lock=4, ... (按上游枚举,上游用 `[Flags]` u32)。配 `_Static_assert` 验证常量值 (e.g., `SF_PARAMDEF_EDIT_FLAGS_LOCK == 4`)。
  4. Format version 常量: `#define SF_PARAMDEF_FORMAT_VERSION_BASIC 0`, `..._101 101`, `..._102 102`, `..._103 103`, `..._104 104`, `..._106 106`, `..._201 201`, `..._202 202`, `..._203 203`。配 `_Static_assert(SF_PARAMDEF_FORMAT_VERSION_203 == 203, ...)` 验证 9 个常量值。
  5. PARAMDEF 主 API: `sf_paramdef_read_from_memory/stream/path` (binary), `sf_paramdef_read_xml_from_memory/path` (mxml), `sf_paramdef_write_to_memory/stream/path` (binary 仅 v104+),`sf_paramdef_destroy`,getters (data_version, param_type, big_endian, unicode, format_version, version_aware, field_count, get_field, get_row_size)。
  5.1. **Paramdex Extension API** (`+ extension`,上游 PARAMDEF 类不含 Index 字段,但 Paramdex XML 含 `<Index>` 元素;为支持 Paramdex 工具生态,添加 `sf_paramdef_get_index(const sf_paramdef_t *def) -> int32_t`,binary 读时返回 -1,XML 读时从 `<Index>` 元素读取,缺失返回 -1)。
  6. Field API: `sf_paramdef_field_get_display_name`, `..._internal_name`, `..._description`, `..._display_type`, `..._display_format`, `..._default_value` (返回 tagged union for v203 variable types), `..._minimum`, `..._maximum`, `..._increment`, `..._edit_flags`, `..._byte_count`, `..._bit_size`, `..._array_length`,`..._sort_id` (Paramdex `<SortID>`,XML-only 元数据,binary 时返回 0),`..._first_regulation_version`, `..._removed_regulation_version` (VersionAware mode)。
  7. ParamUtil 静态函数: `sf_param_util_get_value_size(sf_paramdef_def_type_t)`, `sf_param_util_is_bit_type(...)`, `sf_param_util_get_bit_limit(...)`。
  8. 全部公共符号用 `SF_API` 装饰。

  **Must NOT do**:
  - 不实现 .c 内容
  - 不添加 XmlSerialize 写出 API (推迟 v1.1)
  - 不暴露 internal write helpers (Validate 等)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 头文件声明,但需严格对齐 9 个 FormatVersion + 13 DefType
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 1)
  - **Wave**: 1
  - **Blocks**: T1.6, T2.1, T3.1, T3.3, T3.4
  - **Blocked By**: T0.1

  **References**:

  **Pattern References**:
  - `include/souls_formats/sf_bnd4.h` — opaque type + getter pattern
  - `include/souls_formats/sf_dcx.h` — enum 模式
  - T1.1 的 `_Static_assert` 模式

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs:1-65` — 类声明
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs:283-310` — GetRowSize
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/Field.cs:14-180` — Field 类 + DefType + EditFlags
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/ParamUtil.cs:239-355` — 3 个静态 helper

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_paramdef.h` 文件存在 + GPL header
  - [ ] 编译干净 (`-Wall -Wextra -Wpedantic -Werror`)
  - [ ] 13 个 DefType enum 值 + `_Static_assert`
  - [ ] 9 个 FormatVersion 常量 (0/101/102/103/104/106/201/202/203)
  - [ ] tagged union 表达 v203 variable-typed default value
  - [ ] `grep -c '^SF_API' include/souls_formats/sf_paramdef.h` ≥ 25

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 完整编译):
    Tool: Bash
    Steps:
      1. x86_64-w64-mingw32-gcc -fsyntax-only -Wall -Werror -std=c11 -I include \
           include/souls_formats/sf_paramdef.h 2>&1 | tee /tmp/T1.2-syntax.log
      2. grep -c 'SF_PARAMDEF_FORMAT_VERSION_' include/souls_formats/sf_paramdef.h
    Expected: Step 1 空 + 退出 0; Step 2 ≥ 9
    Evidence: .sisyphus/evidence/task-T1.2-syntax.log

  Scenario B (FAILURE — DefType count check):
    Tool: Bash
    Steps:
      1. grep -E '^\s*SF_PARAMDEF_DEF_TYPE_(S8|U8|S16|U16|S32|U32|B32|F32|ANGLE32|F64|DUMMY8|FIXSTR|FIXSTR_W)' \
           include/souls_formats/sf_paramdef.h | wc -l
    Expected: 13 (一个不多一个不少)
    Failure: < 13 → DefType 缺失;> 13 → 添加了上游没有的类型
    Evidence: .sisyphus/evidence/task-T1.2-deftype-count.log
  ```

  **Commit**: `phase4(paramdef): add sf_paramdef.h public types, DefType, FormatVersion constants`
  - Files: `include/souls_formats/sf_paramdef.h`
  - Pre-commit: 同 T1.1 syntax check

- [x] **T1.3. `sf_paramtdf.h` 公共类型 (含限定 type 集合)**

  **What to do**:
  在 `include/souls_formats/sf_paramtdf.h` 创建公共 API,镜像 `PARAMTDF.cs`:
  1. 不透明类型: `typedef struct sf_paramtdf sf_paramtdf_t; typedef struct sf_paramtdf_entry sf_paramtdf_entry_t;`
  2. Enum `sf_paramtdf_type_t` 仅 6 个: S8/U8/S16/U16/S32/U32 (PARAMTDF.cs:25-29 限定)。配 `_Static_assert(SF_PARAMTDF_TYPE_U32 + 1 - SF_PARAMTDF_TYPE_S8 == 6, ...)` 计数断言 (不用 sizeof)。
  3. PARAMTDF API: `sf_paramtdf_read_from_text` (UTF-8 string),`sf_paramtdf_write_to_text` (返回堆 string),`sf_paramtdf_destroy`,getters (name, type, entry_count, get_entry)。
  4. Entry API: `sf_paramtdf_entry_get_name` (可能 NULL,镜像上游 `null name` 行为),`sf_paramtdf_entry_get_value`。
  5. SF_API 装饰。

  **Must NOT do**:
  - 不暴露 binary read/write API (上游纯文本)
  - 不暴露 escape sequence helpers (上游不支持)
  - 不添加 schema 验证函数

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 最简单的头文件,~50 LoC
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 1)
  - **Wave**: 1
  - **Blocks**: T2.3, T3.5
  - **Blocked By**: T0.1

  **References**:

  **Pattern References**:
  - T1.1 sf_param.h 不透明类型模式

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMTDF.cs:1-110` — 完整文件

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_paramtdf.h` 存在 + GPL header
  - [ ] 编译干净
  - [ ] enum 仅 6 个值 (不多不少,严格对齐 `Type` 限制)
  - [ ] `grep -c '^SF_API' include/souls_formats/sf_paramtdf.h` ≥ 6

  **QA Scenarios**:

  ```
  Scenario A (HAPPY):
    Tool: Bash
    Steps:
      1. x86_64-w64-mingw32-gcc -fsyntax-only -Wall -Werror -std=c11 -I include \
           include/souls_formats/sf_paramtdf.h
    Expected: 退出 0,无 stdout
    Evidence: .sisyphus/evidence/task-T1.3-syntax.log

  Scenario B (FAILURE — type enum 严格):
    Tool: Bash  
    Steps:
      1. grep -cE 'SF_PARAMTDF_TYPE_(S8|U8|S16|U16|S32|U32)' include/souls_formats/sf_paramtdf.h
      2. ! grep -E 'SF_PARAMTDF_TYPE_(F32|F64|B32)' include/souls_formats/sf_paramtdf.h
    Expected: Step 1 == 6;Step 2 退出 0 (无禁用类型)
    Evidence: .sisyphus/evidence/task-T1.3-types.log
  ```

  **Commit**: `phase4(paramtdf): add sf_paramtdf.h public types`
  - Files: `include/souls_formats/sf_paramtdf.h`
  - Pre-commit: syntax check

- [x] **T1.4. `sf_fmg.h` 公共类型 + Version enum**

  **What to do**:
  在 `include/souls_formats/sf_fmg.h` 创建公共 API,镜像 `FMG.cs`:
  1. 不透明类型: `typedef struct sf_fmg sf_fmg_t; typedef struct sf_fmg_entry sf_fmg_entry_t;`
  2. Enum `sf_fmg_version_t` 3 值: `SF_FMG_VERSION_DEMONS_SOULS=0`, `SF_FMG_VERSION_DARK_SOULS_1=1`, `SF_FMG_VERSION_DARK_SOULS_3=2` (FMG.cs:334-350 — DS3=BB)。配 `_Static_assert(SF_FMG_VERSION_DARK_SOULS_3 == 2, ...)` 常量值断言。
  3. FMG API: `sf_fmg_read_from_memory/stream/path`, `sf_fmg_write_to_memory/stream/path`, `sf_fmg_create` (allocator + version),`sf_fmg_destroy`,getters (version, big_endian, unicode, has_md5, reuse_offsets, entry_count, get_entry, find_entry_by_id)。
  4. setters (用于 create + 修改): `sf_fmg_set_big_endian`, `_set_unicode`, `_set_md5`, `_set_reuse_offsets`, `_add_entry`, `_remove_entry`, `_set_entry_text`。
  5. Entry API: `sf_fmg_entry_get_id`, `_get_text` (返回 UTF-8,可能 NULL 表示 deleted)。
  6. SF_API 装饰。
  7. 注释明确说明 NULL text == deleted entry,空字符串 != deleted。

  **Must NOT do**:
  - 不添加 hash verification API (上游不验,镜像)
  - 不添加 group/range 暴露 (上游 `Groups` 是 internal write-time concept)
  - 不暴露 string offset 内部状态

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 1)
  - **Wave**: 1
  - **Blocks**: T2.4, T3.6
  - **Blocked By**: T0.1

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:1-50` — 类声明 + properties
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:334-350` — Version enum

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_fmg.h` 存在 + GPL header
  - [ ] 编译干净
  - [ ] 3 个 Version 值 (与上游枚举名等价)
  - [ ] `grep -c '^SF_API' include/souls_formats/sf_fmg.h` ≥ 18
  - [ ] 注释明确说明 deleted vs empty 语义

  **QA Scenarios**:

  ```
  Scenario A (HAPPY):
    Tool: Bash
    Steps:
      1. x86_64-w64-mingw32-gcc -fsyntax-only -Wall -Werror -std=c11 -I include \
           include/souls_formats/sf_fmg.h
      2. grep -E 'NULL.*deleted|deleted.*NULL' include/souls_formats/sf_fmg.h | wc -l
    Expected: Step 1 退出 0;Step 2 ≥ 1 (deleted 语义注释)
    Evidence: .sisyphus/evidence/task-T1.4-syntax.log

  Scenario B (FAILURE — Version enum count):
    Tool: Bash
    Steps:
      1. grep -cE 'SF_FMG_VERSION_(DEMONS_SOULS|DARK_SOULS_1|DARK_SOULS_3)' include/souls_formats/sf_fmg.h
    Expected: 3 (一个不多一个不少)
    Evidence: .sisyphus/evidence/task-T1.4-versions.log
  ```

  **Commit**: `phase4(fmg): add sf_fmg.h public types, Version enum`
  - Files: `include/souls_formats/sf_fmg.h`
  - Pre-commit: syntax check

- [x] **T1.5. `sf_emevd.h` 公共类型 + Format enum + Event/Instruction/Layer/Parameter 类型**

  **What to do**:
  在 `include/souls_formats/sf_emevd.h` 创建公共 API,镜像 `EMEVD.cs` + `Event.cs` + `Instruction.cs` + `Layer.cs` + `Parameter.cs`:
  1. 不透明类型 5 个: `sf_emevd_t`, `sf_emevd_event_t`, `sf_emevd_instruction_t`, `sf_emevd_layer_t`, `sf_emevd_parameter_t`。
  2. Enum `sf_emevd_format_t` (基于 T0.1 探针结果决定):
     - 必有: `SF_EMEVD_FORMAT_DARK_SOULS_1`, `..._DARK_SOULS_1_BE`, `..._BLOODBORNE`, `..._DARK_SOULS_3`, `..._SEKIRO` (5 上游值)
     - 扩展 (Metis 强制 reserve as alias): `..._ELDEN_RING`, `..._ARMORED_CORE_VI`, `..._NIGHTREIGN` 各别名为对应底层 flag 集 (默认全部别名 Sekiro;若 T0.1 探针结果为 Novel,本任务必须实测后扩展)。配 `_Static_assert(SF_EMEVD_FORMAT_SEKIRO >= 0, ...)` 常量值断言 (**不**用 sizeof,因 C11 enum 默认 int 4 字节)。
  3. Enum `sf_emevd_rest_behavior_t` 镜像 `Event.cs RestBehavior` 字段 (3-4 值)。配 `_Static_assert` 验证常量值稳定 (e.g., 验证 `RestBehavior.Default` 对应值)。
  4. EMEVD 主 API: `sf_emevd_read_from_memory/stream/path`, `sf_emevd_write_to_memory/stream/path`, `sf_emevd_create` (allocator + format),`sf_emevd_destroy`,getters (format, event_count, get_event, linked_file_count, get_linked_file_offset, get_string_data, get_string_data_size)。
  5. Event API: `sf_emevd_event_get_id`, `_get_rest_behavior`, `_get_instruction_count`, `_get_instruction`, `_get_parameter_count`, `_get_parameter`。
  6. Instruction API: `_get_bank`, `_get_id`, `_get_arg_data` (返回 const uint8_t * + size),`_get_layer` (可能 NULL)。
  7. Layer API: `_get_mask`。
  8. Parameter API: `_get_instruction_index`, `_get_target_start_byte`, `_get_source_start_byte`, `_get_byte_count`, `_get_unk_id`。
  9. SF_API 装饰。
  10. 头部注释引用 T0.1 探针 evidence 文件,说明 ER/AC6/Nightreign enum 处理策略。

  **Must NOT do**:
  - 不添加 EMEDF JSON loader 相关 API
  - 不添加 PackArgs/UnpackArgs API (C# 专用 helper,跳过)
  - 不暴露 `offsets.Layers` 等 read-internal 状态

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 头文件,但需小心处理 Format enum 设计 (依赖 Wave 0 探针结果)
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 1) — 但需在 T0.1 完成后开始
  - **Wave**: 1
  - **Blocks**: T2.5, T3.7
  - **Blocked By**: T0.1 (强约束:Format enum 设计取决于探针结果)

  **References**:

  **Pattern References**:
  - T1.1/T1.2 不透明类型模式

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/EMEVD.cs:1-120` — 类声明 + Game enum
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/EMEVD.cs:262-301` — Game enum complete
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Event.cs:1-50` — Event 类
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Instruction.cs:1-120` — Instruction 类
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Layer.cs:1-27` — Layer (5 字段全部 assert)
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Parameter.cs:1-70` — Parameter

  **Wave 0 Evidence Reference**:
  - `.sisyphus/evidence/phase4-pre-flight.md` (从 T0.1 输出) — ER EMEVD flags 实测结果

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_emevd.h` 存在 + GPL header
  - [ ] 编译干净
  - [ ] Format enum 含 5 上游值 + 3 扩展值 (ER/AC6/Nightreign);若 T0.1 报告 ER 为 Novel,必须新加底层 flag 表示
  - [ ] 注释引用 T0.1 evidence
  - [ ] `grep -c '^SF_API' include/souls_formats/sf_emevd.h` ≥ 28

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 探针为 Sekiro 别名):
    Tool: Bash
    Preconditions: T0.1 evidence 中 "EMEVD format match: Sekiro"
    Steps:
      1. x86_64-w64-mingw32-gcc -fsyntax-only -Wall -Werror -std=c11 -I include \
           include/souls_formats/sf_emevd.h 2>&1
      2. grep -E 'SF_EMEVD_FORMAT_(ELDEN_RING|ARMORED_CORE_VI|NIGHTREIGN)' include/souls_formats/sf_emevd.h
      3. grep -E 'phase4-pre-flight' include/souls_formats/sf_emevd.h
    Expected:
      - Step 1 退出 0
      - Step 2: 3 行 (ER/AC6/Nightreign 都有)
      - Step 3: ≥ 1 行 (引用 evidence)
    Evidence: .sisyphus/evidence/task-T1.5-syntax.log

  Scenario B (HAPPY — 探针为 Novel):
    Tool: Bash
    Preconditions: T0.1 evidence 中 "EMEVD format match: Novel: <hex>"
    Steps:
      1. x86_64-w64-mingw32-gcc -fsyntax-only ... include/souls_formats/sf_emevd.h
      2. grep -E 'SF_EMEVD_FORMAT_(ELDEN_RING)' include/souls_formats/sf_emevd.h
      3. grep -E 'unknown.*EMEVD format|new.*flag.*combination' include/souls_formats/sf_emevd.h
    Expected:
      - Step 2: ≥ 1 (新 enum 值或注释扩展)
      - Step 3: ≥ 1 (注释解释为 novel)
    Evidence: .sisyphus/evidence/task-T1.5-syntax-novel.log

  Scenario C (FAILURE — 编译错误):
    Tool: Bash
    Steps: 故意引入 typo (e.g., `SF_API typedef ...`),编译应报错
    Expected: 退出非 0
    Evidence: 不持久化 (用于内部验证)
  ```

  **Commit**: `phase4(emevd): add sf_emevd.h public types, Format enum, Event/Instruction/Layer/Parameter`
  - Files: `include/souls_formats/sf_emevd.h`, `include/souls_formats/souls_formats.h` (umbrella update)
  - Pre-commit: syntax check + T0.1 evidence 文件存在性 check

- [x] **T1.6. `paramdef_apply.c` bitstream helpers (extract_bits / insert_bits, little-endian)**

  **What to do**:
  在 `src/param/paramdef_apply.c` 创建 bitstream 操作函数,严格镜像上游 `Row.cs:236-244` 的 shift 逻辑:
  1. `static uint64_t extract_bits_unsigned(const uint8_t *buf, size_t bit_offset, size_t bit_size)`:
     - 从 byte buffer 读取 `bit_size` 位,起点 `bit_offset`,little-endian bit ordering (low bit first within a byte)。
     - 算法: 加载 8 字节作为 ulong → `bitValue` → `(bitValue << (64 - bit_size - bit_offset)) >> (64 - bit_size)` 获取无符号值。
  2. `static int64_t extract_bits_signed(...)`: 同上但返回 sign-extended `int64_t` (`Row.cs:241-242` IsSignedBitType)。
  3. `static void insert_bits(uint8_t *buf, size_t bit_offset, size_t bit_size, uint64_t value)`: 写入对应位,保留其它位不变。
  4. `static bool detect_orphaned_bits(uint64_t bit_value, size_t bit_offset)`: 上游 `Row.cs:138` 检查 — 若 `(bit_value >> bit_offset)` 仍非零,数据与 schema 不匹配 → 返回 true (调用方应返回 `SF_ERR_BAD_DATA`)。
  5. 函数全部 static,本任务不暴露公共 API。
  6. 含 `_Static_assert(sizeof(uint64_t) == 8, "bitstream helpers require 64-bit ulong")`。
  7. 单元测试在 `tests/param/test_paramdef_bitstream.c`: 5+ 测试覆盖 (bit_size=1, 8, 12, 16, 32 位; bit_offset=0/3/7; cross byte boundary; signed extension)。

  **Must NOT do**:
  - 不暴露 bitstream helpers 为公共 API
  - 不"美化"shift 数学 (Metis 强制)
  - 不添加额外的 bound check (上游不做)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 数学逻辑严格,边界条件多;需理解 little-endian bit order 与上游 ulong shift 模式
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 1)
  - **Wave**: 1
  - **Blocks**: T3.3
  - **Blocked By**: T1.1, T1.2

  **References**:

  **Pattern References**:
  - `src/core/binary_reader.c` — 字节级读取风格

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/Row.cs:124-281` — ReadCells 完整,bit 解码核心在 232-244
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/Row.cs:330-432` — WriteCells (插入对应)
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/ParamUtil.cs:239-292` — IsBitType / GetBitLimit

  **Acceptance Criteria**:
  - [ ] `src/param/paramdef_apply.c` 文件存在 (本任务仅添加 bitstream helpers,apply 逻辑由 T3.3 加)
  - [ ] `tests/param/test_paramdef_bitstream.c` 5+ 测试 PASS
  - [ ] 编译干净 (`-Werror`)
  - [ ] `_Static_assert(sizeof(uint64_t) == 8)` 存在
  - [ ] 注释引用 `Row.cs:236-244` (literal 镜像)

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 5 bit-stream 用例):
    Tool: interactive_bash (tmux)
    Preconditions: T1.1, T1.2 头文件就绪
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_paramdef_bitstream
      2. ./build-mingw/tests/param/souls_formats_test_paramdef_bitstream.exe 2>&1 | tee /tmp/T1.6.log
      3. grep -E '5 Tests 0 Failures|6 Tests 0 Failures' /tmp/T1.6.log
    Expected:
      - Step 2 退出 0
      - Step 3 至少 1 行 (Unity 全绿)
    Evidence: .sisyphus/evidence/task-T1.6-bitstream.log

  Scenario B (FAILURE — orphaned bits 检测):
    Tool: interactive_bash
    Steps:
      1. 测试用例: bit_offset=0, bit_size=4, 输入 byte 0xFF (高 4 位非零)
      2. 期望 detect_orphaned_bits 返回 true
    Expected: Unity 测试 PASS (验证 detect 函数捕获到 orphan)
    Evidence: .sisyphus/evidence/task-T1.6-orphan.log
  ```

  **Commit**: `phase4(paramdef-apply): add bitstream extract/insert helpers + tests`
  - Files: `src/param/paramdef_apply.c`, `src/param/CMakeLists.txt` (created here), `tests/param/test_paramdef_bitstream.c`, `tests/CMakeLists.txt`
  - Pre-commit: `cmake --build build-mingw --target souls_formats_test_paramdef_bitstream && ctest -R paramdef_bitstream`

- [x] **T1.7. CMakeLists 更新 + 目录脚手架 + sf_add_test 扩展**

  **What to do**:
  CMake 工程更新以纳入 Phase 4 全部源文件 + 测试:
  1. 顶层 `CMakeLists.txt` `SF_PUBLIC_HEADERS` list 添加: `sf_param.h`, `sf_paramdef.h`, `sf_paramtdf.h`, `sf_fmg.h`, `sf_emevd.h`。
  2. 顶层 `CMakeLists.txt` `SF_SOURCES` list 添加: `src/param/param.c`, `src/param/paramdef.c`, `src/param/paramdef_xml_read.c`, `src/param/paramdef_apply.c`, `src/param/paramtdf.c`, `src/text/fmg.c`, `src/script/emevd.c`, `src/script/emevd_event.c`, `src/script/emevd_instruction.c`, `src/script/emevd_layer.c`, `src/script/emevd_parameter.c`。
  3. 创建空 stub 文件 (本任务) 让 build 不破裂: 11 个 `.c` 文件每个含 `#include <souls_formats/sf_*.h>` + `/* TODO: implement in T2.x / T3.x */`。
  4. `tests/CMakeLists.txt` 添加新 labels: `param`, `script`。
  5. `tests/CMakeLists.txt` 注册 11 个新测试 binary (通过 `sf_add_test`),全部初始为空 stub (本任务最小骨架)。
  6. `examples/CMakeLists.txt` 添加 `sf_param_dump`(必须) 与 `sf_emevd_dump`(可选) targets,初始空 stub 。
  7. 验证:`cmake -B build-mingw -G Ninja --toolchain ... && cmake --build build-mingw` 全绿,新增工件齐全。

  **Must NOT do**:
  - 不实现任何业务逻辑 (stubs 只 include header)
  - 不在本任务跑除 build 之外的 ctest

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: CMake 配置 + 11 个一行 stub
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: NO (汇聚 T1.1-T1.6,确保所有 header / bitstream 已就绪后做最终接线)
  - **Wave**: 1 (依赖其它 T1.* 完成)
  - **Blocks**: 全部 Wave 2+ 任务 (build 系统打通)
  - **Blocked By**: T1.1, T1.2, T1.3, T1.4, T1.5, T1.6

  **References**:

  **Pattern References**:
  - `CMakeLists.txt` (顶层) — Phase 1-3 SF_SOURCES list 风格
  - `tests/CMakeLists.txt` — `sf_add_test()` macro 用法 (Phase 1-3 已注册 ~15 个 test binary)

  **Acceptance Criteria**:
  - [ ] `CMakeLists.txt` 含 5 个新 header,11 个新 source
  - [ ] `tests/CMakeLists.txt` 含 11 个新测试 binary,全部 LABEL ∈ {param, script}
  - [ ] `examples/CMakeLists.txt` 含 sf_param_dump target
  - [ ] `cmake -B build-mingw && cmake --build build-mingw` 全绿,零 warning
  - [ ] `ls build-mingw/libsouls_formats.{a,dll,dll.a}` 都存在
  - [ ] `ls build-mingw/tests/{param,script}/*.exe | wc -l` ≥ 11
  - [ ] `ls build-mingw/examples/sf_param_dump.exe` 存在

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — full reconfigure + build):
    Tool: interactive_bash
    Preconditions: 删除 build-mingw 重 configure
    Steps:
      1. rm -rf build-mingw
      2. cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug 2>&1 | tee /tmp/T1.7-cfg.log
      3. cmake --build build-mingw 2>&1 | tee /tmp/T1.7-build.log
      4. ls build-mingw/libsouls_formats.{a,dll,dll.a}
      5. ls build-mingw/tests/param/*.exe build-mingw/tests/script/*.exe | wc -l
    Expected:
      - Step 2 退出 0,无 "Could NOT find" 错误
      - Step 3 退出 0,无 warning
      - Step 4 三个文件都存在
      - Step 5 ≥ 11
    Evidence: .sisyphus/evidence/task-T1.7-build.log

  Scenario B (FAILURE — 缺失 header 检测):
    Tool: Bash
    Preconditions: 临时移除 sf_param.h
    Steps:
      1. mv include/souls_formats/sf_param.h /tmp/
      2. cmake --build build-mingw 2>&1 | grep -c "sf_param.h" 
      3. mv /tmp/sf_param.h include/souls_formats/  # 恢复
    Expected: Step 2 ≥ 1 (build 报告找不到 header)
    Evidence: .sisyphus/evidence/task-T1.7-missing-header.log
  ```

  **Commit**: `phase4(build): wire src/param /text /script + tests/param /script + examples into CMake`
  - Files: `CMakeLists.txt`, `tests/CMakeLists.txt`, `examples/CMakeLists.txt`, 11 stub `.c` files
  - Pre-commit: `cmake -B build-mingw && cmake --build build-mingw` (full reconfigure)

---

### Wave 2 — Binary Readers + Probe Validator (5 parallel after Wave 1)

- [x] **T2.1. PARAMDEF 二进制读取器 (9 versions, all field encoding branches)**

  **What to do**:
  在 `src/param/paramdef.c` 实现 PARAMDEF 二进制读取,严格镜像上游 `PARAMDEF.cs:85-145` + `Field.cs:239-405`:
  1. 头部读取 (`PARAMDEF.cs:85-145`):
     - StringsOffset (varint, varintLong if version ≥ 200) + ID (i16) + DataVersion (i16) + FieldCount (i16) + FieldSize (i16) + ParamType handling (4 个分支:v202+ Shift-JIS @ 64-bit offset / v106..199 Shift-JIS @ 32-bit offset / v0..101 fixstr 0x20 ASCII / v102+ fixstr 0x20 Shift-JIS)
     - BigEndian byte (offset 0x2C) — 0x2C = 1 BE, 0xFF = 1 LE
     - Unicode byte (offset 0x2D)
     - FormatVersion (i16) — 必须 ∈ {0, 101, 102, 103, 104, 106, 201, 202, 203},否则 `SF_ERR_UNSUPPORTED_VERSION`
     - Header size 验证: 0x30 for v<200, 0xFF for v≥200,不匹配 → `SF_ERR_BAD_MAGIC`
  2. Field 表读取 (`Field.cs:239-405`),per-version 分支:
     - DisplayName: 3 编码 (v202+/v106..199 → UTF-16 offset string;else Unicode flag → 0x40 fixstrW;else → 0x40 fixstr)
     - DisplayType (fixstr 0x8 ASCII)
     - DisplayFormat (fixstr 0x8 ASCII)
     - Default/Min/Max/Increment: v203 用 variable-typed (per `Field.cs:358-404`,根据 DisplayType 读取);v<203 用 f32
     - EditFlags (i32) → enum
     - ByteCount (i32)
     - Description offset (varint)
     - InternalType (fixstr 0x20 Shift-JIS) — 仅 v≥102
     - InternalName (fixstr 0x20 Shift-JIS) — 仅 v≥102;含 `name:bits` / `name[N]` 解析 (`Field.cs:300-307` regex)
     - SortID (i32) — 仅 v≥104
     - Padding: v104+ 用 0x00,v<104 用 0x20 (`Field.cs:412-414`)
     - Field record size 必须等于 version-specific 期望值: v0=0x68, v101=0x8C, v102=0xAC, v103=0x6C (上游已注释 "wrong"), v104=0xB0, v106=0x48, v201=0xD0, v202=0x68, v203=0x88。
  3. 字符串表读取: 各 offset 跳到 strings section,按 Unicode flag 用 `read_utf16` 或 `read_shift_jis` 终止串。
  4. Validate(): null ParamType / null Fields / Field.DisplayName 等 → `SF_ERR_INVALID_ARG`。
  5. 公共入口: `sf_paramdef_read_from_memory`, `_from_stream`, `_from_path`。

  **Must NOT do**:
  - 不修改 v103 field size 上游 bug (用 0x6C 即使 "wrong")
  - 不"简化" 4 个 ParamType 编码分支
  - 不添加 FormatVersion 自动 fallback (未知就报错)
  - 不实现 Validate() 之外的额外 schema 校验

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: 9 versions × N branches per field × 3 ParamType encodings → 高复杂度
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 2)
  - **Wave**: 2
  - **Blocks**: T3.1, T3.3, T3.4
  - **Blocked By**: T1.2, T1.6, T1.7

  **References**:

  **Pattern References**:
  - `src/archive/bnd4.c` — header 解析 + 多版本分支模式
  - `src/core/binary_reader.c` — `read_shift_jis`, `read_utf16`, `read_fix_str`, varint API

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs:85-145` — Read 主流程
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs:97-117` — ParamType 4 分支
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs:131-140` — FieldRecordSize 9 versions table
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/Field.cs:239-405` — Field 构造完整
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/Field.cs:300-307` — InternalName regex split
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/ParamUtil.cs:271-292` — GetValueSize per DefType

  **Acceptance Criteria**:
  - [ ] `src/param/paramdef.c` 实现 read 路径 (write 路径在 T3.1)
  - [ ] 编译干净 (`-Werror`)
  - [ ] `tests/param/test_paramdef_binary_read.c` 9 版本合成 fixture 全 PASS
  - [ ] 不实现 write API (本任务读-only)
  - [ ] DLL 新增 ~10 sf_paramdef_* 符号

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 9 versions 合成 fixture):
    Tool: interactive_bash
    Preconditions: T1.7 build 通过
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_paramdef_binary_read
      2. ./build-mingw/tests/param/souls_formats_test_paramdef_binary_read.exe 2>&1 | tee /tmp/T2.1.log
      3. grep -cE '9 Tests 0 Failures|10 Tests' /tmp/T2.1.log
    Expected:
      - Step 2 退出 0
      - Step 3 ≥ 1 (Unity 全绿,9 版本测试)
    Evidence: .sisyphus/evidence/task-T2.1-paramdef-read.log

  Scenario B (FAILURE — unknown FormatVersion):
    Tool: interactive_bash
    Steps:
      1. 测试用例: 构造 PARAMDEF 字节流 FormatVersion=999
      2. 期望 sf_paramdef_read_from_memory 返回 SF_ERR_UNSUPPORTED_VERSION
    Expected: Unity 测试 PASS (错误码正确)
    Evidence: .sisyphus/evidence/task-T2.1-bad-version.log
  ```

  **Commit**: `phase4(paramdef): implement binary reader for 9 FormatVersions`
  - Files: `src/param/paramdef.c`, `tests/param/test_paramdef_binary_read.c`
  - Pre-commit: `ctest -R paramdef_binary_read`

- [x] **T2.2. PARAM 二进制读取器 (header + rows + names,无 apply)**

  **What to do**:
  在 `src/param/param.c` 实现 PARAM 二进制读取,严格镜像 `PARAM.cs:79-201` + `Row.cs:74-116`:
  1. 头部:
     - StringsOffset (i32) + DataStart (i16/i32 取决于 Format2D) + Unk06 (i16) + ParamdefDataVersion (i16) + RowCount (i16) + ParamType (取决于 Format2D.OffsetParamType): 内联 0x20 Shift-JIS / offset 32-bit → Shift-JIS 终止串 / offset 64-bit → Shift-JIS 终止串
     - BigEndian byte (offset 0x2C)
     - Format2D (PARAM.cs:467 enum) — IntDataOffset/LongDataOffset/Flag01/OffsetParamType
     - Format2E (PARAM.cs:519 enum) — UnicodeRowNames
     - ParamdefFormatVersion (byte) — 透传,不验证
  2. Row 表 (`Row.cs:74-116`):
     - 数据 offset 宽度: 16-bit / 32-bit / 64-bit (取决于 Format2D flags 组合)
     - Row.ID (i32) + DataOffset (per-flag 宽度) + NameOffset (varint LongDataOffset → 64-bit;else 32-bit)
     - Name encoding: UTF-16 if `FormatFlags2.UnicodeRowNames`, else Shift-JIS
  3. DetectedSize 计算 (`PARAM.cs:195-199`):
     - 0 行 → -1
     - 1 行 → 推自 strings offset
     - N 行 → row[1].dataOff - row[0].dataOff
  4. 不实现 Cells 解析 (cells 由 T3.3 通过 ApplyParamdef 填充)
  5. UnnamedRows / HeaderlessRows 检测但**返回 `SF_ERR_UNSUPPORTED_VERSION`** (v1 不支持,Metis 强制)
  6. 公共入口: `sf_param_read_from_memory/stream/path`。

  **Must NOT do**:
  - 不实现 ApplyParamdef (T3.3)
  - 不实现 cell value 解析
  - 不支持 UnnamedRows / HeaderlessRows
  - 不验证 endianness 与 PARAMDEF 是否匹配

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: header 多分支 + 数据 offset 3 种宽度 + 字符串 2 编码
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 2)
  - **Wave**: 2
  - **Blocks**: T3.2, T3.3
  - **Blocked By**: T1.1, T1.7

  **References**:

  **Pattern References**:
  - `src/archive/bnd4.c` — header 多分支模式

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/PARAM.cs:79-201` — Read 主流程
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/PARAM.cs:128-176` — 数据 offset 宽度 + UnnamedRows/HeaderlessRows 检测
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/Row.cs:74-116` — Row 构造

  **Acceptance Criteria**:
  - [ ] `src/param/param.c` 实现 read 路径 (write 在 T3.2,apply 在 T3.3)
  - [ ] 编译干净
  - [ ] `tests/param/test_param_binary_read.c` 多场景 PASS:0-row PARAM, 1-row, 3-row, ER-style 64-bit offset, UnicodeRowNames
  - [ ] UnnamedRows / HeaderlessRows fixture → `SF_ERR_UNSUPPORTED_VERSION`

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 多 row 计数):
    Tool: interactive_bash
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_param_binary_read
      2. ./build-mingw/tests/param/souls_formats_test_param_binary_read.exe
    Expected: 5+ 子测试 PASS
    Evidence: .sisyphus/evidence/task-T2.2-param-read.log

  Scenario B (FAILURE — UnnamedRows 拒绝):
    Tool: interactive_bash
    Steps: 构造 UnnamedRows fixture (rowsSize 比 row 表头小);期望 `SF_ERR_UNSUPPORTED_VERSION`
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T2.2-unnamed-reject.log
  ```

  **Commit**: `phase4(param): implement binary reader (header + rows + names, no apply)`
  - Files: `src/param/param.c`, `tests/param/test_param_binary_read.c`
  - Pre-commit: `ctest -R param_binary_read`

- [x] **T2.3. PARAMTDF 文本解析器 (state machine ~200 LoC)**

  **What to do**:
  在 `src/param/paramtdf.c` 实现 PARAMTDF 文本解析,严格镜像 `PARAMTDF.cs:62-92` 的 naive 风格:
  1. `sf_paramtdf_read_from_text(const char *utf8_text, size_t size, sf_paramtdf_t **out, const sf_allocator_t *alloc)`:
     - 按 `\r\n` 分割行 (移除空行,镜像 `StringSplitOptions.RemoveEmptyEntries`)
     - Line 0: name (Trim('"'))
     - Line 1: type 名 — 必须 ∈ {"s8","u8","s16","u16","s32","u32"},否则 `SF_ERR_INVALID_ARG`
     - Line 2..N: split by ',' (限 2 part),part[0] = name (Trim('"'),空 → NULL),part[1] = value (Trim('"'),按 type 解析为 i64/u64,使用 `strtol`/`strtoul` 不 locale-dependent)
  2. 不处理 escape sequence,不处理 quoted-comma,不处理 BOM,不处理 comments — 完全镜像上游限制
  3. 数字解析失败 → `SF_ERR_BAD_DATA`
  4. 输入超出 6 个 type 之一 → `SF_ERR_INVALID_ARG`
  5. 仅实现 read,write 在 T3.5

  **Must NOT do**:
  - 不添加 escape sequence 支持
  - 不添加 BOM 容错
  - 不添加 comment 跳过
  - 不修复上游 culture-default 解析行为 (我们用 strtoul 替代但语义等价)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: state machine + 数字解析 + 错误码映射
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 2)
  - **Wave**: 2
  - **Blocks**: T3.5
  - **Blocked By**: T1.3, T1.7

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMTDF.cs:62-92` — string ctor 完整解析
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMTDF.cs:25-29` — 6 type 限制

  **Acceptance Criteria**:
  - [ ] `src/param/paramtdf.c` 实现 read 路径
  - [ ] `tests/param/test_paramtdf_read.c` 4+ 场景 PASS: 3-entry happy, empty name, type rejection (f32), malformed line
  - [ ] 编译干净

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 标准 3-entry):
    Tool: interactive_bash
    Steps:
      1. 输入 text: `"MyEnum"\n"u32"\n"None","0"\n"On","1"\n"Off","2"`
      2. 期望解析为 3 entries, type=U32
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T2.3-paramtdf-happy.log

  Scenario B (FAILURE — type=f32 拒绝):
    Tool: interactive_bash
    Steps:
      1. 输入 text: `"X"\n"f32"\n"a","1.0"`
      2. 期望返回 SF_ERR_INVALID_ARG
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T2.3-paramtdf-bad-type.log
  ```

  **Commit**: `phase4(paramtdf): implement text parser (state machine, mirrors upstream naive style)`
  - Files: `src/param/paramtdf.c`, `tests/param/test_paramtdf_read.c`
  - Pre-commit: `ctest -R paramtdf_read`

- [x] **T2.4. FMG 读取器 (v0/v1/v2 + MD5 + groups + 双 string offset width)**

  **What to do**:
  在 `src/text/fmg.c` 实现 FMG 二进制读取,严格镜像 `FMG.cs:68-139`:
  1. MD5 prefix 检测 (`FMG.cs:70-74`): peek byte 0;若 != 0 → 16 字节 MD5 prefix 存在,跳过;否则无 prefix。设置 `has_md5` 标志透传。
  2. Magic + flags:
     - byte 0: padding (0x00)
     - byte 1: BigEndian (0=LE, 1=BE)
     - byte 2: Version (0=DemonsSouls, 1=DarkSouls1, 2=DarkSouls3)
     - byte 3: aux byte (DemonsSouls 写 0xFF, others 写 0x00 — 透传记录)
     - i32 file size, i8 group count?... (按 version 分支)
  3. Wide format gating (`FMG.cs:81, 148`): `wide = (version == DarkSouls3)`。Wide → varintLong, +4 padding 在 group header,+4 sentinel 0xFF 在 StringCount 后,8-byte string offsets。Else: 4-byte 全程。
  4. Group 表读取: 每 group 含 `offsetIndex, firstID, lastID` (+ 4 字节 padding if wide)。
  5. String offset 表: 长度 = StringCount × (8 if wide else 4)。
  6. Entry 构造: 对每个 group iterate from firstID to lastID,offsetIndex 自增,从 string offset 表查偏移;偏移 == 0 → text = NULL (deleted);否则 jump 到偏移读 null-terminated UTF-16 (Unicode) 或 Shift-JIS。
  7. 不验证 MD5 (镜像上游)。
  8. 公共入口: `sf_fmg_read_from_memory/stream/path`。

  **Must NOT do**:
  - 不验证 MD5 hash
  - 不暴露 group 内部结构 (上游 groups 是 internal)
  - 不"修复" DemonsSouls 0xFF aux 字节 (透传)
  - 不假设 byte[0]=0 时一定无 MD5 (镜像限制)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 3 versions × wide/narrow 2 模式 × MD5 detection × 2 string encoding
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 2)
  - **Wave**: 2
  - **Blocks**: T3.6
  - **Blocked By**: T1.4, T1.7

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:68-139` — Read 完整
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:81-100` — 头部 + wide gating
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:118-135` — group iteration + string offset lookup

  **Acceptance Criteria**:
  - [ ] `src/text/fmg.c` 实现 read
  - [ ] `tests/param/test_fmg_read.c` 6+ 场景 PASS: v0/v1/v2 各一个,MD5 prefix 含/不含,empty entry (offset=0),null entry vs empty string
  - [ ] 编译干净

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 5-entry v1 + Unicode + 日文):
    Tool: interactive_bash
    Steps:
      1. 构造 v1 FMG 含 5 entries: id 1="Hello", id 2="エルデンリング", id 3=null (deleted), id 5="黒く塗れ", id 100="end"
      2. sf_fmg_read_from_memory → 解析 → 验证每 entry 文本正确
    Expected: 5 子测试 PASS
    Evidence: .sisyphus/evidence/task-T2.4-fmg-read.log

  Scenario B (FAILURE — 1-byte truncated):
    Tool: interactive_bash
    Steps: 输入 1 byte → 期望 SF_ERR_TRUNCATED
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T2.4-fmg-trunc.log
  ```

  **Commit**: `phase4(fmg): implement reader v0/v1/v2 + MD5 detection + groups`
  - Files: `src/text/fmg.c`, `tests/param/test_fmg_read.c`
  - Pre-commit: `ctest -R fmg_read`

- [x] **T2.5. EMEVD 读取器 (5 Game variants + Layer + Parameter + LinkedFile)**

  **What to do**:
  在 `src/script/emevd.c` + `emevd_event.c` + `emevd_instruction.c` + `emevd_layer.c` + `emevd_parameter.c` 实现 EMEVD 二进制读取,严格镜像 `EMEVD.cs:93-149` + 子结构:
  1. Magic check: `"EVD\0"` (4 ASCII)
  2. Format detection (`EMEVD.cs:95-117`): 读 BigEndian byte + Is64Bit byte + Unk06 byte + Unk07 byte + Version (i32) → 5 个 + 探针扩展的 game flag 组合;不匹配 → `SF_ERR_UNSUPPORTED_VERSION` 含 5 字节 diagnostic。
  3. 头部 offsets: events / event_data / instructions / argsBlock / linked_files / parameters / layers / strings (具体顺序见 `EMEVD.cs:119-135`,wide=Is64Bit gating)。
  4. Events 列表 (`Event.cs:48-79`): id (varint long if 64-bit) + instruction count + instruction offset + parameter count + parameter offset + rest behavior (i32) + (DS3+ → +int32 padding,pre-DS3 是 int32+int32(0))。
  5. Instructions (`Instruction.cs:122-152`): bank (i32) + id (i32) + arg data length (varint) + arg data offset (varint) + layer offset (DS3+ int64,else int32+int32(0))。layer offset == -1 → no layer。
  6. Layers (`Layer.cs:7-15`): assert i32==2, read u32 mask, assert varint 0, assert varint -1, assert varint 1。
  7. Parameters (`Parameter.cs:51-58`): instruction index (varint) + target start byte (varint) + source start byte (varint) + byte count (i32) + unk id (i32)。
  8. LinkedFileOffsets: 对所有 game 都读 (Metis 强制纠正:不仅 BB/DS3),计数 == 0 时空数组。
  9. ArgData 透传: `sf_emevd_instruction_get_arg_data` 返回 `const uint8_t * + size`,不解析 EMEDF。
  10. StringData 块透传: `sf_emevd_get_string_data` 返回原始字节数组。

  **Must NOT do**:
  - 不实现 EMEDF JSON loading
  - 不实现 PackArgs/UnpackArgs (C# 专用)
  - 不解析 ArgData 内部结构
  - 不"修复" 5 game variants 中任何 magic / version / flag 组合
  - 不透传 `offsets.Layers` 等 read-internal state

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: 5 子结构 × 5 game variants × wide/narrow gating;最复杂的格式之一
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 2)
  - **Wave**: 2
  - **Blocks**: T3.7
  - **Blocked By**: T0.1 (Format enum), T1.5, T1.7

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/EMEVD.cs:93-149` — Read 主流程
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/EMEVD.cs:106-117` — 5 game variants
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Event.cs:48-79` — Event ctor
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Instruction.cs:122-152` — Instruction ctor + layer offset
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Layer.cs:7-15` — 5 字段 assert
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Parameter.cs:51-58` — Parameter ctor

  **Wave 0 Reference**:
  - `.sisyphus/evidence/phase4-pre-flight.md` — ER flag 实测 (T1.5 已纳入 enum)

  **Acceptance Criteria**:
  - [ ] `src/script/emevd*.c` 5 文件实现 read 路径
  - [ ] `tests/script/test_emevd_read.c` 5+ 场景 PASS: 5 game variants × 1 fixture each (DS1/DS1BE/BB/DS3/Sekiro),plus ER alias
  - [ ] 编译干净
  - [ ] 不实现 write (T3.7)

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — Sekiro variant 合成 fixture):
    Tool: interactive_bash
    Steps:
      1. 构造 Sekiro flag set EMEVD: 1 event × 1 instruction × 0 args × 0 layer
      2. sf_emevd_read_from_memory → format == SEKIRO,event count == 1
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T2.5-emevd-read.log

  Scenario B (FAILURE — novel flag combination):
    Tool: interactive_bash
    Steps: 构造 fixture: bigEndian=0, is64Bit=1, unk06=0, unk07=1, ver=0xCD (任何 5 已知组合外的);期望 SF_ERR_UNSUPPORTED_VERSION
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T2.5-emevd-novel.log
  ```

  **Commit**: `phase4(emevd): implement reader for 5 game variants + Layer/Parameter/LinkedFile`
  - Files: `src/script/{emevd,emevd_event,emevd_instruction,emevd_layer,emevd_parameter}.c`, `tests/script/test_emevd_read.c`
  - Pre-commit: `ctest -R emevd_read`

---

### Wave 3 — Writers + XML + Apply (7 parallel after Wave 2)

- [x] **T3.1. PARAMDEF 二进制写出 (5 versions: v104/v106/v201/v202/v203)**

  **What to do**:
  在 `src/param/paramdef.c` 添加 write 路径,严格镜像 `PARAMDEF.cs:189-278` + `Field.cs:407-509` + `Field.cs:511-564`:
  1. Validate() (`PARAMDEF.cs:280-296`): null ParamType / null Fields / 任一 Field.DisplayName/DisplayFormat/InternalType (v≥102)/InternalName 为 null → 返回 false,write 函数返回 `SF_ERR_INVALID_ARG`
  2. **VersionAware = true 时返回 `SF_ERR_INVALID_ARG`** (Metis 强制,镜像 `PARAMDEF.cs:191-192` throw)
  3. 写出版本限定 (Metis 强制): 仅 v104/v106/v201/v202/v203 支持 write;其它返回 `SF_ERR_UNSUPPORTED_VERSION`
  4. 头部写出: 反向 T2.1 read,使用 Reserve/Fill 模式占位回填 (StringsOffset, FieldsOffset)
  5. Field 写出: 按版本分支编码 DisplayName / Default / Min / Max / Increment / InternalType / InternalName
  6. Strings table 二次扫描合并 + 写出
  7. 公共入口: `sf_paramdef_write_to_memory/stream/path`

  **Must NOT do**:
  - 不实现 v0/v101/v102/v103 写出 (read-only,Metis 强制)
  - 不实现 XML 写出 (推迟 v1.1)
  - 不允许 VersionAware 写出
  - 不"美化" v203 variable-typed 写出

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: 写出复杂度等同读取 + 需正确 Reserve/Fill;version × encoding branches
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 3)
  - **Wave**: 3
  - **Blocks**: T4.2, T4.3
  - **Blocked By**: T2.1

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs:189-278` — Write
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs:191-192` — VersionAware throw
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/Field.cs:407-509` — Field.Write
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/Field.cs:511-564` — Field.WriteStrings

  **Pattern References**:
  - `src/archive/bnd4.c` — Reserve/Fill 模式

  **Acceptance Criteria**:
  - [ ] `src/param/paramdef.c` 含 write 函数
  - [ ] `tests/param/test_paramdef_binary_write.c` 5+ 场景 PASS: 5 versions × round-trip 字节级一致
  - [ ] VersionAware 写出 → `SF_ERR_INVALID_ARG`
  - [ ] v0/v101/v102/v103 写出 → `SF_ERR_UNSUPPORTED_VERSION`

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — v104/v106/v201/v202/v203 round-trip):
    Tool: interactive_bash
    Steps:
      1. 5 个测试用例: 各 version 构造 PARAMDEF → write → read → 字节级一致
    Expected: 5 子测试 PASS
    Evidence: .sisyphus/evidence/task-T3.1-paramdef-write.log

  Scenario B (FAILURE — VersionAware 拒绝):
    Tool: interactive_bash
    Steps: 构造 PARAMDEF VersionAware=true → sf_paramdef_write_to_memory → 期望 SF_ERR_INVALID_ARG
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.1-versionaware-reject.log
  ```

  **Commit**: `phase4(paramdef): implement binary writer for v104/v106/v201/v202/v203`
  - Files: `src/param/paramdef.c` (extend), `tests/param/test_paramdef_binary_write.c`
  - Pre-commit: `ctest -R paramdef_binary_write`

- [x] **T3.2. PARAM 二进制写出 (header + rows + names + strings)**

  **What to do**:
  在 `src/param/param.c` 添加 write 路径,严格镜像 `PARAM.cs:206-302` + `Row.cs:283-455`:
  1. 头部写出: ParamType (按 Format2D.OffsetParamType 决定 inline / offset),BigEndian / Format2D / Format2E / ParamdefFormatVersion / RowCount。Reserve StringsOffset + DataStart。
  2. Row 表 (`Row.cs:283-303` WriteHeader): id (i32) + Reserve dataOffset (per-flag width) + Reserve nameOffset (varint width)。
  3. Cell 数据 (`Row.cs:305-432` WriteCells): 调 T3.3 的 cell→bytes 助手 (本任务依赖 T3.3 的 cell write helper);本任务必须先实现 cell write 但保留 cell read 给 T3.3。
  4. Name 写出 (`Row.cs:434-455` WriteName): UTF-16 if UnicodeRowNames, else Shift-JIS,记录 offset 到 Fill nameOffset。
  5. Strings table 收尾 + Fill StringsOffset。
  6. 不实现 Cell 解析 (T3.3) — 本任务**只**写 cell 字节,不读 (有人认为这相互依赖,但 PARAM write 路径假设 cells 已经构造好;cell 构造由 ApplyParamdef 在 read 路径完成,本任务的 write 测试路径需先 ApplyParamdef)
  7. 公共入口: `sf_param_write_to_memory/stream/path`。

  **Must NOT do**:
  - 不实现 ApplyParamdef
  - 不暴露 StringOffsetDictionary 内部状态
  - 不支持 UnnamedRows / HeaderlessRows

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Reserve/Fill 多层嵌套 + 字符串表合并
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 3) — 但与 T3.3 在 cell write helper 层有协调
  - **Wave**: 3
  - **Blocks**: T4.2, T4.4
  - **Blocked By**: T2.2

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/PARAM.cs:206-302` — Write
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/Row.cs:283-455` — Row 写出三阶段

  **Acceptance Criteria**:
  - [ ] `src/param/param.c` 含 write
  - [ ] `tests/param/test_param_binary_write.c` 4+ 场景 PASS: 0-row, 1-row, 3-row, ER-style 64-bit offset,全部 round-trip 字节级一致
  - [ ] 与 T3.3 协调点 (cell→bytes helper) 共享接口干净

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 3-row × 5-field round-trip):
    Tool: interactive_bash
    Steps: 构造 3 行 × 5 字段 PARAM (跑 ApplyParamdef 填好 cells) → write → read → memcmp 字节级一致
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.2-param-write.log

  Scenario B (FAILURE — 未填 Reserve 检测):
    Tool: interactive_bash
    Steps: 故意 mock cell 数据为 NULL → write 应返回 SF_ERR_INTERNAL (或类似)
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.2-unfilled-reserve.log
  ```

  **Commit**: `phase4(param): implement binary writer (header + rows + names + strings)`
  - Files: `src/param/param.c` (extend), `tests/param/test_param_binary_write.c`
  - Pre-commit: `ctest -R param_binary_write`

- [x] **T3.3. PARAM apply paramdef (3-mode + cell tagged union 填充 + bit-packing 解码) [关键路径]**

  **What to do**:
  在 `src/param/paramdef_apply.c` 实现 PARAM ↔ PARAMDEF 应用逻辑,严格镜像 `PARAM.cs:309-356` + `Row.cs:118-281`:
  1. `sf_param_apply_paramdef(sf_param_t *p, const sf_paramdef_t *def, sf_param_apply_mode_t mode)`:
     - mode == UNCONDITIONAL: 直接应用,无验证 (`PARAM.cs:309-314`)
     - mode == SOMEWHAT_CAREFUL (`PARAM.cs:347-356`): 接受 (a) ParamType empty 或匹配 AND (b) HeaderlessRows OR DataVersion 匹配
     - mode == CAREFUL (`PARAM.cs:316-329`): 严格三检查 — ParamType match AND DataVersion match AND (DetectedSize == -1 OR DetectedSize == def.GetRowSize())
     - 任一检查失败 → 返回 false,不修改 PARAM 状态
  2. 应用成功后调 `populate_cells` (新 helper):
     - 对每行调用 `Row.ReadCells` 等价逻辑 (`Row.cs:118-281`):
       - 普通字段: 按 DefType 直接读 (`Row.cs:175-220`)
       - 数组字段 (ArrayLength > 1): 读 N 字节 (u8/dummy8 only)
       - bit-packed 字段: 累积 bit_offset / bit_value;遇到不同 bitLimit 或非 bit-type 字段时 finalize block;调 T1.6 的 `extract_bits_signed/unsigned`;orphan bits 触发 → `SF_ERR_BAD_DATA`
       - bit_size == 0 → 当作普通字段 (与 `Row.cs:230` NotImplementedException 不同,我们返回 `SF_ERR_BAD_DATA` 含 diagnostic;注:上游 throws,我们镜像但用错误码替代 exception)
  3. `sf_param_apply_paramdef_multi(p, defs[], count, mode)`: 遍历 defs 找第一个适用的;镜像 `PARAM.cs:334`。
  4. cell 字段写回 helper: T3.2 的 write 路径需要从 tagged union 还原字节;本任务实现 `cell_to_bytes(cell, def_type, buf)`。
  5. 13 个 typed getter 实现 (sf_param_cell_get_u8/s8/u16/.../string)。

  **Must NOT do**:
  - 不验证 endianness (上游 ApplyParamdefCarefully 不查,Metis 强制镜像)
  - 不验证 ParamdefFormatVersion (PARAM 字段) 与 PARAMDEF 是否匹配 (上游不查)
  - 不"美化" bit-packing 数学 (Row.cs:236-244 字面镜像)
  - 不实现 RegulationVersioned* 变体
  - 不缓存 PARAMDEF 引用到 PARAM 内 (每次 apply 重做)

  **Recommended Agent Profile**:
  - **Category**: `ultrabrain`
    - Reason: 项目最复杂逻辑;3 mode + 13 DefType + bit-packing chain detection + dual u8/dummy8 modes + sign extension + orphan bit detection。错误极易传染。
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 3) — 与 T3.1/T3.2 并行,但 critical path 上必经
  - **Wave**: 3
  - **Blocks**: T4.4
  - **Blocked By**: T1.6, T2.1, T2.2

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/PARAM.cs:309-356` — 3 个 Apply 模式
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/Row.cs:118-281` — ReadCells 含 bit-packing
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/Row.cs:236-244` — bit shift (literal mirror)
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/Row.cs:305-432` — WriteCells (cell → bytes)
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/Cell.cs:1-100` — Cell value type 推导

  **Acceptance Criteria**:
  - [ ] `src/param/paramdef_apply.c` 含 3 mode + populate_cells + cell_to_bytes
  - [ ] `tests/param/test_param_apply_paramdef.c` 8+ 场景 PASS:
    - 3 mode happy paths (matching def)
    - SOMEWHAT_CAREFUL accepts empty ParamType
    - CAREFUL rejects mismatched DataVersion
    - bit-packed field round-trip (1-bit, 4-bit, 12-bit-cross-byte)
    - Signed bit field sign extension (s8:1)
    - u8 array (ArrayLength=4)
    - dummy8 bit mode + array mode
    - Orphan bits detection
  - [ ] 13 typed getters 全部实现并测试 (一类型至少一测试)

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 3 mode):
    Tool: interactive_bash
    Steps:
      1. 构造 PARAM (ParamType="X", DataVer=1, RowSize=10) + 3 PARAMDEF (一致, 缺 ParamType, 缺 DataVer)
      2. apply UNCONDITIONAL → 全部成功
      3. apply SOMEWHAT_CAREFUL → 1 + 缺 ParamType 成功
      4. apply CAREFUL → 仅 1 成功
    Expected: 3 子测试 PASS
    Evidence: .sisyphus/evidence/task-T3.3-apply-modes.log

  Scenario B (HAPPY — bit-packed cross-byte):
    Tool: interactive_bash
    Steps: 构造 PARAMDEF 含 12-bit u16 字段 + 4-bit u16 字段;写入已知 bit pattern;apply 后 typed getter 返回正确值
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.3-bitpack.log

  Scenario C (FAILURE — orphan bits):
    Tool: interactive_bash
    Steps: 构造 PARAM 数据高 4 位非零,PARAMDEF 声明 4 位 → apply 应返回 SF_ERR_BAD_DATA
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.3-orphan.log

  Scenario D (FAILURE — endian mismatch silent acceptance,镜像上游 bug):
    Tool: interactive_bash
    Steps: 构造 PARAM(BE=true) + PARAMDEF(BE=false),apply CAREFUL 应**返回 true** (上游不检查)。验证 cells 是 garbage 但 apply 不报错。
    Expected: 测试 PASS (验证 mirror)
    Evidence: .sisyphus/evidence/task-T3.3-endian-silent.log
  ```

  **Commit**: `phase4(paramdef-apply): implement 3-mode apply + populate cells + bit-packing decoder`
  - Files: `src/param/paramdef_apply.c` (extend), `tests/param/test_param_apply_paramdef.c`
  - Pre-commit: `ctest -R param_apply_paramdef`

- [x] **T3.4. PARAMDEF XML 反序列化 (mxml DOM walk)**

  **What to do**:
  在 `src/param/paramdef_xml_read.c` 实现 Paramdex 风格 XML 反序列化,严格镜像 `XmlSerializer.cs:18-175`:
  1. `sf_paramdef_read_xml_from_path(const wchar_t *path, sf_paramdef_t **out, ...)`:
     - Win32 `CreateFileW` 打开,读全文 → mxml `mxmlLoadString`
  2. `sf_paramdef_read_xml_from_memory(const char *xml, size_t size, sf_paramdef_t **out, ...)`:
     - mxml DOM root walk
  3. XML 结构 (与上游 `XmlSerializer.cs:18-117` + 真实 `/home/soar/dev/paramdex/ER/Defs/SpEffect.xml` 字面对齐):
     - 根 `<PARAMDEF XmlVersion="2">` (XmlVersion 属性在 v1+ 出现,上游 line 22 注释 "no longer check")
     - 必填子元素: `<ParamType>` (string), `<DataVersion>` (i16; v0 用 `<Unk06>` 别名,line 26), `<BigEndian>` (bool), `<Unicode>` (bool), `<FormatVersion>` (i16; v0 用 `<Version>` 别名,line 29)
     - **`<Index>` (int)**: Paramdex 专有元数据,**上游 `XmlSerializer.cs` 不读取**。本任务暴露为 `sf_paramdef_get_index` extension (Paramdex 工具兼容,在 T1.2 已声明,`extensions.md` T4.8 文档化),缺失时返回 `-1`。
     - `<Fields>` 含多个 `<Field Def="...">` 子元素
     - 每个 `<Field>` 的核心是 **`Def` 属性**(`XmlSerializer.cs:83-117`),用 3 个 regex 解析:
       - `defOuterRx = ^(?<type>\S+)\s+(?<name>.+?)(?:\s*=\s*(?<default>\S+))?$` — 提取 DisplayType / InternalName / 可选 DefaultValue
       - `defBitRx = ^(?<name>.+?)\s*:\s*(?<size>\d+)$` — 提取 bit field name + size
       - `defArrayRx = ^(?<name>.+?)\s*\[\s*(?<length>\d+)\]$` — 提取 array name + length
       - 例: `Def="s32 iconId = -1"` → DisplayType=S32, InternalName="iconId", DefaultValue=-1
       - 例: `Def="dummy8 padding[3]"` → DisplayType=DUMMY8, InternalName="padding", ArrayLength=3
       - 例: `Def="u8 flag:1"` → DisplayType=U8, InternalName="flag", BitSize=1
     - 每个 `<Field>` 的可选**子元素** (sub-elements,`XmlSerializer.cs:DeserializeField` 后续行):
       - `<DisplayName>` (string,UI 显示名,常为日文)
       - `<Description>` (string)
       - `<Minimum>` (按 DisplayType 类型解析)
       - `<Maximum>` (按 DisplayType 类型解析)
       - `<Increment>` (按 DisplayType 类型解析)
       - `<SortID>` (i32,Paramdex 排序键)
       - `<EditFlag>` (字符串,逗号分隔 enum,可选)
       - `<DisplayFormat>` (字符串如 "%d"/"%5.2f",可选)
       - `<ParamRef1>` ... `<ParamRef5>` (字符串,Paramdex 交叉引用,可选;v1 不实现解析,只透传)
     - 每个 `<Field>` 的可选**属性** (VersionAware,line 92-97):
       - `FirstVersion="<ulong>"` — VersionAware mode 才解析
       - `RemovedVersion="<ulong>"` — VersionAware mode 才解析
  4. 缺失必须字段 (ParamType / Fields) → `SF_ERR_BAD_DATA` 含 diagnostic
  5. malformed XML → `SF_ERR_INTERNAL` (mxml 解析失败)
  6. Def 属性 regex 失配 → `SF_ERR_BAD_DATA` 含 Def 原文
  7. `versionAware=false` 时,带 `RemovedVersion` 的 fields 自动跳过 (镜像 `XmlSerializer.cs:101-102`)
  8. 不实现 XML 写出 (推迟 v1.1)
  9. mxml API: `mxmlLoadString`, `mxmlFindElement`, `mxmlGetText`, `mxmlElementGetAttr` 主要 4 个
  10. Def 解析在 C 中用 `<regex.h>` POSIX regex 或手写状态机 (推荐手写以避免 MinGW 上 POSIX regex 兼容问题)

  **Must NOT do**:
  - 不实现 XmlSerialize (写出)
  - 不添加 `<PARAMDEF>` 自动版本检测/转换
  - 不验证 XML 与 PARAMDEF binary 的 round-trip 等价性 (上游也不验)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: mxml 调用 + 多种 XML field 编码 + VersionAware 选填
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 3)
  - **Wave**: 3
  - **Blocks**: T4.3, T4.4
  - **Blocked By**: T1.2, T2.1

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/XmlSerializer.cs:18-42` — Deserialize 入口
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/XmlSerializer.cs:87-175` — DeserializeField

  **External References**:
  - mxml 4.x API: https://www.msweet.org/mxml/
  - 真实 sample: `/home/soar/dev/paramdex/ER/Defs/SpEffect.xml` (FormatVersion=203, ParamType="SP_EFFECT_PARAM_ST", Index=86, DataVersion=4, Unicode=True)

  **Acceptance Criteria**:
  - [ ] `src/param/paramdef_xml_read.c` 实现 read
  - [ ] `tests/param/test_paramdef_xml_read.c` 4+ 场景 PASS: 合成 3-field XML, 缺失 ParamType, 缺失 Field, malformed XML
  - [ ] 编译干净
  - [ ] 不实现 write API

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 合成 3-field):
    Tool: interactive_bash
    Steps: 嵌入 3-field XML 字符串 → sf_paramdef_read_xml_from_memory → 验证 ParamType/Fields.count
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.4-xml-read.log

  Scenario B (FAILURE — malformed XML):
    Tool: interactive_bash
    Steps: 输入 "<PARAMDEF><ParamType>X" (未闭合) → 期望 SF_ERR_INTERNAL
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.4-malformed.log
  ```

  **Commit**: `phase4(paramdef): implement XML deserialization via mxml`
  - Files: `src/param/paramdef_xml_read.c`, `tests/param/test_paramdef_xml_read.c`
  - Pre-commit: `ctest -R paramdef_xml_read`

- [x] **T3.5. PARAMTDF 文本写出 (entries → naive text format)**

  **What to do**:
  在 `src/param/paramtdf.c` 添加 write 路径,严格镜像 `PARAMTDF.cs:97-109`:
  1. `sf_paramtdf_write_to_text(const sf_paramtdf_t *tdf, char **out_text, size_t *out_size, ...)`:
     - Line 0: `"<name>"` (引号包裹)
     - Line 1: `"<type-name>"` (引号包裹,小写 s8/u8/...)
     - Line 2..N: `"<entry-name>","<value>"` (entry-name == NULL 时写 `,"<value>"`)
     - 行分隔: `\r\n`
     - 末尾 `\r\n`
  2. UTF-8 输出
  3. 公共入口: `sf_paramtdf_write_to_text` (返回 caller-owned 堆字符串)

  **Must NOT do**:
  - 不添加 escape sequence 处理
  - 不去重 entries
  - 不格式化 (多余空格)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 简单 string format
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 3)
  - **Wave**: 3
  - **Blocks**: T4.2
  - **Blocked By**: T2.3

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMTDF.cs:97-109` — Write

  **Acceptance Criteria**:
  - [ ] `src/param/paramtdf.c` 含 write
  - [ ] `tests/param/test_paramtdf_write.c` 3+ 场景 PASS: 3-entry, empty name entry, all 6 types
  - [ ] round-trip with T2.3: parse → write → parse → 字段一致

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 3-entry round-trip):
    Tool: interactive_bash
    Steps: 输入文本 → parse → write → parse → 比对 entries
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.5-paramtdf-write.log

  Scenario B (FAILURE — NULL paramtdf):
    Tool: interactive_bash
    Steps: sf_paramtdf_write_to_text(NULL, ...) → 期望 SF_ERR_INVALID_ARG
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.5-null.log
  ```

  **Commit**: `phase4(paramtdf): implement text writer (mirrors upstream naive format)`
  - Files: `src/param/paramtdf.c` (extend), `tests/param/test_paramtdf_write.c`
  - Pre-commit: `ctest -R paramtdf_write`

- [x] **T3.6. FMG 写出 (group merging + MD5 计算 + ReuseOffsets)**

  **What to do**:
  在 `src/text/fmg.c` 添加 write,严格镜像 `FMG.cs:144-218` + `220-276`:
  1. `sf_fmg_write_to_memory/stream/path`:
     - sort entries by ID
     - merge consecutive IDs into groups (if id[i+1] == id[i]+1 → 同组)
     - 头部 + group 表 + string offset 表 + strings 块
     - 若 `has_md5` → 二次 pass: 写完整 payload → 算 MD5 → prepend 16 字节 hash
     - 若 `reuse_offsets` → string 写出时去重相同字符串 (hash table)
  2. wide format gating 同 read
  3. group count = Reserve/Fill (write 时累加)

  **Must NOT do**:
  - 不验证写出后的 MD5 hash 正确性 (但要计算和写入)
  - 不"优化" group 合并算法
  - 不暴露 group 内部结构

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 二次 pass MD5 + group merging + ReuseOffsets dedup
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 3)
  - **Wave**: 3
  - **Blocks**: T4.2, T4.5
  - **Blocked By**: T2.4

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:144-218` — Write 主流程 + MD5
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:172-189` — group merging
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:220-276` — WriteStrings + WriteStringsReuseOffsets

  **Acceptance Criteria**:
  - [ ] `src/text/fmg.c` 含 write
  - [ ] `tests/param/test_fmg_write.c` 5+ 场景 PASS: v0/v1/v2 round-trip, MD5 prefix round-trip, ReuseOffsets dedup
  - [ ] 字节级一致 (相对 fixture)

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — v1 + Unicode + 5 entries):
    Tool: interactive_bash
    Steps: build → write → read → 比对
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.6-fmg-write.log

  Scenario B (HAPPY — MD5 prefix written):
    Tool: interactive_bash
    Steps: write with has_md5=true → first 16 bytes 不全为 0
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.6-fmg-md5.log
  ```

  **Commit**: `phase4(fmg): implement writer with group merging + MD5 + ReuseOffsets`
  - Files: `src/text/fmg.c` (extend), `tests/param/test_fmg_write.c`
  - Pre-commit: `ctest -R fmg_write`

- [x] **T3.7. EMEVD 写出 (5 game variants + Layer/Parameter/Instruction/LinkedFile)**

  **What to do**:
  在 `src/script/emevd*.c` 添加 write,严格镜像 `EMEVD.cs:154-259` + 子结构 write 方法:
  1. 头部反向写: magic "EVD\0" + flags (按 Format) + version + Reserve sizes + Reserve offsets。
  2. EventDataOffsets / Events / Instructions / ArgsBlock / LinkedFiles / Parameters / Layers / Strings 顺序写。
  3. Args section padding: 4-byte 对齐每个 instruction 内的 args (`Instruction.cs:192`),整段 0x10 对齐 (`EMEVD.cs:238-241`)。
  4. Format-specific: pre-DS3 layer offset = int32 + int32(0); DS3+ = int64。pre-BB params = int32; BB/DS2 = int32+int32(0); DS3+ = int64。
  5. Reserve/Fill 全部 sizes / offsets。

  **Must NOT do**:
  - 不实现 PackArgs/UnpackArgs (C# 专用)
  - 不"美化" args padding 算法
  - 不暴露 internal offsets

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: 5 game variants × 多 Reserve/Fill × padding 规则;最复杂的写出
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 3)
  - **Wave**: 3
  - **Blocks**: T4.2, T4.6
  - **Blocked By**: T2.5

  **References**:

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/EMEVD.cs:154-259` — Write 主流程
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Event.cs:81-123` — Event.Write
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Instruction.cs:154-202` — Instruction.Write + args padding
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Layer.cs:17-24` — Layer.Write
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/Parameter.cs:60-67` — Parameter.Write

  **Acceptance Criteria**:
  - [ ] `src/script/emevd*.c` 含 write
  - [ ] `tests/script/test_emevd_write.c` 5+ 场景 PASS: 5 game variants × round-trip 字节级一致
  - [ ] Layer + Parameter + LinkedFile 子结构 round-trip

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — Sekiro round-trip):
    Tool: interactive_bash
    Steps: build Sekiro EMEVD → write → read → 比对
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.7-emevd-write.log

  Scenario B (FAILURE — 不支持 game format):
    Tool: interactive_bash
    Steps: 设 Format 为越界值 → write 应返回 SF_ERR_UNSUPPORTED_VERSION
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T3.7-bad-format.log
  ```

  **Commit**: `phase4(emevd): implement writer for 5 game variants + sub-structures`
  - Files: `src/script/emevd*.c` (extend), `tests/script/test_emevd_write.c`
  - Pre-commit: `ctest -R emevd_write`

---

### Wave 4 — e2e Tests + Examples (parallel after Wave 3)

- [x] **T4.1. `er_test_helper` 扩展: `er_load_param(name, **out, *out_size)`**

  **What to do**:
  在 `tests/e2e/er_test_helper.{h,c}` 添加一站式辅助:
  1. `sf_result_t er_load_param(const char *param_name, void **out_bytes, size_t *out_size)`:
     - 内部调 Phase 2 `regulation_decrypt(/mnt/c/Games/ELDEN RING/Game/regulation.bin)` → BND4 字节
     - Phase 3 `bnd4_read_from_memory` → BND4 对象
     - 遍历 entries 找 name 后缀匹配 `<param_name>.param` (e.g., "SpEffectParam" 匹配 `param/GameParam/SpEffectParam.param`)
     - 复制 entry data 到 caller-owned 堆;返回 size
     - 失败 → 返回相应错误码 (regulation 缺失 → 自定义 SF_ERR_*; entry 找不到 → SF_ERR_BAD_DATA)
  2. 新增 `er_load_msgbnd_entry(const char *msgbnd_path, const char *entry_name, void **out, *out_size)`: 类似但通过 `er_extract_from_data0` 从 Data0 提取。

  **Must NOT do**:
  - 不向公共 API (`include/souls_formats/`) 添加任何东西 (helper 仅 test-only)
  - 不缓存 regulation BND4 (每次重读;e2e 测试不频繁调用)
  - 不实现 PARAM 解析 (caller 拿 raw bytes 自己解)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 复用 Phase 2/3 helpers,轻量 wrapper
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 4)
  - **Wave**: 4
  - **Blocks**: T4.4, T4.5, T4.6
  - **Blocked By**: Phase 3 helper (已就绪)

  **References**:

  **Pattern References**:
  - `tests/e2e/er_test_helper.h` (Phase 3) — `er_extract_from_data0` API 模式

  **Acceptance Criteria**:
  - [ ] `tests/e2e/er_test_helper.h` 含新声明
  - [ ] `tests/e2e/er_test_helper.c` 含实现
  - [ ] 编译干净
  - [ ] 单元 sanity test (在 T4.4 中调用,确认能取到 SpEffectParam)

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — load SpEffectParam):
    Tool: interactive_bash
    Steps:
      1. 在 mock test 中调 er_load_param("SpEffectParam", &bytes, &size)
      2. 期望 size > 100 KB (ER 实测大小),前 4 字节为 PARAM magic 或 raw header
    Expected: 测试 PASS
    Evidence: .sisyphus/evidence/task-T4.1-er-load-param.log

  Scenario B (FAILURE — regulation.bin 缺失 SKIP):
    Tool: interactive_bash
    Steps: 临时 mv regulation.bin → er_load_param 应返回特定错误码 → 测试用 TEST_IGNORE_MESSAGE SKIP 而非 FAIL
    Expected: 测试 SKIP (Unity ignore)
    Evidence: .sisyphus/evidence/task-T4.1-skip.log
  ```

  **Commit**: `phase4(test-helper): add er_load_param + er_load_msgbnd_entry`
  - Files: `tests/e2e/er_test_helper.h` (extend), `tests/e2e/er_test_helper.c` (extend)
  - Pre-commit: `cmake --build build-mingw` 全绿

- [x] **T4.2. PARAM/PARAMDEF/PARAMTDF/FMG/EMEVD 合成 fixture round-trip 整合测试**

  **What to do**:
  在 `tests/param/test_synthetic_roundtrip.c` 与 `tests/script/test_emevd_synthetic.c` 添加合成 fixture 整合测试:
  1. `test_param_synthetic`: 3 行 (id 100/200/300) × 5 字段 (u8/u16/u32/f32/fixstr16) → write → read → memcmp
  2. `test_paramdef_binary_synthetic`: 9 versions × round-trip (与 T2.1/T3.1 相比,本任务跑完整 5 versions write 路径)
  3. `test_paramtdf_synthetic`: 3-entry + empty-name + 6 type → round-trip
  4. `test_fmg_synthetic`: v0/v1/v2 + Unicode (日文 `エルデンリング` + 中文 `黑暗之魂`) + MD5 + ReuseOffsets,5 fixtures × round-trip
  5. `test_emevd_synthetic`: 1 event × 1 instruction (Sekiro flag set) → round-trip;另 1 fixture 含 Layer + Parameter
  本任务把上面 T2.x/T3.x 已写过的合成测试**整合**为一个连贯的 test binary,验证全栈 read+write 路径联通。

  **Must NOT do**:
  - 不重写 T2.x/T3.x 已通过的测试 (本任务是整合,不是新功能)
  - 不引入真实游戏数据

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 需理解 5 格式所有 round-trip 场景;每个测试不复杂但量大
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 4)
  - **Wave**: 4
  - **Blocks**: T4.8
  - **Blocked By**: T3.1, T3.2, T3.5, T3.6, T3.7

  **References**:

  **Pattern References**:
  - `tests/archive/test_bnd4_synthetic.c` — Phase 3 round-trip 模式 (memcmp 字节级)

  **Acceptance Criteria**:
  - [ ] `tests/param/test_synthetic_roundtrip.c` 4 类合成 fixture (param/paramdef/paramtdf/fmg) 全 PASS
  - [ ] `tests/script/test_emevd_synthetic.c` 2 fixtures (basic + layer) 全 PASS
  - [ ] 全部 `memcmp(input, output, size) == 0` 字节级一致
  - [ ] `ctest -L 'param|script' -R synthetic` 退出 0

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 5 格式合成全绿):
    Tool: interactive_bash
    Steps:
      1. cmake --build build-mingw
      2. ctest --test-dir build-mingw -L 'param|script' -R synthetic --output-on-failure
    Expected: 退出 0,全部 PASS
    Evidence: .sisyphus/evidence/task-T4.2-synthetic.log

  Scenario B (FAILURE — 字节级 diff 检测):
    Tool: interactive_bash
    Steps: 故意把 PARAM 写出多 1 字节 → memcmp 应失败 → 测试报告 diff offset
    Expected: 测试 FAIL (验证 diff 检测能触发)
    Evidence: .sisyphus/evidence/task-T4.2-diff.log
  ```

  **Commit**: `phase4(tests): integrated synthetic round-trip for all 5 formats`
  - Files: `tests/param/test_synthetic_roundtrip.c`, `tests/script/test_emevd_synthetic.c`
  - Pre-commit: `ctest -L 'param|script' -R synthetic`

- [x] **T4.3. PARAMDEF XML 真实 Paramdex e2e (`SpEffect.xml`)**

  **What to do**:
  在 `tests/param/test_paramdef_xml_e2e.c` 添加 ER Paramdex e2e:
  1. 读 `/home/soar/dev/paramdex/ER/Defs/SpEffect.xml` (UTF-8) → `sf_paramdef_read_xml_from_path`
  2. Assert: `ParamType == "SP_EFFECT_PARAM_ST"`, `DataVersion == 4`, `Unicode == true`, `BigEndian == false`, `FormatVersion == 203`, `field_count >= 100`, `sf_paramdef_get_index(def) == 86` (Paramdex extension,上游不读取 `<Index>`,我们在 T1.2 已添加 `sf_paramdef_get_index` extension API)
  3. 抽样 3 个已知 fields (按 InternalName 查找): `iconId` (u32), `effectEndurance` (f32), `motionInterval` (f32) — 验证 DisplayType / DefaultValue / Min / Max
  4. SKIP 条件: 文件不存在 → `TEST_IGNORE_MESSAGE("paramdex SpEffect.xml not found")`

  **Must NOT do**:
  - 不依赖 Paramdex 的 commit hash (用文件直接断言)
  - 不假设 field 顺序 (按 InternalName 查找)
  - 不修改 Paramdex 文件

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 单 e2e 测试,逻辑直接
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 4)
  - **Wave**: 4
  - **Blocks**: T4.8
  - **Blocked By**: T3.4

  **References**:

  **Pattern References**:
  - `tests/e2e/test_bhd5_e2e_er.c` — e2e SKIP 模式 (Phase 3 已就绪)

  **Test Data**:
  - `/home/soar/dev/paramdex/ER/Defs/SpEffect.xml` (Wave 0 已验证 ParamType/Index/DataVersion 等)

  **Acceptance Criteria**:
  - [ ] `tests/param/test_paramdef_xml_e2e.c` 测试 PASS (或 SKIP 当文件缺失)
  - [ ] 5+ 断言验证已知字段
  - [ ] 编译干净

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — SpEffect.xml 解析):
    Tool: interactive_bash
    Steps:
      1. ctest -R paramdef_xml_e2e --output-on-failure
    Expected: PASS (输出含 "ParamType=SP_EFFECT_PARAM_ST")
    Evidence: .sisyphus/evidence/task-T4.3-paramdef-e2e.log

  Scenario B (SKIP — Paramdex 缺失):
    Tool: interactive_bash
    Steps: 临时 mv /home/soar/dev/paramdex/ /tmp/;ctest -R paramdef_xml_e2e
    Expected: 退出 0 + Unity IGNORE 输出
    Evidence: .sisyphus/evidence/task-T4.3-skip.log
  ```

  **Commit**: `phase4(tests): paramdef XML e2e against Paramdex SpEffect.xml`
  - Files: `tests/param/test_paramdef_xml_e2e.c`
  - Pre-commit: `ctest -R paramdef_xml_e2e`

- [x] **T4.4. PARAM apply paramdef e2e (regulation.bin → BND4 → SpEffectParam → apply SpEffect.xml) [关键路径]**

  **What to do**:
  在 `tests/param/test_param_apply_paramdef_e2e.c` 实现 Phase 4 最关键 e2e:
  1. `er_load_param("SpEffectParam", &bytes, &size)` (T4.1)
  2. `sf_param_read_from_memory(bytes, size, &param, ...)` (T2.2)
  3. Assert: row_count >= 100, ParamType == "SP_EFFECT_PARAM_ST"
  4. `sf_paramdef_read_xml_from_path("/home/soar/dev/paramdex/ER/Defs/SpEffect.xml", &def, ...)` (T3.4)
  5. `sf_param_apply_paramdef(param, def, SF_PARAM_APPLY_CAREFUL)` (T3.3)
  6. Assert: 返回 SF_OK
  7. Assert: row[0] cell `iconId` 是 u32 类型,值 ∈ [0, 1e6] (合理范围)
  8. Assert: 抽样 5 个已知 SpEffect ID (e.g., id 1, 2, 100, 1000, 10000) 的 cells 可读
  9. 用 13 typed getters 至少调用 5 个不同类型 (u8/u16/u32/f32/fixstr_w)
  10. SKIP 条件: regulation.bin 或 Paramdex 缺失

  **Must NOT do**:
  - 不假设 Paramdex 的具体 schema (ParamType/Index 已验证,但 fields 顺序不假设)
  - 不假设 row[0].id == 1 (ER 内部 ID 排列未必从 1 开始)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 跨 Phase 2/3/4 联通,断言精确到 cell 级
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 4)
  - **Wave**: 4
  - **Blocks**: T4.7, T4.8
  - **Blocked By**: T3.2, T3.3, T3.4, T4.1

  **References**:

  **Test Data**:
  - `/mnt/c/Games/ELDEN RING/Game/regulation.bin`
  - `/home/soar/dev/paramdex/ER/Defs/SpEffect.xml`

  **Acceptance Criteria**:
  - [ ] `tests/param/test_param_apply_paramdef_e2e.c` PASS
  - [ ] 全链路验证 6 步骤
  - [ ] 5+ typed getters 调用并断言
  - [ ] SKIP 优雅 (前置数据缺失)

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 完整链路):
    Tool: interactive_bash
    Steps:
      1. ctest -R param_apply_paramdef_e2e --output-on-failure 2>&1 | tee /tmp/T4.4.log
    Expected: PASS,日志含 "row_count=N>=100", "iconId=...", "SP_EFFECT_PARAM_ST"
    Evidence: .sisyphus/evidence/task-T4.4-apply-e2e.log

  Scenario B (SKIP — regulation 缺失):
    Tool: interactive_bash
    Steps: mv regulation.bin → 测试应 SKIP (TEST_IGNORE)
    Expected: PASS + IGNORE 输出
    Evidence: .sisyphus/evidence/task-T4.4-skip.log

  Scenario C (FAILURE — wrong PARAMDEF):
    Tool: interactive_bash
    Steps: 用 EquipParamWeapon.xml apply CAREFUL 到 SpEffectParam → 期望返回 false (不报错,只 false)
    Expected: PASS (验证 mode 行为正确)
    Evidence: .sisyphus/evidence/task-T4.4-wrong-def.log
  ```

  **Commit**: `phase4(tests): full pipeline e2e — regulation→BND4→SpEffectParam→apply`
  - Files: `tests/param/test_param_apply_paramdef_e2e.c`
  - Pre-commit: `ctest -R param_apply_paramdef_e2e`

- [x] **T4.5. FMG 真实 ER msgbnd e2e (item.msgbnd → ItemName.fmg → query)**

  **What to do**:
  在 `tests/param/test_fmg_e2e_er.c` 添加:
  1. `er_extract_from_data0("/msg/engus/item.msgbnd.dcx", &bytes, &size)` (Phase 3 helper) — 候选路径列表 `/msg/engus/`, `/msg/engUS/`, `/msg/en-US/` 各试一遍
  2. 解析为 BND4 (Phase 3) → 找 entry name 含 "ItemName.fmg" 的
  3. `sf_fmg_read_from_memory(...)` (T2.4)
  4. Assert: entry_count > 100, has_md5 ∈ {true,false} (ER 不一定有 MD5)
  5. 查询若干已知 item id (从 Wave 0 探针得来,或社区已知值如 1030000 = "Dagger" / "短剑"):验证 text != NULL && strlen > 0
  6. SKIP 条件: msgbnd 或 Data0 缺失

  **Must NOT do**:
  - 不验证 MD5 hash
  - 不假设特定语言 (engus 不存在试 engUS)
  - 不假设具体 item id 内容 (只验证 != NULL)

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 4)
  - **Wave**: 4
  - **Blocks**: T4.8
  - **Blocked By**: T3.6, T4.1

  **References**:

  **Wave 0 Reference**:
  - `.sisyphus/evidence/phase4-pre-flight.md` — 实测 msgbnd 路径与 ItemName.fmg size

  **Acceptance Criteria**:
  - [ ] `tests/param/test_fmg_e2e_er.c` PASS 或 SKIP
  - [ ] entry_count > 100
  - [ ] 至少 3 个 item id 文本非空

  **QA Scenarios**:

  ```
  Scenario A (HAPPY):
    Tool: interactive_bash
    Steps: ctest -R fmg_e2e_er
    Expected: PASS,日志含 "ItemName entries=N>100"
    Evidence: .sisyphus/evidence/task-T4.5-fmg-e2e.log

  Scenario B (SKIP):
    Tool: interactive_bash
    Steps: 模拟 Data0 缺失 → 测试 SKIP
    Expected: PASS + IGNORE
    Evidence: .sisyphus/evidence/task-T4.5-skip.log
  ```

  **Commit**: `phase4(tests): FMG e2e against ER ItemName.fmg`
  - Files: `tests/param/test_fmg_e2e_er.c`
  - Pre-commit: `ctest -R fmg_e2e_er`

- [x] **T4.6. EMEVD 真实 ER e2e (`*.emevd.dcx` → 解析 + format 验证)**

  **What to do**:
  在 `tests/script/test_emevd_e2e_er.c` 添加:
  1. 候选路径列表 (从 Wave 0 已选): `/event/m60_42_36_00.emevd.dcx`, `/event/common.emevd.dcx`, `/event/m11_00_00_00.emevd.dcx`,各试一遍 `er_extract_from_data0`
  2. 第一个成功的:DCX 解压后字节流 → `sf_emevd_read_from_memory`
  3. Assert: format ∈ {SEKIRO, ELDEN_RING},event_count > 0,某个已知 event id (从 Wave 0 dump 中获取)能找到
  4. format 报告写入 `.sisyphus/evidence/task-T4.6-emevd-flag-confirmed.log` (跟 T0.1 evidence 比对一致性)
  5. SKIP 条件: Data0 缺失

  **Must NOT do**:
  - 不验证 instruction args 内部结构 (无 EMEDF)
  - 不假设具体 event id (从 Wave 0 取)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 联通 Phase 3 提取 + Phase 4 解析,需对比 Wave 0 的 enum 结果
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 4)
  - **Wave**: 4
  - **Blocks**: T4.8
  - **Blocked By**: T0.1 (evidence), T3.7, T4.1

  **References**:

  **Wave 0 Reference**:
  - `.sisyphus/evidence/phase4-pre-flight.md` (T0.1 evidence)

  **Acceptance Criteria**:
  - [ ] `tests/script/test_emevd_e2e_er.c` PASS 或 SKIP
  - [ ] format == 与 T0.1 报告一致
  - [ ] event_count > 0
  - [ ] Evidence 文件 `task-T4.6-emevd-flag-confirmed.log` 存在并对比 T0.1

  **QA Scenarios**:

  ```
  Scenario A (HAPPY):
    Tool: interactive_bash
    Steps: ctest -R emevd_e2e_er
    Expected: PASS,event_count > 0
    Evidence: .sisyphus/evidence/task-T4.6-emevd-e2e.log

  Scenario B (FAILURE — format mismatch with T0.1):
    Tool: interactive_bash
    Steps: 若 T4.6 验证 format 与 T0.1 不一致 → 测试 FAIL 并指明 diff
    Expected: 测试 PASS (一致性验证)
    Evidence: .sisyphus/evidence/task-T4.6-format-confirm.log
  ```

  **Commit**: `phase4(tests): EMEVD e2e against ER event scripts`
  - Files: `tests/script/test_emevd_e2e_er.c`
  - Pre-commit: `ctest -R emevd_e2e_er`

- [x] **T4.7. Examples: `sf_param_dump.c` (+ 可选 `sf_emevd_dump.c`)**

  **What to do**:
  在 `examples/sf_param_dump.c` 实现命令行 PARAM dump 工具:
  1. 用法: `sf_param_dump.exe <regulation.bin path> <paramdef.xml path> <param-name> [out.tsv]`
  2. 调 Phase 2 regulation_decrypt + Phase 3 BND4 + Phase 4 PARAM read + apply paramdef CAREFUL
  3. 输出格式: TSV,第一行表头 (`id\tname\t<field1>\t<field2>...`),后续每行一行
  4. 字段值用 typed getters 读取,按 DefType 格式化 (i32 → "%d", f32 → "%g", string → 直接输出)
  5. 不存在的字段 → 空字符串
  6. 错误处理:任一步失败 → fprintf(stderr, ...) + exit(1)

  可选: `examples/sf_emevd_dump.c` — 命令行 EMEVD 解析工具 (输入 .emevd.dcx,输出每 event 的 id + instruction count + linked file count)。本任务可只交 sf_param_dump.c,sf_emevd_dump.c 留作 stretch goal。

  **Must NOT do**:
  - 不在 example 中链接 Paramdex 自动发现
  - 不在 example 实现 SQL 输出 / JSON 输出 (TSV-only)

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 4)
  - **Wave**: 4
  - **Blocks**: F1, F2, F3, F4
  - **Blocked By**: T4.4 (regulation 链路验证)

  **References**:

  **Pattern References**:
  - `examples/sf_bnd_extract.c` — Phase 3 example 风格 (命令行参数解析 + 错误处理 + 退出码)

  **Acceptance Criteria**:
  - [ ] `examples/sf_param_dump.c` 编译为 `build-mingw/examples/sf_param_dump.exe`
  - [ ] 跑 `sf_param_dump.exe regulation.bin SpEffect.xml SpEffectParam | head -1` 输出表头
  - [ ] 跑全量 dump 输出行数 ≥ 100 (含表头)

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — dump TSV):
    Tool: interactive_bash
    Steps:
      1. cmake --build build-mingw --target sf_param_dump
      2. ./build-mingw/examples/sf_param_dump.exe \
           "C:/Games/ELDEN RING/Game/regulation.bin" \
           "//wsl.localhost/Ubuntu/home/soar/dev/paramdex/ER/Defs/SpEffect.xml" \
           SpEffectParam > /tmp/spEffect.tsv 2>&1
      3. head -1 /tmp/spEffect.tsv | awk -F'\t' '{print NF}'
      4. wc -l /tmp/spEffect.tsv
    Expected:
      - Step 2 退出 0
      - Step 3 ≥ 5 (表头有多个字段)
      - Step 4 ≥ 100
    Evidence: .sisyphus/evidence/task-T4.7-dump.log + /tmp/spEffect.tsv

  Scenario B (FAILURE — 无效 param-name):
    Tool: interactive_bash
    Steps: sf_param_dump.exe ... NotARealParam → 期望 stderr 错误 + exit code 非 0
    Expected: 退出非 0,stderr 含 "param not found"
    Evidence: .sisyphus/evidence/task-T4.7-bad-name.log
  ```

  **Commit**: `phase4(examples): add sf_param_dump.c (regulation+paramdef → TSV)`
  - Files: `examples/sf_param_dump.c`, `examples/CMakeLists.txt`
  - Pre-commit: `cmake --build build-mingw --target sf_param_dump`

---

### Wave 4.5 — Documentation Polish (1 task after Wave 4 全绿)

- [x] **T4.8. 翻新 5 个 api-mapping format-*.md + extensions.md + POLICY.md**

  **What to do**:
  更新 `docs/api-mapping/` 的 5 个 format 文档与 2 个全局文档:
  1. `format-param.md`: 全部 36 行 "未实现" → "✓ aligned" 或 "+ extension"。"+ extension" 行: `sf_param_apply_mode_t` enum (替代上游 8 method 中的 3 个核心),`sf_param_cell_get_*` 13 typed getters。"_skipped_" 行: ApplyRegulationVersioned* 4 个变体。
  2. `format-paramdef.md`: 全部 33 行翻新。"_skipped_" 行: XmlSerialize (写出推迟 v1.1)。
  3. `format-paramtdf.md`: 翻新。
  4. `format-fmg.md`: 翻新。"_skipped_" 行: 自动 MD5 验证。
  5. `format-emevd.md`: 翻新 + 加注 ER/AC6/Nightreign 探针结果 + 把 "Phase 5" 列改为 "Phase 4"。"_skipped_" 行: PackArgs/UnpackArgs (C# 专用),EMEDF JSON loader。
  6. `extensions.md`: 添加 6 行: `er_load_param`, `er_load_msgbnd_entry` (test helper 扩展),`sf_param_apply_mode_t` (3-mode 折叠),`SF_EMEVD_FORMAT_ELDEN_RING/AC6/NIGHTREIGN` (Sekiro 别名),`sf_emevd_format_probe` (Wave 0 工具),`sf_paramdef_get_index` + `sf_paramdef_field_get_sort_id` (Paramdex XML 专有元数据;上游 PARAMDEF 类不含,但 Paramdex XML 含 `<Index>` / `<SortID>`,为工具兼容暴露)。
  7. `POLICY.md`: 添加 1 节 "Phase 4 adaptations": 解释 PARAMTDF Trim('"') 镜像 + 8→3 Apply 折叠 + bit-packing 字面镜像。
  8. 更新 `docs/api-mapping/UPSTREAM.md` Phase 4 涉及 commit hash 验证 (如果不一致就更新到当前 `9f5848f5f`)。

  **Must NOT do**:
  - 不在 api-mapping 中漏掉任何 Phase 4 涉及的行
  - 不修改 Phase 0-3 的 mapping (只翻新 Phase 4 行)

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: 文档工作,需准确把代码对应到文档
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: NO (汇聚 Wave 4 全部成果)
  - **Wave**: 4.5
  - **Blocks**: F1, F4
  - **Blocked By**: T4.2, T4.3, T4.4, T4.5, T4.6, T4.7

  **References**:

  **Pattern References**:
  - `docs/api-mapping/format-bnd4.md` — Phase 3 翻新后的样板 (`✓ aligned` / `+ extension` / `_skipped_` 应用)
  - `docs/api-mapping/extensions.md` (Phase 1-3 已添加内容)
  - `docs/api-mapping/POLICY.md` (Phase 1-3 adaptations)

  **Acceptance Criteria**:
  - [ ] `grep -c "未实现" docs/api-mapping/format-{param,paramdef,paramtdf,fmg,emevd}.md` = 0 (零残留)
  - [ ] `extensions.md` 含 5 个新行 (Phase 4 markers)
  - [ ] `POLICY.md` 含新 "Phase 4 adaptations" 节
  - [ ] `format-emevd.md` 引用 `.sisyphus/evidence/phase4-pre-flight.md`

  **QA Scenarios**:

  ```
  Scenario A (HAPPY — 零 "未实现" 残留):
    Tool: Bash
    Steps:
      1. cd /home/soar/src/souls-formats-c
      2. grep -c "未实现" docs/api-mapping/format-{param,paramdef,paramtdf,fmg,emevd}.md
    Expected: 全部输出 "0" (或 0 行)
    Evidence: .sisyphus/evidence/task-T4.8-zero-pending.log

  Scenario B (HAPPY — extensions/POLICY 更新):
    Tool: Bash
    Steps:
      1. grep -E '(er_load_param|sf_param_apply_mode_t|SF_EMEVD_FORMAT_ELDEN_RING|3-mode Apply|PARAMTDF Trim)' \
           docs/api-mapping/{extensions,POLICY}.md | wc -l
    Expected: ≥ 5 行
    Evidence: .sisyphus/evidence/task-T4.8-divergences.log
  ```

  **Commit**: `phase4(docs): refresh api-mapping for 5 formats + extensions + POLICY`
  - Files: `docs/api-mapping/format-{param,paramdef,paramtdf,fmg,emevd}.md`, `docs/api-mapping/extensions.md`, `docs/api-mapping/POLICY.md`, `docs/api-mapping/UPSTREAM.md`
  - Pre-commit: 上面两个 grep 验证

---



> 4 review agents 并行运行。**ALL must APPROVE**。Present consolidated results to user and get explicit "okay" before completing。
>
> **Do NOT auto-proceed after verification. Wait for user's explicit approval before marking work complete.**
> **Never mark F1-F4 as checked before getting user's okay.** 拒绝或反馈 → 修复 → 重跑 → 再次呈现 → 等 okay。

- [x] **F1. Plan Compliance Audit** — `oracle`

  完整阅读本计划。对每条 "Must Have": 验证实现存在 (read 文件 / `objdump` 验证导出 / `ctest` 跑测试)。对每条 "Must NOT Have": 在源码中搜索禁用模式 — 找到则以 `file:line` reject。检查 evidence 文件存在于 `.sisyphus/evidence/`。Compare deliverables 对照计划。

  **Output**: `Must Have [N/N] | Must NOT Have [N/N 无违反] | Tasks [N/N] | DLL exports [actual/target] | VERDICT: APPROVE/REJECT`

  Evidence: `.sisyphus/evidence/final-qa/F1-plan-compliance.md`

- [x] **F2. Code Quality Review** — `unspecified-high`

  跑 `cmake --build build-mingw 2>&1 | tee build.log` (验证零 warning + `-Werror` 通过)。跑 `ctest --test-dir build-mingw -L 'param|script' --output-on-failure`。Review 全部 changed 文件: `as any` 类型擦除 (C 中表现为 `void *` 滥用)、空 catch / 忽略错误返回值、`printf`/`fprintf` 调试语句残留、注释掉的代码、未使用的 include / static 函数。检查 AI slop: 过度注释、过度抽象、generic 命名 (data/result/temp)、defensive null checks 上游不存在的。

  **Output**: `Build [PASS/FAIL,zero warnings] | Tests [N pass/N fail] | Files [N clean/N issues] | Slop [清单] | VERDICT`

  Evidence: `.sisyphus/evidence/final-qa/F2-code-quality.md`

- [x] **F3. Real Manual QA** — `unspecified-high`

  从 clean state 起步 (`rm -rf build-mingw && cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug && cmake --build build-mingw`)。Execute EVERY QA scenario from EVERY task — follow exact steps, capture evidence。跑跨任务整合: regulation pipeline 全链路 (AES → BND4 → PARAM → apply PARAMDEF) 一次性跑通 + dump 前 5 行结果。Test edge cases: PARAM 0 行、FMG 全空 entry、EMEVD 0 events、PARAMDEF v203 variable-typed defaults。Save 至 `.sisyphus/evidence/final-qa/F3-real-qa/`。

  **Output**: `Scenarios [N/N pass] | Integration [N/N] | Edge Cases [N tested] | VERDICT`

- [x] **F4. Scope Fidelity Check** — `deep`

  对每个 task: 读 "What to do",读 实际 diff (`git log/diff`)。Verify 1:1 — 计划中所有要做的都已实现 (no missing),计划之外的都未做 (no creep)。Check "Must NOT do" compliance。Detect 跨任务污染: Task N 改了 Task M 的文件。Flag unaccounted changes。验证 `extensions.md` + `POLICY.md` 包含 Phase 4 所有 6 处 divergence。验证 `_Static_assert` 出现在每个新 enum 旁。

  **Output**: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | Divergences documented [6/6] | _Static_assert [present 5/5] | VERDICT`

  Evidence: `.sisyphus/evidence/final-qa/F4-scope-fidelity.md`

---

## Commit Strategy

> 一任务一 commit。每个 commit 在创建前必须本地构建 + 跑相关 ctest label 全绿。
>
> Commit message 格式: `phase4(param|paramdef|paramtdf|fmg|emevd|build|tests|docs): <一句话描述>`
>
> 完整 commit 列表附在每个 TODO 的 "Commit" 区块。最终 PR 包含 ~32 个 atomic commits + 1 final docs commit。

主要 commit 分组:

| Group | Commits | Description |
|---|---|---|
| Wave 0 | 1 | pre-flight probe + evidence |
| Wave 1 | 7 | 5 headers + bitstream helpers + CMakeLists |
| Wave 2 | 5 | 5 binary readers |
| Wave 3 | 7 | 5 binary writers + XML reader + apply paramdef |
| Wave 4 | 7 | helper extension + 5 e2e tests + 1-2 examples |
| Wave 4.5 | 1 | docs/api-mapping refresh + POLICY/extensions |
| Final | 1 | PLAN.md + roadmap README check-off |

---

## Success Criteria

### Verification Commands

```bash
# 完整构建,零 warning
cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mingw 2>&1 | tee /tmp/phase4-build.log
! grep -E 'warning:|error:' /tmp/phase4-build.log
# Expected: 退出码 0 (无 warning/error)

# Phase 4 测试 (param + script labels)
ctest --test-dir build-mingw -L 'param|script' --output-on-failure
# Expected: 11/11 PASS, 0 FAIL, 可能有 SKIP (前置数据缺失合法)

# DLL 导出符号增长
objdump -p build-mingw/libsouls_formats.dll | grep -c '^\s*\[\s*[0-9]\+\]\s*sf_'
# Expected: 在 [544, 564] 范围内 (Phase 3 结束 469, +75~95)

# 示例运行 sanity
./build-mingw/examples/sf_param_dump.exe \
    "C:/Games/ELDEN RING/Game/regulation.bin" \
    "//wsl.localhost/Ubuntu/home/soar/dev/paramdex/ER/Defs/SpEffect.xml" \
    SpEffectParam | head -1
# Expected: 表头行 "id\tname\ticonId\t..." 非空

# Wave 0 evidence 存在
test -f .sisyphus/evidence/phase4-pre-flight.md
# Expected: 存在,内容包含 "EMEVD flags:", "BND4 entry:", "ItemName 1030000:"

# api-mapping 翻新
grep -c "未实现" docs/api-mapping/format-{param,paramdef,paramtdf,fmg,emevd}.md | \
    awk -F: '$2 > 0 {print; bad=1} END {exit bad+0}'
# Expected: 退出码 0 (无 "未实现" 残留)

# 6 处 divergence 文档
grep -E "^- (er_load_param|sf_emevd_format_probe|sf_param_apply_mode_t|PARAMTDF Trim|3-mode Apply|sf_paramdef_get_index)" \
    docs/api-mapping/{extensions,POLICY}.md | wc -l
# Expected: ≥ 6
```

### Final Checklist

- [ ] 所有 "Must Have" 已实现 (F1 audit pass)
- [ ] 所有 "Must NOT Have" 不存在于源码 (F1 audit pass)
- [ ] `cmake --build` 全绿,零 warning (F2 audit pass)
- [ ] `ctest -L 'param|script'` 全绿 (F2 + F3 audit pass)
- [ ] 32 atomic tasks 全部 commit,每个含 references + QA scenarios (F4 audit pass)
- [ ] `_Static_assert` 出现在所有新 enum 旁 (F4 audit pass)
- [ ] 6 处 divergence 文档化于 `extensions.md` / `POLICY.md` (F4 audit pass)
- [ ] DLL 导出 ∈ [544, 564] (F1 audit pass)
- [ ] PLAN.md Phase 4 § ticked + 测试通过数 + 时间戳 (Wave 4.5 完成时)
- [ ] `docs/roadmap/README.md` Phase 4 status 改为 "✅ done — N/N PASS (date)"
- [ ] User 对 F1-F4 results 给出 explicit okay
