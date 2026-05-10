# MSBS (Sekiro) API Mapping

**Phase target**: 5
**Dependencies**: [MSB Common](format-msb-common.md), BinaryReaderEx, BinaryWriterEx

## Contributing Files
| File | Path |
|------|------|
| MSBS.cs | SoulsFormats/Formats/MSB/MSBS/MSBS.cs |
| ModelParam.cs | SoulsFormats/Formats/MSB/MSBS/ModelParam.cs |
| EventParam.cs | SoulsFormats/Formats/MSB/MSBS/EventParam.cs |
| RouteParam.cs | SoulsFormats/Formats/MSB/MSBS/RouteParam.cs |
| PointParam.cs | SoulsFormats/Formats/MSB/MSBS/PointParam.cs |
| PartsParam.cs | SoulsFormats/Formats/MSB/MSBS/PartsParam.cs |

## Related Formats
- [MSB Common](format-msb-common.md)
- [MSBE (Elden Ring)](format-msbe.md)
- [MSBVI (AC6)](format-msbvi.md)

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public partial class MSBS : SoulsFile<MSBS>, IMsb` | MSBS.cs:10 | Class | 未实现 | 未实现 | Sekiro MSB |
| `public abstract class Param<T> where T : Entry` | MSBS.cs:178 | Class | 未实现 | 未实现 | |
| `public abstract class Entry : IMsbEntry` | MSBS.cs:263 | Class | 未实现 | 未实现 | |
| `internal enum ModelType : uint` | ModelParam.cs:10 | Enum | 未实现 | 未实现 | |
| `public class ModelParam : Param<Model>, IMsbParam<IMsbModel>` | ModelParam.cs:22 | Class | 未实现 | 未实现 | |
| `public abstract class Model : Entry, IMsbModel` | ModelParam.cs:120 | Class | 未实现 | 未实现 | |
| `internal enum EventType : uint` | EventParam.cs:10 | Enum | 未实现 | 未实现 | |
| `public class EventParam : Param<Event>, IMsbParam<IMsbEvent>` | EventParam.cs:43 | Class | 未实现 | 未实现 | |
| `public abstract class Event : Entry, IMsbEvent` | EventParam.cs:233 | Class | 未实现 | 未实现 | |
| `internal enum RouteType : uint` | RouteParam.cs:9 | Enum | 未实现 | 未实现 | |
| `public class RouteParam : Param<Route>` | RouteParam.cs:18 | Class | 未实现 | 未实现 | |
| `public abstract class Route : Entry` | RouteParam.cs:84 | Class | 未实现 | 未实现 | |
| `internal enum RegionType : uint` | PointParam.cs:10 | Enum | 未实现 | 未实现 | |
| `public class PointParam : Param<Region>, IMsbParam<IMsbRegion>` | PointParam.cs:44 | Class | 未实现 | 未实现 | |
| `public abstract class Region : Entry, IMsbRegion` | PointParam.cs:295 | Class | 未实现 | 未实现 | |
| `internal enum PartType : uint` | PartsParam.cs:10 | Enum | 未实现 | 未实现 | |
| `public class PartsParam : Param<Part>, IMsbPart` | PartsParam.cs:25 | Class | 未实现 | 未实现 | |
| `public abstract class Part : Entry, IMsbPart` | PartsParam.cs:154 | Class | 未实现 | 未实现 | |
