#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <unordered_map>
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

// Helper function to compare two points with tolerance
bool comparePoints(const dp::Point &p1, const dp::Point &p2, double tolerance = 0.01) {
    return std::abs(p1.x - p2.x) < tolerance && std::abs(p1.y - p2.y) < tolerance && std::abs(p1.z - p2.z) < tolerance;
}

// Helper function to compare two polygons
bool comparePolygons(const dp::Polygon &poly1, const dp::Polygon &poly2) {
    auto points1 = poly1.vertices;
    auto points2 = poly2.vertices;

    if (points1.size() != points2.size()) {
        std::cout << "Polygon size mismatch: " << points1.size() << " vs " << points2.size() << std::endl;
        return false;
    }

    for (size_t i = 0; i < points1.size(); ++i) {
        if (!comparePoints(points1[i], points2[i])) {
            std::cout << "Point " << i << " mismatch: (" << points1[i].x << ", " << points1[i].y << ", " << points1[i].z
                      << ") vs (" << points2[i].x << ", " << points2[i].y << ", " << points2[i].z << ")" << std::endl;
            return false;
        }
    }
    return true;
}

TEST_CASE("Complete file I/O round-trip test") {
    SUBCASE("Zone with mixed element types and raster data") {
        // Create a zone with various element types
        auto field_boundary = createRectangle(0, 0, 200, 150);
        Zone original_zone("Test Agricultural Zone", "field", field_boundary, WAGENINGEN_DATUM, 1.0);

        // Set zone properties
        original_zone.set_property("crop_type", "wheat");
        original_zone.set_property("planting_date", "2024-03-15");
        original_zone.set_property("irrigation_schedule", "daily_morning");
        original_zone.set_property("soil_type", "clay_loam");

        // Add parking spaces (polygons)
        auto parking_area1 = createRectangle(210, 10, 25, 20);
        original_zone.poly().add_polygon_element(parking_area1, "parking_space",
                                                 {{"name", "main_parking"},
                                                  {"capacity", "8_vehicles"},
                                                  {"surface", "asphalt"},
                                                  {"lighting", "LED"},
                                                  {"access_hours", "24_7"}});

        auto parking_area2 = createRectangle(210, 40, 15, 15);
        original_zone.poly().add_polygon_element(
            parking_area2, "parking_space",
            {{"name", "equipment_parking"}, {"capacity", "3_tractors"}, {"surface", "gravel"}, {"covered", "no"}});

        // Add storage areas (polygons)
        auto storage_building = createRectangle(250, 10, 40, 30);
        original_zone.poly().add_polygon_element(storage_building, "storage_facility",
                                                 {{"name", "main_warehouse"},
                                                  {"capacity", "500_tons"},
                                                  {"climate_control", "yes"},
                                                  {"security_level", "high"},
                                                  {"fire_suppression", "sprinkler_system"}});

        auto outdoor_storage = createRectangle(250, 50, 30, 25);
        original_zone.poly().add_polygon_element(outdoor_storage, "storage_area",
                                                 {{"name", "bulk_storage"},
                                                  {"capacity", "200_tons"},
                                                  {"weather_protection", "partial"},
                                                  {"material_type", "fertilizer"}});

        // Add access routes (lines/segments)
        dp::Segment main_road{dp::Point{-10, 75, 0}, dp::Point{300, 75, 0}};
        original_zone.poly().add_line_element(main_road, "access_route",
                                              {{"name", "main_access_road"},
                                               {"width", "6m"},
                                               {"surface", "paved"},
                                               {"speed_limit", "25kmh"},
                                               {"weight_limit", "40_tons"}});

        dp::Segment service_path{dp::Point{100, 0, 0}, dp::Point{100, 150, 0}};
        original_zone.poly().add_line_element(
            service_path, "service_route",
            {{"name", "north_south_service"}, {"width", "3m"}, {"surface", "gravel"}, {"access", "maintenance_only"}});

        // Add equipment stations (points)
        original_zone.poly().add_point_element(dp::Point{50, 50, 0}, "equipment_station",
                                               {{"name", "fuel_station"},
                                                {"fuel_type", "diesel"},
                                                {"capacity", "5000L"},
                                                {"pump_rate", "60L_per_min"},
                                                {"safety_zone", "10m_radius"}});

        original_zone.poly().add_point_element(dp::Point{150, 100, 0}, "monitoring_point",
                                               {{"name", "weather_station"},
                                                {"sensors", "temp_humidity_wind_rain"},
                                                {"data_interval", "5_minutes"},
                                                {"power_source", "solar"},
                                                {"communication", "4G_cellular"}});

        original_zone.poly().add_point_element(dp::Point{75, 25, 0}, "irrigation_hub",
                                               {{"name", "central_irrigation"},
                                                {"water_source", "well"},
                                                {"flow_rate", "200L_per_min"},
                                                {"pressure", "4_bar"},
                                                {"zones_served", "4"}});

        // Add work areas with different purposes
        auto spray_zone = createRectangle(20, 20, 60, 40);
        original_zone.poly().add_polygon_element(spray_zone, "treatment_area",
                                                 {{"name", "pesticide_application_zone"},
                                                  {"last_treated", "2024-06-20"},
                                                  {"chemical_used", "organic_insecticide"},
                                                  {"re_entry_safe", "2024-06-22"},
                                                  {"buffer_zone", "5m"}});

        auto harvest_zone = createRectangle(120, 80, 70, 50);
        original_zone.poly().add_polygon_element(harvest_zone, "harvest_area",
                                                 {{"name", "ready_for_harvest"},
                                                  {"crop_maturity", "95_percent"},
                                                  {"estimated_yield", "8_tons_per_hectare"},
                                                  {"harvest_window", "2024-07-01_to_2024-07-15"},
                                                  {"priority", "high"}});

        // Verify original zone state
        CHECK(original_zone.name() == "Test Agricultural Zone");
        CHECK(original_zone.type() == "field");
        CHECK(original_zone.property("crop_type").value_or("") == "wheat");

        // Count elements
        size_t polygon_count = original_zone.poly().polygon_elements().size();
        size_t line_count = original_zone.poly().line_elements().size();
        size_t point_count = original_zone.poly().point_elements().size();
        CHECK(polygon_count == 6); // 6 polygon elements
        CHECK(line_count == 2);    // 2 line elements
        CHECK(point_count == 3);   // 3 point elements

        // Save to files
        std::string test_dir = "/tmp/test_roundtrip_zone";

        // Clean up any existing files
        std::filesystem::remove_all(test_dir);

        REQUIRE_NOTHROW(original_zone.save(test_dir));
        CHECK(std::filesystem::exists(test_dir));

        // Load the zone back
        Zone loaded_zone = Zone::load(test_dir);

        // === Verify Basic Zone Properties ===
        CHECK(loaded_zone.name() == original_zone.name());
        CHECK(loaded_zone.type() == original_zone.type());

        // Verify zone properties
        CHECK(loaded_zone.property("crop_type").value_or("") == "wheat");
        CHECK(loaded_zone.property("planting_date").value_or("") == "2024-03-15");
        CHECK(loaded_zone.property("irrigation_schedule").value_or("") == "daily_morning");
        CHECK(loaded_zone.property("soil_type").value_or("") == "clay_loam");

        // === Verify Field Boundary ===
        CHECK(loaded_zone.poly().has_field_boundary());
        CHECK(comparePolygons(loaded_zone.poly().field_boundary(), original_zone.poly().field_boundary()));

        // === Verify Vector Elements ===
        // Verify specific element types were preserved
        auto original_parking = original_zone.poly().polygons_by_type("parking_space");
        auto loaded_parking = loaded_zone.poly().polygons_by_type("parking_space");
        CHECK(loaded_parking.size() == original_parking.size());
        CHECK(loaded_parking.size() == 2);

        auto original_storage = original_zone.poly().polygons_by_type("storage_facility");
        auto loaded_storage = loaded_zone.poly().polygons_by_type("storage_facility");
        CHECK(loaded_storage.size() == original_storage.size());
        CHECK(loaded_storage.size() == 1);

        auto original_routes = original_zone.poly().lines_by_type("access_route");
        auto loaded_routes = loaded_zone.poly().lines_by_type("access_route");
        CHECK(loaded_routes.size() == original_routes.size());
        CHECK(loaded_routes.size() == 1);

        auto original_points = original_zone.poly().points_by_type("equipment_station");
        auto loaded_points = loaded_zone.poly().points_by_type("equipment_station");
        CHECK(loaded_points.size() == original_points.size());
        CHECK(loaded_points.size() == 1);

        // Verify raster layers were preserved (should have base grid from resolution)
        CHECK(loaded_zone.plot().grid().has_layers() == original_zone.plot().grid().has_layers());

        // Clean up test files
        std::filesystem::remove_all(test_dir);

        std::cout << "File I/O round-trip test completed!" << std::endl;
    }
}
