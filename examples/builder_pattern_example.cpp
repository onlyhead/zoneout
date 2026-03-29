#include "zoneout/zoneout.hpp"

#include <iostream>

namespace dp = datapod;

namespace {
    dp::Polygon create_rectangle(double width, double height, double x = 0.0, double y = 0.0) {
        dp::Polygon poly;
        poly.vertices.emplace_back(x, y, 0.0);
        poly.vertices.emplace_back(x + width, y, 0.0);
        poly.vertices.emplace_back(x + width, y + height, 0.0);
        poly.vertices.emplace_back(x, y + height, 0.0);
        return poly;
    }
} // namespace

int main() {
    std::cout << "=== Builder Pattern Example ===" << std::endl;

    dp::Geo datum{52.0, 5.0, 0.0};

    auto plot = zoneout::PlotBuilder()
                    .with_name("research_plot")
                    .with_type("agricultural")
                    .with_boundary(create_rectangle(200.0, 140.0))
                    .with_datum(datum)
                    .with_resolution(1.0)
                    .with_properties({{"owner", "AgriTech Labs"}, {"season", "2024"}})
                    .build();

    std::cout << "Plot: " << plot.name() << " (" << plot.type() << ")" << std::endl;
    std::cout << "Has grid: " << plot.has_grid() << std::endl;
    std::cout << "Plot area: " << plot.poly().area() << std::endl;

    auto root_zone = zoneout::ZoneBuilder()
                         .with_name("farm_root")
                         .with_type("root")
                         .with_boundary(create_rectangle(200.0, 140.0))
                         .with_datum(datum)
                         .with_resolution(2.0)
                         .build();

    auto field_zone = zoneout::ZoneBuilder()
                          .with_name("field_a")
                          .with_type("agricultural")
                          .with_boundary(create_rectangle(80.0, 60.0, 10.0, 10.0))
                          .with_datum(datum)
                          .with_resolution(0.5)
                          .with_property("crop", "wheat")
                          .build();

    auto trial_zone = zoneout::ZoneBuilder()
                          .with_name("trial_patch")
                          .with_type("experimental")
                          .with_boundary(create_rectangle(20.0, 15.0, 20.0, 20.0))
                          .with_datum(datum)
                          .with_resolution(0.2)
                          .with_property("experiment", "nitrogen")
                          .build();

    field_zone.add_child(trial_zone);
    root_zone.add_child(field_zone);

    zoneout::Workspace workspace(root_zone);
    auto a = workspace.add_node(dp::Point{15.0, 15.0, 0.0}, {{"kind", "waypoint"}});
    auto b = workspace.add_node(dp::Point{35.0, 25.0, 0.0}, {{"kind", "waypoint"}});
    auto e = workspace.add_edge(a, b, 1.0, graphix::vertex::EdgeType::Undirected, {{"kind", "lane"}});

    std::cout << "Root zone: " << workspace.root_zone().name() << std::endl;
    std::cout << "Child zones: " << workspace.root_zone().child_count() << std::endl;
    std::cout << "Graph nodes: " << workspace.graph().vertex_count() << std::endl;
    std::cout << "Graph edges: " << workspace.graph().edge_count() << std::endl;
    std::cout << "Edge memberships: " << workspace.graph().edge_property(e).zone_ids.size() << std::endl;

    return 0;
}
