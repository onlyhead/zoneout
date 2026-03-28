# Zoneout

**C++ library for agricultural workspace data**

`zoneout` is now organized around four separate concepts:

- `Plot`: spatial payload
- `Zone`: recursive logical region with metadata
- `Graph`: connectivity network using `graphix`
- `Workspace`: one zone tree plus one graph

This replaces the old mental model where `Plot` was a collection of zones and `Zone` tried to be the whole workspace by itself.

## Core Model

```cpp
struct Plot {
    Poly poly;                 // mandatory
    std::optional<Grid> grid;  // optional
};

struct Zone {
    UUID id;
    std::string name;
    std::string type;
    Plot plot;
    std::unordered_map<std::string, std::string> properties;
    std::vector<Zone> children;
};

struct NodeData {
    UUID id;
    datapod::Point position;
    std::vector<UUID> zone_ids;
    std::unordered_map<std::string, std::string> properties;
};

struct EdgeData {
    UUID id;
    std::vector<UUID> zone_ids;
    std::unordered_map<std::string, std::string> properties;
};

using Graph = graphix::vertex::Graph<NodeData, EdgeData>;

struct Workspace {
    Zone root_zone;
    Graph graph;
};
```

## Quick Start

```cpp
#include "zoneout/zoneout.hpp"

namespace dp = datapod;

int main() {
    dp::Polygon boundary;
    boundary.vertices.push_back({0.0, 0.0, 0.0});
    boundary.vertices.push_back({100.0, 0.0, 0.0});
    boundary.vertices.push_back({100.0, 50.0, 0.0});
    boundary.vertices.push_back({0.0, 50.0, 0.0});

    dp::Geo datum{52.0, 5.0, 0.0};

    zoneout::Plot farm_plot = zoneout::PlotBuilder()
                                  .with_name("farm")
                                  .with_type("root")
                                  .with_boundary(boundary)
                                  .with_datum(datum)
                                  .build();

    zoneout::Zone farm("farm", "root", std::move(farm_plot));
    farm.set_property("owner", "research_team");

    zoneout::Zone field("field_a", "field", boundary, datum, 1.0);
    farm.add_child(field);

    zoneout::Workspace workspace(std::move(farm));

    auto a = workspace.add_node(dp::Point{10.0, 10.0, 0.0}, {{"kind", "entry"}});
    auto b = workspace.add_node(dp::Point{80.0, 40.0, 0.0}, {{"kind", "target"}});
    workspace.add_edge(a, b, 1.0, graphix::vertex::EdgeType::Undirected, {{"kind", "lane"}});

    workspace.save("farm_workspace");
    auto loaded = zoneout::Workspace::load("farm_workspace");

    return loaded.graph().edge_count() == 1 ? 0 : 1;
}
```

## Concepts

- `Plot` owns geometry and optional raster data only.
- `Zone` owns identity, metadata, and recursive child zones.
- `Workspace` owns the root zone tree and the graph.
- `Graph` is separate from zones. Nodes and edges carry `zone_ids` instead of being owned by zones.
- Zone nesting uses `depth`, not `layer`.

## Main APIs

### Plot

```cpp
PlotBuilder()
    .with_name(...)
    .with_type(...)
    .with_boundary(...)
    .with_datum(...)
    .with_resolution(...)   // optional, creates a base grid
    .build();

plot.poly();
plot.has_grid();
plot.grid();
plot.set_grid(...);
plot.clear_grid();
plot.save(directory);
Plot::load(directory);
```

### Zone

```cpp
Zone(name, type, boundary, datum);             // poly only
Zone(name, type, boundary, datum, resolution); // poly + generated grid
Zone(name, type, Plot{...});

zone.plot();
zone.poly();
zone.has_grid();
zone.plot().grid();
zone.plot().set_grid(...);
zone.plot().clear_grid();

zone.add_child(child);
zone.remove_child(child_id);
zone.find(zone_id);
zone.find_by_name(name);
zone.depth_of(zone_id);
zone.visit(visitor);

zone.add_raster_layer(...); // creates a grid in the plot if needed
zone.save(directory);
Zone::load(directory);
```

### Workspace

```cpp
Workspace(root_zone);

workspace.root_zone();
workspace.graph();

workspace.add_node(position, properties);
workspace.add_edge(source, target, weight, type, properties);

workspace.find_zone(zone_id);
workspace.find_node(node_id);
workspace.find_edge(edge_id);
workspace.zones_containing(point);
workspace.refresh_graph_zone_membership();

workspace.save(directory);
Workspace::load(directory);
```

## Persistence

`Workspace::save()` writes:

```text
workspace/
  workspace.json
  zones/
    <zone-uuid>/
      zone.json
      vector.geojson
      raster.tiff        # optional
  graph/
    graph.json
```

`Workspace::load()` still supports the older temporary layout for compatibility.

## Examples

- [examples/quickstart.cpp](/home/bresilla/data/code/robolibs/zoneout/examples/quickstart.cpp)
- [examples/builder_pattern_example.cpp](/home/bresilla/data/code/robolibs/zoneout/examples/builder_pattern_example.cpp)
- [examples/overlapping_zones_plot.cpp](/home/bresilla/data/code/robolibs/zoneout/examples/overlapping_zones_plot.cpp)

## Build

```bash
make build
make test
```
