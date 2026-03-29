#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <concord/concord.hpp>
#include <datapod/datapod.hpp>
#include <entropy/generator.hpp>
#include <rastkit/rastkit.hpp>
#include <vectkit/vectkit.hpp>

#include "constants.hpp"
#include "json.hpp"
#include "plot.hpp"
#include "utils/meta.hpp"
#include "utils/time.hpp"
#include "utils/uuid.hpp"

namespace dp = datapod;

namespace zoneout {

    // Forward declaration
    class Zone;
    class ZoneBuilder;

    class Zone {
      private:
        Plot plot_data_;

        UUID id_;
        std::string name_;
        std::string type_;

        std::unordered_map<std::string, std::string> properties_;
        std::vector<UUID> node_ids_;
        std::vector<Zone> children_;

        struct JsonDeleter {
            void operator()(json_value_s *ptr) const {
                if (ptr) {
                    free(ptr);
                }
            }
        };

        using JsonPtr = std::unique_ptr<json_value_s, JsonDeleter>;

        inline static json_object_s *json_object(json_value_s *value) {
            if (!value || value->type != json_type_object) {
                return nullptr;
            }
            return static_cast<json_object_s *>(value->payload);
        }

        inline static json_string_s *json_string(json_value_s *value) {
            if (!value || value->type != json_type_string) {
                return nullptr;
            }
            return static_cast<json_string_s *>(value->payload);
        }

        inline static json_object_element_s *find_element(json_object_s *obj, const char *key) {
            if (!obj) {
                return nullptr;
            }
            for (auto *elem = obj->start; elem; elem = elem->next) {
                if (elem->name && std::string(elem->name->string, elem->name->string_size) == key) {
                    return elem;
                }
            }
            return nullptr;
        }

        inline static std::string parse_string(json_value_s *value, const std::string &fallback = "") {
            auto *str = json_string(value);
            if (!str) {
                return fallback;
            }
            return std::string(str->string, str->string_size);
        }

        inline static std::unordered_map<std::string, std::string> parse_properties(json_value_s *value) {
            std::unordered_map<std::string, std::string> properties;
            auto *obj = json_object(value);
            if (!obj) {
                return properties;
            }
            for (auto *elem = obj->start; elem; elem = elem->next) {
                if (!elem->name) {
                    continue;
                }
                properties[std::string(elem->name->string, elem->name->string_size)] = parse_string(elem->value);
            }
            return properties;
        }

        inline static std::string escape_json(const std::string &value) {
            std::string escaped;
            escaped.reserve(value.size() + 8);
            for (char c : value) {
                switch (c) {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    escaped += c;
                    break;
                }
            }
            return escaped;
        }

        inline static void write_properties_json(std::ostream &out,
                                                 const std::unordered_map<std::string, std::string> &properties) {
            out << "{";
            bool first = true;
            for (const auto &[key, value] : properties) {
                if (!first) {
                    out << ",";
                }
                first = false;
                out << "\"" << escape_json(key) << "\":\"" << escape_json(value) << "\"";
            }
            out << "}";
        }

        inline static void write_uuid_array_json(std::ostream &out, const std::vector<UUID> &ids) {
            out << "[";
            for (size_t i = 0; i < ids.size(); ++i) {
                if (i > 0) {
                    out << ",";
                }
                out << "\"" << ids[i].toString() << "\"";
            }
            out << "]";
        }

        inline static std::vector<UUID> parse_uuid_array(json_value_s *value) {
            std::vector<UUID> ids;
            if (!value || value->type != json_type_array) {
                return ids;
            }
            auto *arr = static_cast<json_array_s *>(value->payload);
            for (auto *elem = arr->start; elem; elem = elem->next) {
                auto parsed = parse_string(elem->value);
                if (!parsed.empty()) {
                    ids.emplace_back(parsed);
                }
            }
            return ids;
        }

        inline void save_metadata(const std::filesystem::path &directory) const {
            save_metadata_file(directory / "zone.json");
        }

        inline static std::filesystem::path metadata_path_for(const std::filesystem::path &vector_path) {
            auto metadata_path = vector_path;
            metadata_path += ".zone.json";
            return metadata_path;
        }

        inline void save_metadata_file(const std::filesystem::path &metadata_path) const {
            std::ofstream out(metadata_path);
            if (!out.is_open()) {
                throw std::runtime_error("Failed to open zone metadata file for writing: " + metadata_path.string());
            }

            out << "{";
            out << "\"id\":\"" << id_.toString() << "\",";
            out << "\"name\":\"" << escape_json(name_) << "\",";
            out << "\"type\":\"" << escape_json(type_) << "\",";
            out << "\"properties\":";
            write_properties_json(out, properties_);
            out << ",\"node_ids\":";
            write_uuid_array_json(out, node_ids_);
            out << "}";
        }

        inline void load_metadata(const std::filesystem::path &directory) {
            load_metadata_file(directory / "zone.json");
        }

        inline void load_metadata_file(const std::filesystem::path &metadata_path) {
            if (!std::filesystem::exists(metadata_path)) {
                return;
            }

            std::ifstream in(metadata_path);
            if (!in.is_open()) {
                throw std::runtime_error("Failed to open zone metadata file: " + metadata_path.string());
            }

            std::stringstream buffer;
            buffer << in.rdbuf();
            const auto json_text = buffer.str();

            JsonPtr root(json_parse(json_text.c_str(), json_text.size()));
            if (!root) {
                throw std::runtime_error("Failed to parse zone metadata JSON: " + metadata_path.string());
            }

            auto *root_obj = json_object(root.get());
            if (!root_obj) {
                throw std::runtime_error("Invalid zone metadata JSON object: " + metadata_path.string());
            }

            if (auto *id_elem = find_element(root_obj, "id")) {
                id_ = UUID(parse_string(id_elem->value));
            }
            if (auto *name_elem = find_element(root_obj, "name")) {
                name_ = parse_string(name_elem->value);
            }
            if (auto *type_elem = find_element(root_obj, "type")) {
                type_ = parse_string(type_elem->value);
            }
            if (auto *props_elem = find_element(root_obj, "properties")) {
                properties_ = parse_properties(props_elem->value);
            }
            if (auto *node_ids_elem = find_element(root_obj, "node_ids")) {
                node_ids_ = parse_uuid_array(node_ids_elem->value);
            }

            sync_to_plot();
        }

        inline void ensure_grid_initialized(const dp::Grid<uint8_t> &seed_grid) {
            if (plot_data_.has_grid()) {
                return;
            }

            Grid grid(name_, type_, "default");
            grid.set_id(id_);
            grid.datum() = plot_data_.datum();
            grid.shift() = seed_grid.pose;
            grid.resolution() = seed_grid.resolution;
            plot_data_.set_grid(std::move(grid));
        }

        inline Grid &require_grid() {
            if (!plot_data_.has_grid()) {
                throw std::runtime_error("Zone '" + name_ + "' has no grid");
            }
            return plot_data_.grid();
        }

        inline const Grid &require_grid() const {
            if (!plot_data_.has_grid()) {
                throw std::runtime_error("Zone '" + name_ + "' has no grid");
            }
            return plot_data_.grid();
        }

      public:
        inline Zone(const std::string &name, const std::string &type, const dp::Polygon &boundary,
                    const dp::Grid<uint8_t> &initial_grid, const dp::Geo &datum)
            : plot_data_(name, type, boundary, initial_grid, datum), id_(generateUUID()), name_(name), type_(type) {
            sync_to_plot();
        }

        inline Zone(const std::string &name, const std::string &type, const dp::Polygon &boundary, const dp::Geo &datum,
                    double resolution = 1.0)
            : plot_data_(name, type, boundary, datum, resolution), id_(generateUUID()), name_(name), type_(type) {
            sync_to_plot();
        }

        inline Zone(const std::string &name, const std::string &type, const Plot &plot)
            : plot_data_(plot), id_(generateUUID()), name_(name), type_(type) {
            sync_to_plot();
        }

        inline Zone(const std::string &name, const std::string &type, Plot &&plot)
            : plot_data_(std::move(plot)), id_(generateUUID()), name_(name), type_(type) {
            sync_to_plot();
        }

        inline const UUID &id() const { return id_; }
        inline const std::string &name() const { return name_; }
        inline const std::string &type() const { return type_; }

        inline void set_id(const UUID &id) {
            id_ = id;
            sync_to_plot();
        }

        inline void set_name(const std::string &name) {
            name_ = name;
            plot_data_.set_name(name);
        }

        inline void set_type(const std::string &type) {
            type_ = type;
            plot_data_.set_type(type);
        }

        inline void set_property(const std::string &key, const std::string &value) { properties_[key] = value; }

        inline dp::Optional<std::string> property(const std::string &key) const {
            auto it = properties_.find(key);
            if (it != properties_.end())
                return it->second;
            return dp::nullopt;
        }

        inline const std::unordered_map<std::string, std::string> &properties() const { return properties_; }
        inline const std::vector<UUID> &node_ids() const { return node_ids_; }
        inline std::vector<UUID> &node_ids() { return node_ids_; }
        inline const std::vector<Zone> &children() const { return children_; }
        inline std::vector<Zone> &children() { return children_; }
        inline size_t child_count() const { return children_.size(); }

        inline void add_child(const Zone &child) {
            if (!child.id().isNull() && find(child.id()) != nullptr) {
                throw std::runtime_error("Cannot add child zone: duplicate zone UUID in subtree");
            }

            if (plot_data_.poly().has_field_boundary() && child.poly().has_field_boundary()) {
                const auto &parent_boundary = plot_data_.poly().field_boundary();
                for (const auto &point : child.poly().field_boundary().vertices) {
                    if (!parent_boundary.contains(point)) {
                        throw std::runtime_error("Cannot add child zone: child boundary must lie inside parent zone");
                    }
                }
            }

            children_.push_back(child);
        }

        inline void add_child(Zone &&child) {
            if (!child.id().isNull() && find(child.id()) != nullptr) {
                throw std::runtime_error("Cannot add child zone: duplicate zone UUID in subtree");
            }

            if (plot_data_.poly().has_field_boundary() && child.poly().has_field_boundary()) {
                const auto &parent_boundary = plot_data_.poly().field_boundary();
                for (const auto &point : child.poly().field_boundary().vertices) {
                    if (!parent_boundary.contains(point)) {
                        throw std::runtime_error("Cannot add child zone: child boundary must lie inside parent zone");
                    }
                }
            }

            children_.push_back(std::move(child));
        }

        inline bool remove_child(const UUID &child_id) {
            auto it = std::find_if(children_.begin(), children_.end(),
                                   [&child_id](const Zone &child) { return child.id() == child_id; });
            if (it != children_.end()) {
                children_.erase(it);
                return true;
            }

            for (auto &child : children_) {
                if (child.remove_child(child_id)) {
                    return true;
                }
            }
            return false;
        }

        inline Zone *find(const UUID &zone_id) {
            if (id_ == zone_id) {
                return this;
            }
            for (auto &child : children_) {
                if (auto *found = child.find(zone_id); found != nullptr) {
                    return found;
                }
            }
            return nullptr;
        }

        inline const Zone *find(const UUID &zone_id) const {
            if (id_ == zone_id) {
                return this;
            }
            for (const auto &child : children_) {
                if (auto *found = child.find(zone_id); found != nullptr) {
                    return found;
                }
            }
            return nullptr;
        }

        inline Zone *find_by_name(const std::string &zone_name) {
            if (name_ == zone_name) {
                return this;
            }
            for (auto &child : children_) {
                if (auto *found = child.find_by_name(zone_name); found != nullptr) {
                    return found;
                }
            }
            return nullptr;
        }

        inline const Zone *find_by_name(const std::string &zone_name) const {
            if (name_ == zone_name) {
                return this;
            }
            for (const auto &child : children_) {
                if (auto *found = child.find_by_name(zone_name); found != nullptr) {
                    return found;
                }
            }
            return nullptr;
        }

        template <typename F> inline void visit(F &&visitor, size_t depth = 0) {
            visitor(*this, depth);
            for (auto &child : children_) {
                child.visit(visitor, depth + 1);
            }
        }

        template <typename F> inline void visit(F &&visitor, size_t depth = 0) const {
            visitor(*this, depth);
            for (const auto &child : children_) {
                child.visit(visitor, depth + 1);
            }
        }

        inline dp::Optional<size_t> depth_of(const UUID &zone_id, size_t depth = 0) const {
            if (id_ == zone_id) {
                return depth;
            }
            for (const auto &child : children_) {
                auto child_depth = child.depth_of(zone_id, depth + 1);
                if (child_depth.has_value()) {
                    return child_depth;
                }
            }
            return dp::nullopt;
        }

        /// Remove a property by key. Returns true if the property was found and removed.
        inline bool remove_property(const std::string &key) { return properties_.erase(key) > 0; }

        /// Clear all properties
        inline void clear_properties() { properties_.clear(); }

        /// Check if a property exists
        inline bool has_property(const std::string &key) const { return properties_.find(key) != properties_.end(); }
        inline void set_node_ids(const std::vector<UUID> &node_ids) { node_ids_ = node_ids; }
        inline void clear_node_ids() { node_ids_.clear(); }

        inline const dp::Geo &datum() const { return plot_data_.datum(); }

        inline void set_datum(const dp::Geo &datum) { plot_data_.set_datum(datum); }

        inline void add_raster_layer(const dp::Grid<uint8_t> &grid, const std::string &name,
                                     const std::string &type = "",
                                     const std::unordered_map<std::string, std::string> &properties = {},
                                     bool poly_cut = false, int layer_index = -1) {
            (void)layer_index;
            ensure_grid_initialized(grid);

            if (poly_cut && plot_data_.poly().has_field_boundary()) {
                auto modified_grid = grid;

                auto boundary = plot_data_.poly().field_boundary();

                size_t cells_inside = 0;
                size_t total_cells = modified_grid.rows * modified_grid.cols;

                for (size_t r = 0; r < modified_grid.rows; ++r) {
                    for (size_t c = 0; c < modified_grid.cols; ++c) {
                        auto cell_center = modified_grid.get_point(r, c);

                        if (boundary.contains(cell_center)) {
                            cells_inside++;
                        } else {
                            modified_grid(r, c) = 0;
                        }
                    }
                }

                require_grid().add_grid(modified_grid, name, type, properties);
            } else {
                require_grid().add_grid(grid, name, type, properties);
            }
        }

        inline std::string raster_info() const {
            if (plot_data_.has_grid() && plot_data_.grid().layer_count() > 0) {
                const auto &first_layer = plot_data_.grid().get_layer(0);
                return "Raster size: " + std::to_string(first_layer.width) + "x" + std::to_string(first_layer.height) +
                       " (" + std::to_string(plot_data_.grid().layer_count()) + " layers)";
            }
            return "No raster layers";
        }

        inline void add_polygon_element(const dp::Polygon &geometry, const std::string &name,
                                        const std::string &type = "", const std::string &subtype = "default",
                                        const std::unordered_map<std::string, std::string> &properties = {}) {
            if (plot_data_.poly().has_field_boundary()) {
                auto boundary = plot_data_.poly().field_boundary();

                for (const auto &point : geometry.vertices) {
                    if (!boundary.contains(point)) {
                        throw std::runtime_error("Polygon element '" + name +
                                                 "' is not valid: points must be inside field boundary");
                    }
                }
            }

            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> color_dist(50, 200);
            uint8_t polygon_color = static_cast<uint8_t>(color_dist(gen));

            if (plot_data_.has_grid() && plot_data_.grid().layer_count() > 0) {
                auto &grid_variant = plot_data_.grid().get_layer(0).grid;
                std::visit(
                    [&](auto &base_grid) {
                        using GridType = std::decay_t<decltype(base_grid)>;
                        if constexpr (!std::is_same_v<GridType, dp::Grid<rastkit::RGBA>>) {
                            for (size_t r = 0; r < base_grid.rows; ++r) {
                                for (size_t c = 0; c < base_grid.cols; ++c) {
                                    auto cell_center = base_grid.get_point(r, c);
                                    if (geometry.contains(cell_center)) {
                                        using CellType = typename decltype(base_grid.data)::value_type;
                                        base_grid(r, c) = static_cast<CellType>(polygon_color);
                                    }
                                }
                            }
                        }
                    },
                    grid_variant);
            }

            UUID element_id = generateUUID();

            plot_data_.poly().add_polygon_element(element_id, name, type, subtype, geometry, properties);
        }

        inline std::string element_info() const {
            const auto &polygon_elements = plot_data_.poly().polygon_elements();
            const auto &line_elements = plot_data_.poly().line_elements();
            const auto &point_elements = plot_data_.poly().point_elements();

            size_t total_elements = polygon_elements.size() + line_elements.size() + point_elements.size();

            if (total_elements > 0) {
                return "Elements: " + std::to_string(polygon_elements.size()) + " polygons, " +
                       std::to_string(line_elements.size()) + " lines, " + std::to_string(point_elements.size()) +
                       " points (" + std::to_string(total_elements) + " total)";
            }
            return "No elements";
        }

        // ============ Spatial Queries ============

        /// Check if a point is inside this zone's boundary
        inline bool contains(const dp::Point &point) const { return plot_data_.poly().contains(point); }

        /// Get all polygon elements that intersect with the given bounding box
        inline std::vector<PolygonElement> polygon_elements_in_area(const dp::AABB &bbox) const {
            std::vector<PolygonElement> result;
            for (const auto &elem : plot_data_.poly().polygon_elements()) {
                auto elem_aabb = elem.geometry.get_aabb();
                // Use AABB's built-in intersects method
                if (elem_aabb.intersects(bbox)) {
                    result.push_back(elem);
                }
            }
            return result;
        }

        /// Get all point elements within the given bounding box
        inline std::vector<PointElement> point_elements_in_area(const dp::AABB &bbox) const {
            std::vector<PointElement> result;
            for (const auto &elem : plot_data_.poly().point_elements()) {
                if (bbox.contains(elem.geometry)) {
                    result.push_back(elem);
                }
            }
            return result;
        }

        /// Get all line elements that intersect with the given bounding box
        inline std::vector<LineElement> line_elements_in_area(const dp::AABB &bbox) const {
            std::vector<LineElement> result;
            for (const auto &elem : plot_data_.poly().line_elements()) {
                // Check if either endpoint is in the bbox
                if (bbox.contains(elem.geometry.start) || bbox.contains(elem.geometry.end)) {
                    result.push_back(elem);
                }
            }
            return result;
        }

        /// Get all point elements that are inside the given polygon
        inline std::vector<PointElement> points_in_polygon(const dp::Polygon &area) const {
            std::vector<PointElement> result;
            for (const auto &elem : plot_data_.poly().point_elements()) {
                if (area.contains(elem.geometry)) {
                    result.push_back(elem);
                }
            }
            return result;
        }

        /// Get the zone's bounding box
        inline dp::AABB bounding_box() const {
            if (plot_data_.poly().has_field_boundary()) {
                return plot_data_.poly().field_boundary().get_aabb();
            }
            return dp::AABB{};
        }

        inline bool is_valid() const { return plot_data_.is_valid(); }

        inline static Zone load_plot_files(const std::filesystem::path &vector_path,
                                           const std::filesystem::path &raster_path) {
            auto plot = Plot::from_files(vector_path, raster_path);
            const auto loaded_id = plot.id();
            Zone zone(plot.name(), plot.type(), std::move(plot));
            zone.name_ = zone.plot_data_.name();
            zone.type_ = zone.plot_data_.type();
            zone.id_ = loaded_id;

            if (std::filesystem::exists(vector_path)) {
                auto field_props = zone.plot_data_.poly().global_properties();
                for (const auto &[key, value] : field_props) {
                    if (key.substr(0, 5) == "prop_") {
                        zone.set_property(key.substr(5), value);
                    }
                }
            }

            zone.load_metadata_file(metadata_path_for(vector_path));
            zone.sync_to_plot();
            return zone;
        }

        inline void save_plot_files(const std::filesystem::path &vector_path,
                                    const std::filesystem::path &raster_path) const {
            const_cast<Zone *>(this)->sync_to_plot();

            auto plot_copy = plot_data_;
            plot_copy.poly().set_id(id_);
            if (plot_copy.has_grid()) {
                plot_copy.grid().set_id(id_);
            }

            for (const auto &[key, value] : properties_) {
                plot_copy.poly().set_global_property("prop_" + key, value);
            }

            plot_copy.to_files(vector_path, raster_path);
            save_metadata_file(metadata_path_for(vector_path));
        }

        inline void save(const std::filesystem::path &directory) const {
            std::filesystem::create_directories(directory);
            auto vector_path = directory / "vector.geojson";
            auto raster_path = directory / "raster.tiff";
            save_plot_files(vector_path, raster_path);
            save_metadata(directory);

            for (size_t i = 0; i < children_.size(); ++i) {
                children_[i].save(directory / ("child_" + std::to_string(i)));
            }
        }

        inline static Zone load(const std::filesystem::path &directory) {
            auto vector_path = directory / "vector.geojson";
            auto raster_path = directory / "raster.tiff";
            auto zone = load_plot_files(vector_path, raster_path);
            zone.load_metadata(directory);

            for (size_t i = 0;; ++i) {
                auto child_dir = directory / ("child_" + std::to_string(i));
                if (!std::filesystem::exists(child_dir)) {
                    break;
                }
                zone.children_.push_back(load(child_dir));
            }

            return zone;
        }

        inline bool has_grid() const { return plot_data_.has_grid(); }

        inline void sync_to_plot() {
            plot_data_.set_name(name_);
            plot_data_.set_type(type_);
            plot_data_.poly().set_id(id_);
            if (plot_data_.has_grid()) {
                plot_data_.grid().set_id(id_);
            }
        }

        // Accessors for internal data structures
        inline Plot &plot() { return plot_data_; }
        inline const Plot &plot() const { return plot_data_; }

        inline Poly &poly() { return plot_data_.poly(); }
        inline const Poly &poly() const { return plot_data_.poly(); }
    };

    // Factory helper for creating zones with validation
    inline Zone make_zone(const std::string &name, const std::string &type, const dp::Polygon &boundary,
                          const dp::Geo &datum, double resolution = DEFAULT_RESOLUTION) {
        // Validate inputs
        if (name.empty()) {
            throw std::invalid_argument("Zone name cannot be empty");
        }
        if (type.empty()) {
            throw std::invalid_argument("Zone type cannot be empty");
        }
        if (boundary.vertices.size() < 3) {
            throw std::invalid_argument("Boundary polygon must have at least 3 points");
        }
        if (resolution <= 0.0) {
            throw std::invalid_argument("Resolution must be positive");
        }

        // Create zone with auto-generated grid
        return Zone(name, type, boundary, datum, resolution);
    }

    /**
     * @brief Builder pattern for constructing Zone objects with fluent interface
     */
    class ZoneBuilder {
      private:
        // Required fields
        std::optional<std::string> name_;
        std::optional<std::string> type_;
        std::optional<dp::Polygon> boundary_;
        std::optional<dp::Geo> datum_;

        // Optional fields with defaults
        double resolution_ = 1.0;
        std::optional<dp::Grid<uint8_t>> initial_grid_;

        // Collections
        std::unordered_map<std::string, std::string> properties_;

        struct RasterLayerConfig {
            dp::Grid<uint8_t> grid;
            std::string name;
            std::string type;
            std::unordered_map<std::string, std::string> properties;
            bool poly_cut;
            int layer_index;
        };
        std::vector<RasterLayerConfig> raster_layers_;

        struct PolygonElementConfig {
            dp::Polygon geometry;
            std::string name;
            std::string type;
            std::string subtype;
            std::unordered_map<std::string, std::string> properties;
        };
        std::vector<PolygonElementConfig> polygon_elements_;

      public:
        ZoneBuilder() = default;

        // Required configuration methods
        inline ZoneBuilder &with_name(const std::string &name) {
            name_ = name;
            return *this;
        }

        inline ZoneBuilder &with_type(const std::string &type) {
            type_ = type;
            return *this;
        }

        inline ZoneBuilder &with_boundary(const dp::Polygon &boundary) {
            boundary_ = boundary;
            return *this;
        }

        inline ZoneBuilder &with_datum(const dp::Geo &datum) {
            datum_ = datum;
            return *this;
        }

        // Optional configuration methods
        inline ZoneBuilder &with_resolution(double resolution) {
            resolution_ = resolution;
            return *this;
        }

        inline ZoneBuilder &with_initial_grid(const dp::Grid<uint8_t> &grid) {
            initial_grid_ = grid;
            return *this;
        }

        inline ZoneBuilder &with_property(const std::string &key, const std::string &value) {
            properties_[key] = value;
            return *this;
        }

        inline ZoneBuilder &with_properties(const std::unordered_map<std::string, std::string> &properties) {
            for (const auto &[key, value] : properties) {
                properties_[key] = value;
            }
            return *this;
        }

        // Element configuration methods
        inline ZoneBuilder &with_raster_layer(const dp::Grid<uint8_t> &grid, const std::string &name,
                                              const std::string &type = "",
                                              const std::unordered_map<std::string, std::string> &properties = {},
                                              bool poly_cut = false, int layer_index = -1) {
            RasterLayerConfig config;
            config.grid = grid;
            config.name = name;
            config.type = type;
            config.properties = properties;
            config.poly_cut = poly_cut;
            config.layer_index = layer_index;
            raster_layers_.push_back(config);
            return *this;
        }

        inline ZoneBuilder &with_polygon_element(const dp::Polygon &geometry, const std::string &name,
                                                 const std::string &type = "", const std::string &subtype = "default",
                                                 const std::unordered_map<std::string, std::string> &properties = {}) {
            PolygonElementConfig config;
            config.geometry = geometry;
            config.name = name;
            config.type = type;
            config.subtype = subtype;
            config.properties = properties;
            polygon_elements_.push_back(config);
            return *this;
        }

        // Validation
        inline bool is_valid() const { return validation_error().empty(); }

        inline std::string validation_error() const {
            if (!name_.has_value() || name_->empty()) {
                return "Zone name is required and cannot be empty";
            }

            if (!type_.has_value() || type_->empty()) {
                return "Zone type is required and cannot be empty";
            }

            if (!boundary_.has_value()) {
                return "Zone boundary is required";
            }

            if (boundary_->vertices.size() < 3) {
                return "Boundary polygon must have at least 3 points";
            }

            if (!datum_.has_value()) {
                return "Zone datum is required";
            }

            if (resolution_ <= 0.0) {
                return "Resolution must be positive (got " + std::to_string(resolution_) + ")";
            }

            return "";
        }

        // Building
        inline Zone build() const {
            // Validate before building
            std::string error = validation_error();
            if (!error.empty()) {
                throw std::invalid_argument("ZoneBuilder validation failed: " + error);
            }

            // Create zone with either initial grid or auto-generated grid
            Zone zone =
                initial_grid_.has_value()
                    ? Zone(name_.value(), type_.value(), boundary_.value(), initial_grid_.value(), datum_.value())
                    : Zone(name_.value(), type_.value(), boundary_.value(), datum_.value(), resolution_);

            // Add properties
            for (const auto &[key, value] : properties_) {
                zone.set_property(key, value);
            }

            // Add raster layers
            for (const auto &layer : raster_layers_) {
                zone.add_raster_layer(layer.grid, layer.name, layer.type, layer.properties, layer.poly_cut,
                                      layer.layer_index);
            }

            // Add polygon elements
            for (const auto &elem : polygon_elements_) {
                zone.add_polygon_element(elem.geometry, elem.name, elem.type, elem.subtype, elem.properties);
            }

            return zone;
        }

        // Reset builder to initial state
        inline void reset() {
            name_.reset();
            type_.reset();
            boundary_.reset();
            datum_.reset();
            resolution_ = 1.0;
            initial_grid_.reset();
            properties_.clear();
            raster_layers_.clear();
            polygon_elements_.clear();
        }
    };

} // namespace zoneout
