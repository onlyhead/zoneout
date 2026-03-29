#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <datapod/datapod.hpp>

#include "microtar/microtar.hpp"
#include "polygrid.hpp"

namespace dp = datapod;

namespace zoneout {

    class Plot {
      private:
        Poly poly_;
        std::optional<Grid> grid_;

        inline static Grid make_base_grid(const dp::Polygon &boundary, double resolution, const dp::Geo &datum,
                                          const std::string &name, const std::string &type) {
            auto aabb = boundary.get_aabb();

            double padding = resolution * 2.0;
            dp::Point aabb_size = aabb.max_point - aabb.min_point;
            double grid_width = aabb_size.x + padding;
            double grid_height = aabb_size.y + padding;

            size_t grid_rows = static_cast<size_t>(std::ceil(grid_height / resolution));
            size_t grid_cols = static_cast<size_t>(std::ceil(grid_width / resolution));

            grid_rows = std::max(grid_rows, static_cast<size_t>(1));
            grid_cols = std::max(grid_cols, static_cast<size_t>(1));

            dp::Pose grid_pose{aabb.center(), dp::Euler{0, 0, 0}.to_quaternion()};
            dp::Grid<uint8_t> generated_grid;
            generated_grid.rows = grid_rows;
            generated_grid.cols = grid_cols;
            generated_grid.resolution = resolution;
            generated_grid.centered = true;
            generated_grid.pose = grid_pose;
            generated_grid.data.resize(grid_rows * grid_cols, 0);

            for (size_t r = 0; r < generated_grid.rows; ++r) {
                for (size_t c = 0; c < generated_grid.cols; ++c) {
                    auto cell_center = generated_grid.get_point(r, c);
                    generated_grid(r, c) = boundary.contains(cell_center) ? uint8_t{255} : uint8_t{0};
                }
            }

            Grid grid(name, type, "default");
            grid.datum() = datum;
            grid.shift() = grid_pose;
            grid.resolution() = resolution;
            grid.add_grid(generated_grid, "base_layer", "terrain");
            return grid;
        }

      public:
        Plot() : poly_(), grid_(std::nullopt) {}

        Plot(const Poly &poly) : poly_(poly), grid_(std::nullopt) {}
        Plot(Poly &&poly) : poly_(std::move(poly)), grid_(std::nullopt) {}

        Plot(const Poly &poly, const Grid &grid) : poly_(poly), grid_(grid) {}
        Plot(Poly &&poly, Grid &&grid) : poly_(std::move(poly)), grid_(std::move(grid)) {}

        Plot(const std::string &name, const std::string &type, const dp::Polygon &boundary, const dp::Geo &datum)
            : poly_(name, type, "default", boundary), grid_(std::nullopt) {
            poly_.set_datum(datum);
        }

        Plot(const std::string &name, const std::string &type, const dp::Polygon &boundary, const dp::Geo &datum,
             double resolution)
            : poly_(name, type, "default", boundary), grid_(make_base_grid(boundary, resolution, datum, name, type)) {
            poly_.set_datum(datum);
        }

        Plot(const std::string &name, const std::string &type, const dp::Polygon &boundary,
             const dp::Grid<uint8_t> &initial_grid, const dp::Geo &datum)
            : poly_(name, type, "default", boundary), grid_(Grid(name, type, "default")) {
            poly_.set_datum(datum);
            grid_->datum() = datum;
            auto aabb = boundary.get_aabb();
            grid_->shift() = dp::Pose{aabb.center(), dp::Euler{0, 0, 0}.to_quaternion()};
            grid_->resolution() = initial_grid.resolution;
            grid_->add_grid(initial_grid, "base_layer", "terrain");
        }

        inline Poly &poly() { return poly_; }
        inline const Poly &poly() const { return poly_; }

        inline bool has_grid() const { return grid_.has_value(); }
        inline Grid &grid() { return grid_.value(); }
        inline const Grid &grid() const { return grid_.value(); }

        inline void set_grid(const Grid &grid) { grid_ = grid; }
        inline void set_grid(Grid &&grid) { grid_ = std::move(grid); }
        inline void clear_grid() { grid_.reset(); }

        inline const UUID &id() const { return poly_.id(); }

        inline const std::string &name() const { return poly_.name(); }
        inline void set_name(const std::string &name) {
            poly_.set_name(name);
            if (grid_) {
                grid_->set_name(name);
            }
        }

        inline const std::string &type() const { return poly_.type(); }
        inline void set_type(const std::string &type) {
            poly_.set_type(type);
            if (grid_) {
                grid_->set_type(type);
            }
        }

        inline const dp::Geo &datum() const { return poly_.datum(); }
        inline void set_datum(const dp::Geo &datum) {
            poly_.set_datum(datum);
            if (grid_) {
                grid_->datum() = datum;
            }
        }

        inline void set_property(const std::string &key, const std::string &value) {
            poly_.set_global_property("prop_" + key, value);
        }

        inline dp::Optional<std::string> property(const std::string &key) const {
            return poly_.global_property("prop_" + key);
        }

        inline bool has_property(const std::string &key) const { return poly_.has_global_property("prop_" + key); }

        inline bool remove_property(const std::string &key) { return poly_.remove_global_property("prop_" + key); }

        inline void clear_properties() {
            auto props = poly_.global_properties();
            for (const auto &[key, value] : props) {
                (void)value;
                if (key.rfind("prop_", 0) == 0) {
                    poly_.remove_global_property(key);
                }
            }
        }

        inline std::unordered_map<std::string, std::string> properties() const {
            std::unordered_map<std::string, std::string> result;
            for (const auto &[key, value] : poly_.global_properties()) {
                if (key.rfind("prop_", 0) == 0) {
                    result[key.substr(5)] = value;
                }
            }
            return result;
        }

        inline bool is_valid() const { return poly_.is_valid(); }

        inline void to_files(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) const {
            if (grid_) {
                savePolyGrid(poly_, *grid_, vector_path, raster_path);
            } else {
                poly_.to_file(vector_path, vectkit::CRS::WGS);
            }
        }

        inline void save(const std::filesystem::path &directory) const {
            std::filesystem::create_directories(directory);
            auto vector_path = directory / "vector.geojson";
            auto raster_path = directory / "raster.tiff";
            to_files(vector_path, raster_path);
        }

        inline static Plot from_files(const std::filesystem::path &vector_path,
                                      const std::filesystem::path &raster_path) {
            auto [poly, grid] = loadPolyGrid(vector_path, raster_path);
            if (grid.has_layers()) {
                if (poly.name().empty()) {
                    poly.set_name(grid.name());
                }
                if (poly.type().empty()) {
                    poly.set_type(grid.type());
                }
                if (poly.id().isNull()) {
                    poly.set_id(grid.id());
                }
            }
            if (grid.has_layers()) {
                return Plot(std::move(poly), std::move(grid));
            }
            return Plot(std::move(poly));
        }

        inline static Plot load(const std::filesystem::path &directory) {
            return from_files(directory / "vector.geojson", directory / "raster.tiff");
        }

        inline static Plot load(const std::filesystem::path &directory, const std::string &name,
                                const std::string &type, const dp::Geo &datum) {
            (void)name;
            (void)type;
            (void)datum;
            return load(directory);
        }

        inline void save_tar(const std::filesystem::path &tar_file) const {
            mtar_t tar;
            int err = mtar_open(&tar, tar_file.string().c_str(), "w");
            if (err != MTAR_ESUCCESS) {
                throw std::runtime_error("Could not create tar file: " + std::string(mtar_strerror(err)));
            }

            auto temp_dir = std::filesystem::temp_directory_path() / ("plot_" + id().toString());
            save(temp_dir);

            for (const auto &entry : std::filesystem::recursive_directory_iterator(temp_dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                auto relative_path = std::filesystem::relative(entry.path(), temp_dir);
                std::ifstream file(entry.path(), std::ios::binary);
                file.seekg(0, std::ios::end);
                size_t file_size = static_cast<size_t>(file.tellg());
                file.seekg(0, std::ios::beg);

                err = mtar_write_file_header(&tar, relative_path.string().c_str(), file_size);
                if (err != MTAR_ESUCCESS) {
                    file.close();
                    mtar_close(&tar);
                    std::filesystem::remove_all(temp_dir);
                    throw std::runtime_error("Could not write file header: " + std::string(mtar_strerror(err)));
                }

                std::vector<char> buffer(8192);
                while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
                    err = mtar_write_data(&tar, buffer.data(), static_cast<unsigned>(file.gcount()));
                    if (err != MTAR_ESUCCESS) {
                        file.close();
                        mtar_close(&tar);
                        std::filesystem::remove_all(temp_dir);
                        throw std::runtime_error("Could not write file data: " + std::string(mtar_strerror(err)));
                    }
                }
            }

            mtar_finalize(&tar);
            mtar_close(&tar);
            std::filesystem::remove_all(temp_dir);
        }

        inline static Plot load_tar(const std::filesystem::path &tar_file) {
            mtar_t tar;
            int err = mtar_open(&tar, tar_file.string().c_str(), "r");
            if (err != MTAR_ESUCCESS) {
                throw std::runtime_error("Could not open tar file: " + std::string(mtar_strerror(err)));
            }

            auto temp_dir = std::filesystem::temp_directory_path() / ("plot_extract_" + generateUUID().toString());
            std::filesystem::create_directories(temp_dir);

            mtar_header_t header;
            while ((err = mtar_read_header(&tar, &header)) == MTAR_ESUCCESS) {
                auto out_path = temp_dir / header.name;
                std::filesystem::create_directories(out_path.parent_path());

                std::ofstream out(out_path, std::ios::binary);
                std::vector<char> buffer(header.size);
                if (header.size > 0) {
                    mtar_read_data(&tar, buffer.data(), header.size);
                    out.write(buffer.data(), static_cast<std::streamsize>(header.size));
                }
                out.close();
                mtar_next(&tar);
            }

            mtar_close(&tar);
            auto plot = load(temp_dir);
            std::filesystem::remove_all(temp_dir);
            return plot;
        }

        inline static Plot load_tar(const std::filesystem::path &tar_file, const std::string &name,
                                    const std::string &type, const dp::Geo &datum) {
            (void)name;
            (void)type;
            (void)datum;
            return load_tar(tar_file);
        }
    };

    class PlotBuilder {
      private:
        std::optional<std::string> name_;
        std::optional<std::string> type_;
        std::optional<dp::Polygon> boundary_;
        std::optional<dp::Geo> datum_;
        std::optional<dp::Grid<uint8_t>> initial_grid_;
        std::optional<double> resolution_;
        std::unordered_map<std::string, std::string> properties_;

      public:
        inline PlotBuilder &with_name(const std::string &name) {
            name_ = name;
            return *this;
        }

        inline PlotBuilder &with_type(const std::string &type) {
            type_ = type;
            return *this;
        }

        inline PlotBuilder &with_boundary(const dp::Polygon &boundary) {
            boundary_ = boundary;
            return *this;
        }

        inline PlotBuilder &with_datum(const dp::Geo &datum) {
            datum_ = datum;
            return *this;
        }

        inline PlotBuilder &with_initial_grid(const dp::Grid<uint8_t> &grid) {
            initial_grid_ = grid;
            return *this;
        }

        inline PlotBuilder &with_resolution(double resolution) {
            resolution_ = resolution;
            return *this;
        }

        inline PlotBuilder &with_property(const std::string &key, const std::string &value) {
            properties_[key] = value;
            return *this;
        }

        inline PlotBuilder &with_properties(const std::unordered_map<std::string, std::string> &properties) {
            for (const auto &[key, value] : properties) {
                properties_[key] = value;
            }
            return *this;
        }

        inline bool is_valid() const {
            return name_.has_value() && type_.has_value() && boundary_.has_value() && datum_.has_value();
        }

        inline std::string validation_error() const {
            if (!name_.has_value() || name_->empty())
                return "Plot name is required and cannot be empty";
            if (!type_.has_value() || type_->empty())
                return "Plot type is required and cannot be empty";
            if (!boundary_.has_value())
                return "Plot boundary is required";
            if (!datum_.has_value())
                return "Plot datum is required";
            return "";
        }

        inline Plot build() const {
            if (!is_valid()) {
                throw std::invalid_argument(validation_error());
            }

            Plot plot =
                initial_grid_.has_value()
                    ? Plot(name_.value(), type_.value(), boundary_.value(), initial_grid_.value(), datum_.value())
                    : (resolution_.has_value()
                           ? Plot(name_.value(), type_.value(), boundary_.value(), datum_.value(), resolution_.value())
                           : Plot(name_.value(), type_.value(), boundary_.value(), datum_.value()));

            for (const auto &[key, value] : properties_) {
                plot.set_property(key, value);
            }
            return plot;
        }

        inline void reset() {
            name_.reset();
            type_.reset();
            boundary_.reset();
            datum_.reset();
            initial_grid_.reset();
            resolution_.reset();
            properties_.clear();
        }
    };

} // namespace zoneout
