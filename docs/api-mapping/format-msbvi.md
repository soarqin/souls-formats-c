# MSBVI (AC6) API Mapping

**Phase target**: 5
**Dependencies**: [MSB Common](format-msb-common.md), BinaryReaderEx, BinaryWriterEx

## Contributing Files
| File | Path |
|------|------|
| MSBVI.cs | SoulsFormats/Formats/MSB/MSBVI/MSBVI.cs |
| ModelParam.cs | SoulsFormats/Formats/MSB/MSBVI/ModelParam.cs |
| EventParam.cs | SoulsFormats/Formats/MSB/MSBVI/EventParam.cs |
| RouteParam.cs | SoulsFormats/Formats/MSB/MSBVI/RouteParam.cs |
| PointParam.cs | SoulsFormats/Formats/MSB/MSBVI/PointParam.cs |
| PartsParam.cs | SoulsFormats/Formats/MSB/MSBVI/PartsParam.cs |
| LayerParam.cs | SoulsFormats/Formats/MSB/MSBVI/LayerParam.cs |

## Related Formats
- [MSB Common](format-msb-common.md)
- [MSBS (Sekiro)](format-msbs.md)
- [MSBE (Elden Ring)](format-msbe.md)

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public partial class MSBVI : SoulsFile<MSBVI>, IMsb` | MSBVI.cs:12 | Class | 已实现 | 已实现 | Armored Core VI MSB |
| `public abstract class Param<T> where T : Entry` | MSBVI.cs:192 | Class | 已实现 | 已实现 | |
| `public abstract class Entry : IMsbEntry` | MSBVI.cs:282 | Class | 已实现 | 已实现 | |
| `internal enum ModelType : uint` | ModelParam.cs:10 | Enum | 已实现 | 已实现 | |
| `public class ModelParam : Param<Model>, IMsbParam<IMsbModel>` | ModelParam.cs:28 | Class | 已实现 | 已实现 | |
| `public abstract class Model : Entry, IMsbModel` | ModelParam.cs:136 | Class | 已实现 | 已实现 | |
| `internal enum EventType : uint` | EventParam.cs:9 | Enum | 已实现 | 已实现 | |
| `public class EventParam : Param<Event>, IMsbParam<IMsbEvent>` | EventParam.cs:42 | Class | 已实现 | 已实现 | |
| `public abstract class Event : Entry, IMsbEvent` | EventParam.cs:166 | Class | 已实现 | 已实现 | |
| `public class RouteParam : Param<Route>` | RouteParam.cs:10 | Class | 已实现 | 已实现 | |
| `public class Route : NamedEntry` | RouteParam.cs:42 | Class | 已实现 | 已实现 | |
| `public enum RegionType : uint` | PointParam.cs:9 | Enum | 已实现 | 已实现 | |
| `public class PointParam : Param<Region>, IMsbParam<IMsbRegion>` | PointParam.cs:44 | Class | 已实现 | 已实现 | |
| `public abstract class Region : Entry, IMsbRegion` | PointParam.cs:512 | Class | 已实现 | 已实现 | |
| `public enum PartType : uint` | PartsParam.cs:9 | Enum | 已实现 | 已实现 | |
| `public class PartsParam : Param<Part>, IMsbPart` | PartsParam.cs:30 | Class | 已实现 | 已实现 | |
| `public abstract class Part : Entry, IMsbPart` | PartsParam.cs:249 | Class | 已实现 | 已实现 | |
| `public class LayerParam : Param<Layer>` | LayerParam.cs:10 | Class | 已实现 | 已实现 | |
| `public class Layer : NamedEntry` | LayerParam.cs:41 | Class | 已实现 | 已实现 | |
| `MSBVI.Read` | MSBVI.cs | Method | `sf_msbvi_read_from_memory` | 已实现 | |
| `MSBVI.Write` | MSBVI.cs | Method | `sf_msbvi_write_to_memory` | 已实现 | |

