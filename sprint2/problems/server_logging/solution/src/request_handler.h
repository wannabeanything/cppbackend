#pragma once

#include "model.h"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/attributes.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <optional>
#include <unordered_map>
#include <chrono>
#include <algorithm>

namespace http_handler {

namespace http = boost::beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace logging = boost::log;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

inline void InitLogging() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    logging::add_common_attributes();
    logging::add_console_log(std::cout, boost::log::keywords::auto_flush = true, logging::keywords::format = [](const logging::record_view& rec, logging::formatting_ostream& strm) {
        using boost::posix_time::to_iso_extended_string;
        json::object obj;
        if (auto ts = logging::extract<boost::posix_time::ptime>("TimeStamp", rec)) {
            obj["timestamp"] = to_iso_extended_string(ts.get());
        }
        if (auto data = rec[additional_data]) {
            obj["data"] = data.get();
        } else {
            obj["data"] = json::object{};
        }
        if (auto msg = rec[logging::expressions::smessage]) {
            obj["message"] = msg.get();
        }
        
        strm << json::serialize(obj);
    });
}

class RequestHandler {
public:
    RequestHandler(model::Game& game, fs::path static_root)
        : game_{game}, static_root_{std::move(static_root)} {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using namespace std::literals;
        
        const auto bad_request = [&req](std::string_view text) {
            http::response<http::string_body> res{http::status::bad_request, req.version()};
            res.set(http::field::content_type, "application/json");
            json::object obj;
            obj["code"] = "badRequest";
            obj["message"] = text;
            res.body() = json::serialize(obj);
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            return res;
        };

        const auto not_found_json = [&req](std::string_view text) {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::content_type, "application/json");
            json::object obj;
            obj["code"] = "mapNotFound";
            obj["message"] = text;
            res.body() = json::serialize(obj);
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            return res;
        };

        const auto not_found_file = [&req]() {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::content_type, "text/plain");
            res.body() = "File not found";
            res.prepare_payload();
            return res;
        };

        const std::string target = std::string(req.target());

        if (target.starts_with("/api/")) {
            if (req.method() != http::verb::get) {
                return send(bad_request("Only GET method supported"));
            }

            if (target == "/api/v1/maps") {
                json::array arr;
                for (const auto& map : game_.GetMaps()) {
                    arr.emplace_back(json::object{
                        {"id", *map.GetId()},
                        {"name", map.GetName()}});
                }
                http::response<http::string_body> res{http::status::ok, req.version()};
                res.set(http::field::content_type, "application/json");
                res.body() = json::serialize(arr);
                res.content_length(res.body().size());
                res.keep_alive(req.keep_alive());
                return send(std::move(res));
            }

            if (target.starts_with("/api/v1/maps/")) {
                std::string map_id = target.substr(std::string("/api/v1/maps/").size());
                if (auto map = game_.FindMap(model::Map::Id{map_id})) {
                    json::object map_obj;
                    map_obj["id"] = *map->GetId();
                    map_obj["name"] = map->GetName();

                    json::array roads_arr;
                    for (const auto& road : map->GetRoads()) {
                        if (road.IsHorizontal()) {
                            roads_arr.emplace_back(json::object{
                                {"x0", road.GetStart().x},
                                {"y0", road.GetStart().y},
                                {"x1", road.GetEnd().x}});
                        } else {
                            roads_arr.emplace_back(json::object{
                                {"x0", road.GetStart().x},
                                {"y0", road.GetStart().y},
                                {"y1", road.GetEnd().y}});
                        }
                    }
                    map_obj["roads"] = std::move(roads_arr);

                    json::array buildings_arr;
                    for (const auto& building : map->GetBuildings()) {
                        buildings_arr.emplace_back(json::object{
                            {"x", building.GetBounds().position.x},
                            {"y", building.GetBounds().position.y},
                            {"w", building.GetBounds().size.width},
                            {"h", building.GetBounds().size.height}});
                    }
                    map_obj["buildings"] = std::move(buildings_arr);

                    json::array offices_arr;
                    for (const auto& office : map->GetOffices()) {
                        offices_arr.emplace_back(json::object{
                            {"id", *office.GetId()},
                            {"x", office.GetPosition().x},
                            {"y", office.GetPosition().y},
                            {"offsetX", office.GetOffset().dx},
                            {"offsetY", office.GetOffset().dy}});
                    }
                    map_obj["offices"] = std::move(offices_arr);

                    http::response<http::string_body> res{http::status::ok, req.version()};
                    res.set(http::field::content_type, "application/json");
                    res.body() = json::serialize(map_obj);
                    res.content_length(res.body().size());
                    res.keep_alive(req.keep_alive());
                    return send(std::move(res));
                } else {
                    return send(not_found_json("Map not found"));
                }
            }

            return send(bad_request("Bad request"));
        }

        // Статика
        std::string path = UrlDecode(req.target());
        fs::path full_path = static_root_ / path.substr(1);

        if (fs::is_directory(full_path)) {
            full_path /= "index.html";
        }

        if (!IsSubPath(static_root_, full_path)) {
            return send(bad_request("Invalid path"));
        }

        if (!fs::exists(full_path) || fs::is_directory(full_path)) {
            return send(not_found_file());
        }

        std::ifstream file(full_path, std::ios::binary);
        std::ostringstream ss;
        ss << file.rdbuf();
        std::string body = ss.str();

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, GetMimeType(full_path));
        res.content_length(body.size());
        if (req.method() == http::verb::get) {
            res.body() = std::move(body);
        }
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

private:
    model::Game& game_;
    fs::path static_root_;

    std::string UrlDecode(std::string_view str) const {
        std::ostringstream result;
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '%' && i + 2 < str.size()) {
                std::string hex{str[i + 1], str[i + 2]};
                result << static_cast<char>(std::stoi(hex, nullptr, 16));
                i += 2;
            } else if (str[i] == '+') {
                result << ' ';
            } else {
                result << str[i];
            }
        }
        return result.str();
    }

    std::string GetMimeType(const fs::path& path) const {
        static const std::unordered_map<std::string, std::string> types = {
            {".htm", "text/html"}, {".html", "text/html"}, {".css", "text/css"},
            {".txt", "text/plain"}, {".js", "text/javascript"}, {".json", "application/json"},
            {".xml", "application/xml"}, {".png", "image/png"}, {".jpg", "image/jpeg"},
            {".jpe", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".gif", "image/gif"},
            {".bmp", "image/bmp"}, {".ico", "image/vnd.microsoft.icon"}, {".tiff", "image/tiff"},
            {".tif", "image/tiff"}, {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"},
            {".mp3", "audio/mpeg"}
        };
        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (auto it = types.find(ext); it != types.end()) {
            return it->second;
        }
        return "application/octet-stream";
    }

    bool IsSubPath(const fs::path& base, const fs::path& path) const {
        auto base_abs = fs::weakly_canonical(base);
        auto path_abs = fs::weakly_canonical(path);
        return std::mismatch(base_abs.begin(), base_abs.end(), path_abs.begin()).first == base_abs.end();
    }
};

class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(RequestHandler& decorated)
        : decorated_{decorated} {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send,
                    const std::string& ip) {
        auto start = std::chrono::steady_clock::now();

        LogRequest(ip, std::string(req.target()), std::string(req.method_string()));

        auto wrapped_send = [this, start, &send, &ip](auto&& response) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - start).count();
            std::optional<std::string> content_type;
            if (response.find(http::field::content_type) != response.end()) {
                content_type = std::string(response[http::field::content_type]);
            }
            LogResponse(ip, response.result_int(), content_type, ms);
            send(std::forward<decltype(response)>(response));
        };

        decorated_(std::move(req), std::move(wrapped_send));
    }

private:
    RequestHandler& decorated_;

    void LogRequest(const std::string& ip, const std::string& uri, const std::string& method) {
        json::value req_data{{"ip", ip}, {"URI", uri}, {"method", method}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, req_data) << "request received";
    }

    void LogResponse(const std::string& ip, int code, const std::optional<std::string>& content_type, long response_time_ms) {
        json::object resp_data;
        resp_data["ip"] = ip;
        resp_data["response_time"] = static_cast<int>(response_time_ms);
        resp_data["code"] = code;
        if (content_type) {
            resp_data["content_type"] = *content_type;
        } else {
            resp_data["content_type"] = nullptr;
        }
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, resp_data) << "response sent";
    }
};

}  // namespace http_handler
