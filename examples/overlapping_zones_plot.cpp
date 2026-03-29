#include "zoneout/zoneout.hpp"

#include <iostream>

namespace dp = datapod;

int main() {
    std::cout << "=== Nested Zones Workspace Example ===" << std::endl;

    dp::Geo datum{52.0, 5.0, 0.0};

    dp::Polygon farm_boundary;
    farm_boundary.vertices.push_back(dp::Point{0.0, 0.0, 0.0});
    farm_boundary.vertices.push_back(dp::Point{100.0, 0.0, 0.0});
    farm_boundary.vertices.push_back(dp::Point{100.0, 50.0, 0.0});
    farm_boundary.vertices.push_back(dp::Point{0.0, 50.0, 0.0});

    zoneout::Zone farm("farm", "root", farm_boundary, datum, 2.0);

    zoneout::Zone high_res("field_high_res", "agricultural", farm_boundary, datum, 0.5);
    zoneout::Zone medium_res("field_medium_res", "agricultural", farm_boundary, datum, 1.0);

    dp::Polygon shifted_boundary;
    shifted_boundary.vertices.push_back(dp::Point{5.0, 5.0, 0.0});
    shifted_boundary.vertices.push_back(dp::Point{95.0, 5.0, 0.0});
    shifted_boundary.vertices.push_back(dp::Point{95.0, 45.0, 0.0});
    shifted_boundary.vertices.push_back(dp::Point{5.0, 45.0, 0.0});
    zoneout::Zone low_res("field_low_res", "agricultural", shifted_boundary, datum, 2.0);

    farm.add_child(high_res);
    farm.add_child(medium_res);
    farm.add_child(low_res);

    zoneout::Workspace workspace(farm);
    auto n1 = workspace.add_node(dp::Point{20.0, 20.0, 0.0});
    auto n2 = workspace.add_node(dp::Point{80.0, 25.0, 0.0});
    auto e = workspace.add_edge(n1, n2);

    std::cout << "Root zone: " << workspace.root_zone().name() << std::endl;
    std::cout << "Child zones: " << workspace.root_zone().child_count() << std::endl;
    std::cout << "Graph nodes: " << workspace.graph().vertex_count() << std::endl;
    std::cout << "Graph edges: " << workspace.graph().edge_count() << std::endl;
    std::cout << "Edge zone memberships: " << workspace.graph().edge_property(e).zone_ids.size() << std::endl;

    return 0;
}
