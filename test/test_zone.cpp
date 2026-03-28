#include <doctest/doctest.h>

#include <chrono>
#include <thread>
#include <vector>

#include "zoneout/zoneout.hpp"

namespace dp = datapod;
using namespace zoneout;

// Wageningen Research Labs coordinates
const dp::Geo WAGENINGEN_DATUM{51.98776171041831, 5.662378206146002, 0.0};

// Helper function to create a simple rectangular polygon
dp::Polygon createRectangle(double x, double y, double width, double height) {
    dp::Polygon poly;
    poly.vertices.emplace_back(x, y, 0.0);
    poly.vertices.emplace_back(x + width, y, 0.0);
    poly.vertices.emplace_back(x + width, y + height, 0.0);
    poly.vertices.emplace_back(x, y + height, 0.0);
    return poly;
}

TEST_CASE("Zone creation and basic properties") {
    SUBCASE("Default constructor") {
        // Create simple base grid for Zone constructor
        dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = shift;
        base_grid.data.resize(100);

        dp::Polygon default_boundary;
        Zone zone("", "other", default_boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(zone.name().empty());
        CHECK(zone.type() == "other");
        CHECK(!zone.poly().has_field_boundary());
        CHECK(zone.plot().grid().raster().layers.size() == 1); // Zone now includes the base grid
    }

    SUBCASE("Named constructor") {
        // Create simple base grid for Zone constructor
        dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = shift;
        base_grid.data.resize(100);

        dp::Polygon default_boundary;
        Zone zone("Test Zone", "field", default_boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(zone.name() == "Test Zone");
        CHECK(zone.type() == "field");
        CHECK(!zone.poly().has_field_boundary());
        CHECK(zone.plot().grid().raster().layers.size() == 1); // Zone now includes the base grid
    }

    SUBCASE("Constructor with boundary") {
        // Create simple base grid for Zone constructor
        dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = shift;
        base_grid.data.resize(100);

        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Test Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(zone.name() == "Test Zone");
        CHECK(zone.type() == "field");
        CHECK(zone.poly().has_field_boundary());
        CHECK(zone.poly().area() == 5000.0); // 100 * 50
    }
}

TEST_CASE("Zone constructor with auto-generated grid") {
    SUBCASE("Constructor with default resolution (1.0)") {
        // Create a 100x50 rectangle - default resolution should be 1.0
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Auto Grid Zone", "field", boundary, WAGENINGEN_DATUM);

        CHECK(zone.name() == "Auto Grid Zone");
        CHECK(zone.type() == "field");
        CHECK(zone.poly().has_field_boundary());
        CHECK(zone.plot().grid().layer_count() == 1); // Should have the auto-generated base grid with noise

        // Check that grid dimensions are reasonable for the polygon size
        // For a 100x50 rectangle with resolution 1.0, we expect roughly 100x50 cells
        auto grid_info = zone.raster_info();
        CHECK(!grid_info.empty());
    }

    SUBCASE("Constructor with custom resolution (2.0)") {
        // Create a 100x50 rectangle with 2x2 cell resolution
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Custom Resolution Zone", "field", boundary, WAGENINGEN_DATUM, 2.0);

        CHECK(zone.name() == "Custom Resolution Zone");
        CHECK(zone.type() == "field");
        CHECK(zone.poly().has_field_boundary());
        CHECK(zone.plot().grid().layer_count() == 1);

        // With resolution 2.0, grid should have roughly half the cells in each dimension
        auto grid_info = zone.raster_info();
        CHECK(!grid_info.empty());
    }

    SUBCASE("Constructor with fine resolution (0.5)") {
        // Create a smaller polygon with fine resolution
        auto boundary = createRectangle(0, 0, 10, 5);
        Zone zone("Fine Resolution Zone", "field", boundary, WAGENINGEN_DATUM, 0.5);

        CHECK(zone.name() == "Fine Resolution Zone");
        CHECK(zone.type() == "field");
        CHECK(zone.poly().has_field_boundary());
        CHECK(zone.plot().grid().layer_count() == 1);

        // With resolution 0.5, grid should have more cells for the same area
        auto grid_info = zone.raster_info();
        CHECK(!grid_info.empty());
    }

    SUBCASE("Constructor with complex polygon") {
        // Create L-shaped polygon
        dp::Polygon l_boundary;
        l_boundary.vertices.emplace_back(0, 0, 0);
        l_boundary.vertices.emplace_back(60, 0, 0);
        l_boundary.vertices.emplace_back(60, 30, 0);
        l_boundary.vertices.emplace_back(30, 30, 0);
        l_boundary.vertices.emplace_back(30, 60, 0);
        l_boundary.vertices.emplace_back(0, 60, 0);
        Zone zone("L-Shape Auto Grid", "field", l_boundary, WAGENINGEN_DATUM);

        CHECK(zone.name() == "L-Shape Auto Grid");
        CHECK(zone.type() == "field");
        CHECK(zone.poly().has_field_boundary());
        CHECK(zone.plot().grid().layer_count() == 1);

        // Grid should be generated based on the OBB of the L-shape
        auto grid_info = zone.raster_info();
        CHECK(!grid_info.empty());
    }
}

TEST_CASE("Zone poly_cut functionality") {
    SUBCASE("addRasterLayer with poly_cut=true") {
        // Create an L-shaped polygon
        dp::Polygon l_boundary;
        l_boundary.vertices.emplace_back(0, 0, 0);
        l_boundary.vertices.emplace_back(60, 0, 0);
        l_boundary.vertices.emplace_back(60, 30, 0);
        l_boundary.vertices.emplace_back(30, 30, 0);
        l_boundary.vertices.emplace_back(30, 60, 0);
        l_boundary.vertices.emplace_back(0, 60, 0);

        Zone zone("L-Shape Zone", "field", l_boundary, WAGENINGEN_DATUM);

        // Create a separate grid that covers the entire bounding box
        dp::Pose shift{dp::Point{30.0, 30.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> full_grid;
        full_grid.rows = 60;
        full_grid.cols = 60;
        full_grid.resolution = 1.0;
        full_grid.centered = true;
        full_grid.pose = shift;
        full_grid.data.resize(60 * 60);

        // Fill the entire grid with value 255
        for (size_t r = 0; r < full_grid.rows; ++r) {
            for (size_t c = 0; c < full_grid.cols; ++c) {
                full_grid(r, c) = 255;
            }
        }

        // Add the grid with poly_cut=true - should zero out cells outside the L-shape
        zone.add_raster_layer(full_grid, "test_layer", "test", {}, true);

        CHECK(zone.plot().grid().layer_count() == 2); // Base grid + test layer
    }

    SUBCASE("addRasterLayer with poly_cut=false (default)") {
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Test Zone", "field", boundary, WAGENINGEN_DATUM);

        // Create a grid
        dp::Pose shift{dp::Point{50.0, 25.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> test_grid;
        test_grid.rows = 50;
        test_grid.cols = 100;
        test_grid.resolution = 1.0;
        test_grid.centered = true;
        test_grid.pose = shift;
        test_grid.data.resize(50 * 100);

        // Fill with test values
        for (size_t r = 0; r < test_grid.rows; ++r) {
            for (size_t c = 0; c < test_grid.cols; ++c) {
                test_grid(r, c) = 128;
            }
        }

        // Add without poly_cut - should preserve all values
        zone.add_raster_layer(test_grid, "no_cut_layer", "test");

        CHECK(zone.plot().grid().layer_count() == 2); // Base grid + no_cut_layer
    }
}

TEST_CASE("Zone factory methods") {
    auto boundary = createRectangle(0, 0, 100, 50);

    SUBCASE("createField") {
        // Create simple base grid for Zone constructor
        dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = shift;
        base_grid.data.resize(100);

        auto field = Zone("Wheat Field", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(field.name() == "Wheat Field");
        CHECK(field.type() == "field");
        CHECK(field.poly().has_field_boundary());
    }

    SUBCASE("createBarn") {
        // Create simple base grid for Zone constructor
        dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = shift;
        base_grid.data.resize(100);

        auto barn = Zone("Main Barn", "barn", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(barn.name() == "Main Barn");
        CHECK(barn.type() == "barn");
        CHECK(barn.poly().has_field_boundary());
    }

    SUBCASE("createGreenhouse") {
        // Create simple base grid for Zone constructor
        dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = shift;
        base_grid.data.resize(100);

        auto greenhouse = Zone("Tomato House", "greenhouse", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(greenhouse.name() == "Tomato House");
        CHECK(greenhouse.type() == "greenhouse");
        CHECK(greenhouse.poly().has_field_boundary());
    }
}

TEST_CASE("Zone properties") {
    // Create simple base grid for Zone constructor
    dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
    dp::Grid<uint8_t> base_grid;
    base_grid.rows = 10;
    base_grid.cols = 10;
    base_grid.resolution = 1.0;
    base_grid.centered = true;
    base_grid.pose = shift;
    base_grid.data.resize(100);

    dp::Polygon default_boundary;
    Zone zone("Test Zone", "field", default_boundary, base_grid, WAGENINGEN_DATUM);

    SUBCASE("Set and get properties") {
        zone.set_property("crop_type", "wheat");
        zone.set_property("planted_date", "2024-03-15");

        CHECK(zone.property("crop_type").value_or("") == "wheat");
        CHECK(zone.property("planted_date").value_or("") == "2024-03-15");
        CHECK(!zone.property("non_existent").has_value());
        CHECK(zone.property("non_existent").value_or("default") == "default");
    }
}

TEST_CASE("Zone field elements") {
    // Create simple base grid for Zone constructor
    dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
    dp::Grid<uint8_t> base_grid;
    base_grid.rows = 10;
    base_grid.cols = 10;
    base_grid.resolution = 1.0;
    base_grid.centered = true;
    base_grid.pose = shift;
    base_grid.data.resize(100);

    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    SUBCASE("Add crop rows") {
        dp::Segment row_line({10, 10, 0}, {90, 10, 0});

        std::unordered_map<std::string, std::string> props;
        props["row_number"] = "1";

        zone.poly().add_line_element(row_line, "crop_row", props);

        auto crop_rows = zone.poly().lines_by_type("crop_row");
        CHECK(crop_rows.size() == 1);
    }
}

TEST_CASE("Zone raster layers") {
    // Create simple base grid for Zone constructor
    dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
    dp::Grid<uint8_t> base_grid;
    base_grid.rows = 10;
    base_grid.cols = 10;
    base_grid.resolution = 1.0;
    base_grid.centered = true;
    base_grid.pose = shift;
    base_grid.data.resize(100);

    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    SUBCASE("Add elevation layer") {
        dp::Grid<uint8_t> elevation_grid;
        elevation_grid.rows = 10;
        elevation_grid.cols = 20;
        elevation_grid.resolution = 5.0;
        elevation_grid.centered = true;
        elevation_grid.pose = dp::Pose{};
        elevation_grid.data.resize(10 * 20);

        // Fill with test data
        for (size_t r = 0; r < 10; ++r) {
            for (size_t c = 0; c < 20; ++c) {
                elevation_grid(r, c) = static_cast<uint8_t>(100 + r + c);
            }
        }

        zone.plot().grid().add_grid(elevation_grid, "elevation", "terrain");

        CHECK(zone.plot().grid().raster().layers.size() == 2); // Base grid + elevation layer
    }
}

TEST_CASE("Zone point containment") {
    // Create simple base grid for Zone constructor
    dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
    dp::Grid<uint8_t> base_grid;
    base_grid.rows = 10;
    base_grid.cols = 10;
    base_grid.resolution = 1.0;
    base_grid.centered = true;
    base_grid.pose = shift;
    base_grid.data.resize(100);

    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    SUBCASE("Point inside") {
        dp::Point inside_point(50, 25, 0);
        CHECK(zone.poly().contains(inside_point));
    }

    SUBCASE("Point outside") {
        dp::Point outside_point(150, 25, 0);
        CHECK(!zone.poly().contains(outside_point));
    }

    SUBCASE("Point on boundary") {
        dp::Point boundary_point(0, 25, 0);
        // Note: Polygon.poly().contains() behavior on boundary may vary
        // This test just checks the method works
        zone.poly().contains(boundary_point);
    }
}

TEST_CASE("Zone validation") {
    SUBCASE("Valid zone with boundary") {
        // Create simple base grid for Zone constructor
        dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = shift;
        base_grid.data.resize(100);

        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Test Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(zone.is_valid());
    }

    SUBCASE("Invalid zone without boundary") {
        // Create simple base grid for Zone constructor
        dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = shift;
        base_grid.data.resize(100);

        dp::Polygon default_boundary;
        Zone zone("Test Zone", "field", default_boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(!zone.is_valid());
    }

    SUBCASE("Invalid zone without name") {
        // Create simple base grid for Zone constructor
        dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = shift;
        base_grid.data.resize(100);

        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(!zone.is_valid());
    }
}
