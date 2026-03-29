#pragma once

#include "json.hpp"

#include <filesystem>
#include <fstream>
#include <graphix/vertex/graph.hpp>
#include <memory>
#include <sstream>
#include <unordered_set>

#include "zone.hpp"

namespace dp = datapod;

namespace zoneout {

    enum class CoordMode { Global, Local };

    struct NodeData {
        UUID id;
        dp::Point position;
        std::vector<UUID> zone_ids;
        std::unordered_map<std::string, std::string> properties;

        NodeData() : id(generateUUID()), position{} {}

        NodeData(const dp::Point &pos, const std::unordered_map<std::string, std::string> &props = {})
            : id(generateUUID()), position(pos), properties(props) {}

        NodeData(const UUID &node_id, const dp::Point &pos,
                 const std::unordered_map<std::string, std::string> &props = {})
            : id(node_id), position(pos), properties(props) {}
    };

    struct EdgeData {
        UUID id;
        std::vector<UUID> zone_ids;
        std::unordered_map<std::string, std::string> properties;

        EdgeData() : id(generateUUID()) {}

        EdgeData(const std::unordered_map<std::string, std::string> &props) : id(generateUUID()), properties(props) {}

        EdgeData(const UUID &edge_id, const std::unordered_map<std::string, std::string> &props = {})
            : id(edge_id), properties(props) {}
    };

    using Graph = graphix::vertex::Graph<NodeData, EdgeData>;

    class Workspace {
      private:
        Zone root_zone_;
        Graph graph_;
        dp::Geo ref_{};
        bool ref_set_ = false;
        CoordMode coord_mode_ = CoordMode::Global;

        struct ZoneMetadata {
            UUID id;
            std::string name;
            std::string type;
            std::unordered_map<std::string, std::string> properties;
            std::vector<UUID> node_ids;
            std::vector<UUID> child_ids;
        };

        struct JsonDeleter {
            void operator()(json_value_s *ptr) const {
                if (ptr) {
                    free(ptr);
                }
            }
        };

        using JsonPtr = std::unique_ptr<json_value_s, JsonDeleter>;

        inline std::vector<UUID> zone_membership_for_point(const dp::Point &point) const {
            std::vector<UUID> zone_ids;
            root_zone_.visit(
                [&](const Zone &zone, [[maybe_unused]] size_t depth) {
                    if (zone.contains(point)) {
                        zone_ids.push_back(zone.id());
                    }
                },
                0);
            return zone_ids;
        }

        inline static json_object_s *json_object(json_value_s *value) {
            if (!value || value->type != json_type_object) {
                return nullptr;
            }
            return static_cast<json_object_s *>(value->payload);
        }

        inline static json_array_s *json_array(json_value_s *value) {
            if (!value || value->type != json_type_array) {
                return nullptr;
            }
            return static_cast<json_array_s *>(value->payload);
        }

        inline static json_string_s *json_string(json_value_s *value) {
            if (!value || value->type != json_type_string) {
                return nullptr;
            }
            return static_cast<json_string_s *>(value->payload);
        }

        inline static json_number_s *json_number(json_value_s *value) {
            if (!value || value->type != json_type_number) {
                return nullptr;
            }
            return static_cast<json_number_s *>(value->payload);
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

        inline static void write_zone_ids_json(std::ostream &out, const std::vector<UUID> &zone_ids) {
            out << "[";
            for (size_t i = 0; i < zone_ids.size(); ++i) {
                if (i > 0) {
                    out << ",";
                }
                out << "\"" << zone_ids[i].toString() << "\"";
            }
            out << "]";
        }

        inline static void write_uuid_json(std::ostream &out, const char *key, const UUID &id,
                                           bool trailing_comma = true) {
            out << "\"" << key << "\":\"" << id.toString() << "\"";
            if (trailing_comma) {
                out << ",";
            }
        }

        inline static std::unordered_map<std::string, std::string> parse_properties(json_value_s *value) {
            std::unordered_map<std::string, std::string> properties;
            auto *obj = json_object(value);
            if (!obj) {
                return properties;
            }
            for (auto *elem = obj->start; elem; elem = elem->next) {
                auto *str = json_string(elem->value);
                if (!elem->name || !str) {
                    continue;
                }
                properties[std::string(elem->name->string, elem->name->string_size)] =
                    std::string(str->string, str->string_size);
            }
            return properties;
        }

        inline static std::vector<UUID> parse_zone_ids(json_value_s *value) {
            std::vector<UUID> zone_ids;
            auto *arr = json_array(value);
            if (!arr) {
                return zone_ids;
            }
            for (auto *elem = arr->start; elem; elem = elem->next) {
                auto *str = json_string(elem->value);
                if (!str) {
                    continue;
                }
                zone_ids.emplace_back(std::string(str->string, str->string_size));
            }
            return zone_ids;
        }

        inline static double parse_number(json_value_s *value, double fallback = 0.0) {
            auto *num = json_number(value);
            if (!num) {
                return fallback;
            }
            return std::stod(std::string(num->number, num->number_size));
        }

        inline static std::string parse_string_value(json_value_s *value, const std::string &fallback = "") {
            auto *str = json_string(value);
            if (!str) {
                return fallback;
            }
            return std::string(str->string, str->string_size);
        }

        inline static UUID parse_uuid_value(json_value_s *value) { return UUID(parse_string_value(value)); }

        inline static std::string coord_mode_string(CoordMode mode) {
            return mode == CoordMode::Local ? "local" : "global";
        }

        inline static CoordMode parse_coord_mode_value(json_value_s *value) {
            return parse_string_value(value, "global") == "local" ? CoordMode::Local : CoordMode::Global;
        }

        inline static ZoneMetadata load_zone_metadata(const std::filesystem::path &zone_json_path) {
            std::ifstream in(zone_json_path);
            if (!in.is_open()) {
                throw std::runtime_error("Failed to open zone metadata file: " + zone_json_path.string());
            }

            std::stringstream buffer;
            buffer << in.rdbuf();
            const auto json_text = buffer.str();

            JsonPtr root(json_parse(json_text.c_str(), json_text.size()));
            if (!root) {
                throw std::runtime_error("Failed to parse zone metadata JSON: " + zone_json_path.string());
            }

            auto *root_obj = json_object(root.get());
            if (!root_obj) {
                throw std::runtime_error("Invalid zone metadata JSON object: " + zone_json_path.string());
            }

            auto *id_elem = find_element(root_obj, "id");
            auto *name_elem = find_element(root_obj, "name");
            auto *type_elem = find_element(root_obj, "type");
            if (!id_elem || !name_elem || !type_elem) {
                throw std::runtime_error("Zone metadata missing required fields: " + zone_json_path.string());
            }

            ZoneMetadata metadata;
            metadata.id = parse_uuid_value(id_elem->value);
            metadata.name = parse_string_value(name_elem->value);
            metadata.type = parse_string_value(type_elem->value);

            if (auto *props_elem = find_element(root_obj, "properties")) {
                metadata.properties = parse_properties(props_elem->value);
            }
            if (auto *node_ids_elem = find_element(root_obj, "node_ids")) {
                metadata.node_ids = parse_zone_ids(node_ids_elem->value);
            }
            if (auto *child_ids_elem = find_element(root_obj, "child_ids")) {
                metadata.child_ids = parse_zone_ids(child_ids_elem->value);
            }

            return metadata;
        }

        struct WorkspaceManifest {
            UUID root_zone_id;
            dp::Geo ref{};
            bool ref_set = false;
            CoordMode coord_mode = CoordMode::Global;
        };

        inline static WorkspaceManifest load_workspace_manifest(const std::filesystem::path &workspace_json_path) {
            std::ifstream in(workspace_json_path);
            if (!in.is_open()) {
                throw std::runtime_error("Failed to open workspace manifest: " + workspace_json_path.string());
            }

            std::stringstream buffer;
            buffer << in.rdbuf();
            const auto json_text = buffer.str();

            JsonPtr root(json_parse(json_text.c_str(), json_text.size()));
            if (!root) {
                throw std::runtime_error("Failed to parse workspace manifest JSON: " + workspace_json_path.string());
            }

            auto *root_obj = json_object(root.get());
            if (!root_obj) {
                throw std::runtime_error("Invalid workspace manifest JSON object: " + workspace_json_path.string());
            }

            auto *root_zone_elem = find_element(root_obj, "root_zone_id");
            if (!root_zone_elem) {
                throw std::runtime_error("Workspace manifest missing root_zone_id: " + workspace_json_path.string());
            }
            WorkspaceManifest manifest;
            manifest.root_zone_id = parse_uuid_value(root_zone_elem->value);
            if (auto *coord_mode_elem = find_element(root_obj, "coord_mode")) {
                manifest.coord_mode = parse_coord_mode_value(coord_mode_elem->value);
            }
            if (auto *ref_elem = find_element(root_obj, "ref")) {
                auto *ref_obj = json_object(ref_elem->value);
                if (ref_obj) {
                    auto *lat_elem = find_element(ref_obj, "lat");
                    auto *lon_elem = find_element(ref_obj, "lon");
                    if (lat_elem && lon_elem) {
                        manifest.ref.latitude = parse_number(lat_elem->value, 0.0);
                        manifest.ref.longitude = parse_number(lon_elem->value, 0.0);
                        manifest.ref.altitude = 0.0;
                        manifest.ref_set = true;
                    }
                }
            }
            return manifest;
        }

        inline static void save_zone_recursive(const Zone &zone, const std::filesystem::path &zones_dir) {
            const auto zone_dir = zones_dir / zone.id().toString();
            std::filesystem::create_directories(zone_dir);

            zone.save_plot_files(zone_dir / "vector.geojson", zone_dir / "raster.tiff");

            std::ofstream out(zone_dir / "zone.json");
            if (!out.is_open()) {
                throw std::runtime_error("Failed to open zone metadata file for writing: " +
                                         (zone_dir / "zone.json").string());
            }

            out << "{";
            write_uuid_json(out, "id", zone.id());
            out << "\"name\":\"" << escape_json(zone.name()) << "\",";
            out << "\"type\":\"" << escape_json(zone.type()) << "\",";
            out << "\"properties\":";
            write_properties_json(out, zone.properties());
            out << ",\"node_ids\":";
            write_zone_ids_json(out, zone.node_ids());
            out << ",\"child_ids\":";

            std::vector<UUID> child_ids;
            child_ids.reserve(zone.children().size());
            for (const auto &child : zone.children()) {
                child_ids.push_back(child.id());
            }
            write_zone_ids_json(out, child_ids);
            out << "}";

            for (const auto &child : zone.children()) {
                save_zone_recursive(child, zones_dir);
            }
        }

        inline static Zone load_zone_recursive(const std::filesystem::path &zones_dir, const UUID &zone_id) {
            const auto zone_dir = zones_dir / zone_id.toString();
            const auto metadata = load_zone_metadata(zone_dir / "zone.json");

            auto zone = Zone::load_plot_files(zone_dir / "vector.geojson", zone_dir / "raster.tiff");
            zone.set_name(metadata.name);
            zone.set_type(metadata.type);
            zone.clear_properties();
            zone.set_node_ids(metadata.node_ids);
            for (const auto &[key, value] : metadata.properties) {
                zone.set_property(key, value);
            }

            for (const auto &child_id : metadata.child_ids) {
                zone.add_child(load_zone_recursive(zones_dir, child_id));
            }

            return zone;
        }

        inline void save_graph(const std::filesystem::path &file_path) const {
            std::ofstream out(file_path);
            if (!out.is_open()) {
                throw std::runtime_error("Failed to open graph file for writing: " + file_path.string());
            }

            out << "{";

            out << "\"nodes\":[";
            auto vertices = graph_.vertices();
            for (size_t i = 0; i < vertices.size(); ++i) {
                if (i > 0) {
                    out << ",";
                }
                const auto vertex_id = vertices[i];
                const auto &node = graph_[vertex_id];
                out << "{"
                    << "\"id\":\"" << node.id.toString() << "\","
                    << "\"position\":{"
                    << "\"x\":" << node.position.x << ","
                    << "\"y\":" << node.position.y << ","
                    << "\"z\":" << node.position.z << "},"
                    << "\"zone_ids\":";
                write_zone_ids_json(out, node.zone_ids);
                out << ",\"properties\":";
                write_properties_json(out, node.properties);
                out << "}";
            }
            out << "],";

            out << "\"edges\":[";
            auto edges = graph_.edges();
            for (size_t i = 0; i < edges.size(); ++i) {
                if (i > 0) {
                    out << ",";
                }
                const auto &edge_desc = edges[i];
                const auto &edge = graph_.edge_property(edge_desc.id);
                out << "{"
                    << "\"id\":\"" << edge.id.toString() << "\","
                    << "\"source_node_id\":\"" << graph_[edge_desc.source].id.toString() << "\","
                    << "\"target_node_id\":\"" << graph_[edge_desc.target].id.toString() << "\","
                    << "\"weight\":" << edge_desc.weight << ","
                    << "\"type\":\""
                    << (edge_desc.type == graphix::vertex::EdgeType::Directed ? "directed" : "undirected") << "\","
                    << "\"zone_ids\":";
                write_zone_ids_json(out, edge.zone_ids);
                out << ",\"properties\":";
                write_properties_json(out, edge.properties);
                out << "}";
            }
            out << "]";

            out << "}";
        }

        inline static Graph load_graph(const std::filesystem::path &file_path) {
            if (!std::filesystem::exists(file_path)) {
                return {};
            }

            std::ifstream in(file_path);
            std::stringstream buffer;
            buffer << in.rdbuf();
            const auto json_text = buffer.str();

            JsonPtr root(json_parse(json_text.c_str(), json_text.size()));
            if (!root) {
                throw std::runtime_error("Failed to parse workspace graph JSON: " + file_path.string());
            }

            auto *root_obj = json_object(root.get());
            if (!root_obj) {
                throw std::runtime_error("Invalid workspace graph JSON object: " + file_path.string());
            }

            Graph graph;
            std::unordered_map<std::string, Graph::VertexId> node_map;

            if (auto *nodes_elem = find_element(root_obj, "nodes")) {
                if (auto *nodes = json_array(nodes_elem->value)) {
                    for (auto *node_elem = nodes->start; node_elem; node_elem = node_elem->next) {
                        auto *node_obj = json_object(node_elem->value);
                        if (!node_obj) {
                            continue;
                        }

                        const auto *id_elem = find_element(node_obj, "id");
                        const auto *position_elem = find_element(node_obj, "position");
                        if (!id_elem || !position_elem) {
                            continue;
                        }

                        auto *id_str = json_string(id_elem->value);
                        auto *position_obj = json_object(position_elem->value);
                        if (!id_str || !position_obj) {
                            continue;
                        }

                        auto *x_elem = find_element(position_obj, "x");
                        auto *y_elem = find_element(position_obj, "y");
                        auto *z_elem = find_element(position_obj, "z");
                        if (!x_elem || !y_elem || !z_elem) {
                            continue;
                        }

                        NodeData node;
                        node.id = UUID(std::string(id_str->string, id_str->string_size));
                        node.position.x = parse_number(x_elem->value, 0.0);
                        node.position.y = parse_number(y_elem->value, 0.0);
                        node.position.z = parse_number(z_elem->value, 0.0);

                        if (auto *zone_ids_elem = find_element(node_obj, "zone_ids")) {
                            node.zone_ids = parse_zone_ids(zone_ids_elem->value);
                        }
                        if (auto *props_elem = find_element(node_obj, "properties")) {
                            node.properties = parse_properties(props_elem->value);
                        }

                        auto vertex_id = graph.add_vertex(node);
                        node_map[node.id.toString()] = vertex_id;
                    }
                }
            }

            if (auto *edges_elem = find_element(root_obj, "edges")) {
                if (auto *edges = json_array(edges_elem->value)) {
                    for (auto *edge_elem = edges->start; edge_elem; edge_elem = edge_elem->next) {
                        auto *edge_obj = json_object(edge_elem->value);
                        if (!edge_obj) {
                            continue;
                        }

                        auto *id_elem = find_element(edge_obj, "id");
                        auto *source_elem = find_element(edge_obj, "source_node_id");
                        auto *target_elem = find_element(edge_obj, "target_node_id");
                        auto *type_elem = find_element(edge_obj, "type");
                        auto *weight_elem = find_element(edge_obj, "weight");
                        if (!id_elem || !source_elem || !target_elem || !type_elem) {
                            continue;
                        }

                        auto *id_str = json_string(id_elem->value);
                        auto *source_str = json_string(source_elem->value);
                        auto *target_str = json_string(target_elem->value);
                        auto *type_str = json_string(type_elem->value);
                        if (!id_str || !source_str || !target_str || !type_str) {
                            continue;
                        }

                        const std::string source_id(source_str->string, source_str->string_size);
                        const std::string target_id(target_str->string, target_str->string_size);
                        if (!node_map.contains(source_id) || !node_map.contains(target_id)) {
                            continue;
                        }

                        EdgeData edge;
                        edge.id = UUID(std::string(id_str->string, id_str->string_size));
                        if (auto *zone_ids_elem = find_element(edge_obj, "zone_ids")) {
                            edge.zone_ids = parse_zone_ids(zone_ids_elem->value);
                        }
                        if (auto *props_elem = find_element(edge_obj, "properties")) {
                            edge.properties = parse_properties(props_elem->value);
                        }

                        auto weight = parse_number(weight_elem ? weight_elem->value : nullptr, 1.0);
                        const std::string edge_type(type_str->string, type_str->string_size);
                        graph.add_edge(node_map[source_id], node_map[target_id], weight,
                                       edge_type == "directed" ? graphix::vertex::EdgeType::Directed
                                                               : graphix::vertex::EdgeType::Undirected,
                                       edge);
                    }
                }
            }

            return graph;
        }

        inline void clear_all_zone_node_ids() {
            root_zone_.visit([](Zone &zone, [[maybe_unused]] size_t depth) { zone.clear_node_ids(); }, 0);
        }

        inline void refresh_zone_node_membership_impl() {
            clear_all_zone_node_ids();
            for (auto vertex_id : graph_.vertices()) {
                for (const auto &zone_id : graph_[vertex_id].zone_ids) {
                    if (auto *zone = find_zone(zone_id)) {
                        auto &node_ids = zone->node_ids();
                        if (std::find(node_ids.begin(), node_ids.end(), graph_[vertex_id].id) == node_ids.end()) {
                            node_ids.push_back(graph_[vertex_id].id);
                        }
                    }
                }
            }
        }

      public:
        Workspace(const Zone &root_zone) : root_zone_(root_zone), graph_() {}
        Workspace(Zone &&root_zone) : root_zone_(std::move(root_zone)), graph_() {}
        Workspace(const Zone &root_zone, const Graph &graph) : root_zone_(root_zone), graph_(graph) {}
        Workspace(Zone &&root_zone, Graph &&graph) : root_zone_(std::move(root_zone)), graph_(std::move(graph)) {}

        inline Zone &root_zone() { return root_zone_; }
        inline const Zone &root_zone() const { return root_zone_; }

        inline Graph &graph() { return graph_; }
        inline const Graph &graph() const { return graph_; }
        inline const dp::Geo &ref() const { return ref_; }
        inline bool has_ref() const { return ref_set_; }
        inline void set_ref(const dp::Geo &ref) {
            ref_ = ref;
            ref_set_ = true;
        }
        inline void clear_ref() { ref_set_ = false; }
        inline CoordMode coord_mode() const { return coord_mode_; }
        inline void set_coord_mode(CoordMode mode) { coord_mode_ = mode; }

        inline Zone *find_zone(const UUID &zone_id) { return root_zone_.find(zone_id); }
        inline const Zone *find_zone(const UUID &zone_id) const { return root_zone_.find(zone_id); }

        inline std::vector<const Zone *> zones_containing(const dp::Point &point) const {
            std::vector<const Zone *> zones;
            root_zone_.visit(
                [&](const Zone &zone, [[maybe_unused]] size_t depth) {
                    if (zone.contains(point)) {
                        zones.push_back(&zone);
                    }
                },
                0);
            return zones;
        }

        inline dp::Optional<Graph::VertexId> find_node(const UUID &node_id) const {
            for (auto vertex_id : graph_.vertices()) {
                if (graph_[vertex_id].id == node_id) {
                    return vertex_id;
                }
            }
            return dp::nullopt;
        }

        inline dp::Optional<graphix::vertex::EdgeId> find_edge(const UUID &edge_id) const {
            for (const auto &edge : graph_.edges()) {
                if (graph_.edge_property(edge.id).id == edge_id) {
                    return edge.id;
                }
            }
            return dp::nullopt;
        }

        inline Graph::VertexId add_node(const dp::Point &position,
                                        const std::unordered_map<std::string, std::string> &properties = {}) {
            NodeData node(position, properties);
            node.zone_ids = zone_membership_for_point(position);
            auto vertex_id = graph_.add_vertex(node);
            refresh_zone_node_membership();
            return vertex_id;
        }

        inline Graph::VertexId add_node(const NodeData &node) {
            auto node_copy = node;
            node_copy.zone_ids = zone_membership_for_point(node.position);
            auto vertex_id = graph_.add_vertex(node_copy);
            refresh_zone_node_membership();
            return vertex_id;
        }

        inline graphix::vertex::EdgeId add_edge(Graph::VertexId source, Graph::VertexId target, double weight = 1.0,
                                                graphix::vertex::EdgeType type = graphix::vertex::EdgeType::Undirected,
                                                const std::unordered_map<std::string, std::string> &properties = {}) {
            EdgeData edge(properties);
            auto edge_id = graph_.add_edge(source, target, weight, type, edge);
            refresh_edge_zone_membership(edge_id);
            return edge_id;
        }

        inline graphix::vertex::EdgeId
        add_edge(Graph::VertexId source, Graph::VertexId target, const EdgeData &edge, double weight = 1.0,
                 graphix::vertex::EdgeType type = graphix::vertex::EdgeType::Undirected) {
            auto edge_id = graph_.add_edge(source, target, weight, type, edge);
            refresh_edge_zone_membership(edge_id);
            return edge_id;
        }

        inline void refresh_zone_node_membership() { refresh_zone_node_membership_impl(); }

        inline void refresh_node_zone_membership(Graph::VertexId vertex_id) {
            graph_[vertex_id].zone_ids = zone_membership_for_point(graph_[vertex_id].position);
            refresh_zone_node_membership_impl();
        }

        inline void refresh_edge_zone_membership(graphix::vertex::EdgeId edge_id) {
            const auto source_id = graph_.source(edge_id);
            const auto target_id = graph_.target(edge_id);

            refresh_node_zone_membership(source_id);
            refresh_node_zone_membership(target_id);

            std::unordered_set<std::string> seen;
            std::vector<UUID> zone_ids;

            for (const auto &zone_id : graph_[source_id].zone_ids) {
                if (seen.insert(zone_id.toString()).second) {
                    zone_ids.push_back(zone_id);
                }
            }

            for (const auto &zone_id : graph_[target_id].zone_ids) {
                if (seen.insert(zone_id.toString()).second) {
                    zone_ids.push_back(zone_id);
                }
            }

            graph_.edge_property(edge_id).zone_ids = std::move(zone_ids);
        }

        inline void refresh_graph_zone_membership() {
            for (auto vertex_id : graph_.vertices()) {
                refresh_node_zone_membership(vertex_id);
            }

            for (const auto &edge : graph_.edges()) {
                refresh_edge_zone_membership(edge.id);
            }
            refresh_zone_node_membership_impl();
        }

        inline void save(const std::filesystem::path &directory) const {
            std::filesystem::create_directories(directory);
            std::filesystem::create_directories(directory / "zones");
            std::filesystem::create_directories(directory / "graph");

            std::ofstream out(directory / "workspace.json");
            if (!out.is_open()) {
                throw std::runtime_error("Failed to open workspace manifest for writing: " +
                                         (directory / "workspace.json").string());
            }

            out << "{";
            out << "\"format_version\":2,";
            write_uuid_json(out, "root_zone_id", root_zone_.id());
            out << "\"coord_mode\":\"" << coord_mode_string(coord_mode_) << "\"";
            if (ref_set_) {
                out << ",\"ref\":{\"lat\":" << ref_.latitude << ",\"lon\":" << ref_.longitude << "}";
            }
            out << "}";

            save_zone_recursive(root_zone_, directory / "zones");
            save_graph(directory / "graph" / "graph.json");
        }

        inline static Workspace load(const std::filesystem::path &directory) {
            const auto workspace_manifest = directory / "workspace.json";
            const auto zones_dir = directory / "zones";
            const auto graph_file = directory / "graph" / "graph.json";

            if (std::filesystem::exists(workspace_manifest) && std::filesystem::exists(zones_dir)) {
                const auto manifest = load_workspace_manifest(workspace_manifest);
                auto root_zone = load_zone_recursive(zones_dir, manifest.root_zone_id);
                auto graph = load_graph(graph_file);
                Workspace workspace(std::move(root_zone), std::move(graph));
                workspace.coord_mode_ = manifest.coord_mode;
                if (manifest.ref_set) {
                    workspace.set_ref(manifest.ref);
                }
                workspace.refresh_zone_node_membership();
                return workspace;
            }

            auto root_zone = Zone::load(directory / "root_zone");
            auto graph = load_graph(directory / "graph.json");
            Workspace workspace(std::move(root_zone), std::move(graph));
            workspace.refresh_zone_node_membership();
            return workspace;
        }
    };

} // namespace zoneout

#include "wire.hpp"
