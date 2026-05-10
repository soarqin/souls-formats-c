# MSBE (Elden Ring) API Mapping

**Phase target**: 5
**Dependencies**: [MSB Common](format-msb-common.md), BinaryReaderEx, BinaryWriterEx

## Contributing Files
| File | Path |
|------|------|
| MSBE.cs | SoulsFormats/Formats/MSB/MSBE/MSBE.cs |
| ModelParam.cs | SoulsFormats/Formats/MSB/MSBE/ModelParam.cs |
| EventParam.cs | SoulsFormats/Formats/MSB/MSBE/EventParam.cs |
| RouteParam.cs | SoulsFormats/Formats/MSB/MSBE/RouteParam.cs |
| PointParam.cs | SoulsFormats/Formats/MSB/MSBE/PointParam.cs |
| PartsParam.cs | SoulsFormats/Formats/MSB/MSBE/PartsParam.cs |

## Related Formats
- [MSB Common](format-msb-common.md)
- [MSBS (Sekiro)](format-msbs.md)
- [MSBVI (AC6)](format-msbvi.md)

## API Mapping

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|--------------------|-------------|------|---------|--------|-------|
| `public partial class MSBE : SoulsFile<MSBE>, IMsb` | MSBE.cs:11 | Class | 未实现 | 未实现 | Elden Ring MSB |
| `public abstract class Param<T> where T : Entry` | MSBE.cs:160 | Class | 未实现 | 未实现 | |
| `public abstract class Entry : IMsbEntry` | MSBE.cs:247 | Class | 未实现 | 未实现 | |
| `internal enum ModelType : uint` | ModelParam.cs:10 | Enum | 未实现 | 未实现 | |
| `public class ModelParam : Param<Model>, IMsbParam<IMsbModel>` | ModelParam.cs:22 | Class | 未实现 | 未实现 | |
| `public abstract class Model : Entry, IMsbModel` | ModelParam.cs:120 | Class | 未实现 | 未实现 | |
| `internal enum EventType : uint` | EventParam.cs:10 | Enum | 未实现 | 未实现 | |
| `public class EventParam : Param<Event>, IMsbParam<IMsbEvent>` | EventParam.cs:29 | Class | 未实现 | 未实现 | |
| `public abstract class Event : Entry, IMsbEvent` | EventParam.cs:198 | Class | 未实现 | 未实现 | |
| `internal enum RouteType : uint` | RouteParam.cs:9 | Enum | 未实现 | 未实现 | |
| `public class RouteParam : Param<Route>` | RouteParam.cs:19 | Class | 未实现 | 未实现 | |
| `public abstract class Route : Entry` | RouteParam.cs:96 | Class | 未实现 | 未实现 | |
| `internal enum RegionType : uint` | PointParam.cs:11 | Enum | 未实现 | 未实现 | |
| `public class PointParam : Param<Region>, IMsbParam<IMsbRegion>` | PointParam.cs:57 | Class | 未实现 | 未实现 | |
| `public abstract class Region : Entry, IMsbRegion` | PointParam.cs:504 | Class | 未实现 | 未实现 | |
| `internal enum PartType : uint` | PartsParam.cs:12 | Enum | 未实现 | 未实现 | |
| `public class PartsParam : Param<Part>, IMsbPart` | PartsParam.cs:27 | Class | 未实现 | 未实现 | |
| `public abstract class Part : Entry, IMsbPart` | PartsParam.cs:156 | Class | 未实现 | 未实现 | |
