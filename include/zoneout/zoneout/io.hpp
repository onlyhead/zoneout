#pragma once

#include "plot.hpp"
#include "zone.hpp"
#include <concord/concord.hpp>
#include <datapod/datapod.hpp>
#include <filesystem>
#include <optional>

namespace dp = datapod;

namespace zoneout {
    namespace io {

        // Zone I/O functions
        inline void save_zone(const Zone &zone, const std::filesystem::path &vector_path,
                              const std::filesystem::path &raster_path) {
            zone.save_plot_files(vector_path, raster_path);
        }

        inline Zone load_zone(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) {
            return Zone::load_plot_files(vector_path, raster_path);
        }

        inline void save_zone(const Zone &zone, const std::filesystem::path &directory) { zone.save(directory); }

        inline Zone load_zone(const std::filesystem::path &directory) { return Zone::load(directory); }

        // Plot I/O functions
        inline void save_plot(const Plot &plot, const std::filesystem::path &directory) { plot.save(directory); }

        inline Plot load_plot(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                              const dp::Geo &datum) {
            return Plot::load(directory, name, type, datum);
        }

        inline void save_plot_tar(const Plot &plot, const std::filesystem::path &tar_file) { plot.save_tar(tar_file); }

        inline Plot load_plot_tar(const std::filesystem::path &tar_file, const std::string &name,
                                  const std::string &type, const dp::Geo &datum) {
            return Plot::load_tar(tar_file, name, type, datum);
        }

    } // namespace io
} // namespace zoneout
