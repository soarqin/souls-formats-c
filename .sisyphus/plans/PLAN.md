# souls-formats-c 项目计划

> Pure C 语言移植版本的 [SoulsFormatsNEXT](https://github.com/soulsmods/SoulsFormatsNEXT)，用于读写 FromSoftware 系列游戏的二进制资源格式。

---

## 1. 项目概述

### 1.1 项目目标
- 将 SoulsFormatsNEXT (C# / .NET 9) 的核心子集移植为 **Pure C** 静态/动态库。
- 优先支持 **Sekiro / Elden Ring / Nightreign / Armored Core VI** 所需的格式。
- 提供干净的 C ABI，便于 C / C++ / Rust / Go / Python 等语言绑定。
- 设计为**只读 + 写出**双向支持的 round-trip 库，与上游保持语义一致。

### 1.2 与上游的关系
- 当前快照基线：`/home/soar/src/SoulsFormatsNEXT`（net9.0 分支，413 个 `.cs`，约 152K LOC）。
- 移植**不是**逐行翻译，而是按"格式 → 模块"的语义重写：保留 `BinaryReaderEx` / `BinaryWriterEx` 的 API 形状，但用 C 风格命名与内存语义。
- 上游 net9.0 分支引入了 Zstd 与 ER / AC6 新格式，这些都纳入 v1 考虑。

### 1.3 关键约束
| 约束 | 说明 |
|---|---|
| **平台** | **仅 Windows**。理由：DCX_KRAK 必须运行时加载游戏自带的 `oo2core_{6,8,9}_win64.dll`。库构建本身不带 Oodle 二进制。 |
| **C 标准** | C11，允许 `_Generic` / `static_assert` / 匿名结构体。**不**使用 GNU 扩展。 |
| **编译器矩阵** | MSVC ≥ 2022 (cl)，clang-cl ≥ 16，MinGW-w64 GCC ≥ 13。 |
| **架构** | 仅 x86_64（与 FromSoft 游戏保持一致）。ARM64 列入 v2 评估。 |
| **许可证** | **GPLv3**，与上游 SoulsFormatsNEXT 保持一致（不可变）。 |
| **语言/编码** | 内部全 UTF-8；与游戏交换时通过 Win32 `MultiByteToWideChar` / `WideCharToMultiByte` 处理 Shift-JIS / UTF-16 LE / UTF-16 BE。 |
| **线程模型** | 单上下文单线程；多个独立上下文可并行使用（库本身可重入但非线程安全的状态共享）。 |
| **零异常 / setjmp** | 全部用 `sf_result_t` 错误码 + 输出参数返回。 |

---

## 2. 范围

### 2.1 v1 必交付（用户明确指定 + 我判断的关联依赖）

| 类别 | 格式 | 用途 / 备注 |
|---|---|---|
| **基础运行时** | binary IO、stream、encoding、math 类型、错误、分配器、文件名哈希 | 全部下游格式的地基 |
| **压缩** | DCX 全类型（None / Zlib / DCP_EDGE / DCX_EDGE / DCP_DFLT / DCX_DFLT / DCX_KRAK / DCX_ZSTD） | DCX 是几乎每个格式的外层壳 |
| **加密** | AES-128-ECB（BHD5）、AES-128-CBC（SL2）、AES-256-CBC（regulation.bin）、MD5（FMG/SL2） | 通过 Windows CNG 实现 |
| **容器** | BND3、BND4、BXF3、BXF4、BHD5、TPF、ENFL | 用户指定的容器全集 + 加载流相关 |
| **参数与文本** | PARAM、PARAMDEF（二进制）、PARAMDEF（XML 反序列化）、PARAMTDF、FMG | 用户指定 + Paramdex 配套 |
| **脚本** | EMEVD、ESD | 用户指定 EMEVD；ESD 是 Sekiro/ER 同时使用的状态机（已在 Phase 4 完成，2026-05-11） |
| **地图** | MSBS（Sekiro）、MSBE（ER + Nightreign）、MSBVI（AC6）、MSB 公共骨架 | 覆盖目标四款游戏的地图格式 |
| **几何与材质** | FLVER2、MTD、MATBIN | 所有四款目标游戏的网格 + 材质 |

### 2.2 v1 显式**不**实现（推迟到 v1.x / v2）
- **动画与特效**：TAE3 / TAE4、FXR3、FFXDLSE、ANI、MQB —— 单独立 Phase 7，可在 v1.1 增量交付。
- **TAE Template subsystem**（`Template.cs` 801 LOC，`ApplyTemplate` / `BankTemplate` / `EventTemplate` / `ParameterTemplate`）—— v1.1 不实现，参数容器维持 opaque bytes；typed access 推 v1.2。
- **TAE 非 SDT 格式**（DS1 / SOTFS / DS3 / BB / DES / DESR）—— v1.1 仅实现 SDT (version 0x1000D)；其他 format byte 路径返回 `SF_ERR_UNSUPPORTED_VERSION`，推 v2 legacy。
- **遗留地图**：MSB1 / MSB2 / MSB3 / MSBB / MSBD / MSBN / MSBV / MSBVD / MSBFA / MSBAC4。
- **遗留几何**：FLVER0、MDL / MDL0 / MDL4 / SMD4 / OM2、CLM2、F2TR、GRASS。
- **遗留几何**：FLVER0、MDL / MDL0 / MDL4 / SMD4 / OM2、CLM2、F2TR、GRASS。Edge Geometry / SPU vertex format / RSX vertex format（PS3-era console-specific 顶点压缩；v1 4 款目标游戏均不使用，推迟到 v2）。
- **遗留容器**：BND2、DVDBND、FSDATA、LDMU、MGF、Zero3、ACE3 / AC3SL / Kuon BND。
- **导航网格**：NVA / NVM / NGP / MCG / MCP / EDGE。
- **照明**：BTAB / BTL / BTPB / PMDCL。
- **AC 系列专属**：AcParts4 / MLB_AC4 / MLB_AC5 / FXR1。
- **杂项**：ACB、CCM、DRB、RMB、AIP、FMB、EDD、EMELD、LUAGNL、LUAINFO。
- **PARAMDEF XML 序列化**（写出方向）—— 阶段 4 仅做读，写入推迟。

### 2.3 后续路线图速览
- **v1.1**：TAE / FXR3 已交付 (2026-05-12)；PARAMDEF XML 写出推迟。TAE 仅 SDT 格式；Template subsystem 推 v1.2。
- **v2.0**：DS3 / Bloodborne / DS1 / DS2 / DeS 全部格式。
- **v3.0**：Armored Core 4/ACFA/ACV/ACVD、King's Field、Kuon、Otogi、Dreamcast 系列。

---

## 3. 整体架构

### 3.1 分层
```
┌────────────────────────────────────────────────────────────┐
│  Public C ABI  (souls_formats/*.h)                          │
├────────────────────────────────────────────────────────────┤
│  Format Modules                                             │
│    container/  param/  text/  script/  map/  geom/  ...    │
├────────────────────────────────────────────────────────────┤
│  Service Layer                                              │
│    compression  crypto   encoding   filename_hash           │
├────────────────────────────────────────────────────────────┤
│  Core Runtime                                               │
│    error  allocator  stream  binary_reader  binary_writer   │
│    math_types  utility  endian                              │
├────────────────────────────────────────────────────────────┤
│  Third-party                                                │
│    zlib-ng   libzstd   mxml   klib   Win32 CNG   Oodle DLL  │
└────────────────────────────────────────────────────────────┘
```

### 3.2 依赖原则
- **只能向下依赖**。Format 模块不能互相依赖（除非 MSB 共享骨架这种明确的 sub-shared）。
- **Service 层是叶子**。压缩、加密、编码不知道任何上层格式。
- **Core 层零第三方依赖**，仅依赖 C11 标准库 + Win32。
- 上层 → 下层依赖通过头文件包含 + 静态链接，不出现动态注册表。

### 3.3 后端可替换层（为未来 Linux/Wine 子集预留）
v1 仍然 Windows-only，但目录结构与 CMake 选项预留替换点，未来增量启用。

| 后端 | Windows 实现（v1） | 未来可替换 |
|---|---|---|
| `compression/oodle/` | `LoadLibraryW` + `oo2core_*_win64.dll` | ooz（GPLv3）/ liboodle stub / 无支持 |
| `crypto/aes_*` | Windows CNG / BCrypt | mbedTLS / OpenSSL |
| `crypto/md5_*` | Windows CNG / BCrypt | mbedTLS / 自带 RFC 1321 实现 |
| `core/encoding_win32.c` | `MultiByteToWideChar` (CP 932) | iconv / ICU / 自带 Shift-JIS 转换表 |
| `core/path.c` | Win32 wide path | POSIX `char*` |

**约束**：每个后端单元只在其文件夹下存在一个**实现集**编译进二进制；切换通过 CMake 选项（如 `SF_BACKEND_CRYPTO=cng|mbedtls`）控制，**不**做运行时 dispatch。这保持 v1 路径单纯、二进制最小，又为 v1.x+ 的 Linux/Wine 端口零成本扩展。

---

## 4. 技术选型（已锁定）

| 决策点 | 选择 | 备选已淘汰 | 理由 |
|---|---|---|---|
| 构建系统 | **CMake ≥ 3.24** | meson、xmake | 用户指定 |
| 库构建目标 | **STATIC + SHARED 双构建** | 仅 STATIC / 仅 SHARED | 用户确认。同一 source set，CMake 通过 `souls_formats_static` 与 `souls_formats_shared` 两个 target 暴露 |
| 起始版本 | **SemVer 0.2.0** [^1]，v1 范围 ship 后再升 1.0.0 | 直接 1.0.0 | 用户确认。0.x 期间允许 ABI 不稳定 |
| 依赖管理 | **CPM.cmake** | FetchContent / vcpkg / Conan | 用户选择，零工具链上手 |
| 测试框架 | **Unity (ThrowTheSwitch)** | CMocka / Criterion | 单头文件、MIT、最简单 |
| DEFLATE | **zlib-ng**（compat 模式） | zlib / libdeflate / miniz | 用户选择，drop-in API + 显著加速 |
| Zstandard | **libzstd** (Meta 官方) | — | 唯一现实选择 |
| Oodle Kraken | **运行时 `LoadLibraryW` 加载游戏自带 `oo2core_{6,8,9}_win64.dll`** | ooz (GPLv3 开源解码器) | 与上游对等；游戏自带 DLL 即官方实现，正向/反向都支持 |
| AES / MD5 | **Windows CNG / BCrypt** | mbedTLS / OpenSSL / BearSSL | 用户选择，零依赖、Windows-only 项目最干净 |
| Shift-JIS / UTF-16 转码 | **Win32 `MultiByteToWideChar` / `WideCharToMultiByte`** | iconv / ICU | 系统内置、CP 932 全支持 |
| 数学类型 | **POD struct 自研**（`sf_vec3_t` / `sf_quat_t` / `sf_mat4_t`） | cglm / HandmadeMath | 库本身只做读写，不做运算；消费者自行选数学库 |
| 内部容器 | **klib (kvec / khash)** | stb_ds / uthash / 自研 | 用户选择，性能最佳 |
| XML 处理 | **mxml (Mini-XML)** | libxml2 / Expat / yxml | 用户选择，DOM 风格读写都支持，体积小 |
| 字符串容器 | 内部自定义 `sf_strbuf_t`（小型 builder）+ klib 的 `kstring_t` | — | 避免又拉一个字符串库 |

### 4.1 第三方库版本钉子（CPM 中）
```cmake
CPMAddPackage(NAME zlib-ng        VERSION 2.2.4   GITHUB_REPOSITORY zlib-ng/zlib-ng
              OPTIONS "ZLIB_COMPAT ON" "ZLIB_ENABLE_TESTS OFF" "WITH_GTEST OFF")
CPMAddPackage(NAME zstd           VERSION 1.5.7   GITHUB_REPOSITORY facebook/zstd
              SOURCE_SUBDIR build/cmake
              OPTIONS "ZSTD_BUILD_PROGRAMS OFF" "ZSTD_BUILD_TESTS OFF" "ZSTD_BUILD_STATIC ON" "ZSTD_BUILD_SHARED OFF")
CPMAddPackage(NAME mxml           VERSION 4.0.4   GITHUB_REPOSITORY michaelrsweet/mxml)
CPMAddPackage(NAME klib           GIT_TAG  master GITHUB_REPOSITORY attractivechaos/klib
              DOWNLOAD_ONLY YES)              # 单头文件，纳入 third_party include path
CPMAddPackage(NAME Unity          VERSION 2.6.1  GITHUB_REPOSITORY ThrowTheSwitch/Unity)
```

> 上方版本号是计划撰写时的最新稳定版，Phase 0 落地时统一锁版本到 commit hash 以保证可重现。

---

## 5. 公共 API 设计原则

### 5.1 命名约定
- 全局前缀：`sf_`（souls-formats 缩写）。
- 类型：`sf_<format>_t`（小写 + 下划线，如 `sf_bnd4_t`、`sf_param_row_t`）。
- 函数：`sf_<format>_<verb>[_<noun>]`（如 `sf_bnd4_read_from_file`、`sf_dcx_decompress`）。
- 常量：`SF_<CATEGORY>_<NAME>`（如 `SF_OK`、`SF_ERR_BAD_MAGIC`、`SF_DCX_TYPE_KRAK`）。
- 不透明指针类型用前置声明，定义只在 `.c`：`typedef struct sf_bnd4 sf_bnd4_t;`。

### 5.2 错误处理
所有可能失败的函数返回 `sf_result_t`，输出走指针参数：
```c
typedef enum sf_result {
    SF_OK = 0,
    SF_ERR_INVALID_ARG,
    SF_ERR_OOM,
    SF_ERR_IO,
    SF_ERR_BAD_MAGIC,
    SF_ERR_UNSUPPORTED_VERSION,
    SF_ERR_TRUNCATED,
    SF_ERR_OUT_OF_RANGE,
    SF_ERR_DECOMPRESS,
    SF_ERR_OODLE_NOT_FOUND,
    SF_ERR_CRYPTO,
    SF_ERR_INTERNAL,
    /* ... */
} sf_result_t;

const char *sf_result_str(sf_result_t r);

/* 线程局部最近一次错误的额外细节（可选检索） */
const char *sf_last_error_detail(void);
```

### 5.3 内存与分配器
所有"创建"类 API 都接受可选 `const sf_allocator_t *alloc`，传 `NULL` 用默认 `malloc/free`：
```c
typedef struct sf_allocator {
    void *(*alloc)(size_t size, void *user);
    void *(*realloc)(void *p, size_t old_size, size_t new_size, void *user);
    void  (*free)(void *p, void *user);
    void  *user;
} sf_allocator_t;
```
- 每个对象由它自己的分配器创建并拥有；`sf_*_destroy` 必然调用同一分配器。
- 没有任何隐式共享 / 引用计数 / 静态全局 owner（分配器本身可被注册为进程默认）。

### 5.4 流抽象
统一的只读 / 只写流，覆盖文件路径、内存、用户回调：
```c
typedef struct sf_istream sf_istream_t;
sf_result_t sf_istream_open_file (sf_istream_t **out, const wchar_t *path, const sf_allocator_t *a);
sf_result_t sf_istream_open_memory(sf_istream_t **out, const void *data, size_t size, const sf_allocator_t *a);
void        sf_istream_close(sf_istream_t *s);
```
所有 `sf_<format>_read_*` 都在 stream 上消费。

### 5.5 字节序
- 库内部对每条流维护当前字节序状态（仿 `BinaryReaderEx.BigEndian`），格式模块按需切换。
- 不在公共类型上暴露字节序：所有解析后的字段都已经是 host endian。

### 5.6 ABI 导出宏 `SF_API`
所有公共符号统一通过 `SF_API` 装饰，便于 SHARED 构建生成正确的 `__declspec(dllexport)` / `__declspec(dllimport)`，亦便于 Rust / Python / Go 等语言绑定时识别 ABI 表面：

```c
/* sf_common.h */
#if defined(_WIN32) && defined(SF_BUILD_SHARED)
#  if defined(SF_BUILD_DLL)
#    define SF_API __declspec(dllexport)
#  else
#    define SF_API __declspec(dllimport)
#  endif
#else
#  define SF_API   /* STATIC 构建或非 Windows 时为空 */
#endif

/* 用法 */
SF_API sf_result_t sf_bnd4_read_from_path(sf_bnd4_t **out, const wchar_t *path,
                                          const sf_allocator_t *a);
```

CMake 端 `souls_formats_shared` 设 `SF_BUILD_SHARED` + `SF_BUILD_DLL`；`souls_formats_static` 不设；下游消费者使用 SHARED 时仅设 `SF_BUILD_SHARED`。我们用 CMake `generate_export_header` 验证导出表，但**不**直接采用其生成的宏（避免命名污染消费者）。

### 5.7 范例 API 形状（BND4）
```c
/* 读 */
sf_bnd4_t *bnd = NULL;
sf_result_t r = sf_bnd4_read_from_path(&bnd, L"C:\\game\\chrbnd\\c0000.chrbnd.dcx", NULL);
if (r != SF_OK) { fprintf(stderr, "%s\n", sf_result_str(r)); return 1; }

size_t n;
sf_bnd4_file_count(bnd, &n);
for (size_t i = 0; i < n; i++) {
    const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
    /* f->name (UTF-8), f->id, f->data, f->size, f->compression_type */
}
sf_bnd4_destroy(bnd);

/* 写 */
sf_bnd4_t *out = NULL;
sf_bnd4_create(&out, NULL);
sf_bnd4_set_version(out, "07D7R6");          /* ER 版本号 */
sf_bnd4_set_compression(out, SF_DCX_TYPE_KRAK);
sf_bnd4_add_file(out, /* id */ 100, "MyAsset.bin", data, size, /* compress */ false);
sf_bnd4_write_to_path(out, L"C:\\out\\custom.chrbnd.dcx");
sf_bnd4_destroy(out);
```

### 5.x Strict upstream alignment

Two mandatory rules that apply to **every** code change in this repo:

1. **STRICT UPSTREAM REFERENCE**: Every code implementation must strictly reference upstream code at the pinned commit (see `docs/api-mapping/UPSTREAM.md`). Guessing at semantics, signatures, or wire formats is FORBIDDEN. When in doubt, read the `.cs` file.

2. **API MIRRORS UPSTREAM**: Public C API design must mirror upstream as closely as possible. Minor C-style adjustments (out-param error returns, pointer-based ownership, snake_case) are explicitly allowed. Functional differences are FORBIDDEN. Every divergence must be either (a) a documented C-style adaptation in `docs/api-mapping/POLICY.md`, or (b) a tracked extension in `docs/api-mapping/extensions.md`.

See also: `docs/api-mapping/README.md` for the full upstream→ours mapping table index.

---

## 6. 目录结构

```
souls-formats-c/
├── CMakeLists.txt
├── PLAN.md                              ← 本文件
├── README.md                            ← Phase 0 末尾交付
├── LICENSE                              ← GPL-3.0 全文
├── .gitignore
├── .clang-format
├── .editorconfig
├── cmake/
│   ├── CPM.cmake                        ← cpm.cmake 自身
│   ├── compiler_warnings.cmake
│   ├── sanitizers.cmake
│   └── deps/
│       ├── zlib-ng.cmake
│       ├── zstd.cmake
│       ├── mxml.cmake
│       ├── klib.cmake
│       └── unity.cmake
├── include/
│   └── souls_formats/
│       ├── souls_formats.h              ← 全总入口（包含全部子头）
│       ├── sf_common.h                  ← 错误码、分配器、版本宏
│       ├── sf_io.h                      ← 流、binary reader / writer
│       ├── sf_math.h                    ← vec / quat / mat 等 POD
│       ├── sf_dcx.h
│       ├── sf_bnd3.h
│       ├── sf_bnd4.h
│       ├── sf_bxf3.h
│       ├── sf_bxf4.h
│       ├── sf_binder.h                  ← BND/BXF 共享类型
│       ├── sf_bhd5.h
│       ├── sf_tpf.h
│       ├── sf_enfl.h
│       ├── sf_param.h
│       ├── sf_paramdef.h
│       ├── sf_paramtdf.h
│       ├── sf_fmg.h
│       ├── sf_emevd.h
│       ├── sf_esd.h
│       ├── sf_msb.h                     ← MSB 公共
│       ├── sf_msbe.h
│       ├── sf_msbs.h
│       ├── sf_msbvi.h
│       ├── sf_flver.h                   ← FLVER 公共 enum / 顶点格式
│       ├── sf_flver2.h
│       ├── sf_mtd.h
│       └── sf_matbin.h
├── src/
│   ├── core/
│   │   ├── error.c
│   │   ├── allocator.c
│   │   ├── stream.c
│   │   ├── binary_reader.c              ← 对应 BinaryReaderEx
│   │   ├── binary_writer.c              ← 对应 BinaryWriterEx
│   │   ├── encoding_win32.c             ← Shift-JIS / UTF-16 转 UTF-8
│   │   ├── math.c                       ← 仅 swap / 转换辅助
│   │   ├── path.c                       ← 路径正规化
│   │   └── filename_hash.c              ← BND/BHD 的 path 哈希
│   ├── compression/
│   │   ├── dcx.c                        ← DCX 主入口
│   │   ├── deflate_zlibng.c             ← deflate 双向
│   │   ├── deflate_chunked.c            ← DCP_EDGE / DCX_EDGE 分块
│   │   ├── zstd_wrap.c
│   │   └── oodle/
│   │       ├── oodle_loader.c           ← LoadLibraryW + GetProcAddress
│   │       ├── oodle_v6.c
│   │       ├── oodle_v8.c
│   │       └── oodle_v9.c
│   ├── crypto/
│   │   ├── aes_cng.c                    ← BCryptOpenAlgorithmProvider("AES")
│   │   ├── md5_cng.c                    ← BCryptOpenAlgorithmProvider("MD5")
│   │   ├── regulation.c                 ← AES-256-CBC 包装
│   │   └── sl2.c                        ← AES-128-CBC + MD5 校验
│   ├── archive/
│   │   ├── binder_common.c              ← BND/BXF 共享逻辑
│   │   ├── bnd3.c
│   │   ├── bnd4.c
│   │   ├── bxf3.c
│   │   ├── bxf4.c
│   │   ├── bhd5.c
│   │   ├── tpf.c
│   │   └── enfl.c
│   ├── param/
│   │   ├── param.c
│   │   ├── paramdef.c
│   │   ├── paramdef_xml_read.c          ← mxml 反序列化
│   │   └── paramtdf.c
│   ├── text/
│   │   └── fmg.c
│   ├── script/
│   │   ├── emevd.c
│   │   └── esd.c
│   ├── map/
│   │   ├── msb_common.c                 ← Entry / Part / Region 共享骨架
│   │   ├── msbs.c
│   │   ├── msbe.c
│   │   └── msbvi.c
│   └── geom/
│       ├── flver_common.c
│       ├── flver2.c
│       ├── flver2_vertex.c              ← 顶点格式表
│       ├── mtd.c
│       └── matbin.c
├── third_party/
│   └── klib/                            ← CPM 拉下来后通过 include path 暴露
├── tests/
│   ├── CMakeLists.txt
│   ├── unity_runner.c
│   ├── core/
│   │   ├── test_binary_reader.c
│   │   ├── test_binary_writer.c
│   │   ├── test_encoding.c
│   │   └── test_filename_hash.c
│   ├── compression/
│   │   ├── test_dcx_dflt.c
│   │   ├── test_dcx_edge.c
│   │   ├── test_dcx_zstd.c
│   │   └── test_dcx_krak.c              ← 仅在 oo2core_*.dll 可达时运行
│   ├── crypto/
│   │   ├── test_aes_kat.c               ← NIST 已知答案
│   │   └── test_md5_kat.c
│   ├── archive/
│   │   ├── test_bnd4_roundtrip.c
│   │   └── ...
│   ├── param/
│   ├── map/
│   ├── geom/
│   ├── fixtures/
│   │   ├── synthetic/                   ← 我们手工构造的最小合规样本（可入库）
│   │   └── README.md                    ← 解释真实游戏样本来源（不入库）
│   └── e2e/                             ← 通过 SOULS_FORMATS_C_GAMES_DIR 找游戏目录
│       └── ...
├── examples/
│   ├── CMakeLists.txt
│   ├── sf_dcx_unwrap.c                  ← 命令行：解包 .dcx
│   ├── sf_bnd_extract.c                 ← 命令行：解包 BND
│   └── sf_param_dump.c                  ← 命令行：dump PARAM 行
└── docs/
    ├── architecture.md
    ├── api_conventions.md
    ├── format_dcx.md
    ├── format_bnd4.md
    └── ...
```

---

## 7. 阶段化里程碑

> **运行时工作文档已外移**：每个未完成阶段的实施细节（具体文件路径、上游 `.cs` 引用、公共 API 草图、实现注意点、风险）落在
> [`docs/roadmap/phase-N-*.md`](../../docs/roadmap/README.md) 中。本节是 Momus 已审计的"做什么 + 退出验证"清单；docs/roadmap/ 是"怎么做"。
> 两者不一致时，本文件优先。

每个阶段的退出标准：
- 该阶段所有头文件 / 源文件存在并无 LSP 警告。
- 该阶段所有目标格式至少有 **一个合成 fixture** 通过单元测试。
- 该阶段所有"在 Elden Ring 中出现"的格式必须通过 **真实 e2e**（路径见 §8.4）；Sekiro / AC6 / Nightreign 独占的格式以合成 fixture 为准，真实 e2e 以 SKIP 标记跳过（§8.6）。
- `cmake --build` 在 MSVC + clang-cl + MinGW-w64 三个工具链全绿；MinGW 构建产物通过 WSL interop 直接 `./test.exe` 执行通过。
- 添加该阶段对应的 `docs/format_*.md` 简述文件 + 字段表。

### Phase 0 — 工程脚手架（预估 0.5 周）✅ 完成 (2026-05-10)
**Completion Retrospective**: Completed. Build system, CI, Unity smoke test.
- [x] 顶层 `CMakeLists.txt`（项目元数据、`souls_formats_static` + `souls_formats_shared` 双目标、版本宏 0.1.0、`SF_API` 导出宏接入）。
- [x] `cmake/CPM.cmake` 引入（v0.42.0，1363 行，sha256 `2020b4fc...`）。
- [x] `cmake/toolchain-mingw-w64.cmake`（WSL2 cross-compile to Windows PE，含 POSIX 线程 fallback + `-static-libgcc` 链接）。
- [x] `cmake/compiler_warnings.cmake`：`/W4 /WX /permissive-` (MSVC) + `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wcast-align -Wstrict-prototypes` (GCC/Clang/MinGW)。
- [x] `cmake/sanitizers.cmake`：opt-in ASan/UBSan（Clang/MinGW），`SF_ENABLE_SANITIZERS=ON` 启用。
- [x] `cmake/deps/{zlib-ng,zstd,mxml,klib,unity}.cmake`：CPM 包定义。Unity 已在 Phase 0 接入；其他四个为后续阶段预占（`include_guard(GLOBAL)` 防重）。
- [x] `.clang-format`（LLVM 基底 + 4 空格 + 列宽 100 + 指针右靠）。
- [x] `.editorconfig`、`.gitignore`、`LICENSE` (GPL-3.0 全文 674 行)、`README.md`、`.clangd`（跨编译 LSP 配置）。
- [x] CI（GitHub Actions）：`windows-latest` × {MSVC, clang-cl + ASAN, MinGW-w64 via msys2} 矩阵 + `ubuntu-24.04` MinGW-w64 cross-compile 仅 build sanity。
- [x] Unity 接入（`unity_runner.c` 4 测试）：`test_sf_result_str_basic`、`test_sf_default_allocator_roundtrip`、`test_bcrypt_aes_provider_reachable`（BCrypt smoke）、`test_sf_last_error_detail_stub`。
- [x] 公共头文件 `include/souls_formats/souls_formats.h` (umbrella) + `sf_common.h`（`SF_API` 宏 + `sf_result_t` 14 枚举值 + `sf_allocator_t` + `sf_default_allocator()`）。
- [x] 源文件 `src/core/error.c`（`sf_result_str` 用静态查表 + `_Static_assert` 守护表/枚举对齐 + 默认 malloc/realloc/free 分配器）。
- [x] **退出验证**（WSL2 实测全绿）：
  - 配置：`cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug` ✅
  - 构建：`cmake --build build-mingw` → 8/8 编译/链接成功，无 warning（`-Werror` 严格通过）✅
  - 产物：`libsouls_formats.a` (静态) + `libsouls_formats.dll` + `libsouls_formats.dll.a` (导入库) + `souls_formats_test_smoke.exe` (515 KB)。`objdump -p` 验证 DLL 仅导出三个 SF_API 符号：`sf_default_allocator` / `sf_last_error_detail` / `sf_result_str` ✅
  - 测试：`ctest --test-dir build-mingw -V` → `4 Tests 0 Failures 0 Ignored OK`（PE 通过 WSL interop 直接执行，BCryptOpenAlgorithmProvider 返回 STATUS_SUCCESS）✅

### Phase 1 — 运行时基础设施（预估 1.5 周）✅ 完成 (2026-05-10)
**Completion Retrospective**: Completed. See `docs/roadmap/phase-1-runtime.md`. Wave 2 alignment fixes: BinaryReaderEx, BinaryWriterEx, PathHelper, HashHelper, SFEncoding, Math, Oodle enums.
- [x] `sf_result_t` + `sf_result_str` + `sf_last_error_detail`（Phase 0 已完成）。
- [x] `sf_allocator_t` + 默认 malloc/free 实现（Phase 0 已完成）+ `sf_free()` 通用释放器。
- [x] `sf_istream_t` / `sf_ostream_t`：Win32 `CreateFileW` 文件后端 + 内存后端 + UTF-8 路径辅助；ostream 支持 detach buffer。
- [x] `sf_binary_reader_t`：覆盖上游 `BinaryReaderEx` 全部读 API
  - 基本类型：bool/i8/u8/i16/u16/i32/u32/i64/u64/f32/f64/varint
  - Get*（绝对偏移读不移位 cursor）、Assert*（断言期望值）
  - 字节序切换：`sf_binary_reader_set_big_endian(r, bool)` / `set_varint_long`
  - 字符串：`read_ascii` / `read_ascii_n` / `read_shift_jis` / `read_shift_jis_n` / `read_utf16` (LE/BE 跟随字节序) / `read_fix_str` / `read_fix_str_w` / `assert_ascii`
  - StepIn / StepOut 偏移栈（任意嵌套深度）
  - Pad / Pad_relative / Skip / Position / Length / Remaining
  - 颜色：`read_argb / abgr / rgba / bgra`
  - 向量：`read_vec2/3/4 / quat / vec3_11_11_10`
  - `assert_pattern`（任意长度模式断言）
- [x] `sf_binary_writer_t`：对应 `BinaryWriterEx`
  - 全部 primitive `write_*`，对称 LE/BE
  - **Reserve / Fill 占位回填**（u32 / i32 / u64 / i64 / varint）：与上游 `(name, typeName)` 双键检索一致，未填充时 `sf_binary_writer_finish` 返回 `SF_ERR_INTERNAL`
  - StepIn/Out + Pad/Pad_byte/Pad_relative
  - 字符串 (`write_ascii` / `write_shift_jis` / `write_utf16` / `write_fix_str` / `write_fix_str_w`) + 颜色 + 向量
- [x] `src/core/encoding_win32.c`：通过 Win32 `MultiByteToWideChar` / `WideCharToMultiByte` 实现 ASCII / Shift-JIS (CP 932) / UTF-16 LE / UTF-16 BE ↔ UTF-8 全部六向转换。
- [x] `include/souls_formats/sf_math.h`：纯 POD `sf_vec2/3/4_t / sf_quat_t / sf_mat4_t / sf_color_t`。
- [x] `src/core/filename_hash.c`：上游 `HashHelper.FromPathHash` 等价实现，对 16 个 golden 路径与 Python 参考逐字节匹配。
- [x] **QA 场景**（必跑）— **实测全绿 (2026-05-10)**：
  - **工具**：cmake 4.3.2 / ninja 1.11.1 / `x86_64-w64-mingw32-gcc-posix` 13 / ctest / WSL interop
  - **命令**：
    ```bash
    cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
    cmake --build build-mingw
    ctest --test-dir build-mingw -L core --output-on-failure
    ```
  - **实测结果**（5/5 测试 PASS，0 失败 0 跳过）：
    - `souls_formats_test_smoke.exe` (Phase 0)：4 sub-case PASS — sf_result_str / 默认分配器 / BCrypt AES provider / sf_last_error_detail。
    - `souls_formats_test_filename_hash.exe`：5 sub-case PASS — 16 个 golden 路径全匹配上游 `HashHelper.FromPathHash`（含 ER 真实路径如 `/chr/c0000.chrbnd` → `0xbb893839`、`/N:/GR/data/Param/Item.param` → `0x9bb75e55`）；NULL 输入、大小写折叠、斜杠归一化、自动前置斜杠四类不变量验证。
    - `souls_formats_test_encoding.exe`：11 sub-case PASS — ASCII/Shift-JIS/UTF-16 LE/UTF-16 BE 全六向转换，含日文 `エルデンリング` (Shift-JIS 14 字节) ↔ UTF-8 round-trip + 中文 `黑暗之魂` (UTF-16 LE/BE 8 字节) ↔ UTF-8 round-trip。
    - `souls_formats_test_binary_reader.exe`：20 sub-case PASS（共计 ~70 个 TEST_ASSERT_*）— LE/BE primitives、endian flip 中流切换、varint 短/长、StepIn/Out 三层嵌套、Pad/Skip、Get*、Assert*（u32/pattern/ASCII）、ASCII/Shift-JIS/UTF-16 LE/BE 终止字符串、fix_str、vec3+quat、ARGB/ABGR/RGBA/BGRA、11_11_10 packed vec3、truncation 与 invalid bool 错误路径。
    - `souls_formats_test_binary_writer.exe`：14 sub-case PASS — LE/BE primitives、Reserve/Fill 三种宽度（u32/u64/varint 短+长）、未填充阻塞 finish、name 重复检测、StepIn/Out、Pad 0x00 与 0xFF、ASCII 终止字符串、Shift-JIS 日文写出、fix_str、writer→reader 完整 round-trip。
  - **DLL 导出**：`libsouls_formats.dll` 导出 137 个 `sf_*` 公共符号，无内部符号泄露。
  - **跳过条件**：无（基础设施全部必跑）。

### Phase 2 — 压缩与加密（预估 2 周）✅ 完成 (2026-05-10)
**Completion Retrospective**: Completed. See `docs/roadmap/phase-2-compression-crypto.md`. Wave 3 alignment fixes: DCX tagged union, ZlibHelper, ZstdHelper, SFUtil, RegulationDecryptor, SL2Decryptor.
**压缩：**
- [x] `sf_dcx.h` 公共类型：`sf_dcx_type_t`（None/Zlib/DCP_EDGE/DCX_EDGE/DCP_DFLT/DCX_DFLT/DCX_KRAK/DCX_ZSTD/DCX_KRAK_MAX 别名）。
- [x] `sf_dcx_decompress(in, in_size, out_buf, out_size, *type)` + `sf_dcx_compress(...)`。
- [x] `sf_dcx_sniff(buf, size, *type)`：仅嗅探不解压。
- [x] zlib-ng 封装：raw deflate + zlib wrapper，双向。
- [x] DCP_EDGE / DCX_EDGE 分块解压器（对照上游 `DCX.cs` 244–374 行手写）。
- [x] libzstd 封装：单 frame 解压 / 压缩。
- [x] **Oodle 加载器**：
  - 启动时按顺序尝试 `oo2core_9_win64.dll` → `oo2core_8_win64.dll` → `oo2core_6_win64.dll`。
  - 搜索路径：进程目录 → 通过 `sf_oodle_set_search_path` 配置的额外目录 → 当前 `PATH`。
  - 缺失时 `sf_dcx_decompress` 在遇到 KRAK 流时返回 `SF_ERR_OODLE_NOT_FOUND`，**绝不**自动 fallback 到内置实现（避免 GPL 风险与正确性争议）。
  - 入口：`OodleLZ_Decompress` / `OodleLZ_Compress` / `OodleLZ_GetCompressedBufferSizeNeeded` / `OodleLZ_CompressOptions_GetDefault` / `OodleLZ_GetDecodeBufferSize`。
- [x] `sf_oodle_set_search_path(const wchar_t*)` 公共 API，便于工具显式指向游戏目录。

**加密：**
- [x] `crypto/aes_cng.c`：AES-128/256-ECB/CBC over BCrypt。
- [x] `crypto/md5_cng.c`：MD5 over BCrypt。
- [x] `crypto/regulation.c`：DS3 / ER / AC6 / Nightreign 的 regulation.bin 解 / 加密。
- [x] `crypto/sl2.c`：SL2 存档解 / 加密。
- [x] **QA 场景**：
  - **工具**：cmake / ninja / ctest / WSL interop / Oodle DLL（`/home/soar/dev/oodle/`）/ ER 副本（`/mnt/c/Games/ELDEN RING/`）
  - **命令**：
    ```bash
    cmake --build build-mingw --target souls_formats_test_compression souls_formats_test_crypto
    ctest --test-dir build-mingw -L 'compression|crypto' --output-on-failure
    ```
  - **预期 — 压缩**：
    - `test_dcx_dflt.exe`：合成 1 KB / 16 KB / 1 MB 三档随机 + 高熵 + 全零 buffer，zlib 解压/压缩 round-trip 字节级一致 → PASS。
    - `test_dcx_edge.exe`：上游 `DCX.cs` 244–374 行的 chunk 结构合成最小 fixture（2 chunk × 8 KB），round-trip → PASS。
    - `test_dcx_zstd.exe`：libzstd 单 frame round-trip → PASS。
    - `test_dcx_krak.exe`：本测试**不依赖**任何游戏文件。SetUp 调 `sf_oodle_set_search_path(L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle")`；准备 4 KB / 64 KB / 1 MB 三档随机+全零+高熵 buffer，调 `sf_dcx_compress(buf, size, SF_DCX_TYPE_KRAK, &out, &out_size)` 再 `sf_dcx_decompress(out, out_size, &back, &back_size, &type)`，断言 `back == buf` 字节级一致且 `type == SF_DCX_TYPE_KRAK` → PASS。再单独构造一个**仅 4 字节** KRAK magic（`KRAK`）的合成 fixture 跑 `sf_dcx_sniff` → 类型判定为 KRAK → PASS。卸载 DLL（`sf_oodle_unload`）后再调，返回 `SF_ERR_OODLE_NOT_FOUND` → PASS。
    - 真实游戏 KRAK 流的端到端验证**推迟到 Phase 3** —— 由 `test_bhd5_e2e_er` 在 Data0.bhd/bdt 提取流程中**隐式**走完 BHD5+AES+DCX+KRAK+Oodle 全链路。
    - `test_dcx_sniff.exe`：对每个合成 fixture 嗅探类型，与构造时类型一致 → PASS。
  - **预期 — 加密**：
    - `test_aes_kat.exe`：NIST CAVP `AES128-ECB.rsp` / `AES128-CBC.rsp` / `AES256-CBC.rsp` 抽样 ≥20 向量，全部一致 → PASS。
    - `test_md5_kat.exe`：RFC 1321 附录 A.5 全部 7 向量一致 → PASS。
    - `test_regulation_decrypt.exe`：读取 `/mnt/c/Games/ELDEN RING/Game/regulation.bin`（实测 ~2.0 MB，头 16 字节为 IV：`00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00`，其后是 AES-256-CBC 密文）；用内嵌 ER known key 解密；inner 字节流前 4 字节应为 `BND4` magic → PASS。再加密回写并字节级 round-trip → PASS。
  - **跳过条件**：
    - `~/dev/oodle/oo2core_6_win64.dll` 缺失 → `test_dcx_krak` SKIP（`TEST_IGNORE_MESSAGE("oodle dll missing")`）。
    - `/mnt/c/Games/ELDEN RING/` 缺失 → `test_regulation_decrypt`、`test_dcx_krak` SKIP。

### Phase 3 — 档案容器（预估 2 周）✅ **DONE 2026-05-10 — 32/32 PASS across 12 test binaries**
- [x] `sf_binder.h`：共享 `sf_binder_file_t { id, name(UTF-8), data, size, flags, compressed_size }`。
- [x] `sf_bnd3.{h,c}`：读 + 写，DCX 自动识别 / 重新打包。
- [x] `sf_bnd4.{h,c}`：同上，含 ER/AC6 的 unicode 名称、hash table、长 ID 表。
- [x] `sf_bxf3.{h,c}` / `sf_bxf4.{h,c}`：分离的 .bhd + .bdt。
- [x] `sf_bhd5.{h,c}`：带 AES 范围加密 + 32 字节 salted SHA blob 解析（仅存储不计算，与上游一致）。
- [x] `sf_tpf.{h,c}`：DDS / platform-specific texture container；不解 DDS 像素数据（透传）。
- [x] `sf_enfl.{h,c}`：load screen preload list（zlib 压缩负载）。
- [x] 示例：`examples/sf_bnd_extract.c`。
- [x] **QA 场景**：
  - **工具**：cmake / ninja / ctest / WSL interop / ER 副本
  - **命令**：
    ```bash
    cmake --build build-mingw --target souls_formats_test_archive
    ctest --test-dir build-mingw -L archive --output-on-failure
    ```
  - **预期 — 合成 fixture**（每个 PASS）：
    - `test_bnd3_synthetic.exe`：3 个 entry（id 100/200/300，名 `a.txt`/`b.bin`/`c.dat`）的最小 BND3 v1.0，round-trip 字节级一致。
    - `test_bnd4_synthetic.exe`：3 entry + Unicode 名 `日本.bin`、64-bit ID、hash table 的 BND4 ER 风格头，round-trip 字节级一致。
    - `test_bxf3_synthetic.exe` / `test_bxf4_synthetic.exe`：BHD 头 + BDT 数据分离，round-trip 后两文件均字节级一致。
    - `test_bhd5_synthetic.exe`：1 bucket × 2 file，含一个 AES-128-ECB 加密区段（256 字节），round-trip 字节级一致。
    - `test_tpf_synthetic.exe`：2 个最小 8×8 BC1 DDS 文件 round-trip。
    - `test_enfl_synthetic.exe`：5 项 + zlib 压缩 payload round-trip。
  - **预期 — ER e2e**（注：ER 把所有资源都打包进 `Data0-3.bhd/bdt` + `DLC.bhd/bdt`，**不存在松散 `.dcx`**；e2e 必须经 BHD5 解包）：
    - `test_bhd5_e2e_er.exe`（最关键，下游所有 e2e 的基石）：
      1. 打开 `/mnt/c/Games/ELDEN RING/Game/Data0.bhd` (~1.0 MB) + `Data0.bdt` (~10.9 GB)。
      2. ER known AES-128-ECB key（hex 常量内嵌于 `archive/bhd5_keys.c`）解密 BHD5 加密区段。
      3. 列出文件计数 > 1000、bucket 数 > 0 → PASS。
      4. 用内置 `HashHelper.FromPathHash` 计算 `/chr/c0000.chrbnd.dcx` 的 path hash，在 BHD5 中查到 entry → PASS。
      5. 从 BDT 中读取该 entry 原始字节（DCX 包），调 `sf_dcx_sniff` 判定为 `SF_DCX_TYPE_KRAK` → PASS。
      6. 调 `sf_dcx_decompress` (前置 `sf_oodle_set_search_path`)，解压字节流以 `BND4` magic 开头 → PASS。**此一测试隐式验证 BHD5 + AES + Path Hash + DCX + KRAK + Oodle 全链路**。
    - `test_bnd4_e2e_er.exe`：建立在 `er_test_helper::extract_from_data0("/chr/c0000.chrbnd.dcx")` 之上，得到 BND4 字节流；解析后 entry 数 ≥ 5；存在名为 `c0000.flver` 的 entry，其 size > 100 KB → PASS。
    - `test_bxf4_e2e_er.exe`：从 Data0 提取任一 `.tpfbhd` (路径如 `/parts/wp_a_0010.tpfbhd`) + 同名 `.tpfbdt`，作为 BXF4 解析；文件计数 > 0 → PASS。若 ER Data0 未含独立的 .tpfbhd/.tpfbdt 对（部分版本将贴图直接 inline 进 BND4），SKIP 并日志记录。
    - `test_tpf_e2e_er.exe`：从 BND4 提取的 `.tpf`（如 `c0000_0010.tpf`）解析；DDS 头 magic == `'DDS '` → PASS。
  - **测试辅助库**（在 `tests/e2e/er_test_helper.{h,c}` 中实现）：
    ```c
    // 单例：打开 Data0.bhd + Data0.bdt（首次调用时初始化）
    sf_result_t er_helper_init(void);
    // 通过 BHD5 路径哈希提取 entry，自动 DCX 解包，返回堆缓冲（caller 释放）
    sf_result_t er_extract_from_data0(const char *bhd5_path_utf8,
                                      void **out, size_t *out_size);
    void        er_helper_shutdown(void);
    ```
  - **跳过条件**：
    - `/mnt/c/Games/ELDEN RING/Game/Data0.bhd` 或 `Data0.bdt` 缺失 → 所有 `*_e2e_er` SKIP。
    - `~/dev/oodle/oo2core_6_win64.dll` 缺失 → 所有依赖 KRAK 的 e2e（即所有 ER e2e）SKIP。

### Phase 4 — 参数与文本（预估 1.5 周） ✅ 完成 (2026-05-11) — 20/20 PASS across 20 test binaries
- [x] `sf_param.{h,c}`：行/单元 / DataVersion / 字节序自动识别。
- [x] `sf_paramdef.{h,c}`：二进制读 + 写，含字段类型、位偏移、单元枚举。
- [x] `paramdef_xml_read.c`：通过 mxml 反序列化 Paramdex 风格 XML 到 `sf_paramdef_t`。
  - **不在 v1 内**：XML 写出（推迟到 v1.1）。
- [x] `sf_param_apply_paramdef(param, defs, count, careful)`：对应上游 `ApplyParamdefCarefully`。
- [x] `sf_paramtdf.{h,c}`：参数 enum 友好名。
- [x] `sf_fmg.{h,c}`：含可选 MD5 头校验。
- [x] 示例：`examples/sf_param_dump.c`。
- [x] **QA 场景**：
  - **工具**：cmake / ninja / ctest / mxml / Paramdex（`/home/soar/dev/paramdex/`）/ ER 副本
  - **命令**：
    ```bash
    cmake --build build-mingw --target souls_formats_test_param
    ctest --test-dir build-mingw -L param --output-on-failure
    ```
  - **预期 — 合成 fixture**：
    - `test_param_synthetic.exe`：3 行（id=100/200/300）× 5 字段（u8/u16/u32/f32/fixstr16）round-trip 字节级一致 → PASS。
    - `test_paramdef_binary.exe`：自构造一个 ER 风格 PARAMDEF（10 字段，混合 bit-aligned）round-trip 字节级一致 → PASS。
    - `test_fmg_synthetic.exe`：5 字符串（含日文 `エルデンリング` 与中文 `黑暗之魂`）round-trip 字节级一致 → PASS。
  - **预期 — ER e2e（PARAMDEF XML via Paramdex）**：
    - `test_paramdef_xml_e2e.exe`：加载 `/home/soar/dev/paramdex/ER/Defs/SpEffect.xml`（实测文件名为 `SpEffect.xml`，不是 `SpEffectParam.xml`）；解析后 `ParamType == "SP_EFFECT_PARAM_ST"`、`Index == 86`、`DataVersion == 4`、`Unicode == true`、字段定义数 ≥ 100 → PASS。
    - `test_param_apply_paramdef_e2e.exe`：完整链路 = (a) 读 `regulation.bin` → (b) AES-256-CBC 解密（用 ER known key，内嵌于 `crypto/regulation_keys.c`）→ (c) 解析为 BND4 → (d) 通过 entry 名 `param/GameParam/SpEffectParam.param` 查找 → (e) 解析为 PARAM → (f) 用 `SpEffect.xml` `ApplyParamdefCarefully`。断言：row 数 ≥ 100；ParamType 字段匹配 → PASS。**注：ER 中所有 .param 在 BND4 内部不再 DCX 包装，所以这条链路不依赖 Oodle**。
    - `test_fmg_e2e_er.exe`：经 `er_extract_from_data0("/msg/engus/item.msgbnd.dcx")` 取出 BND4 → 找 `ItemName.fmg` → 按已知 itemId（如 1030000，社区已知 "短剑"）查得字符串非空且为日文/英文之一 → PASS。**依赖 Phase 3 helper**。
  - **跳过条件**：
    - `/home/soar/dev/paramdex/ER/Defs/SpEffect.xml` 缺失 → `test_paramdef_xml_e2e`、`test_param_apply_paramdef_e2e` SKIP。
    - `/mnt/c/Games/ELDEN RING/Game/regulation.bin` 缺失 → `test_param_apply_paramdef_e2e` SKIP。
    - Phase 3 `er_extract_from_data0` 未通过 → `test_fmg_e2e_er` SKIP（依赖前置阶段）。

### Phase 5 — 脚本与地图（预估 3 周） ✅ 完成 (2026-05-12) — 5/5 PASS across 32 test binaries
- [x] `sf_emevd.{h,c}`：事件脚本字节码读写（DS3/Sekiro/ER/AC6 可能略有差异，需要按 game 分支）。
- [x] `sf_esd.{h,c}`：状态机读写。
- [x] `sf_msb.h`：公共 `sf_msb_part_t / sf_msb_region_t / sf_msb_event_t / sf_msb_route_t / sf_msb_layer_t / sf_msb_model_t` 抽象。
- [x] `msb_common.c`：基类 entry list 读写骨架。
- [x] `sf_msbs.{h,c}` (Sekiro)。
- [x] `sf_msbe.{h,c}` (Elden Ring + Nightreign)。
- [x] `sf_msbvi.{h,c}` (AC6)。
- [x] **QA 场景**：
  - **工具**：cmake / ninja / ctest / WSL interop / ER 副本
  - **命令**：
    ```bash
    cmake --build build-mingw --target souls_formats_test_script souls_formats_test_map
    ctest --test-dir build-mingw -L 'script|map' --output-on-failure
    ```
  - **预期 — 合成 fixture**：
    - `test_emevd_synthetic.exe`：1 event × 1 instruction round-trip → PASS。
    - `test_esd_synthetic.exe`：2 state × 1 transition round-trip → PASS。
    - `test_msbe_synthetic.exe` / `test_msbs_synthetic.exe` / `test_msbvi_synthetic.exe`：1 Part + 1 Region + 1 Event + 1 Model 的最小 MSB round-trip → PASS。
  - **预期 — ER e2e**（全部经 Phase 3 `er_extract_from_data0` 提取，**不存在松散文件**）：
    - `test_emevd_e2e_er.exe`：经 `er_extract_from_data0("/event/m60_42_36_00.emevd.dcx")` 提取（Limgrave 中段事件文件，BHD5 路径哈希内置已知存在）→ 解析事件数 > 50、event id 4234（社区文档已知存在）能找到 → PASS。
    - `test_msbe_e2e_er.exe`：经 `er_extract_from_data0("/map/mapstudio/m60_42_36_00.msb.dcx")` 提取 → 解析 Part count > 0、Region count > 0、Event count > 0；任一 Part 的 BoundingBox 非全零 → PASS。
    - `test_esd_e2e_er.exe`：经 `er_extract_from_data0("/script/talk/m10_00_00_00.talkesdbnd.dcx")` 提取 BND4 → 找任一 `.esd` → state count > 0 → PASS。
  - **跳过条件**：
    - Phase 3 `er_extract_from_data0` 不可用（即 Phase 3 e2e 整体 SKIP）→ 本阶段全部 ER e2e 自动级联 SKIP。
    - `/mnt/c/Games/Sekiro/` 缺失 → `test_msbs_e2e` SKIP（v1 仅占位，无文件即略过）。
    - `/mnt/c/Games/ARMORED CORE VI FIRES OF RUBICON/` 缺失 → `test_msbvi_e2e` SKIP。
    - `/mnt/c/Games/ELDEN RING NIGHTREIGN/` 缺失 → `test_msbe_e2e_nightreign` SKIP（Nightreign 复用 MSBE）。

### Phase 6 — 几何与材质 ✅ 完成 (2026-05-12) — 15/15 PASS across 8 test binaries
- [x] `sf_flver.h`：公共顶点元素枚举 / 半浮点 / 11_11_10 / 法线打包工具。
- [x] `sf_flver2.{h,c}`：
  - [x] Mesh / Vertex Buffer / Vertex Element Layout / Bone / Material / Texture / Dummy / Bounding Box。
  - [x] **顶点格式表**（`flver2_vertex.c`）覆盖 Sekiro / ER / AC6 用到的全部 layout type。
  - [x] 顶点解码：对消费者可选地展开为 host-friendly 顶点结构（`sf_flver2_decode_mesh`）。
- [x] `sf_mtd.{h,c}`（Sekiro 用）。
- [x] `sf_matbin.{h,c}`（ER / AC6 / Nightreign 用）。
- [x] **QA 场景**：
  - **工具**：cmake / ninja / ctest / WSL interop / ER 副本 / Oodle DLL
  - **命令**：
    ```bash
    cmake --build build-mingw --target souls_formats_test_geom
    ctest --test-dir build-mingw -L geom --output-on-failure
    ```
  - **预期 — 合成 fixture**：
    - `test_flver2_synthetic.exe`：1 mesh × 1 material × 8 顶点 × 12 索引（标准 cube）的最小 FLVER2，round-trip 字节级一致 → PASS。
    - `test_mtd_synthetic.exe`：3 param × 2 sampler 的最小 MTD round-trip → PASS。
    - `test_matbin_synthetic.exe`：5 param × 3 sampler 的最小 MATBIN round-trip → PASS。
  - **预期 — ER e2e**（全部经 Phase 3 `er_extract_from_data0` 提取，**不存在松散文件**）：
    - `test_flver2_e2e_er.exe`：经 `er_extract_from_data0("/chr/c0000.chrbnd.dcx")` 提取 → BND4 → 找 `c0000.flver` → 解析；mesh count > 0、bone count > 0、material count > 0；所有 vertex layout type 在内置注册表中（无 `SF_ERR_UNSUPPORTED_VERSION`）→ PASS。
    - `test_matbin_e2e_er.exe`：经 `er_extract_from_data0("/material/allmaterial.matbinbnd.dcx")` 提取 → BND4 → 取任一 `.matbin`；shader 名非空（`ER_*.spx` 风格）、param 表非空 → PASS。
  - **特殊状态**：
    - 若发现未知 vertex layout type → 记录为 `KNOWN_LAYOUT_GAP`，单元测试 PASS 但日志记录待补；不阻塞 Phase 6 退出。
  - **跳过条件**：
    - Phase 3 `er_extract_from_data0` 不可用 → 本阶段全部 ER e2e 自动级联 SKIP。

### Phase 7 — 动画与特效（v1 可选 / v1.1 默认） ✅ 完成 (2026-05-12) — 5/5 PASS across 5 test binaries
- [x] TAE3 / TAE4。
- [x] FXR3（含 mxml 读写双向）。
- [x] **QA 场景**（仅当本阶段在 v1 内才跑；v1.1 时默认启用）：
  - **工具**：cmake / ninja / ctest / mxml / ER 副本 / Oodle DLL
  - **命令**：
    ```bash
    cmake --build build-mingw --target souls_formats_test_anim
    ctest --test-dir build-mingw -L anim --output-on-failure
    ```
  - **预期 — 合成 fixture**：
    - `test_tae_synthetic.exe`：1 anim × 1 event 的最小 TAE round-trip → PASS。
    - `test_fxr3_synthetic.exe`：最小 FXR3 树 round-trip 二进制一致 → PASS；mxml 写出后再读回，字段匹配 → PASS。
  - **预期 — ER e2e**：
    - `test_tae_e2e_er.exe`：经 `er_extract_from_data0("/chr/c0000.anibnd.dcx")` 提取 → BND4 → 任一 `.tae`，anim count > 0 → PASS。
    - `test_fxr3_e2e_er.exe`：经 `er_extract_from_data0("/sfx/sfxbnd_commoneffects.ffxbnd.dcx")` 提取 → BND4 → 任一 `.fxr`，节点数 > 0 → PASS。
  - **跳过条件**：v1.0 默认整段 SKIP（CMake `SF_ENABLE_PHASE7=OFF` 时不进 build）；v1.1 启用时按上规则。

> 如果时间紧，整个 Phase 7 推迟到 v1.1，v1.0 标记为 "geometry-complete"。CMake 选项 `-DSF_ENABLE_PHASE7=ON/OFF` 控制 v1.0 中是否启用。

### Phase v2.0 — 旧世代支持
- DS3 / DS2 / DS1 / Bloodborne / Demon's Souls 全格式。
- FLVER0、MSB1/2/3/B/D、所有遗留容器。
- 单独立计划文档 `PLAN_v2.md` 在 v1.0 GA 后撰写。

---

## 8. 测试策略

### 8.1 三层金字塔
1. **单元测试 (Unity)**：纯计算 / 编码 / 字节序 / DCX 子流程，跑得快、必跑。
2. **合成 fixture round-trip**：库内自构造的最小合规样本（每格式 ≤ 4 KB），全部入库。
3. **真实游戏 e2e**：使用开发者本地游戏目录的硬编码路径（详见 §8.4），CI **不**跑这一层（涉及版权），仅开发机本地跑。

### 8.2 模糊测试
- 用 libFuzzer (clang-cl) 对每个 `sf_*_read_from_memory` 写一个 fuzz harness。
- 入口在 `tests/fuzz/`，独立 CMake target，**不**进默认构建。
- 期望：≥ 24 小时无 crash / leak。

### 8.3 静态分析
- MSVC `/analyze`、clang-tidy 在 CI 跑（warnings 不强制）。
- `--sanitize=address,undefined` 在 clang-cl/MinGW build 跑。

### 8.4 测试数据 — 硬编码路径

**测试代码不读取任何环境变量**。以下路径在 `tests/e2e/CMakeLists.txt` 中通过 `target_compile_definitions` 烘焙为预处理宏注入 e2e 二进制：

| 项 | 编译时 / WSL 视角 | 运行时 / Win32 视角 | 状态 |
|---|---|---|---|
| Elden Ring 游戏目录 | `/mnt/c/Games/ELDEN RING` | `C:/Games/ELDEN RING` | ✅ 用户已提供 |
| Sekiro 游戏目录 | `/mnt/c/Games/Sekiro` | `C:/Games/Sekiro` | ⏳ 暂未提供 → 测试 SKIP |
| AC6 游戏目录 | `/mnt/c/Games/ARMORED CORE VI FIRES OF RUBICON` | `C:/Games/ARMORED CORE VI FIRES OF RUBICON` | ⏳ 暂未提供 → 测试 SKIP |
| Nightreign 游戏目录 | `/mnt/c/Games/ELDEN RING NIGHTREIGN` | `C:/Games/ELDEN RING NIGHTREIGN` | ⏳ 暂未提供 → 测试 SKIP |
| Oodle DLL 目录 | `/home/soar/dev/oodle/` | `\\wsl.localhost\Ubuntu\home\soar\dev\oodle` | ✅ 用户承诺放 `oo2core_6_win64.dll` (Sekiro/ER) 与 `oo2core_9_win64.dll` (AC6/Nightreign) |
| Paramdex 仓库 | `/home/soar/dev/paramdex/` | `\\wsl.localhost\Ubuntu\home\soar\dev\paramdex` | ✅ 用户已克隆 soulsmods/Paramdex |

**注**：因为最终二进制是 Windows PE，所有路径常量以 **Win32 视角**形式写入二进制。文件 API 调用使用 `wchar_t`，`L"C:/Games/ELDEN RING"` 风格（正斜杠 Win32 接受），UNC 路径用反斜杠 `L"\\\\wsl.localhost\\..."`。

**实现机制示例**（Phase 0 落地时入仓）：
```cmake
# tests/e2e/CMakeLists.txt
target_compile_definitions(souls_formats_e2e PRIVATE
    SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
    SF_E2E_SEKIRO_DIR=L"C:/Games/Sekiro"
    SF_E2E_AC6_DIR=L"C:/Games/ARMORED CORE VI FIRES OF RUBICON"
    SF_E2E_NIGHTREIGN_DIR=L"C:/Games/ELDEN RING NIGHTREIGN"
    SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle"
    SF_E2E_PARAMDEX_DIR=L"\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\paramdex"
)
```

**目录存在性检查**：每个游戏 e2e 在 `setUp` 时用 `GetFileAttributesW` 检查根目录存在；不存在则 `TEST_IGNORE_MESSAGE("game dir not found, skipping")`，不算失败。

### 8.5 入仓 / 不入仓划分
| 路径 | 入仓 | 来源 |
|---|---|---|
| `tests/fixtures/synthetic/` | ✅ 入仓 | 我们手写的最小合规字节样本（每格式 ≤ 4 KB） |
| 真实游戏文件 | ❌ 不入仓 | §8.4 硬编码路径，开发者本地合法持有 |
| `oo2core_*_win64.dll` | ❌ 不入仓 | 同上 |
| Paramdex XML | ❌ 不入仓 | 用户本地 clone，硬编码路径 |

### 8.6 v1 GA 的 e2e 覆盖范围
**当前用户仅提供 Elden Ring 副本**，所以 v1 GA 时：
- ✅ Elden Ring 走完整 e2e：DCX_KRAK / BND4 / BXF4 / BHD5 / TPF / PARAM / PARAMDEF / PARAMTDF / FMG / EMEVD / ESD / MSBE / FLVER2 / MTD / MATBIN / ENFL。
- ⏳ Sekiro / AC6 / Nightreign：代码实现 + 单元 + 合成 fixture 通过，**但缺真实游戏 e2e 校验**；待用户提供副本后补全（按 §8.4 表中预定路径）。
- 这一不对称在 README 显眼处标注，不视为阻塞 v1 GA。

---

## 9. CI 策略

GitHub Actions matrix：

| OS | 工具链 | Build Type | 测试 | 备注 |
|---|---|---|---|---|
| windows-latest | MSVC 2022 (cl) | Debug + RelWithDebInfo | 单元 + 合成 | 主干 canonical build |
| windows-latest | clang-cl 18 | Debug | 单元 + 合成 + ASAN/UBSAN | sanitizer pass |
| windows-latest | MinGW-w64 GCC 14 (msys2) | Release | 单元 + 合成 | 验证 MinGW ABI |
| ubuntu-24.04 | MinGW-w64 cross (apt) | Debug | **仅 build**，不 run | 验证 WSL2 dev loop 的工具链文件不漂移；runner 无 Windows 内核 |

**不在 CI 跑**：
- DCX_KRAK 单元测试（CI runner 无 oo2core DLL）。
- e2e（CI 无合法游戏副本）。
- Fuzz（独立 nightly cron job）。

**镜像本地 WSL2 dev loop 的优势**：开发者本地用 `cmake/toolchain-mingw-w64.cmake` 跑 MinGW cross；CI 上的 Ubuntu MinGW cross 任务保证这个 toolchain 文件持续可用，不被 Windows-side 修改"漂走"。

---

## 10. 许可证

- **GPL-3.0**，强制继承自 SoulsFormatsNEXT。
- 第三方依赖许可证表：

| 依赖 | 许可证 | 兼容 GPLv3 |
|---|---|---|
| zlib-ng | zlib | ✅ |
| libzstd | BSD-3 | ✅ |
| mxml | Apache 2.0 | ✅ |
| klib | MIT | ✅ |
| Unity | MIT | ✅ |
| Win32 / CNG | 系统提供 | ✅ |
| Oodle (运行时 DLL) | 不在源码树 | 不分发 |

> **重要**：我们绝不在仓库或二进制 release 中分发 Oodle DLL。用户须从自己合法持有的 FromSoftware 游戏副本中复制。

---

## 11. 风险与未决问题

| 项 | 风险 | 缓解 |
|---|---|---|
| Oodle 版本探测 | 不同游戏 / 补丁切换 oo2core 大版本，函数签名微变 | 加载器对每个版本 (.6/.8/.9) 维护独立 vtable；探测后选用对应版本的 op |
| API drift over time | upstream advances, our mapping goes stale | re-audit policy in `docs/api-mapping/UPSTREAM.md` (every 50 commits, re-survey core 6 files: BinaryReaderEx, BinaryWriterEx, DCX.cs, RegulationDecryptor.cs, SL2Decryptor.cs, HashHelper.cs, Oodle.cs) |
| FLVER2 顶点 layout 多 | Sekiro / ER / AC6 顶点格式可能上百种排列 | 维护 layout 注册表 + 真实样本回归测试；初期只支持公开样本中出现的 layout，遇未知 layout 报 `SF_ERR_UNSUPPORTED_VERSION` 并打日志 |
| Edge geometry 字段 sneak into BHD 数据 | 文件中含 EdgeCompression flag，且会误进 v1 解析路径 | 文件中含 EdgeCompression flag 时返回 `SF_ERR_UNSUPPORTED_VERSION`，T17/T15 显式分支拒绝 |
| MSB 跨游戏微差 | 同一概念在 MSBE / MSBS / MSBVI 字段次序、可选字段不同 | 用代码生成器从 schema 生成 entry list 解析器（v2 考虑）；v1 手写每个 |
| PARAMDEF 数据来源 | Sekiro 之后的 PARAMDEF 不随游戏发货，需要 Paramdex | 接入 Paramdex 路径作为开发文档，e2e 测试假设其存在 |
| Unicode 文件路径 | FromSoft 资源经常包含日文路径 | 公共 API 内部用 UTF-8，Win32 边界用 wide；提供 `sf_path_*` 帮助函数 |
| GPLv3 与商业用户 | 库 GPLv3 → 静态链接的工具也必须 GPLv3 | README 明确说明；建议用 LGPL 不在选项内（上游已 GPL3） |
| 真实测试数据获取 | 无法入仓；当前仅 Elden Ring 副本可用 | 路径硬编码 (§8.4)；其余三款游戏 e2e 暂以 SKIP 跳过，待用户补齐副本后无须改代码即生效 |

**已锁定决策**（用户 2026-05-10 确认，全部已落入相应章节）：
1. ✅ **STATIC + SHARED 双构建** —— 详 §4 表格 + §5.6 `SF_API`。
2. ✅ **`SF_API` 导出宏** —— 详 §5.6。
3. ✅ **SemVer 0.2.0 起步** [^1]，v1 范围 GA 后升 1.0.0 —— 详 §4 表格。
4. ✅ **后端可替换结构** —— 详 §3.3。v1 仅交付 Windows 后端，但 `crypto/`、`compression/oodle/`、`core/encoding_*` 全部按"单后端单实现集 + CMake 选项切换"方式组织，零成本扩展 Linux/Wine 子集。

---

## 12. 附录 A：上游格式完整对照清单（v2+ 路线图参考）

> 计 80+ 格式，下面按"v1 / v1.x / v2 / v3"划分。

### v1（本计划）

| Format | Mapping doc |
|---|---|
| DCX | `docs/api-mapping/format-dcx.md` |
| BND3 | `docs/api-mapping/format-bnd3.md` |
| BND4 | `docs/api-mapping/format-bnd4.md` |
| BXF3 | `docs/api-mapping/format-bxf3.md` |
| BXF4 | `docs/api-mapping/format-bxf4.md` |
| BHD5 | `docs/api-mapping/format-bhd5.md` |
| TPF | `docs/api-mapping/format-tpf.md` |
| ENFL | `docs/api-mapping/format-enfl.md` |
| PARAM | `docs/api-mapping/format-param.md` |
| PARAMDEF | `docs/api-mapping/format-paramdef.md` |
| PARAMTDF | `docs/api-mapping/format-paramtdf.md` |
| FMG | `docs/api-mapping/format-fmg.md` |
| EMEVD | `docs/api-mapping/format-emevd.md` |
| ESD | `docs/api-mapping/format-esd.md` |
| MSB (common) | `docs/api-mapping/format-msb-common.md` |
| MSBS | `docs/api-mapping/format-msbs.md` |
| MSBE | `docs/api-mapping/format-msbe.md` |
| MSBVI | `docs/api-mapping/format-msbvi.md` |
| FLVER (common) | `docs/api-mapping/format-flver-common.md` |
| FLVER2 | `docs/api-mapping/format-flver2.md` |
| MTD | `docs/api-mapping/format-mtd.md` |
| MATBIN | `docs/api-mapping/format-matbin.md` |

### v1.1

| Format | Mapping doc |
|---|---|
| TAE3 | `docs/api-mapping/format-tae.md` |
| TAE4 | `docs/api-mapping/format-tae.md` |
| FXR3 | `docs/api-mapping/format-fxr3.md` |
| PARAMDEF XML 写出 | `docs/api-mapping/format-paramdef.md` |

### v2.0（DS1/DS2/DS3/BB/DeS）

| Format | Mapping doc |
|---|---|
| FLVER0, MSB1, MSB2, MSB3, MSBB, MSBD, MSBN, MCG, MCP, NVA, NVM, NGP, EDGE, BTAB, BTL, BTPB, GRASS, PMDCL, ACB, FFXDLSE, RMB, DRB, CCM, CLM2, F2TR, EMELD, AIP, FMB, EDD, LUAGNL, LUAINFO | `docs/api-mapping/legacy.md` |

### v3.0（AC4/ACFA/ACV/ACVD、King's Field、Kuon、Otogi、Dreamcast）

| Format | Mapping doc |
|---|---|
| MSBV, MSBVD, MSBFA, MSBAC4, AcParts4, MLB_AC4, MLB_AC5, ANI, FXR1, MQB, NMB, NSA, MDL, MDL0, MDL4, OM2, SMD4, BND2, DVDBND, FSDATA, LDMU, MGF, Zero3, ACE3 BND, AC3SL BND, Kuon BND, FSLIBLZS | `docs/api-mapping/legacy.md` |

---

## 13. 下一步

**当前进度**：Phase 0（脚手架）+ Phase 1（运行时）已完成（2026-05-10），实测全绿。

**下一阶段**：Phase 2（压缩与加密）——
详细实施指引见 [`docs/roadmap/phase-2-compression-crypto.md`](../../docs/roadmap/phase-2-compression-crypto.md)。

**长期路线**：
1. 按 Phase 顺序推进 Phase 2 → 3 → 4 → 5 → 6（v1.0 GA），每个 Phase 收尾时勾选本文件对应 checkbox + 实测数字。
2. v1.0 ship 后再决定 Phase 7 是否在 v1.1 内交付（[`docs/roadmap/phase-7-animation-effects.md`](../../docs/roadmap/phase-7-animation-effects.md)）。
3. v1.0 GA 前**不开始** v2 / v3 的任何代码（[`docs/roadmap/post-v1.md`](../../docs/roadmap/post-v1.md)）。
4. 每次本文件被实质性修改（增减 Phase / 改 QA 契约 / 改硬编码路径）后，重新跑 Momus 审计：
   `task(subagent_type="momus", prompt=".sisyphus/plans/PLAN.md", ...)`。

[^1]: (post-API-realignment minor bump)
