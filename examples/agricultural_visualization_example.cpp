#include <iostream>

#include "zoneout/zoneout.hpp"

#ifdef HAS_RERUN
#include <rerun/recording_stream.hpp>
#endif

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
    std::cout << "Zoneout Agricultural Visualization Example" << std::endl;

#ifdef HAS_RERUN
    auto rec = std::make_shared<rerun::RecordingStream>("zoneout_agricultural", "space");
    auto result = rec->connect_grpc("rerun+http://0.0.0.0:9876/proxy");
    if (result.is_err()) {
        std::cout << "Failed to connect to Rerun viewer" << std::endl;
        return 1;
    }

    const dp::Geo datum{51.98776171041831, 5.662378206146002, 0.0};

    zoneout::Zone farm("Wageningen_Farm", "root", rectangle(0.0, 0.0, 300.0, 200.0), datum);
    zoneout::Zone wheat_field("Wheat_Field_North", "field", rectangle(0.0, 0.0, 300.0, 200.0), datum, 1.0);

    wheat_field.set_property("crop_type", "wheat");
    wheat_field.set_property("planting_date", "2024-10-15");
    wheat_field.set_property("area_hectares", "6.0");

    auto obstacle = rectangle(80.0, 60.0, 20.0, 15.0);
    wheat_field.add_polygon_element(obstacle, "storage_pad", "obstacle");
    farm.add_child(wheat_field);

    zoneout::Workspace workspace(std::move(farm));

    auto lane_start = workspace.add_node(dp::Point{20.0, 20.0, 0.0}, {{"kind", "entry"}});
    auto lane_mid = workspace.add_node(dp::Point{140.0, 80.0, 0.0}, {{"kind", "turn"}}); 
    auto lane_end = workspace.add_node(dp::Point{260.0, 160.0, 0.0}, {{"kind", "exit"}});
    workspace.add_edge(lane_start, lane_mid, 1.0, graphix::vertex::EdgeType::Undirected, {{"kind", "lane"}});
    workspace.add_edge(lane_mid, lane_end, 1.0, graphix::vertex::EdgeType::Undirected, {{"kind", "lane"}});

    std::cout << "Root zone: " << workspace.root_zone().name() << std::endl;
    std::cout << "Field zones: " << workspace.root_zone().child_count() << std::endl;
    std::cout << "Graph nodes: " << workspace.graph().vertex_count() << std::endl;

    zoneout::visualize::show_zone(workspace.root_zone(), rec, datum, workspace.root_zone().name(), 0);
    for (const auto &child : workspace.root_zone().children()) {
        zoneout::visualize::show_zone(child, rec, datum, child.name(), 1);
    }

    std::cout << "Open http://localhost:9876" << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

#else
    std::cout << "Visualization disabled - build with HAS_RERUN" << std::endl;
#endif

    return 0;
}
