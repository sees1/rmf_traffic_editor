# Door and lift navigation vertices

The level's `vertices` sequence is the source of truth. Door and Lift serialize
both zero-based references and coordinate snapshots of their navigation vertices.
Building-map JSON written by `Building::save()` uses the YAML schema; standalone
`Door::toJson()` / `Lift::toJson()` export Features as described below.
Coordinates are `(Vertex::x, Vertex::y)` in the
corresponding level's scene frame and scene units (pixels for image-based maps),
not cabin-local coordinates and not automatically converted to meters or lon/lat.
Call `Building::syncDoorVertices()` after mutating geometry or the graph to refresh
these derived snapshots; assigning coordinates directly does not trigger observers.

`Door::frontPosition()` and `backPosition()` return `std::optional<Eigen::Vector2d>`.
They return `std::nullopt` for older maps without coordinate snapshots. Coordinates
are stored as DOUBLE Params in the base Edge, so conversion between Edge and Door,
undo snapshots and the existing door serialization preserve them:

```yaml
front_vertex: [2, 2]
front_vertex_x: [3, 20.0]
front_vertex_y: [3, 10.0]
back_vertex: [2, 3]
back_vertex_x: [3, 20.0]
back_vertex_y: [3, -10.0]
```

`setApproachVertices()` clears old coordinates; `setApproachPositions()` sets a
new snapshot after valid references have been assigned. Normal editor usage does
both through `syncDoorVertices()`.

`Lift::NavigationVertices` keeps the existing `cabin` and `doors` index fields and
adds `std::optional<Eigen::Vector2d> cabin_position` and
`std::map<std::string, Eigen::Vector2d> door_positions`. Each level has its own
coordinates, including when the level transform has a different scale/rotation.
Older files without these optional fields load with absent/empty coordinates;
synchronization reconstructs them from the graph.

`Door` preserves the existing `[start, end, params]` door representation.
`start` and `end` are doorway endpoints; `params.front_vertex` and
`params.back_vertex` are approach vertices, encoded as integer Params (`[2, index]`).
The approach vertices have `door_name` and `door_side` parameters.
`Door::frontVertex()` and `backVertex()` return -1 when references are absent.

`Lift::navigation_vertices` stores references separately for each served level:

```yaml
navigation_vertices:
  L1:
    cabin: 5
    cabin_position: [100.0, 200.0]
    doors:
      entry: 4
    door_positions:
      entry: [100.0, 170.0]
```

`cabin` refers to the vertex with `lift_cabin`; door entries refer to outside
approach vertices with `lift_name` and `lift_door_name`. An existing manually
created cabin vertex is reused when available.

`Building::syncDoorVertices()` creates/updates the navigation graph, removes
obsolete generated objects, and publishes final indices after remapping.
References are derived data and are rebuilt for older maps that lack them.
The default approach distance is 0.5 m, configured with the building parameter
`door_vertex_offset` or Building → Door vertex offset in the editor.

## LineString JSON export

`Door::toJson()` returns a `Feature` with `geometry.type = "LineString"` and
`coordinates = [[front_x, front_y], [back_x, back_y]]` (no MultiLineString nesting).
`properties.startid` / `endid` are the approach vertex indices. `door_startid` /
`door_endid` retain the doorway endpoint indices. `properties.metadata` contains
all door Params as ordinary JSON values, without the YAML `[type, value]` wrapper.
If snapshots are absent or non-finite, the Feature has `geometry: null`.

`Lift::toJson()` returns a `FeatureCollection` with one Feature per level/door:

```json
{
  "type": "FeatureCollection",
  "features": [
    {
      "type": "Feature",
      "properties": {
        "startid": 4,
        "endid": 5,
        "metadata": {
          "lift_name": "lift",
          "door_name": "entry",
          "level_name": "L1"
        }
      },
      "geometry": {
        "type": "LineString",
        "coordinates": [[100.0, 170.0], [100.0, 200.0]]
      }
    }
  ]
}
```

The direction is outside approach vertex -> cabin vertex. Indices are local to
`level_name`; geometries on different levels must not be joined into one line.
Entries without valid indices, coordinate snapshots or finite coordinates are
omitted; an empty lift exports `features: []`. The collection also has a foreign
`properties` member containing the existing lift configuration (the toYaml schema).
No inter-floor edges, synthetic IDs, costs or operations are generated here.

This uses the GeoJSON Feature/LineString structure with **local scene coordinates**,
not WGS84 longitude/latitude; it is not a georeferenced RFC 7946 export.
Call `Building::syncDoorVertices()` before exporting after model edits.

`toJson()` is now an export contract, not a direct input to `fromYaml()`.
Map persistence is unchanged: `toYaml()` / `fromYaml()` preserve the building-map
schema, including for JSON maps saved by `Building::save()`.

