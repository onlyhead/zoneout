#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#include "zoneout/zoneout.hpp"

namespace dp = datapod;
using namespace zoneout;

// Wageningen Research Labs coordinates
const dp::Geo WAGENINGEN_DATUM{51.98776171041831, 5.662378206146002, 0.0};

// Helper function to create a rectangular polygon
dp::Polygon createRectangle(double x, double y, double width, double height) {
    dp::Polygon poly;
    poly.vertices.emplace_back(x, y, 0.0);
    poly.vertices.emplace_back(x + width, y, 0.0);
    poly.vertices.emplace_back(x + width, y + height, 0.0);
    poly.vertices.emplace_back(x, y + height, 0.0);
    return poly;
}

TEST_CASE("Zone field elements management") {
    // Create simple base grid for Zone constructor
    dp::Grid<uint8_t> base_grid;
    base_grid.rows = 10;
    base_grid.cols = 10;
    base_grid.resolution = 1.0;
    base_grid.centered = true;
    base_grid.pose = dp::Pose{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
    base_grid.data.resize(10 * 10, 0);

    auto boundary = createRectangle(0, 0, 200, 100);
    Zone zone("Field Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    SUBCASE("Add irrigation lines") {
        dp::Segment line({10, 50, 0}, {190, 50, 0});

        std::unordered_map<std::string, std::string> props;
        props["flow_rate"] = "50L/min";
        props["pressure"] = "2.5bar";

        zone.poly().add_line_element(line, "irrigation_line", props);

        auto irrigation_lines = zone.poly().lines_by_type("irrigation_line");
        CHECK(irrigation_lines.size() == 1);

        auto all_elements = zone.poly().feature_count();
        CHECK(all_elements == 1);

        auto irrigation_only = zone.poly().lines_by_type("irrigation_line");
        CHECK(irrigation_only.size() == 1);

        auto crop_rows = zone.poly().lines_by_type("crop_row");
        CHECK(crop_rows.size() == 0);
    }

    SUBCASE("Add crop rows") {
        for (int i = 0; i < 5; ++i) {
            double y = 10.0 + i * 15.0;
            dp::Segment row_line({5, y, 0}, {195, y, 0});

            std::unordered_map<std::string, std::string> props;
            props["row_number"] = std::to_string(i + 1);
            props["crop_type"] = "wheat";
            props["planting_date"] = "2024-03-15";

            zone.poly().add_line_element(row_line, "crop_row", props);
        }

        auto crop_rows = zone.poly().lines_by_type("crop_row");
        CHECK(crop_rows.size() == 5);

        auto all_elements = zone.poly().feature_count();
        CHECK(all_elements == 5);
    }

    SUBCASE("Add obstacles") {
        // Add a rectangular obstacle (e.g., building)
        auto obstacle_boundary = createRectangle(50, 25, 20, 10);

        std::unordered_map<std::string, std::string> props;
        props["type"] = "building";
        props["height"] = "5.0m";
        props["material"] = "concrete";

        zone.poly().add_polygon_element(obstacle_boundary, "obstacle", props);

        auto obstacles = zone.poly().polygons_by_type("obstacle");
        CHECK(obstacles.size() == 1);
    }

    SUBCASE("Add access paths") {
        // For a path with multiple points, we'd need to add multiple line segments
        // or use a different approach. For now, add a simple line segment.
        dp::Segment path_line({0, 0, 0}, {200, 100, 0});

        std::unordered_map<std::string, std::string> props;
        props["width"] = "3.0m";
        props["surface"] = "gravel";
        props["max_speed"] = "15km/h";

        zone.poly().add_line_element(path_line, "access_path", props);

        auto access_paths = zone.poly().lines_by_type("access_path");
        CHECK(access_paths.size() == 1);
    }

    SUBCASE("Mixed field elements") {
        // Add multiple types
        dp::Segment line({10, 30, 0}, {190, 30, 0});
        zone.poly().add_line_element(line, "irrigation_line");

        dp::Segment row({5, 70, 0}, {195, 70, 0});
        zone.poly().add_line_element(row, "crop_row");

        auto obstacle = createRectangle(100, 10, 10, 10);
        zone.poly().add_polygon_element(obstacle, "obstacle");

        // Check totals
        CHECK(zone.poly().lines_by_type("irrigation_line").size() == 1);
        CHECK(zone.poly().lines_by_type("crop_row").size() == 1);
        CHECK(zone.poly().polygons_by_type("obstacle").size() == 1);
        CHECK(zone.poly().feature_count() == 3);
    }
}

TEST_CASE("Zone raster layers management") {
    // Create simple base grid for Zone constructor
    dp::Grid<uint8_t> base_grid;
    base_grid.rows = 10;
    base_grid.cols = 10;
    base_grid.resolution = 1.0;
    base_grid.centered = true;
    base_grid.pose = dp::Pose{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
    base_grid.data.resize(10 * 10, 0);

    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Raster Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    SUBCASE("Add elevation layer") {
        zone.plot().grid().add_grid(20, 10, "elevation", "terrain", {{"units", "meters"}});

        CHECK(zone.plot().grid().layer_count() == 2); // Base grid + elevation layer
    }

    SUBCASE("Add soil moisture layer") {
        zone.plot().grid().add_grid(16, 8, "soil_moisture", "environmental", {{"units", "percentage"}});

        CHECK(zone.plot().grid().layer_count() == 2); // Base grid + soil moisture layer
    }

    SUBCASE("Add crop health layer") {
        zone.plot().grid().add_grid(24, 12, "crop_health", "vegetation", {{"units", "NDVI"}});

        CHECK(zone.plot().grid().layer_count() == 2); // Base grid + crop health layer
    }

    SUBCASE("Multiple raster layers") {
        zone.plot().grid().add_grid(20, 10, "elevation", "terrain", {{"units", "meters"}});
        zone.plot().grid().add_grid(20, 10, "soil_moisture", "environmental", {{"units", "percentage"}});
        zone.plot().grid().add_grid(20, 10, "crop_health", "vegetation", {{"units", "NDVI"}});

        CHECK(zone.plot().grid().layer_count() == 4); // Base grid + 3 additional layers
    }

    SUBCASE("Custom raster layer") {
        std::unordered_map<std::string, std::string> props;
        props["sensor_type"] = "infrared";
        props["measurement_date"] = "2024-06-15";
        props["weather_conditions"] = "sunny";

        zone.plot().grid().add_grid(10, 5, "temperature", "thermal", props);

        CHECK(zone.plot().grid().layer_count() == 2); // Base grid + temperature layer
    }
}

TEST_CASE("Zone raster sampling") {
    // Create simple base grid for Zone constructor
    dp::Grid<uint8_t> base_grid;
    base_grid.rows = 10;
    base_grid.cols = 10;
    base_grid.resolution = 1.0;
    base_grid.centered = true;
    base_grid.pose = dp::Pose{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
    base_grid.data.resize(10 * 10, 0);

    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Sampling Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    zone.plot().grid().add_grid(20, 10, "elevation", "terrain", {{"units", "meters"}});

    // Get the layer and fill it with test data
    auto &layer = zone.plot().grid().get_layer(1); // Index 1 is the elevation layer (0 is base)
    auto &layer_grid = std::get<dp::Grid<uint8_t>>(layer.grid);
    for (size_t r = 0; r < layer_grid.rows; ++r) {
        for (size_t c = 0; c < layer_grid.cols; ++c) {
            layer_grid(r, c) = static_cast<uint8_t>(100 + r + c);
        }
    }

    SUBCASE("Sample at specific points") {
        // Direct access to raster grid for sampling
        const auto &elev_layer = zone.plot().grid().get_layer(1);
        const auto &elev_grid = std::get<dp::Grid<uint8_t>>(elev_layer.grid);

        // Sample at grid cell (2, 2) - should have value 100 + 2 + 2 = 104
        auto cell = elev_grid(2, 2);
        CHECK(cell == 104);

        // Sample at grid cell (5, 10) - should have value 100 + 5 + 10 = 115
        auto cell2 = elev_grid(5, 10);
        CHECK(cell2 == 115);
    }

    SUBCASE("Sample at grid corners") {
        const auto &elev_layer = zone.plot().grid().get_layer(1);
        const auto &elev_grid = std::get<dp::Grid<uint8_t>>(elev_layer.grid);

        // Sample at corner (0, 0) - should have value 100 + 0 + 0 = 100
        auto corner = elev_grid(0, 0);
        CHECK(corner == 100);

        // Sample at opposite corner (9, 19) - should have value 100 + 9 + 19 = 128
        auto far_corner = elev_grid(9, 19);
        CHECK(far_corner == 128);
    }
}

TEST_CASE("Zone geometric operations") {
    SUBCASE("Area and perimeter calculations") {
        // Create simple base grid for Zone constructor
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = dp::Pose{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        base_grid.data.resize(10 * 10, 0);

        // Rectangle: 100m x 50m = 5000 m²
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Geometry Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

        CHECK(zone.poly().area() == doctest::Approx(5000.0));
        CHECK(zone.poly().perimeter() == doctest::Approx(300.0)); // 2 * (100 + 50)
    }

    SUBCASE("Point containment") {
        // Create simple base grid for Zone constructor
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = dp::Pose{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        base_grid.data.resize(10 * 10, 0);

        auto boundary = createRectangle(10, 10, 80, 60);
        Zone zone("Containment Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

        // Points inside
        CHECK(zone.poly().contains(dp::Point(50, 40, 0)));
        CHECK(zone.poly().contains(dp::Point(20, 20, 0)));
        CHECK(zone.poly().contains(dp::Point(80, 60, 0)));

        // Points outside
        CHECK(!zone.poly().contains(dp::Point(5, 5, 0)));
        CHECK(!zone.poly().contains(dp::Point(100, 100, 0)));
        CHECK(!zone.poly().contains(dp::Point(50, 5, 0)));

        // Points on boundary (behavior may vary)
        zone.poly().contains(dp::Point(10, 40, 0)); // Left edge
        zone.poly().contains(dp::Point(90, 40, 0)); // Right edge
    }

    SUBCASE("Complex polygon") {
        // Create L-shaped polygon
        dp::Polygon l_boundary;
        l_boundary.vertices.emplace_back(0, 0, 0);
        l_boundary.vertices.emplace_back(60, 0, 0);
        l_boundary.vertices.emplace_back(60, 30, 0);
        l_boundary.vertices.emplace_back(30, 30, 0);
        l_boundary.vertices.emplace_back(30, 60, 0);
        l_boundary.vertices.emplace_back(0, 60, 0);

        // Create simple base grid for Zone constructor
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = dp::Pose{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        base_grid.data.resize(10 * 10, 0);

        Zone l_zone("L-Shape Zone", "field", l_boundary, base_grid, WAGENINGEN_DATUM);

        // Points in different parts of the L
        CHECK(l_zone.poly().contains(dp::Point(15, 15, 0))); // Bottom part
        CHECK(l_zone.poly().contains(dp::Point(15, 45, 0))); // Left part
        CHECK(l_zone.poly().contains(dp::Point(45, 15, 0))); // Right part

        // Point in the "notch" of the L
        CHECK(!l_zone.poly().contains(dp::Point(45, 45, 0)));

        // Area should be: 60*30 + 30*30 = 1800 + 900 = 2700
        CHECK(l_zone.poly().area() == doctest::Approx(2700.0));
    }
}

TEST_CASE("Zone validation rules") {
    SUBCASE("Valid zones") {
        // Create simple base grid for Zone constructor
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = dp::Pose{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        base_grid.data.resize(10 * 10, 0);

        auto boundary = createRectangle(0, 0, 100, 50);
        Zone valid_zone("Valid Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(valid_zone.is_valid());

        // Zone with just name and boundary is valid
        Zone minimal_zone("Minimal", "other", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(minimal_zone.is_valid());
    }

    SUBCASE("Invalid zones") {
        // Create simple base grid for Zone constructor
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = dp::Pose{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        base_grid.data.resize(10 * 10, 0);

        // No boundary
        dp::Polygon default_boundary;
        Zone no_boundary("No Boundary", "field", default_boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(!no_boundary.is_valid());

        // Empty name
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone empty_name("", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(!empty_name.is_valid());

        // Both empty name and no boundary
        Zone completely_invalid("", "field", default_boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(!completely_invalid.is_valid());
    }
}

TEST_CASE("Zone file I/O operations") {
    // Create simple base grid for Zone constructor
    dp::Grid<uint8_t> base_grid;
    base_grid.rows = 10;
    base_grid.cols = 10;
    base_grid.resolution = 1.0;
    base_grid.centered = true;
    base_grid.pose = dp::Pose{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
    base_grid.data.resize(10 * 10, 0);

    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("File I/O Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    zone.plot().grid().add_grid(10, 5, "elevation", "terrain", {{"units", "meters"}});

    // Add field elements
    dp::Segment row_line({10, 25, 0}, {90, 25, 0});
    zone.poly().add_line_element(row_line, "crop_row");

    SUBCASE("Save and load files") {
        const std::string vector_path = "/tmp/zoneout_test_zone.geojson";
        const std::string raster_path = "/tmp/zoneout_test_zone.tiff";

        // Save zone
        zone.save_plot_files(vector_path, raster_path);

        // Load zone back
        auto loaded_zone = Zone::load_plot_files(vector_path, raster_path);

        // Basic properties should be preserved
        CHECK(loaded_zone.name() == "File I/O Zone");
        CHECK(loaded_zone.type() == "field");

        // Note: Detailed validation depends on actual vectkit/rastkit implementation
        // This test mainly ensures the methods don't crash
    }
}

TEST_CASE("Zone property edge cases") {
    // Create simple base grid for Zone constructor
    dp::Grid<uint8_t> base_grid;
    base_grid.rows = 10;
    base_grid.cols = 10;
    base_grid.resolution = 1.0;
    base_grid.centered = true;
    base_grid.pose = dp::Pose{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
    base_grid.data.resize(10 * 10, 0);

    dp::Polygon default_boundary;
    Zone zone("Edge Case Zone", "field", default_boundary, base_grid, WAGENINGEN_DATUM);

    SUBCASE("Property overwrites") {
        zone.set_property("test_key", "value1");
        CHECK(zone.property("test_key").value_or("") == "value1");

        zone.set_property("test_key", "value2");
        CHECK(zone.property("test_key").value_or("") == "value2");
    }

    SUBCASE("Empty property values") {
        zone.set_property("empty_key", "");
        CHECK(zone.property("empty_key").value_or("missing") == "");
        CHECK(zone.property("empty_key").has_value());
    }

    SUBCASE("Special characters in properties") {
        zone.set_property("special", "value with spaces and symbols!@#$%");
        CHECK(zone.property("special").value_or("") == "value with spaces and symbols!@#$%");

        zone.set_property("unicode", "café naïve résumé");
        CHECK(zone.property("unicode").value_or("") == "café naïve résumé");
    }

    SUBCASE("Large number of properties") {
        // Add many properties
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = "value_" + std::to_string(i * 2);
            zone.set_property(key, value);
        }

        // Verify they're all there
        auto properties = zone.properties();
        CHECK(properties.size() == 1000);

        // Check specific values
        CHECK(zone.property("key_42").value_or("") == "value_84");
        CHECK(zone.property("key_999").value_or("") == "value_1998");
    }
}
