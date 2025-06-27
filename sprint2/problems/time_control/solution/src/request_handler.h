#pragma once

#include "model.h"
#include "objects.h"
#include <boost/beast/http.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/attributes.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json/serializer.hpp>

#include <iomanip>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <optional>
#include <unordered_map>
#include <chrono>
#include <algorithm>

namespace http_handler
{

    namespace http = boost::beast::http;
    namespace json = boost::json;
    namespace fs = std::filesystem;
    namespace logging = boost::log;

    BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

    inline void InitLogging()
    {
        static bool initialized = false;
        if (initialized)
            return;
        initialized = true;
        logging::add_common_attributes();
        logging::add_console_log(std::cout, boost::log::keywords::auto_flush = true, logging::keywords::format = [](const logging::record_view &rec, logging::formatting_ostream &strm)
                                                                                     {
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
        
        strm << json::serialize(obj); });
    }
    class ApiRequestHandler
    {
    public:
        ApiRequestHandler(model::Game &game, boost::asio::io_context &ioc)
            : game_(game), strand_(boost::asio::make_strand(ioc)) {}

        template <typename Body, typename Allocator, typename Send>
        void HandleRequest(const http::request<Body, http::basic_fields<Allocator>> &req, Send &&send)
        {
            using namespace std::literals;
            const std::string target = std::string(req.target());

            // Безопасный: сразу вызываем send
            if (target == "/api/v1/maps")
            {
                if (req.method() != http::verb::get)
                {
                    send(MakeError(http::status::bad_request, "badRequest", "Only GET method supported", req));
                }
                else
                {
                    send(HandleMapsList(req));
                }
                return;
            }

            if (target.starts_with("/api/v1/maps/"))
            {
                if (req.method() != http::verb::get)
                {
                    send(MakeError(http::status::bad_request, "badRequest", "Only GET method supported", req));
                }
                else
                {
                    send(HandleMapById(req));
                }
                return;
            }

            // Потенциально изменяет состояние — оборачиваем в strand
            if (target.starts_with("/api/v1/game/join"))
            {
                boost::asio::dispatch(strand_, [this, req, send = std::forward<Send>(send)]() mutable
                                      { send(HandleJoinPlayer(req)); });
                return;
            }

            if (target.starts_with("/api/v1/game/players"))
            {
                boost::asio::post(strand_, [this, req, send = std::forward<Send>(send)]() mutable
                                      { send(HandlePlayersList(req)); });
                return;
            }

            if (target.starts_with("/api/v1/game/state"))
            {
                boost::asio::dispatch(strand_, [this, req, send = std::forward<Send>(send)]() mutable
                                      { send(HandleGameState(req)); });
                return;
            }

            if (target.starts_with("/api/v1/game/player/action"))
            {
                boost::asio::dispatch(strand_, [this, req, send = std::forward<Send>(send)]() mutable
                                      { send(HandleGameActions(req)); });
                return;
            }
            if (target.starts_with("/api/v1/game/tick"))
            {
                boost::asio::dispatch(strand_, [this, req, send = std::forward<Send>(send)]() mutable
                                      { send(HandleGameTick(req)); });
                return;
            }
            // Всё остальное — ошибка
            send(MakeError(http::status::bad_request, "badRequest", "Bad request", req));
        }

    private:
        model::Game &game_;
        Players players_;
        boost::asio::strand<boost::asio::io_context::executor_type> strand_;
        std::unordered_map<std::string, std::shared_ptr<GameSession>> sessions_;
        template <typename Req>
        http::response<http::string_body> HandleMapsList(const Req &req) const
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
            res.set(http::field::cache_control, "no-cache");
            res.body() = json::serialize(arr);
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            return res;
        }

        template <typename Req>
        http::response<http::string_body> HandleMapById(const Req &req) const
        {
            std::string map_id = std::string(req.target()).substr(std::string("/api/v1/maps/").size());
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
                res.set(http::field::cache_control, "no-cache");
                res.body() = json::serialize(map_obj);
                res.content_length(res.body().size());
                res.keep_alive(req.keep_alive());
                return res;
            }

            return MakeError(http::status::not_found, "mapNotFound", "Map not found", req);
        }
        json::array SerializeRoads(const std::list<model::Road> &roads) const
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

        json::array SerializeBuildings(const std::vector<model::Building> &buildings) const
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

        json::array SerializeOffices(const std::vector<model::Office> &offices) const
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
        template <typename Req>
        http::response<http::string_body> MakeError(http::status status,
                                                    std::string_view code,
                                                    std::string_view msg,
                                                    const Req &req) const
        {
            json::object obj;
            obj["code"] = code;
            obj["message"] = msg;

            http::response<http::string_body> res{status, req.version()};
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.body() = json::serialize(obj);
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            return res;
        }
        template <typename Req>
        http::response<http::string_body> HandleJoinPlayer(const Req &req)
        {
            using namespace std::literals;

            if (req.method() != http::verb::post)
            {
                json::object obj;
                obj["code"] = "invalidMethod";
                obj["message"] = "Only POST method is expected";
                http::response<http::string_body> res{http::status::method_not_allowed, req.version()};
                res.set(http::field::content_type, "application/json");
                res.set(http::field::cache_control, "no-cache");
                res.set(http::field::allow, "POST");
                res.body() = json::serialize(obj);
                res.content_length(res.body().size());
                res.keep_alive(req.keep_alive());
                return res;
            }

            if (req[http::field::content_type] != "application/json")
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Expected application/json", req);
            }

            json::value json_body;
            try
            {
                json_body = json::parse(req.body());
            }
            catch (...)
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Join game request parse error", req);
            }

            if (!json_body.is_object())
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Join game request parse error", req);
            }

            const auto &obj = json_body.as_object();
            if (!obj.contains("userName") || !obj.contains("mapId") ||
                !obj.at("userName").is_string() || !obj.at("mapId").is_string())
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Join game request parse error", req);
            }

            const std::string user_name = obj.at("userName").as_string().c_str();
            const std::string map_id = obj.at("mapId").as_string().c_str();

            if (user_name.empty())
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Invalid name", req);
            }

            model::Map *map = const_cast<model::Map *>(game_.FindMap(model::Map::Id{map_id}));
            if (!map)
            {
                return MakeError(http::status::not_found, "mapNotFound", "Map not found", req);
            }

            auto it = sessions_.find(map_id);
            std::shared_ptr<GameSession> session;
            if (it == sessions_.end())
            {
                session = std::make_shared<GameSession>(map);
                sessions_[map_id] = session;
            }
            else
            {
                session = it->second;
            }
            std::shared_ptr<Dog> dog = session->AddDog(user_name);
            //dog->SetSpeed(map->GetSpeedForThisMap());
            Player &player = players_.AddPlayer(session, dog);
            // Player &player = players_.AddPlayer(nullptr, dog);

            json::object res_obj;
            res_obj["authToken"] = *player.GetToken().value(); // Token — Tagged<std::string>
            res_obj["playerId"] = dog->GetId();                // id собаки = id игрока

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.body() = json::serialize(res_obj);
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            return res;
        }
        template <typename Req>
        http::response<http::string_body> HandlePlayersList(const Req &req) const
        {
            using namespace std::literals;

            if (req.method() != http::verb::get && req.method() != http::verb::head)
            {
                json::object obj;
                obj["code"] = "invalidMethod";
                obj["message"] = "Invalid method";
                http::response<http::string_body> res{http::status::method_not_allowed, req.version()};
                res.set(http::field::content_type, "application/json");
                res.set(http::field::cache_control, "no-cache");
                res.set(http::field::allow, "GET, HEAD");
                res.body() = json::serialize(obj);
                res.content_length(res.body().size());
                res.keep_alive(req.keep_alive());
                return res;
            }

            http::response<http::string_body> err;
            auto player_opt = TryExtractPlayer(req, err);
            if (!player_opt)
                return err;

            Player *player = *player_opt;

            // model::Map::Id map_id = player->GetSession()->GetMap()->GetId();

            json::object response_body;
            for (const auto &player_ptr : players_.GetPlayers())
            {

                if (!player_ptr)
                    continue;

                Player *p = player_ptr.get();

                response_body[std::to_string(p->GetDog()->GetId())] = {
                    {"name", p->GetDog()->GetName()}};
            }

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            if (req.method() != http::verb::head)
            {
                res.body() = json::serialize(response_body);
                res.content_length(res.body().size());
            }
            else
            {
                res.content_length(json::serialize(response_body).size()); // нужно указать Content-Length, даже если тело не отправляется
            }
            res.keep_alive(req.keep_alive());
            return res;
        }
        template <typename Req>
        http::response<http::string_body> HandleGameState(const Req &req) const
        {
            using namespace std::literals;

            if (req.method() != http::verb::get && req.method() != http::verb::head)
            {
                json::object obj;
                obj["code"] = "invalidMethod";
                obj["message"] = "Invalid method";
                http::response<http::string_body> res{http::status::method_not_allowed, req.version()};
                res.set(http::field::content_type, "application/json");
                res.set(http::field::cache_control, "no-cache");
                res.set(http::field::allow, "GET, HEAD");
                res.body() = json::serialize(obj);
                res.content_length(res.body().size());
                res.keep_alive(req.keep_alive());
                return res;
            }

            http::response<http::string_body> err;
            auto player_opt = TryExtractPlayer(req, err);
            if (!player_opt)
                return err;

            Player *player = *player_opt;

            // const model::Map::Id map_id = player->GetSession()->GetMap()->GetId();

            json::object players_json;

            for (const auto &p : players_.GetPlayers())
            {

                if (!p)
                    continue;

                std::shared_ptr<Dog> dog = p->GetDog();
                std::string dir;
                json::array pos{
                    static_cast<double>(dog->GetPosition().x),
                    static_cast<double>(dog->GetPosition().y)};
                json::array speed{
                    static_cast<double>(dog->GetSpeed().x),
                    static_cast<double>(dog->GetSpeed().y)};
                switch (dog->GetDirection())
                {
                case Direction::NORTH:
                    dir = "U";
                    break;
                case Direction::SOUTH:
                    dir = "D";
                    break;
                case Direction::WEST:
                    dir = "L";
                    break;
                case Direction::EAST:
                    dir = "R";
                    break;
                }

                players_json[std::to_string(dog->GetId())] = {
                    {"pos", pos},
                    {"speed", speed},
                    {"dir", dir}};
            }

            json::object res_body;
            res_body["players"] = players_json;

            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1);
            oss << json::serialize(res_body);

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.body() = oss.str();
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            return res;
        }
        template <typename Req>
        std::optional<Player *> TryExtractPlayer(const Req &req, http::response<http::string_body> &error_response) const
        {
            const auto auth_header = req[http::field::authorization];
            if (auth_header.empty() || !auth_header.starts_with("Bearer "))
            {
                error_response = MakeError(http::status::unauthorized, "invalidToken", "Authorization header is missing", req);
                return std::nullopt;
            }

            const std::string token_str = std::string(auth_header.substr(7));
            if (token_str.size() != 32)
            {
                error_response = MakeError(http::status::unauthorized, "invalidToken", "Invalid token length", req);
                return std::nullopt;
            }

            Token token{token_str};
            Player *player = players_.FindByToken(token);
            if (!player)
            {
                error_response = MakeError(http::status::unauthorized, "unknownToken", "Player token has not been found", req);
                return std::nullopt;
            }

            return player;
        }
        template <typename Req>
        http::response<http::string_body> HandleGameActions(const Req &req)
        {
            using namespace std::literals;

            if (req.method() != http::verb::post)
            {
                json::object obj;
                obj["code"] = "invalidMethod";
                obj["message"] = "Invalid method";
                http::response<http::string_body> res{http::status::method_not_allowed, req.version()};
                res.set(http::field::content_type, "application/json");
                res.set(http::field::cache_control, "no-cache");
                res.set(http::field::allow, "POST");
                res.body() = json::serialize(obj);
                res.content_length(res.body().size());
                res.keep_alive(req.keep_alive());
                return res;
            }

            if (req[http::field::content_type] != "application/json")
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Expected application/json", req);
            }

            json::value json_body;
            try
            {
                json_body = json::parse(req.body());
            }
            catch (...)
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Failed to parse request body", req);
            }

            if (!json_body.is_object())
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Expected JSON object", req);
            }

            const auto &obj = json_body.as_object();

            if (!obj.contains("move") || !obj.at("move").is_string())
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Missing or invalid 'move' field", req);
            }

            const std::string dir = std::string(obj.at("move").as_string());

            http::response<http::string_body> err;
            auto player_opt = TryExtractPlayer(req, err);
            if (!player_opt)
                return err;

            Player *player = *player_opt;
            std::shared_ptr<Dog> dog = player->GetDog();

            if (!dog)
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Dog not found", req);
            }
            
            // Устанавливаем направление
            if (dir == "U"){
                dog->SetDirection(Direction::NORTH);
            }
            else if (dir == "D"){
                dog->SetDirection(Direction::SOUTH);
            }
            else if (dir == "L"){
                dog->SetDirection(Direction::WEST);
            }
            else if (dir == "R"){
                dog->SetDirection(Direction::EAST);
            }
            else{
                return MakeError(http::status::bad_request, "invalidArgument", "Invalid direction", req);
            }
            // Берём скорость из карты
            const double speed = player->GetSession()->GetMap()->GetSpeedForThisMap();
            dog->SetSpeed(speed);
            
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.body() = "{}";
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            return res;
        }
        template <typename Req>
        http::response<http::string_body> HandleGameTick(const Req &req)
        {
            using namespace std::literals;

            if (req.method() != http::verb::post)
            {
                return MakeError(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", req);
            }

            if (req[http::field::content_type] != "application/json")
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Expected application/json", req);
            }

            json::value json_body;
            try
            {
                json_body = json::parse(req.body());
            }
            catch (...)
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", req);
            }

            if (!json_body.is_object())
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Expected JSON object", req);
            }

            const auto &obj = json_body.as_object();
            if (!obj.contains("timeDelta") || !obj.at("timeDelta").is_int64())
            {
                return MakeError(http::status::bad_request, "invalidArgument", "Missing or invalid 'timeDelta' field", req);
            }

            int64_t time_delta_ms = obj.at("timeDelta").as_int64();
            if (time_delta_ms < 0)
            {
                return MakeError(http::status::bad_request, "invalidArgument", "timeDelta must be non-negative", req);
            }

            for (const auto &player_ptr : players_.GetPlayers())
            {
                auto dog = player_ptr->GetDog();
                dog->UpdatePosition(time_delta_ms, player_ptr->GetSession()->GetMap());
            }

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.body() = "{}";
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            return res;
        }
    };

    class RequestHandler
    {
    public:
        RequestHandler(model::Game &game, fs::path static_root, boost::asio::io_context &ioc)
            : game_{game}, static_root_{std::move(static_root)}, api_handler_(game, ioc) {}

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>> &&req, Send &&send)
        {
            using namespace std::literals;

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

            const auto not_found_json = [&req](std::string_view text)
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

            const auto not_found_file = [&req]()
            {
                http::response<http::string_body> res{http::status::not_found, req.version()};
                res.set(http::field::content_type, "text/plain");
                res.body() = "File not found";
                res.prepare_payload();
                return res;
            };

            const std::string target = std::string(req.target());

            if (target.starts_with("/api/"))
            {
                api_handler_.HandleRequest(req, std::forward<Send>(send));
                return;
            }

            // Статика
            std::string path = UrlDecode(req.target());
            fs::path full_path = static_root_ / path.substr(1);

            if (fs::is_directory(full_path))
            {
                full_path /= "index.html";
            }

            if (!IsSubPath(static_root_, full_path))
            {
                return send(bad_request("Invalid path"));
            }

            if (!fs::exists(full_path) || fs::is_directory(full_path))
            {
                return send(not_found_file());
            }

            std::ifstream file(full_path, std::ios::binary);
            std::ostringstream ss;
            ss << file.rdbuf();
            std::string body = ss.str();

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, GetMimeType(full_path));
            res.content_length(body.size());
            if (req.method() == http::verb::get)
            {
                res.body() = std::move(body);
            }
            res.keep_alive(req.keep_alive());
            return send(std::move(res));
        }

    private:
        model::Game &game_;
        fs::path static_root_;
        ApiRequestHandler api_handler_;
        std::string UrlDecode(std::string_view str) const
        {
            std::ostringstream result;
            for (size_t i = 0; i < str.size(); ++i)
            {
                if (str[i] == '%' && i + 2 < str.size())
                {
                    std::string hex{str[i + 1], str[i + 2]};
                    result << static_cast<char>(std::stoi(hex, nullptr, 16));
                    i += 2;
                }
                else if (str[i] == '+')
                {
                    result << ' ';
                }
                else
                {
                    result << str[i];
                }
            }
            return result.str();
        }

        std::string GetMimeType(const fs::path &path) const
        {
            static const std::unordered_map<std::string, std::string> types = {
                {".htm", "text/html"}, {".html", "text/html"}, {".css", "text/css"}, {".txt", "text/plain"}, {".js", "text/javascript"}, {".json", "application/json"}, {".xml", "application/xml"}, {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpe", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".gif", "image/gif"}, {".bmp", "image/bmp"}, {".ico", "image/vnd.microsoft.icon"}, {".tiff", "image/tiff"}, {".tif", "image/tiff"}, {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"}, {".mp3", "audio/mpeg"}};
            auto ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (auto it = types.find(ext); it != types.end())
            {
                return it->second;
            }
            return "application/octet-stream";
        }

        bool IsSubPath(const fs::path &base, const fs::path &path) const
        {
            auto base_abs = fs::weakly_canonical(base);
            auto path_abs = fs::weakly_canonical(path);
            return std::mismatch(base_abs.begin(), base_abs.end(), path_abs.begin()).first == base_abs.end();
        }
    };

    class LoggingRequestHandler
    {
    public:
        explicit LoggingRequestHandler(RequestHandler &decorated)
            : decorated_{decorated} {}

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>> &&req,
                        Send &&send,
                        const std::string &ip)
        {
            auto start = std::chrono::steady_clock::now();

            LogRequest(ip, std::string(req.target()), std::string(req.method_string()));

            auto wrapped_send = [this, start, &send, &ip](auto &&response)
            {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start)
                              .count();
                std::optional<std::string> content_type;
                if (response.find(http::field::content_type) != response.end())
                {
                    content_type = std::string(response[http::field::content_type]);
                }
                LogResponse(ip, response.result_int(), content_type, ms);
                send(std::forward<decltype(response)>(response));
            };

            decorated_(std::move(req), std::move(wrapped_send));
        }

    private:
        RequestHandler &decorated_;

        void LogRequest(const std::string &ip, const std::string &uri, const std::string &method)
        {
            json::value req_data{{"ip", ip}, {"URI", uri}, {"method", method}};
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, req_data) << "request received";
        }

        void LogResponse(const std::string &ip, int code, const std::optional<std::string> &content_type, long response_time_ms)
        {
            json::object resp_data;
            resp_data["ip"] = ip;
            resp_data["response_time"] = static_cast<int>(response_time_ms);
            resp_data["code"] = code;
            if (content_type)
            {
                resp_data["content_type"] = *content_type;
            }
            else
            {
                resp_data["content_type"] = nullptr;
            }
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, resp_data) << "response sent";
        }
    };

} // namespace http_handler
