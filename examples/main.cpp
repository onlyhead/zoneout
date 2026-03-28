#include "zoneout/zoneout.hpp"

#include <entropy/generator.hpp>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace dp = datapod;

zoneout::Plot create_field(const std::string &zone_name, const std::string &crop_type,
                           const dp::Geo &datum = dp::Geo{51.98776171041831, 5.662378206146002, 0.0}) {
    zoneout::Plot plot("Wageningen Farm", "agricultural", datum);
    plot.set_property("farm_type", "research");
    plot.set_property("owner", "Wageningen Research Labs");

    typedef std::unordered_map<std::string, std::string> Props;

    geoget::PolygonDrawer drawer(datum);
    if (drawer.start(8080)) {
        const auto polygons = drawer.get_polygons();
        drawer.stop();

        if (!polygons.empty()) {
            zoneout::Zone zone(zone_name, "field", polygons[0], datum, 0.1);

            const auto &base_grid = std::get<dp::Grid<uint8_t>>(zone.grid().get_layer(0).grid);
            auto temp_grid = base_grid;
            auto moisture_grid = base_grid;

            entropy::noise::NoiseGen temp_noise, moisture_noise;
            temp_noise.SetNoiseType(entropy::noise::NoiseGen::NoiseType_Perlin);
            temp_noise.SetFrequency(0.08f);
            temp_noise.SetSeed(std::random_device{}());

            moisture_noise.SetNoiseType(entropy::noise::NoiseGen::NoiseType_OpenSimplex2);
            moisture_noise.SetFrequency(0.05f);
            moisture_noise.SetSeed(std::random_device{}() + 100);

            for (size_t r = 0; r < temp_grid.rows; ++r) {
                for (size_t c = 0; c < temp_grid.cols; ++c) {
                    float temp_noise_val = temp_noise.GetNoise(static_cast<float>(r), static_cast<float>(c));
                    uint8_t temp_value = static_cast<uint8_t>(15 + (temp_noise_val + 1.0f) * 0.5f * (35 - 15));
                    temp_grid(r, c) = temp_value;

                    float moisture_noise_val = moisture_noise.GetNoise(static_cast<float>(r), static_cast<float>(c));
                    uint8_t moisture_value = static_cast<uint8_t>(20 + (moisture_noise_val + 1.0f) * 0.5f * (80 - 20));
                    moisture_grid(r, c) = moisture_value;
                }
            }

            zone.add_raster_layer(temp_grid, "temperature", "environmental", {{"units", "celsius"}}, true);
            zone.add_raster_layer(moisture_grid, "moisture", "environmental", {{"units", "percentage"}}, true);

            zone.set_property("crop_type", crop_type);
            zone.set_property("planting_date", "2024-04-15");
            zone.set_property("irrigation", "true");

            plot.add_zone(zone);
            std::cout << "Added zone: " << zone.name() << " (ID: " << zone.id().toString() << ")" << std::endl;

            // Add remaining polygons as features to the zone that's now in the plot
            if (plot.zone_count() > 0) {
                auto &plot_zone = plot.zones().back(); // Get the zone we just added

                for (size_t i = 1; i < polygons.size(); ++i) {
                    Props properties = {{"area_m2", std::to_string(static_cast<int>(polygons[i].area()))}};
                    std::string feature_name = zone_name + "_feature_" + std::to_string(i);
                    try {
                        plot_zone.add_polygon_element(polygons[i], feature_name, "obstacle", "obstacle", properties);
                    } catch (const std::exception &e) {
                        std::cout << "Failed to add feature " << feature_name << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    }
    return plot;
}

int main() {
    std::string plot_path = "/home/bresilla/farm_plot_2";
    std::cout << "Opening geoget on http://localhost:8080" << std::endl;
    auto farm = create_field("Pea_Field", "pea", dp::Geo{51.73019, 4.23883, 0.0});
    if (farm.zone_count() > 0) {
        std::cout << "Saving farm to: " << plot_path << std::endl;
        farm.save(plot_path);
    }

    auto zones = farm.zones();
    std::cout << "Num zones: " << zones.size() << std::endl;

    if (zones.empty()) {
        std::cerr << "No zones available to visualize" << std::endl;
        return 1;
    }

    auto zone0 = zones.at(0);
    auto boundary = zone0.poly().field_boundary();
    std::cout << "Zone 0 boundary: " << boundary.vertices.size() << " points" << std::endl;

    for (size_t i = 0; i < zones.size(); ++i) {
        const auto &zone = zones.at(i);
        std::cout << "Zone " << i << ": " << zone.name() << " with "
                  << zone.poly().field_boundary().vertices.size() << " boundary points" << std::endl;
    }

    return 0;
}
