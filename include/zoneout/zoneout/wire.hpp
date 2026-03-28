#pragma once

#include "json.hpp"
#include "workspace.hpp"

#include <algorithm>
#include <cmath>
#include <concord/concord.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dp = datapod;

namespace zoneout {

    struct JsonPoint {
        double lat = 0.0;
        double lon = 0.0;
    };

    struct ZoneJson {
        std::string id = generateUUID().toString();
        std::string name = "Zone";
        std::string type = "zone";
        std::string parent_id;
        std::vector<std::string> child_ids;
        std::unordered_map<std::string, std::string> properties;
        std::vector<JsonPoint> polygon_latlon;
        bool grid_enabled = false;
        double grid_resolution = 1.0;
    };

    struct NodeJson {
        std::string id = generateUUID().toString();
        std::string name = "Node";
        JsonPoint latlon{};
        std::vector<std::string> zone_ids;
        std::unordered_map<std::string, std::string> properties;
    };

    struct EdgeJson {
        std::string id = generateUUID().toString();
        std::string source_id;
        std::string target_id;
        bool directed = false;
        double weight = 1.0;
        std::vector<std::string> zone_ids;
        std::unordered_map<std::string, std::string> properties;
    };

    struct WorkspaceJson {
        std::string root_zone_id;
        std::unordered_map<std::string, ZoneJson> zones;
        std::unordered_map<std::string, NodeJson> nodes;
        std::unordered_map<std::string, EdgeJson> edges;
        dp::Geo datum;
        bool datum_set = false;
        std::string name = "Workspace";
    };

    namespace detail {
        struct DraftJsonDeleter {
            void operator()(json_value_s *ptr) const {
                if (ptr) {
                    free(ptr);
                }
            }
        };

        using DraftJsonPtr = std::unique_ptr<json_value_s, DraftJsonDeleter>;

        inline json_object_s *draft_json_object(json_value_s *value) {
            if (!value || value->type != json_type_object) {
                return nullptr;
            }
            return static_cast<json_object_s *>(value->payload);
        }

        inline json_array_s *draft_json_array(json_value_s *value) {
            if (!value || value->type != json_type_array) {
                return nullptr;
            }
            return static_cast<json_array_s *>(value->payload);
        }

        inline json_string_s *draft_json_string(json_value_s *value) {
            if (!value || value->type != json_type_string) {
                return nullptr;
            }
            return static_cast<json_string_s *>(value->payload);
        }

        inline json_number_s *draft_json_number(json_value_s *value) {
            if (!value || value->type != json_type_number) {
                return nullptr;
            }
            return static_cast<json_number_s *>(value->payload);
        }

        inline bool draft_get_bool(json_value_s *value, bool fallback = false) {
            if (!value) {
                return fallback;
            }
            if (value->type == json_type_true) {
                return true;
            }
            if (value->type == json_type_false) {
                return false;
            }
            return fallback;
        }

        inline json_object_element_s *draft_find_element(json_object_s *obj, const char *key) {
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

        inline double draft_get_number(json_value_s *value, double fallback = 0.0) {
            auto *num = draft_json_number(value);
            if (!num) {
                return fallback;
            }
            return std::stod(std::string(num->number, num->number_size));
        }

        inline std::string draft_get_string(json_value_s *value, const std::string &fallback = "") {
            auto *str = draft_json_string(value);
            if (!str) {
                return fallback;
            }
            return std::string(str->string, str->string_size);
        }

        inline std::string draft_escape_json(const std::string &value) {
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

        inline std::unordered_map<std::string, std::string> parse_properties(json_value_s *value) {
            std::unordered_map<std::string, std::string> properties;
            auto *obj = draft_json_object(value);
            if (!obj) {
                return properties;
            }
            for (auto *elem = obj->start; elem; elem = elem->next) {
                if (!elem->name) {
                    continue;
                }
                properties[std::string(elem->name->string, elem->name->string_size)] = draft_get_string(elem->value);
            }
            return properties;
        }

        inline std::vector<std::string> parse_string_array(json_value_s *value) {
            std::vector<std::string> out;
            auto *arr = draft_json_array(value);
            if (!arr) {
                return out;
            }
            for (auto *elem = arr->start; elem; elem = elem->next) {
                out.push_back(draft_get_string(elem->value));
            }
            return out;
        }

        inline JsonPoint parse_point(json_value_s *value) {
            auto *obj = draft_json_object(value);
            if (!obj) {
                return {};
            }
            auto *lat_elem = draft_find_element(obj, "lat");
            auto *lon_elem = draft_find_element(obj, "lon");
            return {draft_get_number(lat_elem ? lat_elem->value : nullptr),
                    draft_get_number(lon_elem ? lon_elem->value : nullptr)};
        }

        inline std::vector<JsonPoint> parse_polygon(json_value_s *value) {
            std::vector<JsonPoint> polygon;
            auto *arr = draft_json_array(value);
            if (!arr) {
                return polygon;
            }
            for (auto *elem = arr->start; elem; elem = elem->next) {
                polygon.push_back(parse_point(elem->value));
            }
            return polygon;
        }

        inline std::string serialize_properties(const std::unordered_map<std::string, std::string> &properties) {
            std::ostringstream out;
            out << "{";
            bool first = true;
            for (const auto &[key, value] : properties) {
                if (!first) {
                    out << ",";
                }
                first = false;
                out << "\"" << draft_escape_json(key) << "\":\"" << draft_escape_json(value) << "\"";
            }
            out << "}";
            return out.str();
        }

        inline std::string serialize_string_array(const std::vector<std::string> &values) {
            std::ostringstream out;
            out << "[";
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) {
                    out << ",";
                }
                out << "\"" << draft_escape_json(values[i]) << "\"";
            }
            out << "]";
            return out.str();
        }

        inline std::string serialize_polygon(const std::vector<JsonPoint> &polygon) {
            std::ostringstream out;
            out << "[";
            for (size_t i = 0; i < polygon.size(); ++i) {
                if (i > 0) {
                    out << ",";
                }
                out << "{\"lat\":" << polygon[i].lat << ",\"lon\":" << polygon[i].lon << "}";
            }
            out << "]";
            return out.str();
        }

        inline UUID parse_uuid(const std::string &text, const std::string &context) {
            try {
                return UUID(text);
            } catch (const std::exception &e) {
                throw std::runtime_error("Invalid " + context + " UUID '" + text + "': " + e.what());
            }
        }

        inline std::vector<UUID> parse_uuid_list(const std::vector<std::string> &ids, const std::string &context) {
            std::vector<UUID> out;
            out.reserve(ids.size());
            for (const auto &id : ids) {
                out.push_back(parse_uuid(id, context));
            }
            return out;
        }

        inline bool valid_latlon(const JsonPoint &point) {
            return std::isfinite(point.lat) && std::isfinite(point.lon) && point.lat >= -90.0 && point.lat <= 90.0 &&
                   point.lon >= -180.0 && point.lon <= 180.0;
        }

        inline size_t unique_point_count(const std::vector<JsonPoint> &polygon) {
            std::unordered_set<std::string> unique;
            unique.reserve(polygon.size());
            for (const auto &point : polygon) {
                std::ostringstream key;
                key << point.lat << "," << point.lon;
                unique.insert(key.str());
            }
            return unique.size();
        }

        inline dp::Geo infer_datum(const WorkspaceJson &json) {
            if (json.datum_set) {
                return json.datum;
            }

            for (const auto &[id, zone] : json.zones) {
                (void)id;
                if (!zone.polygon_latlon.empty()) {
                    return {zone.polygon_latlon.front().lat, zone.polygon_latlon.front().lon, 0.0};
                }
            }

            for (const auto &[id, node] : json.nodes) {
                (void)id;
                return {node.latlon.lat, node.latlon.lon, 0.0};
            }

            throw std::runtime_error("Cannot infer workspace datum from empty draft");
        }

        inline dp::Point to_local_point(const JsonPoint &point, const dp::Geo &datum) {
            concord::earth::WGS wgs{point.lat, point.lon, 0.0};
            const auto enu = concord::frame::to_enu(datum, wgs);
            return {enu.east(), enu.north(), enu.up()};
        }

        inline JsonPoint to_json_point(const dp::Point &point, const dp::Geo &datum) {
            const concord::frame::ENU enu{point.x, point.y, point.z, datum};
            const auto wgs = concord::frame::to_wgs(enu);
            return {wgs.latitude, wgs.longitude};
        }

        inline dp::Polygon to_local_polygon(const std::vector<JsonPoint> &polygon, const dp::Geo &datum) {
            dp::Polygon out;
            out.vertices.reserve(polygon.size());
            for (const auto &point : polygon) {
                out.vertices.push_back(to_local_point(point, datum));
            }
            return out;
        }

        inline std::vector<JsonPoint> to_json_polygon(const dp::Polygon &polygon, const dp::Geo &datum) {
            std::vector<JsonPoint> out;
            out.reserve(polygon.vertices.size());
            for (const auto &point : polygon.vertices) {
                out.push_back(to_json_point(point, datum));
            }
            if (out.size() > 1 && out.front().lat == out.back().lat && out.front().lon == out.back().lon) {
                out.pop_back();
            }
            return out;
        }

        inline std::string infer_root_zone_id(const WorkspaceJson &json) {
            if (!json.root_zone_id.empty()) {
                return json.root_zone_id;
            }

            std::string candidate;
            for (const auto &[id, zone] : json.zones) {
                if (zone.parent_id.empty()) {
                    if (!candidate.empty()) {
                        throw std::runtime_error("Draft has multiple root-zone candidates and no root_zone_id");
                    }
                    candidate = id;
                }
            }
            if (candidate.empty()) {
                throw std::runtime_error("Draft has no root zone");
            }
            return candidate;
        }

        inline Zone build_zone_tree(const WorkspaceJson &json, const std::string &zone_id, const dp::Geo &datum,
                                    const std::unordered_map<std::string, std::vector<std::string>> &children_by_parent,
                                    std::unordered_map<std::string, bool> &visiting,
                                    std::unordered_map<std::string, bool> &built) {
            auto zone_it = json.zones.find(zone_id);
            if (zone_it == json.zones.end()) {
                throw std::runtime_error("Zone id '" + zone_id + "' not found in draft");
            }
            if (visiting[zone_id]) {
                throw std::runtime_error("Zone hierarchy contains a cycle at zone '" + zone_id + "'");
            }
            if (built[zone_id]) {
                throw std::runtime_error("Zone '" + zone_id + "' appears more than once in the hierarchy");
            }

            const auto &draft_zone = zone_it->second;
            if (draft_zone.polygon_latlon.size() < 3) {
                throw std::runtime_error("Zone '" + zone_id + "' must have at least 3 polygon vertices");
            }

            visiting[zone_id] = true;

            const auto boundary = to_local_polygon(draft_zone.polygon_latlon, datum);
            const double resolution = draft_zone.grid_resolution > 0.0 ? draft_zone.grid_resolution : 1.0;
            Plot plot = draft_zone.grid_enabled ? Plot(draft_zone.name, draft_zone.type, boundary, datum, resolution)
                                                : Plot(draft_zone.name, draft_zone.type, boundary, datum);

            Zone zone(draft_zone.name, draft_zone.type, std::move(plot));
            zone.set_id(parse_uuid(draft_zone.id, "zone"));
            for (const auto &[key, value] : draft_zone.properties) {
                zone.set_property(key, value);
            }

            auto child_it = children_by_parent.find(zone_id);
            if (child_it != children_by_parent.end()) {
                for (const auto &child_id : child_it->second) {
                    zone.add_child(build_zone_tree(json, child_id, datum, children_by_parent, visiting, built));
                }
            }

            visiting[zone_id] = false;
            built[zone_id] = true;
            return zone;
        }

        inline void append_zone_to_json(WorkspaceJson &json, const Zone &zone, const std::string &parent_id,
                                        const dp::Geo &datum) {
            ZoneJson out_zone;
            out_zone.id = zone.id().toString();
            out_zone.name = zone.name();
            out_zone.type = zone.type();
            out_zone.parent_id = parent_id;
            out_zone.properties = zone.properties();
            out_zone.polygon_latlon = to_json_polygon(zone.plot().poly().field_boundary(), datum);
            out_zone.grid_enabled = zone.plot().has_grid();
            out_zone.grid_resolution = zone.plot().has_grid() ? zone.plot().grid().resolution() : 1.0;
            out_zone.child_ids.clear();
            for (const auto &child : zone.children()) {
                out_zone.child_ids.push_back(child.id().toString());
            }
            json.zones[out_zone.id] = out_zone;

            for (const auto &child : zone.children()) {
                append_zone_to_json(json, child, zone.id().toString(), datum);
            }
        }
    } // namespace detail

    inline WorkspaceJson parse_workspace_json(const std::string &body) {
        detail::DraftJsonPtr root(json_parse(body.c_str(), body.size()));
        if (!root) {
            throw std::runtime_error("Failed to parse workspace JSON");
        }

        auto *root_obj = detail::draft_json_object(root.get());
        if (!root_obj) {
            throw std::runtime_error("Workspace payload must be a JSON object");
        }

        WorkspaceJson json;
        if (auto *name_elem = detail::draft_find_element(root_obj, "name")) {
            json.name = detail::draft_get_string(name_elem->value, json.name);
        }
        if (auto *root_zone_elem = detail::draft_find_element(root_obj, "root_zone_id")) {
            json.root_zone_id = detail::draft_get_string(root_zone_elem->value);
        }
        if (auto *datum_elem = detail::draft_find_element(root_obj, "datum")) {
            auto datum_point = detail::parse_point(datum_elem->value);
            json.datum = {datum_point.lat, datum_point.lon, 0.0};
            json.datum_set = true;
        }

        if (auto *zones_elem = detail::draft_find_element(root_obj, "zones")) {
            auto *zones_obj = detail::draft_json_object(zones_elem->value);
            if (zones_obj) {
                for (auto *elem = zones_obj->start; elem; elem = elem->next) {
                    auto *zone_obj = detail::draft_json_object(elem->value);
                    if (!elem->name || !zone_obj) {
                        continue;
                    }
                    ZoneJson zone;
                    zone.id = detail::draft_get_string(detail::draft_find_element(zone_obj, "id")
                                                           ? detail::draft_find_element(zone_obj, "id")->value
                                                           : nullptr,
                                                       std::string(elem->name->string, elem->name->string_size));
                    zone.name = detail::draft_get_string(detail::draft_find_element(zone_obj, "name")
                                                             ? detail::draft_find_element(zone_obj, "name")->value
                                                             : nullptr,
                                                         zone.name);
                    zone.type = detail::draft_get_string(detail::draft_find_element(zone_obj, "type")
                                                             ? detail::draft_find_element(zone_obj, "type")->value
                                                             : nullptr,
                                                         zone.type);
                    zone.parent_id =
                        detail::draft_get_string(detail::draft_find_element(zone_obj, "parent_id")
                                                     ? detail::draft_find_element(zone_obj, "parent_id")->value
                                                     : nullptr);
                    zone.child_ids =
                        detail::parse_string_array(detail::draft_find_element(zone_obj, "child_ids")
                                                       ? detail::draft_find_element(zone_obj, "child_ids")->value
                                                       : nullptr);
                    zone.properties =
                        detail::parse_properties(detail::draft_find_element(zone_obj, "properties")
                                                     ? detail::draft_find_element(zone_obj, "properties")->value
                                                     : nullptr);
                    zone.polygon_latlon =
                        detail::parse_polygon(detail::draft_find_element(zone_obj, "polygon_latlon")
                                                  ? detail::draft_find_element(zone_obj, "polygon_latlon")->value
                                                  : nullptr);
                    zone.grid_enabled =
                        detail::draft_get_bool(detail::draft_find_element(zone_obj, "grid_enabled")
                                                   ? detail::draft_find_element(zone_obj, "grid_enabled")->value
                                                   : nullptr);
                    zone.grid_resolution =
                        detail::draft_get_number(detail::draft_find_element(zone_obj, "grid_resolution")
                                                     ? detail::draft_find_element(zone_obj, "grid_resolution")->value
                                                     : nullptr,
                                                 1.0);
                    json.zones[zone.id] = zone;
                }
            }
        }

        if (auto *nodes_elem = detail::draft_find_element(root_obj, "nodes")) {
            auto *nodes_obj = detail::draft_json_object(nodes_elem->value);
            if (nodes_obj) {
                for (auto *elem = nodes_obj->start; elem; elem = elem->next) {
                    auto *node_obj = detail::draft_json_object(elem->value);
                    if (!elem->name || !node_obj) {
                        continue;
                    }
                    NodeJson node;
                    node.id = detail::draft_get_string(detail::draft_find_element(node_obj, "id")
                                                           ? detail::draft_find_element(node_obj, "id")->value
                                                           : nullptr,
                                                       std::string(elem->name->string, elem->name->string_size));
                    node.name = detail::draft_get_string(detail::draft_find_element(node_obj, "name")
                                                             ? detail::draft_find_element(node_obj, "name")->value
                                                             : nullptr,
                                                         node.name);
                    node.latlon = detail::parse_point(detail::draft_find_element(node_obj, "latlon")
                                                          ? detail::draft_find_element(node_obj, "latlon")->value
                                                          : nullptr);
                    node.zone_ids =
                        detail::parse_string_array(detail::draft_find_element(node_obj, "zone_ids")
                                                       ? detail::draft_find_element(node_obj, "zone_ids")->value
                                                       : nullptr);
                    node.properties =
                        detail::parse_properties(detail::draft_find_element(node_obj, "properties")
                                                     ? detail::draft_find_element(node_obj, "properties")->value
                                                     : nullptr);
                    json.nodes[node.id] = node;
                }
            }
        }

        if (auto *edges_elem = detail::draft_find_element(root_obj, "edges")) {
            auto *edges_obj = detail::draft_json_object(edges_elem->value);
            if (edges_obj) {
                for (auto *elem = edges_obj->start; elem; elem = elem->next) {
                    auto *edge_obj = detail::draft_json_object(elem->value);
                    if (!elem->name || !edge_obj) {
                        continue;
                    }
                    EdgeJson edge;
                    edge.id = detail::draft_get_string(detail::draft_find_element(edge_obj, "id")
                                                           ? detail::draft_find_element(edge_obj, "id")->value
                                                           : nullptr,
                                                       std::string(elem->name->string, elem->name->string_size));
                    edge.source_id =
                        detail::draft_get_string(detail::draft_find_element(edge_obj, "source_id")
                                                     ? detail::draft_find_element(edge_obj, "source_id")->value
                                                     : nullptr);
                    edge.target_id =
                        detail::draft_get_string(detail::draft_find_element(edge_obj, "target_id")
                                                     ? detail::draft_find_element(edge_obj, "target_id")->value
                                                     : nullptr);
                    edge.directed = detail::draft_get_bool(detail::draft_find_element(edge_obj, "directed")
                                                               ? detail::draft_find_element(edge_obj, "directed")->value
                                                               : nullptr);
                    edge.weight = detail::draft_get_number(detail::draft_find_element(edge_obj, "weight")
                                                               ? detail::draft_find_element(edge_obj, "weight")->value
                                                               : nullptr,
                                                           1.0);
                    edge.zone_ids =
                        detail::parse_string_array(detail::draft_find_element(edge_obj, "zone_ids")
                                                       ? detail::draft_find_element(edge_obj, "zone_ids")->value
                                                       : nullptr);
                    edge.properties =
                        detail::parse_properties(detail::draft_find_element(edge_obj, "properties")
                                                     ? detail::draft_find_element(edge_obj, "properties")->value
                                                     : nullptr);
                    json.edges[edge.id] = edge;
                }
            }
        }

        return json;
    }

    inline WorkspaceJson parse_workspace_json_file(const std::filesystem::path &path) {
        std::ifstream in(path);
        if (!in.is_open()) {
            throw std::runtime_error("Failed to open workspace JSON file: " + path.string());
        }
        std::stringstream buffer;
        buffer << in.rdbuf();
        return parse_workspace_json(buffer.str());
    }

    inline std::string workspace_json(const WorkspaceJson &json) {
        std::ostringstream out;
        out << "{";
        out << "\"name\":\"" << detail::draft_escape_json(json.name) << "\",";
        out << "\"root_zone_id\":\"" << detail::draft_escape_json(json.root_zone_id) << "\",";
        out << "\"datum\":";
        if (json.datum_set) {
            out << "{\"lat\":" << json.datum.latitude << ",\"lon\":" << json.datum.longitude << "},";
        } else {
            out << "null,";
        }

        out << "\"zones\":{";
        bool first_zone = true;
        for (const auto &[id, zone] : json.zones) {
            if (!first_zone) {
                out << ",";
            }
            first_zone = false;
            out << "\"" << detail::draft_escape_json(id) << "\":{";
            out << "\"id\":\"" << detail::draft_escape_json(zone.id) << "\",";
            out << "\"name\":\"" << detail::draft_escape_json(zone.name) << "\",";
            out << "\"type\":\"" << detail::draft_escape_json(zone.type) << "\",";
            out << "\"parent_id\":\"" << detail::draft_escape_json(zone.parent_id) << "\",";
            out << "\"child_ids\":" << detail::serialize_string_array(zone.child_ids) << ",";
            out << "\"properties\":" << detail::serialize_properties(zone.properties) << ",";
            out << "\"polygon_latlon\":" << detail::serialize_polygon(zone.polygon_latlon) << ",";
            out << "\"grid_enabled\":" << (zone.grid_enabled ? "true" : "false") << ",";
            out << "\"grid_resolution\":" << zone.grid_resolution;
            out << "}";
        }
        out << "},";

        out << "\"nodes\":{";
        bool first_node = true;
        for (const auto &[id, node] : json.nodes) {
            if (!first_node) {
                out << ",";
            }
            first_node = false;
            out << "\"" << detail::draft_escape_json(id) << "\":{";
            out << "\"id\":\"" << detail::draft_escape_json(node.id) << "\",";
            out << "\"name\":\"" << detail::draft_escape_json(node.name) << "\",";
            out << "\"latlon\":{\"lat\":" << node.latlon.lat << ",\"lon\":" << node.latlon.lon << "},";
            out << "\"zone_ids\":" << detail::serialize_string_array(node.zone_ids) << ",";
            out << "\"properties\":" << detail::serialize_properties(node.properties);
            out << "}";
        }
        out << "},";

        out << "\"edges\":{";
        bool first_edge = true;
        for (const auto &[id, edge] : json.edges) {
            if (!first_edge) {
                out << ",";
            }
            first_edge = false;
            out << "\"" << detail::draft_escape_json(id) << "\":{";
            out << "\"id\":\"" << detail::draft_escape_json(edge.id) << "\",";
            out << "\"source_id\":\"" << detail::draft_escape_json(edge.source_id) << "\",";
            out << "\"target_id\":\"" << detail::draft_escape_json(edge.target_id) << "\",";
            out << "\"directed\":" << (edge.directed ? "true" : "false") << ",";
            out << "\"weight\":" << edge.weight << ",";
            out << "\"zone_ids\":" << detail::serialize_string_array(edge.zone_ids) << ",";
            out << "\"properties\":" << detail::serialize_properties(edge.properties);
            out << "}";
        }
        out << "}";
        out << "}";
        return out.str();
    }

    inline void write_workspace_json_file(const std::filesystem::path &path, const WorkspaceJson &json) {
        std::ofstream out(path);
        if (!out.is_open()) {
            throw std::runtime_error("Failed to open workspace JSON file for writing: " + path.string());
        }
        out << workspace_json(json);
    }

    inline std::vector<std::string> validate_workspace_json(const WorkspaceJson &json) {
        std::vector<std::string> errors;

        if (json.zones.empty()) {
            errors.push_back("Workspace JSON must contain at least one zone");
            return errors;
        }

        if (json.datum_set) {
            const JsonPoint datum_point{json.datum.latitude, json.datum.longitude};
            if (!detail::valid_latlon(datum_point)) {
                errors.push_back("Workspace JSON datum is not a valid latitude/longitude");
            }
        }

        std::string inferred_root_id;
        try {
            inferred_root_id = detail::infer_root_zone_id(json);
        } catch (const std::exception &e) {
            errors.push_back(e.what());
        }

        if (!inferred_root_id.empty() && !json.zones.contains(inferred_root_id)) {
            errors.push_back("Workspace JSON root zone '" + inferred_root_id + "' does not exist");
        }
        if (!json.root_zone_id.empty()) {
            try {
                detail::parse_uuid(json.root_zone_id, "root zone");
            } catch (const std::exception &e) {
                errors.push_back(e.what());
            }
        }

        for (const auto &[id, zone] : json.zones) {
            if (id != zone.id) {
                errors.push_back("Zone map key '" + id + "' does not match zone.id '" + zone.id + "'");
            }
            try {
                detail::parse_uuid(zone.id, "zone");
            } catch (const std::exception &e) {
                errors.push_back(e.what());
            }
            if (zone.polygon_latlon.size() < 3) {
                errors.push_back("Zone '" + id + "' must have at least 3 polygon vertices");
            }
            if (detail::unique_point_count(zone.polygon_latlon) < 3) {
                errors.push_back("Zone '" + id + "' must contain at least 3 unique polygon vertices");
            }
            for (const auto &point : zone.polygon_latlon) {
                if (!detail::valid_latlon(point)) {
                    errors.push_back("Zone '" + id + "' contains an invalid latitude/longitude vertex");
                    break;
                }
            }
            if (!zone.parent_id.empty() && !json.zones.contains(zone.parent_id)) {
                errors.push_back("Zone '" + id + "' references missing parent zone '" + zone.parent_id + "'");
            }
            if (!zone.parent_id.empty()) {
                try {
                    detail::parse_uuid(zone.parent_id, "zone parent");
                } catch (const std::exception &e) {
                    errors.push_back(e.what());
                }
            }
            if (zone.grid_enabled && zone.grid_resolution <= 0.0) {
                errors.push_back("Zone '" + id + "' has non-positive grid_resolution");
            }
            for (const auto &child_id : zone.child_ids) {
                try {
                    detail::parse_uuid(child_id, "zone child");
                } catch (const std::exception &e) {
                    errors.push_back(e.what());
                }
                if (!json.zones.contains(child_id)) {
                    errors.push_back("Zone '" + id + "' references missing child zone '" + child_id + "'");
                    continue;
                }
                const auto &child = json.zones.at(child_id);
                if (child.parent_id != id) {
                    errors.push_back("Zone '" + id + "' lists child '" + child_id +
                                     "' but that child does not point back with matching parent_id");
                }
            }
            if (!zone.parent_id.empty() && json.zones.contains(zone.parent_id)) {
                const auto &parent = json.zones.at(zone.parent_id);
                const auto listed_by_parent =
                    std::find(parent.child_ids.begin(), parent.child_ids.end(), id) != parent.child_ids.end();
                if (!listed_by_parent) {
                    errors.push_back("Zone '" + id + "' points to parent '" + zone.parent_id +
                                     "' but is missing from that parent's child_ids");
                }
            }
        }

        for (const auto &[id, node] : json.nodes) {
            if (id != node.id) {
                errors.push_back("Node map key '" + id + "' does not match node.id '" + node.id + "'");
            }
            try {
                detail::parse_uuid(node.id, "node");
            } catch (const std::exception &e) {
                errors.push_back(e.what());
            }
            if (!detail::valid_latlon(node.latlon)) {
                errors.push_back("Node '" + id + "' has invalid latitude/longitude");
            }
            for (const auto &zone_id : node.zone_ids) {
                try {
                    detail::parse_uuid(zone_id, "node zone");
                } catch (const std::exception &e) {
                    errors.push_back(e.what());
                }
                if (!json.zones.contains(zone_id)) {
                    errors.push_back("Node '" + id + "' references missing zone '" + zone_id + "'");
                }
            }
        }

        for (const auto &[id, edge] : json.edges) {
            if (id != edge.id) {
                errors.push_back("Edge map key '" + id + "' does not match edge.id '" + edge.id + "'");
            }
            try {
                detail::parse_uuid(edge.id, "edge");
            } catch (const std::exception &e) {
                errors.push_back(e.what());
            }
            if (!std::isfinite(edge.weight)) {
                errors.push_back("Edge '" + id + "' has non-finite weight");
            }
            if (!json.nodes.contains(edge.source_id)) {
                errors.push_back("Edge '" + id + "' references unknown source node '" + edge.source_id + "'");
            }
            if (!json.nodes.contains(edge.target_id)) {
                errors.push_back("Edge '" + id + "' references unknown target node '" + edge.target_id + "'");
            }
            try {
                detail::parse_uuid(edge.source_id, "edge source");
            } catch (const std::exception &e) {
                errors.push_back(e.what());
            }
            try {
                detail::parse_uuid(edge.target_id, "edge target");
            } catch (const std::exception &e) {
                errors.push_back(e.what());
            }
            for (const auto &zone_id : edge.zone_ids) {
                try {
                    detail::parse_uuid(zone_id, "edge zone");
                } catch (const std::exception &e) {
                    errors.push_back(e.what());
                }
                if (!json.zones.contains(zone_id)) {
                    errors.push_back("Edge '" + id + "' references missing zone '" + zone_id + "'");
                }
            }
        }

        return errors;
    }

    inline void require_valid_workspace_json(const WorkspaceJson &json) {
        const auto errors = validate_workspace_json(json);
        if (errors.empty()) {
            return;
        }

        std::ostringstream out;
        out << "WorkspaceJson validation failed:";
        for (const auto &error : errors) {
            out << "\n- " << error;
        }
        throw std::runtime_error(out.str());
    }

    inline Workspace to_workspace(const WorkspaceJson &json) {
        require_valid_workspace_json(json);

        const auto datum = detail::infer_datum(json);
        const auto root_zone_id = detail::infer_root_zone_id(json);

        std::unordered_map<std::string, std::vector<std::string>> children_by_parent;
        for (const auto &[id, zone] : json.zones) {
            if (!zone.parent_id.empty()) {
                children_by_parent[zone.parent_id].push_back(id);
            }
        }

        std::unordered_map<std::string, bool> visiting;
        std::unordered_map<std::string, bool> built;
        auto root_zone = detail::build_zone_tree(json, root_zone_id, datum, children_by_parent, visiting, built);

        if (built.size() != json.zones.size()) {
            for (const auto &[id, zone] : json.zones) {
                (void)zone;
                if (!built[id]) {
                    throw std::runtime_error("Zone '" + id + "' is disconnected from the root zone");
                }
            }
        }

        Workspace workspace(std::move(root_zone), Graph{});
        std::unordered_map<std::string, Graph::VertexId> node_map;

        for (const auto &[id, draft_node] : json.nodes) {
            NodeData node(detail::parse_uuid(draft_node.id, "node"), detail::to_local_point(draft_node.latlon, datum),
                          draft_node.properties);
            auto vertex_id = workspace.graph().add_vertex(node);

            auto computed_zone_ids = workspace.zones_containing(node.position);
            workspace.graph()[vertex_id].zone_ids.clear();
            for (const auto *zone : computed_zone_ids) {
                workspace.graph()[vertex_id].zone_ids.push_back(zone->id());
            }

            auto manual_zone_ids = detail::parse_uuid_list(draft_node.zone_ids, "node zone");
            for (const auto &zone_id : manual_zone_ids) {
                const auto already_present = std::find(workspace.graph()[vertex_id].zone_ids.begin(),
                                                       workspace.graph()[vertex_id].zone_ids.end(),
                                                       zone_id) != workspace.graph()[vertex_id].zone_ids.end();
                if (!already_present) {
                    workspace.graph()[vertex_id].zone_ids.push_back(zone_id);
                }
            }

            node_map[id] = vertex_id;
        }

        for (const auto &[id, draft_edge] : json.edges) {
            auto source_it = node_map.find(draft_edge.source_id);
            auto target_it = node_map.find(draft_edge.target_id);
            if (source_it == node_map.end() || target_it == node_map.end()) {
                throw std::runtime_error("Edge '" + id + "' references unknown node ids");
            }

            EdgeData edge(detail::parse_uuid(draft_edge.id, "edge"), draft_edge.properties);
            const auto edge_type =
                draft_edge.directed ? graphix::vertex::EdgeType::Directed : graphix::vertex::EdgeType::Undirected;
            const auto edge_id =
                workspace.graph().add_edge(source_it->second, target_it->second, draft_edge.weight, edge_type, edge);

            std::unordered_map<std::string, UUID> unique_zone_ids;
            for (const auto &zone_id : workspace.graph()[source_it->second].zone_ids) {
                unique_zone_ids[zone_id.toString()] = zone_id;
            }
            for (const auto &zone_id : workspace.graph()[target_it->second].zone_ids) {
                unique_zone_ids[zone_id.toString()] = zone_id;
            }
            for (const auto &zone_id : detail::parse_uuid_list(draft_edge.zone_ids, "edge zone")) {
                unique_zone_ids[zone_id.toString()] = zone_id;
            }

            workspace.graph().edge_property(edge_id).zone_ids.clear();
            for (const auto &[key, zone_id] : unique_zone_ids) {
                (void)key;
                workspace.graph().edge_property(edge_id).zone_ids.push_back(zone_id);
            }
        }

        return workspace;
    }

    inline Workspace load_workspace_json_file(const std::filesystem::path &path) {
        return to_workspace(parse_workspace_json_file(path));
    }

    inline WorkspaceJson from_workspace(const Workspace &workspace) {
        WorkspaceJson json;
        json.name = "Workspace";
        json.root_zone_id = workspace.root_zone().id().toString();
        json.datum = workspace.root_zone().plot().datum();
        json.datum_set = workspace.root_zone().plot().datum().is_set();

        detail::append_zone_to_json(json, workspace.root_zone(), "", json.datum);

        for (auto vertex_id : workspace.graph().vertices()) {
            const auto &node = workspace.graph()[vertex_id];
            NodeJson out_node;
            out_node.id = node.id.toString();
            out_node.name = node.properties.count("name") ? node.properties.at("name") : "Node";
            out_node.latlon = detail::to_json_point(node.position, json.datum);
            out_node.properties = node.properties;
            out_node.zone_ids.clear();
            for (const auto &zone_id : node.zone_ids) {
                out_node.zone_ids.push_back(zone_id.toString());
            }
            json.nodes[out_node.id] = out_node;
        }

        for (const auto &edge_desc : workspace.graph().edges()) {
            const auto &edge = workspace.graph().edge_property(edge_desc.id);
            EdgeJson out_edge;
            out_edge.id = edge.id.toString();
            out_edge.source_id = workspace.graph()[edge_desc.source].id.toString();
            out_edge.target_id = workspace.graph()[edge_desc.target].id.toString();
            out_edge.directed = edge_desc.type == graphix::vertex::EdgeType::Directed;
            out_edge.weight = edge_desc.weight;
            out_edge.properties = edge.properties;
            out_edge.zone_ids.clear();
            for (const auto &zone_id : edge.zone_ids) {
                out_edge.zone_ids.push_back(zone_id.toString());
            }
            json.edges[out_edge.id] = out_edge;
        }

        return json;
    }

    inline void write_workspace_json_file(const std::filesystem::path &path, const Workspace &workspace) {
        write_workspace_json_file(path, from_workspace(workspace));
    }

} // namespace zoneout
