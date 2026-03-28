#include <doctest/doctest.h>

#include "zoneout/zoneout.hpp"

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
