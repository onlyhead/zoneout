#include "zoneout/zoneout.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    const std::filesystem::path draft_path =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path("/tmp/zoneout_workspace.json");
    const std::filesystem::path workspace_path =
        argc > 2 ? std::filesystem::path(argv[2]) : std::filesystem::path("/tmp/zoneout_workspace");

    if (!std::filesystem::exists(draft_path)) {
        std::cerr << "Draft JSON path does not exist: " << draft_path << std::endl;
        std::cerr << "Usage: main [workspace.json] [output_workspace_dir]" << std::endl;
        return 1;
    }

    auto draft = zoneout::parse_workspace_json_file(draft_path);
    auto workspace = zoneout::to_workspace(draft);
    workspace.save(workspace_path);

    std::cout << "Loaded draft from: " << draft_path << std::endl;
    std::cout << "Saved native workspace to: " << workspace_path << std::endl;
    std::cout << "Root zone: " << workspace.root_zone().name() << std::endl;
    std::cout << "Child zones: " << workspace.root_zone().child_count() << std::endl;
    std::cout << "Graph nodes: " << workspace.graph().vertex_count() << std::endl;
    std::cout << "Graph edges: " << workspace.graph().edge_count() << std::endl;

    return 0;
}
