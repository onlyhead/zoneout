#pragma once

#include "json.hpp"
#include <concord/concord.hpp>
#include <datapod/datapod.hpp>

#include <arpa/inet.h>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace dp = ::datapod;

namespace geoget {

    namespace detail {
        struct JsonDeleter {
            void operator()(json_value_s *ptr) const {
                if (ptr)
                    free(ptr);
            }
        };
        using JsonPtr = std::unique_ptr<json_value_s, JsonDeleter>;

        inline json_object_s *get_object(json_value_s *val) {
            if (!val || val->type != json_type_object)
                return nullptr;
            return static_cast<json_object_s *>(val->payload);
        }

        inline json_object_element_s *find_element(json_object_s *obj, const char *key) {
            if (!obj)
                return nullptr;
            for (auto *elem = obj->start; elem; elem = elem->next) {
                if (elem->name && strcmp(elem->name->string, key) == 0) {
                    return elem;
                }
            }
            return nullptr;
        }

        inline double get_number(json_value_s *val) {
            if (!val || val->type != json_type_number)
                return 0.0;
            auto *num = static_cast<json_number_s *>(val->payload);
            return std::stod(std::string(num->number, num->number_size));
        }

        inline std::string json_response(bool success, size_t point_count = 0, size_t polygon_count = 0) {
            std::ostringstream oss;
            oss << "{\"success\":" << (success ? "true" : "false");
            if (point_count > 0 || polygon_count > 0) {
                oss << ",\"pointCount\":" << point_count << ",\"polygonCount\":" << polygon_count;
            }
            oss << "}";
            return oss.str();
        }
    } // namespace detail

    struct Point {
        double lat;
        double lon;
    };

    class PolygonDrawer {
      private:
        int server_fd;
        std::vector<Point> points;
        std::vector<std::vector<Point>> all_polygons;
        std::vector<Point> all_single_points;
        bool is_done;
        std::mutex done_mutex;
        std::condition_variable done_cv;
        bool single_point_mode;
        dp::Geo datum;
        bool select_point;

        std::string get_html() {
            if (single_point_mode) {
                return get_single_point_html();
            } else {
                return get_polygon_html();
            }
        }

        std::string get_polygon_html() {
            std::string html = "<!DOCTYPE html><html><head><title>Draw Polygon</title>"
                               "<link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.css\"/>"
                               "<style>body{margin:0;font-family:Arial}#map{height:100vh;width:100vw}"
                               ".controls{position:absolute;top:10px;right:10px;background:white;padding:10px;border-"
                               "radius:5px;z-index:1000}"
                               "button{background:#4CAF50;color:white;border:none;padding:8px "
                               "16px;border-radius:3px;cursor:pointer;margin:2px}"
                               ".clear{background:#f44336}"
                               "#newBtn{background:#2196F3}</style></head><body>"
                               "<div id=\"map\"></div>"
                               "<div class=\"controls\">"
                               "<button onclick=\"clearPoly()\" class=\"clear\">Clear</button>"
                               "<button onclick=\"newPoly()\" id=\"newBtn\" disabled>New</button>"
                               "<button onclick=\"done()\">Done</button>"
                               "</div>"
                               "<script src=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.js\"></script>"
                               "<script>"
                               "var map=L.map('map').setView([52.1326,5.2913],10);"
                               "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png').addTo(map);"
                               "var points=[],markers=[],poly=null,allPolys=[],allMarkers=[];"
                               "var "
                               "colors=['#FF0000','#00FF00','#0000FF','#FFFF00','#FF00FF','#00FFFF','#FFA500','#800080'"
                               ",'#008000','#FFC0CB','#A52A2A','#808080','#000080','#FFD700','#DC143C','#32CD32','#"
                               "4169E1','#FF1493','#20B2AA','#8B4513'];"
                               "if(navigator.geolocation){"
                               "navigator.geolocation.getCurrentPosition(function(p){"
                               "map.setView([p.coords.latitude,p.coords.longitude],15);"
                               "});}"
                               "map.on('click',function(e){"
                               "var lat=e.latlng.lat,lon=e.latlng.lng;"
                               "points.push([lat,lon]);"
                               "var m=L.marker([lat,lon]).addTo(map);"
                               "m.on('contextmenu',function(me){removePoint(me.target);});"
                               "markers.push(m);"
                               "if(points.length>=3){"
                               "if(poly)map.removeLayer(poly);"
                               "var currentColor=colors[allPolys.length%colors.length];"
                               "poly=L.polygon(points,{color:currentColor,fillOpacity:0.2}).addTo(map);"
                               "document.getElementById('newBtn').disabled=false;"
                               "}"
                               "fetch('/api/addpoint',{method:'POST',headers:{'Content-Type':'application/json'},"
                               "body:JSON.stringify({lat:lat,lon:lon})});"
                               "});"
                               "function removePoint(marker){"
                               "var idx=markers.indexOf(marker);"
                               "if(idx>=0){"
                               "markers.splice(idx,1);"
                               "points.splice(idx,1);"
                               "map.removeLayer(marker);"
                               "if(poly){map.removeLayer(poly);poly=null;}"
                               "if(points.length>=3){"
                               "var currentColor=colors[allPolys.length%colors.length];"
                               "poly=L.polygon(points,{color:currentColor,fillOpacity:0.2}).addTo(map);"
                               "}"
                               "fetch('/api/clearcurrent',{method:'POST'}).then(function(){"
                               "points.forEach(function(p){"
                               "fetch('/api/addpoint',{method:'POST',headers:{'Content-Type':'application/json'},"
                               "body:JSON.stringify({lat:p[0],lon:p[1]})});"
                               "});"
                               "});"
                               "}}"
                               "function newPoly(){"
                               "if(points.length>=3){"
                               "allPolys.push(poly);"
                               "allMarkers.push(markers.slice());"
                               "fetch('/api/newpolygon',{method:'POST'});"
                               "points=[];"
                               "markers=[];"
                               "poly=null;"
                               "document.getElementById('newBtn').disabled=true;"
                               "}}"
                               "function clearPoly(){"
                               "points=[];"
                               "markers.forEach(function(m){map.removeLayer(m);});"
                               "markers=[];"
                               "if(poly){map.removeLayer(poly);poly=null;}"
                               "allPolys.forEach(function(p){if(p)map.removeLayer(p);});"
                               "allMarkers.forEach(function(ms){ms.forEach(function(m){map.removeLayer(m);});});"
                               "allPolys=[];allMarkers=[];"
                               "document.getElementById('newBtn').disabled=true;"
                               "fetch('/api/clear',{method:'POST'});"
                               "}"
                               "function done(){"
                               "if(points.length<3){alert('Need at least 3 points');return;}"
                               "fetch('/api/done',{method:'POST'})"
                               ".then(response=>response.json())"
                               ".then(data=>alert('Polygon complete! '+data.pointCount+' points saved'));"
                               "}"
                               "</script></body></html>";

            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/html\r\n"
                   "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                   "Pragma: no-cache\r\n"
                   "Expires: 0\r\n"
                   "Content-Length: " +
                   std::to_string(html.length()) +
                   "\r\n"
                   "\r\n" +
                   html;
        }

        std::string get_single_point_html() {
            std::string html = "<!DOCTYPE html><html><head><title>Select Point</title>"
                               "<link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.css\"/>"
                               "<style>body{margin:0;font-family:Arial}#map{height:100vh;width:100vw}"
                               ".controls{position:absolute;top:10px;right:10px;background:white;padding:10px;border-"
                               "radius:5px;z-index:1000}"
                               "button{background:#4CAF50;color:white;border:none;padding:8px "
                               "16px;border-radius:3px;cursor:pointer;margin:2px}"
                               ".clear{background:#f44336}"
                               "#newBtn{background:#2196F3}</style></head><body>"
                               "<div id=\"map\"></div>"
                               "<div class=\"controls\">"
                               "<button onclick=\"clearPoint()\" class=\"clear\">Clear</button>"
                               "<button onclick=\"newPoint()\" id=\"newBtn\" disabled>New</button>"
                               "<button onclick=\"done()\">Done</button>"
                               "</div>"
                               "<script src=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.js\"></script>"
                               "<script>"
                               "var map=L.map('map').setView([52.1326,5.2913],10);"
                               "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png').addTo(map);"
                               "var marker=null,allMarkers=[];"
                               "if(navigator.geolocation){"
                               "navigator.geolocation.getCurrentPosition(function(p){"
                               "map.setView([p.coords.latitude,p.coords.longitude],15);"
                               "});}"
                               "map.on('click',function(e){"
                               "var lat=e.latlng.lat,lon=e.latlng.lng;"
                               "if(marker)map.removeLayer(marker);"
                               "marker=L.marker([lat,lon]).addTo(map);"
                               "marker.on('contextmenu',function(){removePoint();});"
                               "document.getElementById('newBtn').disabled=false;"
                               "fetch('/api/setpoint',{method:'POST',headers:{'Content-Type':'application/json'},"
                               "body:JSON.stringify({lat:lat,lon:lon})});"
                               "});"
                               "function removePoint(){"
                               "if(marker){map.removeLayer(marker);marker=null;}"
                               "document.getElementById('newBtn').disabled=true;"
                               "fetch('/api/clearcurrent',{method:'POST'});"
                               "}"
                               "function newPoint(){"
                               "if(marker){"
                               "allMarkers.push(marker);"
                               "fetch('/api/newpoint',{method:'POST'});"
                               "marker=null;"
                               "document.getElementById('newBtn').disabled=true;"
                               "}}"
                               "function clearPoint(){"
                               "if(marker){map.removeLayer(marker);marker=null;}"
                               "allMarkers.forEach(function(m){map.removeLayer(m);});"
                               "allMarkers=[];"
                               "document.getElementById('newBtn').disabled=true;"
                               "fetch('/api/clear',{method:'POST'});"
                               "}"
                               "function done(){"
                               "if(!marker){alert('Please select a point first');return;}"
                               "fetch('/api/done',{method:'POST'})"
                               ".then(response=>response.json())"
                               ".then(data=>alert('Point selected!'));"
                               "}"
                               "</script></body></html>";

            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/html\r\n"
                   "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                   "Pragma: no-cache\r\n"
                   "Expires: 0\r\n"
                   "Content-Length: " +
                   std::to_string(html.length()) +
                   "\r\n"
                   "\r\n" +
                   html;
        }

        void handle_request(int client_fd) {
            char buffer[4096] = {0};
            [[maybe_unused]] auto bytes_read = ::read(client_fd, buffer, 4096);

            std::string request(buffer);
            std::istringstream iss(request);
            std::string method, path, version;
            iss >> method >> path >> version;

            std::cout << "DEBUG: Request " << method << " " << path
                      << " (mode: " << (single_point_mode ? "single" : "polygon") << ")" << std::endl;

            std::string response;
            if (path == "/" || path == "/index.html") {
                response = get_html();
            } else if (path == "/api/addpoint" && method == "POST") {
                response = add_point(request);
            } else if (path == "/api/clear" && method == "POST") {
                response = clear_points();
            } else if (path == "/api/setpoint" && method == "POST") {
                response = set_point(request);
            } else if (path == "/api/newpolygon" && method == "POST") {
                response = new_polygon();
            } else if (path == "/api/newpoint" && method == "POST") {
                response = new_point();
            } else if (path == "/api/clearcurrent" && method == "POST") {
                response = clear_current_only();
            } else if (path == "/api/done" && method == "POST") {
                response = handle_done();
            } else {
                response = "HTTP/1.1 404 Not Found\r\n\r\nNot Found";
            }

            send(client_fd, response.c_str(), response.length(), 0);
            ::close(client_fd);
        }

        std::string add_point(const std::string &request) {
            size_t body_start = request.find("\r\n\r\n");
            if (body_start == std::string::npos) {
                return "HTTP/1.1 400 Bad Request\r\n\r\nNo body";
            }

            std::string body = request.substr(body_start + 4);
            try {
                detail::JsonPtr data(json_parse(body.c_str(), body.size()));
                if (!data) {
                    return "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON";
                }

                auto *obj = detail::get_object(data.get());
                if (!obj) {
                    return "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON object";
                }

                auto *lat_elem = detail::find_element(obj, "lat");
                auto *lon_elem = detail::find_element(obj, "lon");
                if (!lat_elem || !lon_elem) {
                    return "HTTP/1.1 400 Bad Request\r\n\r\nMissing lat/lon";
                }

                double lat = detail::get_number(lat_elem->value);
                double lon = detail::get_number(lon_elem->value);

                points.push_back({lat, lon});

                std::cout << "Point " << points.size() << ": " << lat << ", " << lon << std::endl;

                return "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json\r\n"
                       "\r\n{\"success\":true}";

            } catch (...) {
                return "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON";
            }
        }

        std::string set_point(const std::string &request) {
            size_t body_start = request.find("\r\n\r\n");
            if (body_start == std::string::npos) {
                return "HTTP/1.1 400 Bad Request\r\n\r\nNo body";
            }

            std::string body = request.substr(body_start + 4);
            try {
                detail::JsonPtr data(json_parse(body.c_str(), body.size()));
                if (!data) {
                    return "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON";
                }

                auto *obj = detail::get_object(data.get());
                if (!obj) {
                    return "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON object";
                }

                auto *lat_elem = detail::find_element(obj, "lat");
                auto *lon_elem = detail::find_element(obj, "lon");
                if (!lat_elem || !lon_elem) {
                    return "HTTP/1.1 400 Bad Request\r\n\r\nMissing lat/lon";
                }

                double lat = detail::get_number(lat_elem->value);
                double lon = detail::get_number(lon_elem->value);

                points.clear();
                points.push_back({lat, lon});

                std::cout << "Point set: " << lat << ", " << lon << std::endl;

                return "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json\r\n"
                       "\r\n{\"success\":true}";

            } catch (...) {
                return "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON";
            }
        }

        std::string new_polygon() {
            if (points.size() >= 3) {
                all_polygons.push_back(points);
                std::cout << "New polygon added with " << points.size() << " points" << std::endl;
                points.clear();
            }
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "\r\n{\"success\":true}";
        }

        std::string new_point() {
            if (!points.empty()) {
                all_single_points.push_back(points[0]);
                std::cout << "New point added: " << points[0].lat << ", " << points[0].lon << std::endl;
                points.clear();
            }
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "\r\n{\"success\":true}";
        }

        std::string clear_current_only() {
            points.clear();
            std::cout << (single_point_mode ? "Current point cleared" : "Current polygon cleared") << std::endl;
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "\r\n{\"success\":true}";
        }

        std::string clear_points() {
            points.clear();
            all_polygons.clear();
            all_single_points.clear();
            std::cout << (single_point_mode ? "All points cleared" : "All polygons cleared") << std::endl;
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "\r\n{\"success\":true}";
        }

        std::string handle_done() {
            if (single_point_mode) {
                if (!points.empty()) {
                    all_single_points.push_back(points[0]);
                }

                std::cout << "\n=== ALL POINTS SELECTED ===" << std::endl;
                std::cout << "Total points: " << all_single_points.size() << std::endl;
                for (size_t i = 0; i < all_single_points.size(); ++i) {
                    std::cout << "Point " << (i + 1) << ": " << all_single_points[i].lat << ", "
                              << all_single_points[i].lon << std::endl;
                }
                std::cout << "========================\n" << std::endl;
            } else {
                if (points.size() >= 3) {
                    all_polygons.push_back(points);
                }

                std::cout << "\n=== ALL POLYGONS COMPLETE ===" << std::endl;
                std::cout << "Total polygons: " << all_polygons.size() << std::endl;
                for (size_t p = 0; p < all_polygons.size(); ++p) {
                    std::cout << "Polygon " << (p + 1) << " (" << all_polygons[p].size() << " points):" << std::endl;
                    for (size_t i = 0; i < all_polygons[p].size(); ++i) {
                        std::cout << "  Point " << (i + 1) << ": " << all_polygons[p][i].lat << ", "
                                  << all_polygons[p][i].lon << std::endl;
                    }
                }
                std::cout << "===========================\n" << std::endl;
            }

            {
                std::lock_guard<std::mutex> lock(done_mutex);
                is_done = true;
            }
            done_cv.notify_one();

            std::string response_json;
            if (single_point_mode) {
                response_json = detail::json_response(true, all_single_points.size(), 0);
            } else {
                response_json = detail::json_response(true, 0, all_polygons.size());
            }

            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "\r\n" +
                   response_json;
        }

        std::vector<Point> collect_points() {
            single_point_mode = false;

            {
                std::lock_guard<std::mutex> lock(done_mutex);
                is_done = false;
            }
            points.clear();

            std::thread server_thread([this]() {
                while (!is_done) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (client_fd >= 0) {
                        std::thread([this, client_fd]() { handle_request(client_fd); }).detach();
                    }
                }
            });

            std::unique_lock<std::mutex> lock(done_mutex);
            done_cv.wait(lock, [this] { return is_done; });

            server_thread.detach();
            stop();

            return points;
        }

        Point collect_single_point() {
            single_point_mode = true;

            {
                std::lock_guard<std::mutex> lock(done_mutex);
                is_done = false;
            }
            points.clear();

            std::thread server_thread([this]() {
                while (!is_done) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (client_fd >= 0) {
                        std::thread([this, client_fd]() { handle_request(client_fd); }).detach();
                    }
                }
            });

            std::unique_lock<std::mutex> lock(done_mutex);
            done_cv.wait(lock, [this] { return is_done; });

            server_thread.detach();
            stop();

            return points.empty() ? Point{0, 0} : points[0];
        }

        void set_datum_from_point(const Point &point) {
            datum.latitude = point.lat;
            datum.longitude = point.lon;
            datum.altitude = 0.0;
        }

      public:
        PolygonDrawer(bool select_point = false)
            : server_fd(-1), is_done(false), single_point_mode(false), select_point(select_point) {}

        PolygonDrawer(const dp::Geo &d)
            : server_fd(-1), is_done(false), single_point_mode(false), datum(d), select_point(false) {}

        ~PolygonDrawer() { stop(); }

        bool start(int port = 8080) {
            if (server_fd != -1) {
                ::close(server_fd);
                server_fd = -1;
            }

            {
                std::lock_guard<std::mutex> lock(done_mutex);
                is_done = false;
            }
            points.clear();

            server_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (server_fd == -1) {
                return false;
            }

            int opt = 1;
            setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

            struct sockaddr_in address;
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port);

            if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
                return false;
            }

            if (listen(server_fd, 3) < 0) {
                return false;
            }

            std::cout << "Polygon Drawer on http://localhost:" << port << std::endl;
            return true;
        }

        void stop() {
            if (server_fd != -1) {
                shutdown(server_fd, SHUT_RDWR);
                ::close(server_fd);
                server_fd = -1;
            }
        }

        dp::Geo add_datum() {
            single_point_mode = true;

            {
                std::lock_guard<std::mutex> lock(done_mutex);
                is_done = false;
            }
            points.clear();

            std::cout << "Select datum point on the map..." << std::endl;

            std::thread server_thread([this]() {
                while (!is_done) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (client_fd >= 0) {
                        std::thread([this, client_fd]() { handle_request(client_fd); }).detach();
                    }
                }
            });

            std::unique_lock<std::mutex> lock(done_mutex);
            done_cv.wait(lock, [this] { return is_done; });

            server_thread.detach();
            stop();

            if (!points.empty()) {
                datum.latitude = points[0].lat;
                datum.longitude = points[0].lon;
                datum.altitude = 0.0;
                std::cout << "Datum set to: " << datum.latitude << ", " << datum.longitude << std::endl;
            }

            return datum;
        }

        dp::Geo get_datum() { return datum; }

        const std::vector<std::vector<Point>> &get_all_polygons() {
            collect_points();
            return all_polygons;
        }

        const std::vector<Point> &get_all_points() {
            collect_single_point();
            return all_single_points;
        }

        std::vector<dp::Polygon> get_polygons() {
            collect_points();
            if (!datum.is_set()) {
                if (select_point) {
                    throw std::runtime_error("Datum not set. Call add_datum() first or use constructor with datum.");
                } else {
                    set_datum_from_point(all_single_points[0]);
                }
            }
            std::vector<dp::Polygon> polygons;
            for (const auto &polygon : all_polygons) {
                dp::Polygon dp_polygon;
                for (const auto &point : polygon) {
                    concord::earth::WGS wgs{point.lat, point.lon, 0.0};
                    auto enu = concord::frame::to_enu(datum, wgs);
                    double x = enu.east(), y = enu.north(), z = enu.up();
                    dp_polygon.vertices.push_back(dp::Point{x, y, z});
                }
                if (polygon.size() >= 3) {
                    concord::earth::WGS wgs{polygon[0].lat, polygon[0].lon, 0.0};
                    auto enu = concord::frame::to_enu(datum, wgs);
                    double x = enu.east(), y = enu.north(), z = enu.up();
                    dp_polygon.vertices.push_back(dp::Point{x, y, z});
                }
                polygons.push_back(dp_polygon);
            }
            return polygons;
        }

        std::vector<dp::Point> get_points() {
            collect_single_point();
            if (!datum.is_set()) {
                if (select_point) {
                    throw std::runtime_error("Datum not set. Call add_datum() first or use constructor with datum.");
                } else {
                    set_datum_from_point(all_single_points[0]);
                }
            }
            std::vector<dp::Point> dp_points;
            for (const auto &point : all_single_points) {
                concord::earth::WGS wgs{point.lat, point.lon, 0.0};
                auto enu = concord::frame::to_enu(datum, wgs);
                double x = enu.east(), y = enu.north(), z = enu.up();
                dp_points.emplace_back(x, y, z);
            }
            return dp_points;
        }
    };

} // namespace geoget
