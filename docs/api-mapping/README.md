# API Mapping

This directory contains the row-level mapping between the `souls-formats-c` API and the upstream `SoulsFormatsNEXT` C# implementation. These documents serve as the source of truth for API alignment, documenting every symbol, its upstream counterpart, and any intentional deviations or extensions.

For high-level context, see [UPSTREAM.md](UPSTREAM.md) and the project [POLICY.md](POLICY.md).

## Tier A — Row-level mapping

These documents provide a detailed mapping for each module and format.

- [BinaryReaderEx](util-io-binary-reader-ex.md)
- [BinaryWriterEx](util-io-binary-writer-ex.md)
- [PathHelper](util-io-path-helper.md)
- [Encoding](util-text-sf-encoding.md)
- [HashHelper](util-cryptography-hash-helper.md)
- [RegulationDecryptor](util-cryptography-regulation-decryptor.md)
- [SL2Decryptor](util-cryptography-sl2-decryptor.md)
- [ZlibHelper](util-compression-zlib-helper.md)
- [ZstdHelper](util-compression-zstd-helper.md)
- [Oodle](util-compression-oodle.md)
- [SFUtil](util-sf-util.md)
- [Math](math.md)
- [DCX](format-dcx.md)
- [Binder Common](format-binder-common.md)
- [BND3](format-bnd3.md)
- [BND4](format-bnd4.md)
- [BXF3](format-bxf3.md)
- [BXF4](format-bxf4.md)
- [BHD5](format-bhd5.md)
- [TPF](format-tpf.md)
- [ENFL](format-enfl.md)
- [PARAM](format-param.md)
- [PARAMDEF](format-paramdef.md)
- [PARAMTDF](format-paramtdf.md)
- [FMG](format-fmg.md)
- [EMEVD](format-emevd.md)
- [ESD](format-esd.md)
- [MSB Common](format-msb-common.md)
- [MSBS](format-msbs.md)
- [MSBE](format-msbe.md)
- [MSBVI](format-msbvi.md)
- [FLVER Common](format-flver-common.md)
- [FLVER2](format-flver2.md)
- [MTD](format-mtd.md)
- [MATBIN](format-matbin.md)
- [TAE](format-tae.md)
- [FXR3](format-fxr3.md)
- [BTAB](format-btab.md)
- [BTL](format-btl.md)
- [GPARAM](format-gparam.md)
- [PMDCL](format-pmdcl.md)

## Tier B — Legacy inventory

Inventory of legacy formats and symbols deferred to v2.0.

- [Legacy Inventory](legacy.md)

## Status Legend

| Status | Description |
| :--- | :--- |
| `✓ aligned` | Symbol matches upstream signature and behavior exactly. |
| `~ partial` | Symbol exists but has minor signature or behavior differences. |
| `✗ deviation` | Symbol intentionally differs from upstream for C-idiomatic reasons. |
| `+ extension` | Symbol has no upstream equivalent (see [extensions.md](extensions.md)). |
| `未实现` | Symbol is planned but not yet implemented. |
| `_skipped_` | Symbol is intentionally omitted from this implementation. |

## How to add a new mapping doc

1. Create a new markdown file using the appropriate naming convention.
2. Define the mapping table with columns for the C symbol, C# counterpart, and status.
3. Document any deviations or extensions clearly.
4. Add a link to the new file in this README under the appropriate tier.
5. Update the [drift-checklist.md](drift-checklist.md) if any new misalignments are identified.
