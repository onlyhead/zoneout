#include "zoneout/zoneout.hpp"

#include <iostream>

namespace dp = datapod;

namespace {
    dp::Polygon rectangle(double x, double y, double width, double height) {
        dp::Polygon poly;
        poly.vertices.push_back({x, y, 0.0});
        poly.vertices.push_back({x + width, y, 0.0});
        poly.vertices.push_back({x + width, y + height, 0.0});
        poly.vertices.push_back({x, y + height, 0.0});
        return poly;
    }
} // namespace

int main() {
    std::cout << "=== Zoneout Quickstart ===" << std::endl;

    const dp::Geo datum{52.0, 5.0, 0.0};

    auto farm_plot = zoneout::PlotBuilder()
                         .with_name("farm")
                         .with_type("root")
                         .with_boundary(rectangle(0.0, 0.0, 120.0, 80.0))
                         .with_datum(datum)
                         .build();

    zoneout::Zone farm("farm", "root", std::move(farm_plot));
    farm.set_property("owner", "research_team");

    zoneout::Zone field_a("field_a", "field", rectangle(10.0, 10.0, 40.0, 25.0), datum, 1.0);
    zoneout::Zone field_b("field_b", "field", rectangle(60.0, 15.0, 35.0, 30.0), datum);

    farm.add_child(field_a);
    farm.add_child(field_b);

    zoneout::Workspace workspace(std::move(farm));

    auto n1 = workspace.add_node(dp::Point{15.0, 15.0, 0.0}, {{"kind", "entry"}});
    auto n2 = workspace.add_node(dp::Point{75.0, 25.0, 0.0}, {{"kind", "loading"}}); 
    auto edge = workspace.add_edge(n1, n2, 1.0, graphix::vertex::EdgeType::Undirected, {{"kind", "lane"}});

    workspace.save("quickstart_workspace");

    std::cout << "Root zone: " << workspace.root_zone().name() << std::endl;
    std::cout << "Child zones: " << workspace.root_zone().child_count() << std::endl;
    std::cout << "Graph nodes: " << workspace.graph().vertex_count() << std::endl;
    std::cout << "Graph edges: " << workspace.graph().edge_count() << std::endl;
    std::cout << "Saved to ./quickstart_workspace" << std::endl;
    std::cout << "Edge memberships: " << workspace.graph().edge_property(edge).zone_ids.size() << std::endl;

    return 0;
}
