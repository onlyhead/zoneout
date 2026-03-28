#include "zoneout/zoneout.hpp"

#include <iostream>

namespace dp = datapod;

namespace {
    dp::Polygon rectangle(double x, double y, double width, double height) {
        dp::Polygon poly;
        poly.vertices.emplace_back(x, y, 0.0);
        poly.vertices.emplace_back(x + width, y, 0.0);
        poly.vertices.emplace_back(x + width, y + height, 0.0);
        poly.vertices.emplace_back(x, y + height, 0.0);
        return poly;
    }
}

int main() {
    const dp::Geo datum{51.73019, 4.23883, 0.0};
    zoneout::Zone root("Farm", "root", rectangle(0.0, 0.0, 100.0, 100.0), datum, 1.0);
    root.add_child(zoneout::Zone("Field A", "field", rectangle(10.0, 10.0, 30.0, 30.0), datum, 1.0));

    zoneout::Workspace workspace(root);
    workspace.add_node(dp::Point{20.0, 20.0, 0.0}, {{"name", "start"}});
    workspace.add_node(dp::Point{80.0, 80.0, 0.0}, {{"name", "end"}});

    const std::string save_path = "/tmp/zoneout_example_workspace";
    workspace.save(save_path);

    std::cout << "Saved workspace to: " << save_path << std::endl;
    std::cout << "Root zone: " << workspace.root_zone().name() << std::endl;
    std::cout << "Child zones: " << workspace.root_zone().child_count() << std::endl;
    std::cout << "Graph nodes: " << workspace.graph().vertex_count() << std::endl;
    return 0;
}
