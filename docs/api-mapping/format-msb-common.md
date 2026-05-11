# MSB Common API Mapping

**Phase target**: 5
**Dependencies**: BinaryReaderEx, BinaryWriterEx, Math

## Contributing Files
| File | Path |
|------|------|
| MSB.cs | SoulsFormats/Formats/MSB/MSB.cs |
| IMsb.cs | SoulsFormats/Formats/MSB/IMsb.cs |
| FormatReference.cs | SoulsFormats/Formats/MSB/FormatReference.cs |
| MSBReference.cs | SoulsFormats/Formats/MSB/MSBReference.cs |
| MsbBoundingBox.cs | SoulsFormats/Formats/MSB/MsbBoundingBox.cs |
| Shape.cs | SoulsFormats/Formats/MSB/Shape.cs |

## Related Formats
- [MSBS (Sekiro)](format-msbs.md)
- [MSBE (Elden Ring)](format-msbe.md)
- [MSBVI (AC6)](format-msbvi.md)

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public static partial class MSB` | MSB.cs:11 | Class | 已实现 | 已实现 | Common MSB utilities |
| `public class MissingReferenceException : Exception` | MSB.cs:13 | Class | 已实现 | 已实现 | |
| `internal static void AssertHeader(BinaryReaderEx br)` | MSB.cs:26 | Method | 已实现 | 已实现 | |
| `internal static void WriteHeader(BinaryWriterEx bw)` | MSB.cs:37 | Method | 已实现 | 已实现 | |
| `internal static void DisambiguateNames<T>(List<T> entries, string className = "")` | MSB.cs:48 | Method | 已实现 | 已实现 | |
| `internal static string ReambiguateName(string name)` | MSB.cs:78 | Method | 已实现 | 已实现 | |
| `internal static string FindName<T>(List<T> list, int index)` | MSB.cs:83 | Method | 已实现 | 已实现 | |
| `internal static int FindIndex<T>(List<T> list, string name)` | MSB.cs:116 | Method | 已实现 | 已实现 | |
| `public interface IMsb : ISoulsFile` | IMsb.cs:10 | Interface | 已实现 | 已实现 | Generic MSB interface |
| `public interface IMsbBound<TTree> : IMsb` | IMsb.cs:34 | Interface | 已实现 | 已实现 | |
| `public interface IMsbParam<T> where T : IMsbEntry` | IMsb.cs:43 | Interface | 已实现 | 已实现 | |
| `public interface IMsbTreeParam<TTree> where TTree : IMsbTree` | IMsb.cs:59 | Interface | 已实现 | 已实现 | |
| `public interface IMsbEntry` | IMsb.cs:74 | Interface | 已实现 | 已实现 | |
| `public interface IMsbModel : IMsbEntry` | IMsb.cs:83 | Interface | 已实现 | 已实现 | |
| `public interface IMsbEvent : IMsbEntry` | IMsb.cs:94 | Interface | 已实现 | 已实现 | |
| `public interface IMsbRegion : IMsbEntry` | IMsb.cs:104 | Interface | 已实现 | 已实现 | |
| `public interface IMsbPart : IMsbEntry` | IMsb.cs:131 | Interface | 已实现 | 已实现 | |
| `public interface IMsbTree` | IMsb.cs:162 | Interface | 已实现 | 已实现 | |
| `public struct MsbBoundingBox` | MsbBoundingBox.cs:10 | Struct | 已实现 | 已实现 | |
| `public abstract class Shape` | Shape.cs:34 | Class | 已实现 | 已实现 | Region shape base |
| `internal enum ShapeType : uint` | Shape.cs:10 | Enum | 已实现 | 已实现 | |
| `public class Point : Shape` | Shape.cs:70 | Class | 已实现 | 已实现 | |
| `public class Circle : Shape` | Shape.cs:87 | Class | 已实现 | 已实现 | |
| `public class Sphere : Shape` | Shape.cs:132 | Class | 已实现 | 已实现 | |
| `public class Cylinder : Shape` | Shape.cs:177 | Class | 已实现 | 已实现 | |
| `public class Rectangle : Shape` | Shape.cs:230 | Class | 已实现 | 已实现 | |
| `public class Box : Shape` | Shape.cs:283 | Class | 已实现 | 已实现 | |
| `public class Composite : Shape` | Shape.cs:344 | Class | 已实现 | 已实现 | |
| `public class Child` | Shape.cs:403 | Class | 已实现 | 已实现 | Composite shape child |
| `public class PositionProperty : Attribute` | FormatReference.cs:8 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class RotationProperty : Attribute` | FormatReference.cs:13 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class ScaleProperty : Attribute` | FormatReference.cs:18 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class IndexProperty : Attribute` | FormatReference.cs:23 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class MSBEnum : Attribute` | FormatReference.cs:28 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class MSBAliasEnum : Attribute` | FormatReference.cs:34 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class EldenRingAssetMask : Attribute` | FormatReference.cs:40 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class IgnoreProperty : Attribute` | FormatReference.cs:45 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class EnemyProperty : Attribute` | FormatReference.cs:50 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class MSBReference : Attribute` | MSBReference.cs:8 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class MSBParamReference : Attribute` | MSBReference.cs:13 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `public class MSBEntityReference : Attribute` | MSBReference.cs:19 | Class | _skipped_ | _skipped_ | Reflection attribute |
| `internal static void AssertHeader(BinaryReaderEx br)` | MSB.cs:26 | Method | `msb_common_read_header` | 已实现 | |
| `internal static void WriteHeader(BinaryWriterEx bw)` | MSB.cs:37 | Method | `msb_common_write_header` | 已实现 | |
| `internal static void DisambiguateNames<T>(List<T> entries, string className = "")` | MSB.cs:48 | Method | `msb_common_iter_lists` | 已实现 | (Iterates lists to find entries) |

