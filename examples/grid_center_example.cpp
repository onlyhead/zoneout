#include "zoneout/zoneout.hpp"
#include <iomanip>
#include <iostream>

namespace dp = datapod;

int main() {
    std::cout << "=== Grid Center Position Example ===" << std::endl;
    std::cout << std::endl;

    // Create a 200m x 200m field
    dp::Polygon boundary;
    boundary.vertices.push_back({0, 0, 0});
    boundary.vertices.push_back({200, 0, 0});
    boundary.vertices.push_back({200, 200, 0});
    boundary.vertices.push_back({0, 200, 0});

    dp::Geo datum{52.0, 5.0, 0.0};
    double resolution = 20.0; // 20m per cell -> 10x10 grid

    zoneout::Zone zone("my_field", "crop_field", boundary, datum, resolution);

    // Add a 10x10 grid layer
    dp::Grid<uint8_t> elevation(10, 10, resolution, true, dp::Pose{}, {});

    for (size_t r = 0; r < 10; r++) {
        for (size_t c = 0; c < 10; c++) {
            elevation(r, c) = (r + c) * 10;
        }
    }

    zone.add_raster_layer(elevation, "elevation", "terrain");

    // Get the grid
    auto &raster = zone.plot().grid().raster();
    if (!raster.layers.empty()) {
        auto &grid_variant = raster.layers[0].grid;

        std::cout << std::fixed << std::setprecision(1);

        // We know this is a uint8_t grid since we created it that way
        auto &grid = std::get<dp::Grid<uint8_t>>(grid_variant);

        // Get center position of cell [5, 6]
        dp::Point center = grid.get_point(5, 6);
        std::cout << "Cell [5,6] center: (" << center.x << ", " << center.y << ")" << std::endl;

        // Get value at that cell
        auto value = grid(5, 6);
        std::cout << "Cell [5,6] value:  " << static_cast<int>(value) << std::endl;

        std::cout << std::endl;

        // Reverse: find which cell contains a position
        dp::Point robot_pos{100.0, 120.0, 0.0};
        auto [row, col] = grid.world_to_grid(robot_pos);

        std::cout << "Position (" << robot_pos.x << ", " << robot_pos.y << ") is in cell [" << row << "," << col << "]"
                  << std::endl;
    }

    return 0;
}
