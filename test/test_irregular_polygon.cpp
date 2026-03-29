#include "doctest/doctest.h"
#include "zoneout/zoneout.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace dp = datapod;
using namespace zoneout;

TEST_CASE("Test irregular polygon alignment") {
    // Use fixed coordinates for testing
    dp::Geo test_datum{52.0, 5.0, 0.0};

    SUBCASE("Irregular trapezoid polygon") {
        // Create an irregular trapezoid (not axis-aligned)
        dp::Polygon trapezoid;
        trapezoid.vertices.push_back({10, 20, 0}); // Bottom-left (offset from origin)
        trapezoid.vertices.push_back({90, 15, 0}); // Bottom-right (slightly lower)
        trapezoid.vertices.push_back({85, 85, 0}); // Top-right (not square)
        trapezoid.vertices.push_back({15, 80, 0}); // Top-left (not square)

        // Create zone with 1m resolution
        Zone zone("Irregular Field", "agricultural", trapezoid, test_datum, 1.0);
        zone.set_property("shape", "trapezoid");

        // Save to files
        std::string output_dir = "/tmp/test_irregular";
        std::filesystem::remove_all(output_dir);
        zone.save(output_dir);

        // Get the grid info
        const auto &grid_data = zone.plot().grid().raster();
        if (!grid_data.layers.empty()) {
            const auto &first_layer = grid_data.layers[0];

            std::cout << "\n=== Irregular Polygon Analysis ===" << std::endl;
            std::cout << "Grid dimensions: " << first_layer.width << " x " << first_layer.height << std::endl;

            // Get polygon AABB
            auto aabb = trapezoid.get_aabb();
            std::cout << "Polygon AABB min: " << aabb.min_point.x << ", " << aabb.min_point.y << std::endl;
            std::cout << "Polygon AABB max: " << aabb.max_point.x << ", " << aabb.max_point.y << std::endl;
            std::cout << "Polygon AABB center: " << aabb.center().x << ", " << aabb.center().y << std::endl;
            std::cout << "Polygon AABB size: " << dp::Point(aabb.max_point.x - aabb.min_point.x, aabb.max_point.y - aabb.min_point.y, 0).x << " x " << dp::Point(aabb.max_point.x - aabb.min_point.x, aabb.max_point.y - aabb.min_point.y, 0).y << std::endl;

            // Get grid shift (center)
            auto shift = grid_data.shift;
            std::cout << "Grid center (shift): " << shift.point.x << ", " << shift.point.y << ", " << shift.point.z
                      << std::endl;
            std::cout << "Grid resolution: " << first_layer.resolution << std::endl;

            // Calculate grid bounds
            double grid_width_meters = first_layer.width * 1.0;
            double grid_height_meters = first_layer.height * 1.0;
            double grid_min_x = shift.point.x - grid_width_meters / 2;
            double grid_max_x = shift.point.x + grid_width_meters / 2;
            double grid_min_y = shift.point.y - grid_height_meters / 2;
            double grid_max_y = shift.point.y + grid_height_meters / 2;

            std::cout << "Grid bounds (ENU):" << std::endl;
            std::cout << "  Min: " << grid_min_x << ", " << grid_min_y << std::endl;
            std::cout << "  Max: " << grid_max_x << ", " << grid_max_y << std::endl;

            // Check if grid properly encompasses polygon
            double margin_x_min = aabb.min_point.x - grid_min_x;
            double margin_x_max = grid_max_x - aabb.max_point.x;
            double margin_y_min = aabb.min_point.y - grid_min_y;
            double margin_y_max = grid_max_y - aabb.max_point.y;

            std::cout << "Margins (grid extends beyond polygon):" << std::endl;
            std::cout << "  Left: " << margin_x_min << "m" << std::endl;
            std::cout << "  Right: " << margin_x_max << "m" << std::endl;
            std::cout << "  Bottom: " << margin_y_min << "m" << std::endl;
            std::cout << "  Top: " << margin_y_max << "m" << std::endl;

            // With current padding approach, expect symmetric margins around 1m
            CHECK(margin_x_min >= 0.8); // Left margin ~1m
            CHECK(margin_x_min <= 1.2);
            CHECK(margin_y_min >= 0.8); // Bottom margin ~1m
            CHECK(margin_y_min <= 1.2);
            CHECK(margin_x_max >= 0.8); // Right margin ~1m
            CHECK(margin_x_max <= 1.2);
            CHECK(margin_y_max >= 0.8); // Top margin ~1m
            CHECK(margin_y_max <= 1.2);
        }

        // Verify files exist
        CHECK(std::filesystem::exists(output_dir + "/vector.geojson"));
        CHECK(std::filesystem::exists(output_dir + "/raster.tiff"));

        INFO("Files saved to: " << output_dir);
        INFO("Load both files in QGIS to check alignment");
    }

    SUBCASE("Diamond-shaped polygon") {
        // Create a diamond (45-degree rotated square)
        dp::Polygon diamond;
        diamond.vertices.push_back({50, 0, 0});   // Bottom
        diamond.vertices.push_back({100, 50, 0}); // Right
        diamond.vertices.push_back({50, 100, 0}); // Top
        diamond.vertices.push_back({0, 50, 0});   // Left

        Zone zone("Diamond Field", "agricultural", diamond, test_datum, 1.0);
        zone.set_property("shape", "diamond");

        std::string output_dir = "/tmp/test_diamond";
        std::filesystem::remove_all(output_dir);
        zone.save(output_dir);

        INFO("Diamond field saved to: " << output_dir);
    }
}
