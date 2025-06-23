#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>

namespace http_handler
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace json = boost::json;

    class RequestHandler
    {
    public:
        explicit RequestHandler(model::Game &game)
            : game_{game}
        {
        }

        RequestHandler(const RequestHandler &) = delete;
        RequestHandler &operator=(const RequestHandler &) = delete;

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>> &&req, Send &&send)
        {
            const auto bad_request = [&req](std::string_view text)
            {
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

            const auto not_found = [&req](std::string_view text)
            {
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

            if (req.method() != http::verb::get)
            {
                return send(bad_request("Only GET method supported"));
            }

            const std::string target = std::string(req.target());
            if (target == "/api/v1/maps")
            {
                json::array arr;
                for (const auto &map : game_.GetMaps())
                {
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

            if (target.starts_with("/api/v1/maps/"))
            {
                std::string map_id = target.substr(std::string("/api/v1/maps/").size());
                if (auto map = game_.FindMap(model::Map::Id{map_id}))
                {
                    json::object map_obj;
                    map_obj["id"] = *map->GetId();
                    map_obj["name"] = map->GetName();
                    map_obj["roads"] = SerializeRoads(map->GetRoads());
                    map_obj["buildings"] = SerializeBuildings(map->GetBuildings());
                    map_obj["offices"] = SerializeOffices(map->GetOffices());

                    http::response<http::string_body> res{http::status::ok, req.version()};
                    res.set(http::field::content_type, "application/json");
                    res.body() = json::serialize(map_obj);
                    res.content_length(res.body().size());
                    res.keep_alive(req.keep_alive());
                    return send(std::move(res));
                }
                else
                {
                    return send(not_found("Map not found"));
                }
            }

            return send(bad_request("Bad request"));
        }

    private:
        model::Game &game_;

        json::array SerializeRoads(const std::vector<model::Road> &roads)
        {
            json::array result;
            for (const auto &road : roads)
            {
                if (road.IsHorizontal())
                {
                    result.emplace_back(json::object{
                        {"x0", road.GetStart().x},
                        {"y0", road.GetStart().y},
                        {"x1", road.GetEnd().x}});
                }
                else
                {
                    result.emplace_back(json::object{
                        {"x0", road.GetStart().x},
                        {"y0", road.GetStart().y},
                        {"y1", road.GetEnd().y}});
                }
            }
            return result;
        }

        json::array SerializeBuildings(const std::vector<model::Building> &buildings)
        {
            json::array result;
            for (const auto &building : buildings)
            {
                const auto &bounds = building.GetBounds();
                result.emplace_back(json::object{
                    {"x", bounds.position.x},
                    {"y", bounds.position.y},
                    {"w", bounds.size.width},
                    {"h", bounds.size.height}});
            }
            return result;
        }

        json::array SerializeOffices(const std::vector<model::Office> &offices)
        {
            json::array result;
            for (const auto &office : offices)
            {
                result.emplace_back(json::object{
                    {"id", *office.GetId()},
                    {"x", office.GetPosition().x},
                    {"y", office.GetPosition().y},
                    {"offsetX", office.GetOffset().dx},
                    {"offsetY", office.GetOffset().dy}});
            }
            return result;
        }
    };

} // namespace http_handler