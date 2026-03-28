#include "doctest/doctest.h"
#include "vectkit/vectkit.hpp"
#include "zoneout/zoneout.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace dp = datapod;
using namespace zoneout;

TEST_CASE("Test real irregular field from misc/field4.geojson") {

    SUBCASE("Real field polygon analysis") {
        // Try multiple paths to find the GeoJSON file
        std::string geojson_path;
        std::vector<std::string> possible_paths = {"../misc/field4.geojson", "misc/field4.geojson",
                                                   "../../misc/field4.geojson"};
        for (const auto &path : possible_paths) {
            if (std::filesystem::exists(path)) {
                geojson_path = path;
                break;
            }
        }
        REQUIRE_MESSAGE(!geojson_path.empty(), "Could not find misc/field4.geojson");

        // Use vectkit library to read the GeoJSON file
        auto feature_collection = vectkit::ReadFeatureCollection(geojson_path);

        REQUIRE(feature_collection.features.size() == 1);

        // Extract the polygon and create a Zone
        auto &feature = feature_collection.features[0];
        auto *polygon_ptr = std::get_if<dp::Polygon>(&feature.geometry);
        REQUIRE(polygon_ptr != nullptr);

        // Create Zone with the parsed polygon
        Zone zone("Pea Field", "agricultural", *polygon_ptr, feature_collection.datum, 1.0);
        // Save to files
        std::string output_dir = "/tmp/test_real_field";
        std::filesystem::remove_all(output_dir);
        zone.save(output_dir);

        // Get the grid info for analysis
        const auto &grid_data = zone.plot().grid().raster();
        if (!grid_data.layers.empty()) {
            const auto &first_layer = grid_data.layers[0];

            std::cout << "\n=== Real Field Analysis ===\n";
            std::cout << "Zone name: " << zone.name() << std::endl;
            std::cout << "Grid dimensions: " << first_layer.width << " x " << first_layer.height << std::endl;
            std::cout << "Grid resolution: " << first_layer.resolution << "m" << std::endl;

            // Get polygon boundary for analysis
            auto boundary = zone.poly().field_boundary();
            auto aabb = boundary.get_aabb();

            std::cout << "Polygon AABB (ENU coordinates):\n";
            std::cout << "  Min: " << std::fixed << std::setprecision(6) << aabb.min_point.x << ", " << aabb.min_point.y
                      << std::endl;
            std::cout << "  Max: " << std::fixed << std::setprecision(6) << aabb.max_point.x << ", " << aabb.max_point.y
                      << std::endl;
            double aabb_width = aabb.max_point.x - aabb.min_point.x;
            double aabb_height = aabb.max_point.y - aabb.min_point.y;
            std::cout << "  Size: " << aabb_width << "m x " << aabb_height << "m" << std::endl;

            // Get grid shift (center)
            auto shift = grid_data.shift;
            std::cout << "Grid center (shift): " << shift.point.x << ", " << shift.point.y << std::endl;

            // Check grid vs polygon alignment
            double grid_width = first_layer.width * first_layer.resolution;
            double grid_height = first_layer.height * first_layer.resolution;
            double grid_min_x = shift.point.x - grid_width / 2;
            double grid_max_x = shift.point.x + grid_width / 2;
            double grid_min_y = shift.point.y - grid_height / 2;
            double grid_max_y = shift.point.y + grid_height / 2;

            std::cout << "Grid bounds (ENU):\n";
            std::cout << "  Min: " << grid_min_x << ", " << grid_min_y << std::endl;
            std::cout << "  Max: " << grid_max_x << ", " << grid_max_y << std::endl;
            std::cout << "  Size: " << grid_width << "m x " << grid_height << "m" << std::endl;

            // Check for the vertical shift issue
            double height_diff = grid_height - aabb_height;
            std::cout << "\nHeight difference analysis:" << std::endl;
            std::cout << "  Grid height: " << grid_height << "m" << std::endl;
            std::cout << "  Polygon height: " << aabb_height << "m" << std::endl;
            std::cout << "  Difference: " << height_diff << "m" << std::endl;

            if (std::abs(height_diff) > 0.1) {
                std::cout << "  *** HEIGHT MISMATCH DETECTED ***" << std::endl;
            }
        }

        // Verify files exist
        CHECK(std::filesystem::exists(output_dir + "/vector.geojson"));
        CHECK(std::filesystem::exists(output_dir + "/raster.tiff"));

        std::cout << "\nFiles saved to: " << output_dir << std::endl;
        std::cout << "Load both files in QGIS to check alignment" << std::endl;
    }
}
