#include <doctest/doctest.h>

#include "zoneout/zoneout.hpp"

#include <limits>

namespace dp = datapod;
using namespace zoneout;

namespace {
    dp::Polygon rectangle(double x, double y, double width, double height) {
        dp::Polygon poly;
        poly.vertices.emplace_back(x, y, 0.0);
        poly.vertices.emplace_back(x + width, y, 0.0);
        poly.vertices.emplace_back(x + width, y + height, 0.0);
        poly.vertices.emplace_back(x, y + height, 0.0);
        return poly;
    }

    const dp::Geo datum{52.0, 5.0, 0.0};
} // namespace

TEST_CASE("Zone supports recursive child traversal") {
    Zone root("Farm", "root", rectangle(0.0, 0.0, 100.0, 100.0), datum, 1.0);
    Zone field("Field A", "field", rectangle(10.0, 10.0, 40.0, 40.0), datum, 1.0);
    Zone patch("Patch A1", "patch", rectangle(15.0, 15.0, 10.0, 10.0), datum, 1.0);

    const auto field_id = field.id();
    const auto patch_id = patch.id();

    field.add_child(patch);
    root.add_child(field);

    CHECK(root.child_count() == 1);
    CHECK(root.find(field_id) != nullptr);
    CHECK(root.find(patch_id) != nullptr);
    CHECK(root.depth_of(root.id()).value_or(99) == 0);
    CHECK(root.depth_of(field_id).value_or(99) == 1);
    CHECK(root.depth_of(patch_id).value_or(99) == 2);
}

TEST_CASE("Workspace graph nodes and edges get zone membership") {
    Zone root("Farm", "root", rectangle(0.0, 0.0, 100.0, 100.0), datum, 1.0);
    Zone field("Field A", "field", rectangle(10.0, 10.0, 30.0, 30.0), datum, 1.0);

    const auto root_id = root.id();
    const auto field_id = field.id();
    root.add_child(field);

    Workspace workspace(root);

    auto n1 = workspace.add_node(dp::Point{20.0, 20.0, 0.0});
    auto n2 = workspace.add_node(dp::Point{80.0, 80.0, 0.0});
    auto edge = workspace.add_edge(n1, n2, 1.0, graphix::vertex::EdgeType::Undirected);

    const auto &node1 = workspace.graph()[n1];
    const auto &node2 = workspace.graph()[n2];
    const auto &edge_data = workspace.graph().edge_property(edge);

    CHECK(node1.zone_ids.size() == 2);
    CHECK(std::find(node1.zone_ids.begin(), node1.zone_ids.end(), root_id) != node1.zone_ids.end());
    CHECK(std::find(node1.zone_ids.begin(), node1.zone_ids.end(), field_id) != node1.zone_ids.end());

    CHECK(node2.zone_ids.size() == 1);
    CHECK(std::find(node2.zone_ids.begin(), node2.zone_ids.end(), root_id) != node2.zone_ids.end());

    CHECK(std::find(edge_data.zone_ids.begin(), edge_data.zone_ids.end(), root_id) != edge_data.zone_ids.end());
    CHECK(std::find(edge_data.zone_ids.begin(), edge_data.zone_ids.end(), field_id) != edge_data.zone_ids.end());

    const auto containing_zones = workspace.zones_containing(dp::Point{20.0, 20.0, 0.0});
    CHECK(containing_zones.size() == 2);
    CHECK(std::find_if(containing_zones.begin(), containing_zones.end(),
                       [root_id](const Zone *zone) { return zone->id() == root_id; }) != containing_zones.end());
    CHECK(std::find_if(containing_zones.begin(), containing_zones.end(),
                       [field_id](const Zone *zone) { return zone->id() == field_id; }) != containing_zones.end());
}

TEST_CASE("Workspace save and load roundtrip preserves zone tree and graph IDs") {
    Zone root("Farm", "root", rectangle(0.0, 0.0, 100.0, 100.0), datum, 1.0);
    Zone field("Field A", "field", rectangle(10.0, 10.0, 30.0, 30.0), datum, 1.0);

    const auto root_id = root.id();
    const auto field_id = field.id();
    root.add_child(field);

    Workspace workspace(root);
    auto n1 = workspace.add_node(dp::Point{20.0, 20.0, 0.0}, {{"kind", "start"}});
    auto n2 = workspace.add_node(dp::Point{80.0, 80.0, 0.0}, {{"kind", "end"}});
    auto edge = workspace.add_edge(n1, n2, 2.5, graphix::vertex::EdgeType::Directed, {{"kind", "route"}});

    const auto node1_id = workspace.graph()[n1].id;
    const auto node2_id = workspace.graph()[n2].id;
    const auto edge_id = workspace.graph().edge_property(edge).id;

    const std::filesystem::path save_dir = "/tmp/zoneout_workspace_roundtrip";
    std::filesystem::remove_all(save_dir);
    workspace.save(save_dir);

    CHECK(std::filesystem::exists(save_dir / "workspace.json"));
    CHECK(std::filesystem::exists(save_dir / "zones" / root_id.toString() / "zone.json"));
    CHECK(std::filesystem::exists(save_dir / "zones" / root_id.toString() / "vector.geojson"));
    CHECK(std::filesystem::exists(save_dir / "zones" / field_id.toString() / "zone.json"));
    CHECK(std::filesystem::exists(save_dir / "zones" / field_id.toString() / "vector.geojson"));
    CHECK(std::filesystem::exists(save_dir / "graph" / "graph.json"));

    auto loaded = Workspace::load(save_dir);

    CHECK(loaded.root_zone().id() == root_id);
    CHECK(loaded.find_zone(field_id) != nullptr);
    CHECK(loaded.graph().vertex_count() == 2);
    CHECK(loaded.graph().edge_count() == 1);

    auto loaded_node1 = loaded.find_node(node1_id);
    auto loaded_node2 = loaded.find_node(node2_id);
    auto loaded_edge = loaded.find_edge(edge_id);

    CHECK(loaded_node1.has_value());
    CHECK(loaded_node2.has_value());
    CHECK(loaded_edge.has_value());
    CHECK(loaded.graph()[*loaded_node1].properties.at("kind") == "start");
    CHECK(loaded.graph()[*loaded_node2].properties.at("kind") == "end");
    CHECK(loaded.graph().edge_property(*loaded_edge).properties.at("kind") == "route");
}

TEST_CASE("Zone built from plot can start without grid and gain one later") {
    auto poly_only_plot = PlotBuilder()
                              .with_name("Vector Only")
                              .with_type("field")
                              .with_boundary(rectangle(0.0, 0.0, 20.0, 10.0))
                              .with_datum(datum)
                              .build();

    Zone zone("Vector Only", "field", std::move(poly_only_plot));

    CHECK_FALSE(zone.has_grid());
    CHECK(zone.raster_info() == "No raster layers");
    CHECK_FALSE(zone.plot().has_grid());
    CHECK_THROWS(zone.plot().grid());

    dp::Grid<uint8_t> temp_grid;
    temp_grid.rows = 4;
    temp_grid.cols = 8;
    temp_grid.resolution = 2.0;
    temp_grid.centered = true;
    temp_grid.pose = dp::Pose{dp::Point{10.0, 5.0, 0.0}, dp::Quaternion{}};
    temp_grid.data.resize(temp_grid.rows * temp_grid.cols, 42);

    zone.add_raster_layer(temp_grid, "temperature", "environmental", {{"units", "celsius"}});

    CHECK(zone.has_grid());
    CHECK(zone.plot().grid().layer_count() == 1);
    CHECK(zone.plot().grid().resolution() == doctest::Approx(2.0));
    CHECK(zone.plot().grid().datum().latitude == doctest::Approx(datum.latitude));
    CHECK(zone.plot().grid().datum().longitude == doctest::Approx(datum.longitude));

    zone.plot().clear_grid();
    CHECK_FALSE(zone.has_grid());
    CHECK_FALSE(zone.plot().has_grid());
}

TEST_CASE("Workspace JSON converts to workspace and back") {
    WorkspaceJson draft;
    draft.name = "Farm Draft";
    draft.datum = datum;
    draft.datum_set = true;

    ZoneJson root_zone;
    root_zone.name = "Farm";
    root_zone.type = "root";
    root_zone.grid_enabled = true;
    root_zone.grid_resolution = 2.0;
    root_zone.polygon_latlon = {JsonPoint{52.0, 5.0},
                                JsonPoint{52.0, 5.001456},
                                JsonPoint{52.000899, 5.001456},
                                JsonPoint{52.000899, 5.0}};

    ZoneJson child_zone;
    child_zone.name = "Field A";
    child_zone.type = "field";
    child_zone.parent_id = root_zone.id;
    child_zone.polygon_latlon = {JsonPoint{52.000090, 5.000145},
                                 JsonPoint{52.000090, 5.000582},
                                 JsonPoint{52.000360, 5.000582},
                                 JsonPoint{52.000360, 5.000145}};
    root_zone.child_ids.push_back(child_zone.id);

    draft.root_zone_id = root_zone.id;
    draft.zones[root_zone.id] = root_zone;
    draft.zones[child_zone.id] = child_zone;

    NodeJson start_node;
    start_node.name = "Start";
    start_node.latlon = JsonPoint{52.000180, 5.000291};
    start_node.properties["name"] = "Start";

    NodeJson end_node;
    end_node.name = "End";
    end_node.latlon = JsonPoint{52.000719, 5.001165};
    end_node.properties["name"] = "End";

    EdgeJson route_edge;
    route_edge.source_id = start_node.id;
    route_edge.target_id = end_node.id;
    route_edge.directed = true;
    route_edge.weight = 3.5;
    route_edge.properties["kind"] = "route";

    draft.nodes[start_node.id] = start_node;
    draft.nodes[end_node.id] = end_node;
    draft.edges[route_edge.id] = route_edge;

    auto workspace = to_workspace(draft);

    CHECK(workspace.root_zone().id().toString() == root_zone.id);
    auto child_ptr = workspace.find_zone(UUID(child_zone.id));
    REQUIRE(child_ptr != nullptr);
    CHECK(child_ptr->name() == "Field A");
    CHECK(workspace.root_zone().plot().has_grid());

    REQUIRE(workspace.graph().vertex_count() == 2);
    REQUIRE(workspace.graph().edge_count() == 1);

    auto start_vertex = workspace.find_node(UUID(start_node.id));
    auto end_vertex = workspace.find_node(UUID(end_node.id));
    auto edge_id = workspace.find_edge(UUID(route_edge.id));
    REQUIRE(start_vertex.has_value());
    REQUIRE(end_vertex.has_value());
    REQUIRE(edge_id.has_value());

    CHECK(workspace.graph()[*start_vertex].zone_ids.size() == 2);
    CHECK(workspace.graph()[*end_vertex].zone_ids.size() == 1);
    CHECK(workspace.graph().edge_property(*edge_id).properties.at("kind") == "route");

    auto roundtrip = from_workspace(workspace);
    CHECK(roundtrip.root_zone_id == draft.root_zone_id);
    CHECK(roundtrip.zones.size() == 2);
    CHECK(roundtrip.nodes.size() == 2);
    CHECK(roundtrip.edges.size() == 1);
    CHECK(roundtrip.edges.at(route_edge.id).directed);
    CHECK(roundtrip.edges.at(route_edge.id).weight == doctest::Approx(3.5));
}

TEST_CASE("Workspace JSON parses and serializes") {
    WorkspaceJson draft;
    draft.name = "JSON Draft";
    draft.datum = datum;
    draft.datum_set = true;

    ZoneJson root_zone;
    root_zone.name = "Farm";
    root_zone.type = "root";
    root_zone.polygon_latlon = {JsonPoint{52.0, 5.0}, JsonPoint{52.0, 5.0002}, JsonPoint{52.0002, 5.0002}};
    draft.root_zone_id = root_zone.id;
    draft.zones[root_zone.id] = root_zone;

    NodeJson node;
    node.name = "Node A";
    node.latlon = JsonPoint{52.0001, 5.0001};
    draft.nodes[node.id] = node;

    const auto json_text = workspace_json(draft);
    auto parsed = parse_workspace_json(json_text);

    CHECK(parsed.name == draft.name);
    CHECK(parsed.root_zone_id == draft.root_zone_id);
    CHECK(parsed.datum_set);
    CHECK(parsed.zones.size() == 1);
    CHECK(parsed.nodes.size() == 1);
    CHECK(parsed.edges.empty());
}

TEST_CASE("Workspace JSON rejects disconnected zones") {
    WorkspaceJson draft;
    draft.name = "Broken Draft";
    draft.datum = datum;
    draft.datum_set = true;

    ZoneJson root_zone;
    root_zone.name = "Farm";
    root_zone.type = "root";
    root_zone.polygon_latlon = {JsonPoint{52.0, 5.0}, JsonPoint{52.0, 5.0002}, JsonPoint{52.0002, 5.0002}};

    ZoneJson disconnected_zone;
    disconnected_zone.name = "Detached";
    disconnected_zone.type = "field";
    disconnected_zone.polygon_latlon = {JsonPoint{52.0010, 5.0010},
                                        JsonPoint{52.0010, 5.0012},
                                        JsonPoint{52.0012, 5.0012}};

    draft.root_zone_id = root_zone.id;
    draft.zones[root_zone.id] = root_zone;
    draft.zones[disconnected_zone.id] = disconnected_zone;

    CHECK_THROWS_WITH(to_workspace(draft), doctest::Contains("disconnected from the root zone"));
}

TEST_CASE("Workspace JSON validation reports structural errors") {
    WorkspaceJson draft;

    ZoneJson root_zone;
    root_zone.name = "Farm";
    root_zone.type = "root";
    root_zone.polygon_latlon = {JsonPoint{52.0, 5.0}, JsonPoint{52.0, 5.0002}, JsonPoint{52.0002, 5.0002}};
    root_zone.child_ids.push_back("missing-child");

    NodeJson node;
    node.zone_ids.push_back("missing-zone");

    EdgeJson edge;
    edge.source_id = "missing-source";
    edge.target_id = "missing-target";
    edge.zone_ids.push_back("missing-zone");

    draft.root_zone_id = root_zone.id;
    draft.zones[root_zone.id] = root_zone;
    draft.nodes[node.id] = node;
    draft.edges[edge.id] = edge;

    const auto errors = validate_workspace_json(draft);
    CHECK(errors.size() >= 4);

    CHECK_THROWS_WITH(require_valid_workspace_json(draft), doctest::Contains("validation failed"));
    CHECK_THROWS_WITH(require_valid_workspace_json(draft), doctest::Contains("missing child zone"));
    CHECK_THROWS_WITH(require_valid_workspace_json(draft), doctest::Contains("unknown source node"));
}

TEST_CASE("Workspace JSON validation reports parent child mismatches") {
    WorkspaceJson draft;
    draft.datum = datum;
    draft.datum_set = true;

    ZoneJson root_zone;
    root_zone.name = "Farm";
    root_zone.type = "root";
    root_zone.polygon_latlon = {JsonPoint{52.0, 5.0}, JsonPoint{52.0, 5.0002}, JsonPoint{52.0002, 5.0002}};

    ZoneJson child_zone;
    child_zone.name = "Field A";
    child_zone.type = "field";
    child_zone.parent_id = root_zone.id;
    child_zone.polygon_latlon = {JsonPoint{52.00005, 5.00005},
                                 JsonPoint{52.00005, 5.00015},
                                 JsonPoint{52.00015, 5.00015}};

    draft.root_zone_id = root_zone.id;
    draft.zones[root_zone.id] = root_zone;
    draft.zones[child_zone.id] = child_zone;

    auto errors = validate_workspace_json(draft);
    CHECK_FALSE(errors.empty());
    CHECK(std::find_if(errors.begin(), errors.end(),
                       [](const std::string &error) { return error.find("missing from that parent's child_ids") != std::string::npos; }) != errors.end());

    draft.zones[root_zone.id].child_ids.push_back(child_zone.id);
    draft.zones[child_zone.id].parent_id.clear();

    errors = validate_workspace_json(draft);
    CHECK(std::find_if(errors.begin(), errors.end(),
                       [](const std::string &error) { return error.find("does not point back with matching parent_id") != std::string::npos; }) != errors.end());
}

TEST_CASE("Workspace JSON file helpers roundtrip") {
    Zone root("Farm", "root", rectangle(0.0, 0.0, 100.0, 100.0), datum, 1.0);
    Workspace workspace(root);
    const auto a = workspace.add_node(dp::Point{10.0, 10.0, 0.0}, {{"name", "entry"}});
    const auto b = workspace.add_node(dp::Point{20.0, 20.0, 0.0}, {{"name", "exit"}});
    workspace.add_edge(a, b, 2.0, graphix::vertex::EdgeType::Directed, {{"kind", "route"}});

    const std::filesystem::path json_path = "/tmp/zoneout_workspace_json.json";
    std::filesystem::remove(json_path);

    write_workspace_json_file(json_path, workspace);
    CHECK(std::filesystem::exists(json_path));

    auto loaded = load_workspace_json_file(json_path);
    CHECK(loaded.root_zone().name() == "Farm");
    CHECK(loaded.graph().vertex_count() == 2);
    CHECK(loaded.graph().edge_count() == 1);
}

TEST_CASE("Workspace JSON validation catches malformed ids and coordinates") {
    WorkspaceJson draft;
    draft.root_zone_id = "not-a-uuid";
    draft.datum = dp::Geo{999.0, 999.0, 0.0};
    draft.datum_set = true;

    ZoneJson zone;
    zone.id = "bad-zone-id";
    zone.polygon_latlon = {JsonPoint{100.0, 5.0}, JsonPoint{52.0, 5.0}, JsonPoint{52.0, 5.0}};
    draft.zones["different-key"] = zone;

    NodeJson node;
    node.id = "bad-node-id";
    node.latlon = JsonPoint{52.0, 500.0};
    node.zone_ids.push_back("also-bad-zone-id");
    draft.nodes["different-node-key"] = node;

    EdgeJson edge;
    edge.id = "bad-edge-id";
    edge.source_id = "bad-source-id";
    edge.target_id = "bad-target-id";
    edge.weight = std::numeric_limits<double>::infinity();
    edge.zone_ids.push_back("bad-zone-ref");
    draft.edges["different-edge-key"] = edge;

    const auto errors = validate_workspace_json(draft);
    CHECK_FALSE(errors.empty());
    CHECK(std::find_if(errors.begin(), errors.end(),
                       [](const std::string &error) { return error.find("root zone UUID") != std::string::npos; }) != errors.end());
    CHECK(std::find_if(errors.begin(), errors.end(),
                       [](const std::string &error) { return error.find("does not match zone.id") != std::string::npos; }) != errors.end());
    CHECK(std::find_if(errors.begin(), errors.end(),
                       [](const std::string &error) { return error.find("invalid latitude/longitude") != std::string::npos; }) != errors.end());
    CHECK(std::find_if(errors.begin(), errors.end(),
                       [](const std::string &error) { return error.find("non-finite weight") != std::string::npos; }) != errors.end());
}
