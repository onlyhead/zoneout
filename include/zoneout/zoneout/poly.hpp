#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <concord/concord.hpp>
#include <datapod/datapod.hpp>
#include <vectkit/vectkit.hpp>

#include "utils/meta.hpp"
#include "utils/uuid.hpp"

namespace dp = datapod;

namespace zoneout {

    struct StructuredElement {
        UUID uuid;
        std::string name;
        std::string type;
        std::string subtype;
        std::unordered_map<std::string, std::string> properties;

        inline StructuredElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                                 const std::unordered_map<std::string, std::string> &props = {})
            : uuid(id), name(n), type(t), subtype(st), properties(props) {}

        inline static bool isValid(const vectkit::Feature &feature) {
            const auto &props = feature.properties;
            return props.find("uuid") != props.end() && props.find("name") != props.end() &&
                   props.find("type") != props.end() && props.find("subtype") != props.end();
        }

        inline static std::optional<StructuredElement> fromFeature(const vectkit::Feature &feature) {
            if (!isValid(feature))
                return std::nullopt;

            const auto &props = feature.properties;
            return StructuredElement(UUID(props.at("uuid")), props.at("name"), props.at("type"), props.at("subtype"),
                                     props);
        }

        inline std::unordered_map<std::string, std::string> toProperties() const {
            auto props = properties;
            props["uuid"] = uuid.toString();
            props["name"] = name;
            props["type"] = type;
            props["subtype"] = subtype;
            if (props.find("border") == props.end()) {
                props["border"] = "false";
            }
            return props;
        }
    };

    struct PolygonElement : public StructuredElement {
        dp::Polygon geometry;

        inline PolygonElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                              const dp::Polygon &geom, const std::unordered_map<std::string, std::string> &props = {})
            : StructuredElement(id, n, t, st, props), geometry(geom) {}
    };

    struct LineElement : public StructuredElement {
        dp::Segment geometry;

        inline LineElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                           const dp::Segment &geom, const std::unordered_map<std::string, std::string> &props = {})
            : StructuredElement(id, n, t, st, props), geometry(geom) {}
    };

    struct PointElement : public StructuredElement {
        dp::Point geometry;

        inline PointElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                            const dp::Point &geom, const std::unordered_map<std::string, std::string> &props = {})
            : StructuredElement(id, n, t, st, props), geometry(geom) {}
    };

    class Poly {
      private:
        vectkit::FeatureCollection collection_;
        dp::Polygon field_boundary_;
        Meta meta_;

        std::vector<PolygonElement> polygon_elements_;
        std::vector<LineElement> line_elements_;
        std::vector<PointElement> point_elements_;

        inline void sync_to_global_properties() {
            collection_.global_properties["name"] = meta_.name;
            collection_.global_properties["type"] = meta_.type;
            collection_.global_properties["subtype"] = meta_.subtype;
            collection_.global_properties["uuid"] = meta_.id.toString();
        }

        inline void load_structured_elements() {
            polygon_elements_.clear();
            line_elements_.clear();
            point_elements_.clear();

            for (size_t i = 0; i < collection_.features.size(); ++i) {
                const auto &feature = collection_.features[i];

                if (!StructuredElement::isValid(feature)) {
                    continue;
                }

                auto structured = StructuredElement::fromFeature(feature);
                if (!structured.has_value())
                    continue;

                if (std::holds_alternative<dp::Polygon>(feature.geometry)) {
                    auto polygon = std::get<dp::Polygon>(feature.geometry);
                    polygon_elements_.emplace_back(structured->uuid, structured->name, structured->type,
                                                   structured->subtype, polygon, structured->properties);
                } else if (std::holds_alternative<dp::Segment>(feature.geometry)) {
                    auto segment = std::get<dp::Segment>(feature.geometry);
                    line_elements_.emplace_back(structured->uuid, structured->name, structured->type,
                                                structured->subtype, segment, structured->properties);
                } else if (std::holds_alternative<dp::Point>(feature.geometry)) {
                    auto point = std::get<dp::Point>(feature.geometry);
                    point_elements_.emplace_back(structured->uuid, structured->name, structured->type,
                                                 structured->subtype, point, structured->properties);
                }
            }
        }

      public:
        inline Poly() : collection_(), field_boundary_(), meta_("", "other", "default") { sync_to_global_properties(); }

        inline Poly(const std::string &name, const std::string &type, const std::string &subtype = "default")
            : collection_(), field_boundary_(), meta_(name, type, subtype) {
            sync_to_global_properties();
            load_structured_elements();
        }

        inline Poly(const std::string &name, const std::string &type, const std::string &subtype,
                    const dp::Polygon &boundary)
            : collection_(), field_boundary_(boundary), meta_(name, type, subtype) {
            sync_to_global_properties();
            load_structured_elements();
        }

        inline Poly(const std::string &name, const std::string &type, const std::string &subtype,
                    const dp::Polygon &boundary, const dp::Geo &datum, const dp::Euler &heading, vectkit::CRS crs)
            : collection_(), field_boundary_(boundary), meta_(name, type, subtype) {
            collection_.datum = datum;
            collection_.heading = heading;
            sync_to_global_properties();
            load_structured_elements();
        }

        // Access to the underlying FeatureCollection
        inline vectkit::FeatureCollection &collection() { return collection_; }
        inline const vectkit::FeatureCollection &collection() const { return collection_; }

        // Field boundary access
        inline const dp::Polygon &field_boundary() const { return field_boundary_; }
        inline void set_field_boundary(const dp::Polygon &boundary) { field_boundary_ = boundary; }

        // Datum and heading access
        inline const dp::Geo &datum() const { return collection_.datum; }
        inline void set_datum(const dp::Geo &datum) { collection_.datum = datum; }
        inline const dp::Euler &heading() const { return collection_.heading; }
        inline void set_heading(const dp::Euler &heading) { collection_.heading = heading; }

        // Global properties access
        inline void set_global_property(const std::string &key, const std::string &value) {
            collection_.global_properties[key] = value;
        }

        inline dp::Optional<std::string> global_property(const std::string &key) const {
            auto it = collection_.global_properties.find(key);
            if (it != collection_.global_properties.end()) {
                return it->second;
            }
            return dp::nullopt;
        }

        inline const std::unordered_map<std::string, std::string> &global_properties() const {
            return collection_.global_properties;
        }

        /// Remove a global property by key. Returns true if the property was found and removed.
        inline bool remove_global_property(const std::string &key) {
            return collection_.global_properties.erase(key) > 0;
        }

        /// Clear all global properties
        inline void clear_global_properties() { collection_.global_properties.clear(); }

        /// Check if a global property exists
        inline bool has_global_property(const std::string &key) const {
            return collection_.global_properties.find(key) != collection_.global_properties.end();
        }

        // Feature management
        inline void add_feature(const vectkit::Feature &feature) { collection_.features.push_back(feature); }
        inline size_t feature_count() const { return collection_.features.size(); }
        inline const vectkit::Feature &get_feature(size_t index) const { return collection_.features.at(index); }

        // Field boundary properties
        inline void set_field_property(const std::string &key, const std::string &value) {
            for (auto &feature : collection_.features) {
                auto border_it = feature.properties.find("border");
                if (border_it != feature.properties.end() && border_it->second == "true") {
                    feature.properties[key] = value;
                    return;
                }
            }
        }

        inline const UUID &id() const { return meta_.id; }
        inline const std::string &name() const { return meta_.name; }
        inline const std::string &type() const { return meta_.type; }
        inline const std::string &subtype() const { return meta_.subtype; }

        inline void set_name(const std::string &name) {
            meta_.name = name;
            sync_to_global_properties();
        }

        inline void set_type(const std::string &type) {
            meta_.type = type;
            sync_to_global_properties();
        }

        inline void set_subtype(const std::string &subtype) {
            meta_.subtype = subtype;
            sync_to_global_properties();
        }

        inline void set_id(const UUID &id) {
            meta_.id = id;
            sync_to_global_properties();
        }

        inline void add_polygon_element(const UUID &id, const std::string &name, const std::string &type,
                                        const std::string &subtype, const dp::Polygon &geometry,
                                        const std::unordered_map<std::string, std::string> &props = {}) {
            polygon_elements_.emplace_back(id, name, type, subtype, geometry, props);
            vectkit::Feature feature;
            feature.geometry = geometry;
            feature.properties = polygon_elements_.back().toProperties();
            collection_.features.push_back(feature);
        }

        // Convenience overload: auto-generates UUID, uses type as name and "default" as subtype
        inline void add_polygon_element(const dp::Polygon &geometry, const std::string &type,
                                        const std::unordered_map<std::string, std::string> &props = {}) {
            add_polygon_element(generateUUID(), type, type, "default", geometry, props);
        }

        inline void add_line_element(const UUID &id, const std::string &name, const std::string &type,
                                     const std::string &subtype, const dp::Segment &geometry,
                                     const std::unordered_map<std::string, std::string> &props = {}) {
            line_elements_.emplace_back(id, name, type, subtype, geometry, props);
            vectkit::Feature feature;
            feature.geometry = geometry;
            feature.properties = line_elements_.back().toProperties();
            collection_.features.push_back(feature);
        }

        // Convenience overload: auto-generates UUID, uses type as name and "default" as subtype
        inline void add_line_element(const dp::Segment &geometry, const std::string &type,
                                     const std::unordered_map<std::string, std::string> &props = {}) {
            add_line_element(generateUUID(), type, type, "default", geometry, props);
        }

        inline void add_point_element(const UUID &id, const std::string &name, const std::string &type,
                                      const std::string &subtype, const dp::Point &geometry,
                                      const std::unordered_map<std::string, std::string> &props = {}) {
            point_elements_.emplace_back(id, name, type, subtype, geometry, props);
            vectkit::Feature feature;
            feature.geometry = geometry;
            feature.properties = point_elements_.back().toProperties();
            collection_.features.push_back(feature);
        }

        // Convenience overload: auto-generates UUID, uses type as name and "default" as subtype
        inline void add_point_element(const dp::Point &geometry, const std::string &type,
                                      const std::unordered_map<std::string, std::string> &props = {}) {
            add_point_element(generateUUID(), type, type, "default", geometry, props);
        }

        inline const std::vector<PolygonElement> &polygon_elements() const { return polygon_elements_; }
        inline const std::vector<LineElement> &line_elements() const { return line_elements_; }
        inline const std::vector<PointElement> &point_elements() const { return point_elements_; }

        inline std::vector<PolygonElement> polygons_by_type(const std::string &type) const {
            std::vector<PolygonElement> result;
            for (const auto &elem : polygon_elements_) {
                if (elem.type == type)
                    result.push_back(elem);
            }
            return result;
        }

        inline std::vector<LineElement> lines_by_type(const std::string &type) const {
            std::vector<LineElement> result;
            for (const auto &elem : line_elements_) {
                if (elem.type == type)
                    result.push_back(elem);
            }
            return result;
        }

        inline std::vector<PointElement> points_by_type(const std::string &type) const {
            std::vector<PointElement> result;
            for (const auto &elem : point_elements_) {
                if (elem.type == type)
                    result.push_back(elem);
            }
            return result;
        }

        inline std::vector<PolygonElement> polygons_by_subtype(const std::string &subtype) const {
            std::vector<PolygonElement> result;
            for (const auto &elem : polygon_elements_) {
                if (elem.subtype == subtype)
                    result.push_back(elem);
            }
            return result;
        }

        // ============ Element Removal ============

        /// Remove a polygon element by UUID. Returns true if found and removed.
        inline bool remove_polygon_element(const UUID &id) {
            auto it = std::find_if(polygon_elements_.begin(), polygon_elements_.end(),
                                   [&id](const PolygonElement &elem) { return elem.uuid == id; });
            if (it != polygon_elements_.end()) {
                // Also remove from the feature collection
                auto feat_it =
                    std::find_if(collection_.features.begin(), collection_.features.end(), [&id](const auto &f) {
                        auto uuid_it = f.properties.find("uuid");
                        return uuid_it != f.properties.end() && uuid_it->second == id.toString();
                    });
                if (feat_it != collection_.features.end()) {
                    collection_.features.erase(feat_it);
                }
                polygon_elements_.erase(it);
                return true;
            }
            return false;
        }

        /// Remove a line element by UUID. Returns true if found and removed.
        inline bool remove_line_element(const UUID &id) {
            auto it = std::find_if(line_elements_.begin(), line_elements_.end(),
                                   [&id](const LineElement &elem) { return elem.uuid == id; });
            if (it != line_elements_.end()) {
                auto feat_it =
                    std::find_if(collection_.features.begin(), collection_.features.end(), [&id](const auto &f) {
                        auto uuid_it = f.properties.find("uuid");
                        return uuid_it != f.properties.end() && uuid_it->second == id.toString();
                    });
                if (feat_it != collection_.features.end()) {
                    collection_.features.erase(feat_it);
                }
                line_elements_.erase(it);
                return true;
            }
            return false;
        }

        /// Remove a point element by UUID. Returns true if found and removed.
        inline bool remove_point_element(const UUID &id) {
            auto it = std::find_if(point_elements_.begin(), point_elements_.end(),
                                   [&id](const PointElement &elem) { return elem.uuid == id; });
            if (it != point_elements_.end()) {
                auto feat_it =
                    std::find_if(collection_.features.begin(), collection_.features.end(), [&id](const auto &f) {
                        auto uuid_it = f.properties.find("uuid");
                        return uuid_it != f.properties.end() && uuid_it->second == id.toString();
                    });
                if (feat_it != collection_.features.end()) {
                    collection_.features.erase(feat_it);
                }
                point_elements_.erase(it);
                return true;
            }
            return false;
        }

        /// Clear all polygon elements
        inline void clear_polygon_elements() {
            for (const auto &elem : polygon_elements_) {
                auto it =
                    std::find_if(collection_.features.begin(), collection_.features.end(), [&elem](const auto &f) {
                        auto uuid_it = f.properties.find("uuid");
                        return uuid_it != f.properties.end() && uuid_it->second == elem.uuid.toString();
                    });
                if (it != collection_.features.end()) {
                    collection_.features.erase(it);
                }
            }
            polygon_elements_.clear();
        }

        /// Clear all line elements
        inline void clear_line_elements() {
            for (const auto &elem : line_elements_) {
                auto it =
                    std::find_if(collection_.features.begin(), collection_.features.end(), [&elem](const auto &f) {
                        auto uuid_it = f.properties.find("uuid");
                        return uuid_it != f.properties.end() && uuid_it->second == elem.uuid.toString();
                    });
                if (it != collection_.features.end()) {
                    collection_.features.erase(it);
                }
            }
            line_elements_.clear();
        }

        /// Clear all point elements
        inline void clear_point_elements() {
            for (const auto &elem : point_elements_) {
                auto it =
                    std::find_if(collection_.features.begin(), collection_.features.end(), [&elem](const auto &f) {
                        auto uuid_it = f.properties.find("uuid");
                        return uuid_it != f.properties.end() && uuid_it->second == elem.uuid.toString();
                    });
                if (it != collection_.features.end()) {
                    collection_.features.erase(it);
                }
            }
            point_elements_.clear();
        }

        /// Clear all elements (polygons, lines, points)
        inline void clear_all_elements() {
            clear_polygon_elements();
            clear_line_elements();
            clear_point_elements();
        }

        /// Find polygon element by UUID
        inline dp::Optional<PolygonElement> polygon_element(const UUID &id) const {
            auto it = std::find_if(polygon_elements_.begin(), polygon_elements_.end(),
                                   [&id](const PolygonElement &elem) { return elem.uuid == id; });
            if (it != polygon_elements_.end())
                return *it;
            return dp::nullopt;
        }

        /// Find line element by UUID
        inline dp::Optional<LineElement> line_element(const UUID &id) const {
            auto it = std::find_if(line_elements_.begin(), line_elements_.end(),
                                   [&id](const LineElement &elem) { return elem.uuid == id; });
            if (it != line_elements_.end())
                return *it;
            return dp::nullopt;
        }

        /// Find point element by UUID
        inline dp::Optional<PointElement> point_element(const UUID &id) const {
            auto it = std::find_if(point_elements_.begin(), point_elements_.end(),
                                   [&id](const PointElement &elem) { return elem.uuid == id; });
            if (it != point_elements_.end())
                return *it;
            return dp::nullopt;
        }

        // Higher Level Operations
        inline double area() const { return has_field_boundary() ? field_boundary_.area() : 0.0; }
        inline double perimeter() const { return has_field_boundary() ? field_boundary_.perimeter() : 0.0; }
        inline bool contains(const dp::Point &point) const {
            return has_field_boundary() && field_boundary_.contains(point);
        }
        inline bool has_field_boundary() const { return !field_boundary_.vertices.empty(); }
        inline bool is_valid() const { return has_field_boundary() && !meta_.name.empty(); }

        // File I/O
        inline static Poly from_file(const std::filesystem::path &file_path) {
            if (!std::filesystem::exists(file_path)) {
                throw std::runtime_error("File does not exist: " + file_path.string());
            }

            vectkit::FeatureCollection fc = vectkit::read(file_path);

            Poly poly;
            poly.collection_ = fc;

            // Extract global properties
            auto global_props = fc.global_properties;

            auto name_it = global_props.find("name");
            if (name_it != global_props.end()) {
                poly.meta_.name = name_it->second;
            }

            auto type_it = global_props.find("type");
            if (type_it != global_props.end()) {
                poly.meta_.type = type_it->second;
            }

            auto subtype_it = global_props.find("subtype");
            if (subtype_it != global_props.end()) {
                poly.meta_.subtype = subtype_it->second;
            }

            auto uuid_it = global_props.find("uuid");
            if (uuid_it != global_props.end()) {
                poly.meta_.id = UUID(uuid_it->second);
            }

            // Find and extract field boundary from features
            for (const auto &feature : fc.features) {
                auto border_it = feature.properties.find("border");
                if (border_it != feature.properties.end() && border_it->second == "true") {
                    if (std::holds_alternative<dp::Polygon>(feature.geometry)) {
                        poly.field_boundary_ = std::get<dp::Polygon>(feature.geometry);

                        if (poly.meta_.name.empty()) {
                            auto name_it = feature.properties.find("name");
                            if (name_it != feature.properties.end()) {
                                poly.meta_.name = name_it->second;
                                const std::string suffix = "_boundary";
                                if (poly.meta_.name.size() > suffix.size() &&
                                    poly.meta_.name.substr(poly.meta_.name.size() - suffix.size()) == suffix) {
                                    poly.meta_.name.erase(poly.meta_.name.size() - suffix.size());
                                }
                            }
                        }

                        if (poly.meta_.type.empty()) {
                            auto type_it = feature.properties.find("type");
                            if (type_it != feature.properties.end()) {
                                poly.meta_.type = type_it->second;
                            }
                        }

                        if (poly.meta_.subtype.empty()) {
                            auto subtype_it = feature.properties.find("subtype");
                            if (subtype_it != feature.properties.end()) {
                                poly.meta_.subtype = subtype_it->second;
                            }
                        }

                        if (poly.meta_.id.isNull()) {
                            auto uuid_it = feature.properties.find("uuid");
                            if (uuid_it != feature.properties.end()) {
                                poly.meta_.id = UUID(uuid_it->second);
                            }
                        }

                        break;
                    }
                }
            }

            poly.load_structured_elements();
            poly.sync_to_global_properties();

            return poly;
        }

        inline void to_file(const std::filesystem::path &file_path, vectkit::CRS crs = vectkit::CRS::WGS) const {
            const_cast<Poly *>(this)->sync_to_global_properties();

            // Add field boundary as a feature if it exists
            if (has_field_boundary()) {
                // Check if boundary feature already exists
                bool boundary_exists = false;
                for (auto &feature : const_cast<Poly *>(this)->collection_.features) {
                    auto border_it = feature.properties.find("border");
                    if (border_it != feature.properties.end() && border_it->second == "true") {
                        boundary_exists = true;
                        feature.properties["uuid"] = meta_.id.toString();
                        feature.properties["name"] = meta_.name + "_boundary";
                        feature.properties["type"] = meta_.type;
                        feature.properties["subtype"] = meta_.subtype;
                        break;
                    }
                }

                // Add boundary feature if it doesn't exist
                if (!boundary_exists) {
                    vectkit::Feature boundary_feature;
                    boundary_feature.geometry = field_boundary_;
                    boundary_feature.properties["border"] = "true";
                    boundary_feature.properties["uuid"] = meta_.id.toString();
                    boundary_feature.properties["name"] = meta_.name + "_boundary";
                    boundary_feature.properties["type"] = meta_.type;
                    boundary_feature.properties["subtype"] = meta_.subtype;
                    const_cast<Poly *>(this)->collection_.features.push_back(boundary_feature);
                }
            }

            vectkit::write(collection_, file_path, crs);
        }
    };

} // namespace zoneout
