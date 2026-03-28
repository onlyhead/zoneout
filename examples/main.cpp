#include "zoneout/zoneout.hpp"

#include <iostream>
#include <string>

int main() {
    const std::string workspace_path = "/home/bresilla/farm_plot_2";

    if (!std::filesystem::exists(workspace_path)) {
        std::cerr << "Workspace path does not exist: " << workspace_path << std::endl;
        std::cerr << "Create or save a workspace there first." << std::endl;
        return 1;
    }

    auto workspace = zoneout::Workspace::load(workspace_path);

    std::cout << "Loaded workspace from: " << workspace_path << std::endl;
    std::cout << "Root zone: " << workspace.root_zone().name() << std::endl;
    std::cout << "Child zones: " << workspace.root_zone().child_count() << std::endl;
    std::cout << "Graph nodes: " << workspace.graph().vertex_count() << std::endl;
    std::cout << "Graph edges: " << workspace.graph().edge_count() << std::endl;

    return 0;
}
