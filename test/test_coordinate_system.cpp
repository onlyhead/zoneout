#include "doctest/doctest.h"
#include "vectkit/vectkit.hpp"
#include "zoneout/zoneout.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace dp = datapod;
using namespace zoneout;

TEST_CASE("Test coordinate system ordering") {

    SUBCASE("Analyze grid coordinate system vs polygon") {
        // Create a simple test polygon with known coordinates
        dp::Polygon test_polygon;
        test_polygon.vertices = {
            dp::Point{0.0, 0.0, 0.0},   // Bottom-left
            dp::Point{10.0, 0.0, 0.0},  // Bottom-right
            dp::Point{10.0, 10.0, 0.0}, // Top-right
            dp::Point{0.0, 10.0, 0.0}   // Top-left
        };
        dp::Geo test_datum{0.0, 0.0, 0.0};

        // Create Zone with this simple polygon
        Zone zone("Test", "test", test_polygon, test_datum, 1.0);

        // Get the generated grid
        const auto &grid_data = zone.plot().grid().raster();
        REQUIRE(!grid_data.layers.empty());
        const auto &first_layer = grid_data.layers[0];
        const auto &grid_variant = first_layer.grid;

        std::cout << "\n=== Coordinate System Analysis ===\n";
        std::cout << "Polygon AABB:\n";
        auto aabb = test_polygon.get_aabb();
        std::cout << "  Min: " << aabb.min_point.x << ", " << aabb.min_point.y << std::endl;
        std::cout << "  Max: " << aabb.max_point.x << ", " << aabb.max_point.y << std::endl;
        std::cout << "  Center: " << aabb.center().x << ", " << aabb.center().y << std::endl;

        std::cout << "\nGrid info:\n";
        std::cout << "  Dimensions: " << first_layer.width << " x " << first_layer.height << std::endl;
        std::cout << "  Resolution: " << first_layer.resolution << std::endl;

        std::visit(
            [&](const auto &grid) {
                // Check grid corner coordinates
                std::cout << "\nGrid corner coordinates:\n";
                auto top_left = grid.get_point(0, 0);
                auto top_right = grid.get_point(0, grid.cols - 1);
                auto bottom_left = grid.get_point(grid.rows - 1, 0);
                auto bottom_right = grid.get_point(grid.rows - 1, grid.cols - 1);

                std::cout << "  Top-left (0,0): " << top_left.x << ", " << top_left.y << std::endl;
                std::cout << "  Top-right (0,cols-1): " << top_right.x << ", " << top_right.y << std::endl;
                std::cout << "  Bottom-left (rows-1,0): " << bottom_left.x << ", " << bottom_left.y << std::endl;
                std::cout << "  Bottom-right (rows-1,cols-1): " << bottom_right.x << ", " << bottom_right.y << std::endl;

                // Check center coordinates
                auto center_r = grid.rows / 2;
                auto center_c = grid.cols / 2;
                auto grid_center = grid.get_point(center_r, center_c);
                std::cout << "  Grid center (" << center_r << "," << center_c << "): " << grid_center.x << ", "
                          << grid_center.y << std::endl;

                // Compare expected vs actual
                std::cout << "\nComparison:\n";
                std::cout << "  Expected polygon center: " << aabb.center().x << ", " << aabb.center().y << std::endl;
                std::cout << "  Actual grid center: " << grid_center.x << ", " << grid_center.y << std::endl;
                std::cout << "  Difference: " << (grid_center.x - aabb.center().x) << ", "
                          << (grid_center.y - aabb.center().y) << std::endl;

                // Check if Y increases upward or downward in grid
                auto row_0 = grid.get_point(0, 0);
                auto row_1 = grid.get_point(1, 0);
                std::cout << "\nY coordinate ordering:\n";
                std::cout << "  Row 0 Y: " << row_0.y << std::endl;
                std::cout << "  Row 1 Y: " << row_1.y << std::endl;
                if (row_1.y > row_0.y) {
                    std::cout << "  Y increases downward (row 0 = top)" << std::endl;
                } else {
                    std::cout << "  Y increases upward (row 0 = bottom)" << std::endl;
                }
            },
            grid_variant);

        // Test with reverse_y=true
        std::cout << "\n=== Testing with reverse_y=true ===\n";
        dp::Grid<uint8_t> reverse_grid;
        reverse_grid.rows = 10;
        reverse_grid.cols = 10;
        reverse_grid.resolution = 1.0;
        reverse_grid.centered = true;
        reverse_grid.pose = dp::Pose{aabb.center(), dp::Euler{0, 0, 0}.to_quaternion()};
        reverse_grid.data.resize(10 * 10, 0);

        auto rev_top_left = reverse_grid.get_point(0, 0);
        auto rev_bottom_left = reverse_grid.get_point(9, 0);
        std::cout << "Reverse Y grid:\n";
        std::cout << "  Top-left (0,0): " << rev_top_left.x << ", " << rev_top_left.y << std::endl;
        std::cout << "  Bottom-left (9,0): " << rev_bottom_left.x << ", " << rev_bottom_left.y << std::endl;

        if (rev_top_left.y > rev_bottom_left.y) {
            std::cout << "  With reverse_y=true: row 0 has higher Y (correct for GIS)" << std::endl;
        } else {
            std::cout << "  With reverse_y=true: row 0 has lower Y" << std::endl;
        }
    }
}
