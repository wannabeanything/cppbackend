#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <string>

#include "model.h"

namespace json_loader
{

    namespace json = boost::json;
    void LoadRoadsToMap(model::Map &map, const json::object &obj)
    {
        for (const auto &road_json : obj.at("roads").as_array())
        {
            const auto &road_obj = road_json.as_object();
            const int x0 = road_obj.at("x0").as_int64();
            const int y0 = road_obj.at("y0").as_int64();
            const model::Point start{x0, y0};

            if (road_obj.contains("x1"))
            {
                const int x1 = road_obj.at("x1").as_int64();
                map.AddRoad(model::Road(model::Road::HORIZONTAL, start, x1));
            }
            else
            {
                const int y1 = road_obj.at("y1").as_int64();
                map.AddRoad(model::Road(model::Road::VERTICAL, start, y1));
            }
        }
    }
    void LoadBuildingsToMap(model::Map &map, const json::object &obj)
    {
        for (const auto &building_json : obj.at("buildings").as_array())
        {
            const auto &building_obj = building_json.as_object();
            model::Point pos{
                static_cast<int>(building_obj.at("x").as_int64()),
                static_cast<int>(building_obj.at("y").as_int64())};
            model::Size size{
                static_cast<int>(building_obj.at("w").as_int64()),
                static_cast<int>(building_obj.at("h").as_int64())};
            map.AddBuilding(model::Building(model::Rectangle{pos, size}));
        }
    }
    void LoadOfficesToMap(model::Map &map, const json::object &obj)
    {
        for (const auto &office_json : obj.at("offices").as_array())
        {
            const auto &office_obj = office_json.as_object();

            model::Office::Id id{office_obj.at("id").as_string().c_str()};
            model::Point pos{
                static_cast<int>(office_obj.at("x").as_int64()),
                static_cast<int>(office_obj.at("y").as_int64())};
            model::Offset offset{
                static_cast<int>(office_obj.at("offsetX").as_int64()),
                static_cast<int>(office_obj.at("offsetY").as_int64())};

            map.AddOffice(model::Office{id, pos, offset});
        }
    }
    model::Game LoadGame(const std::filesystem::path &path)
    {
        std::ifstream input(path);
        if (!input)
        {
            throw std::runtime_error("Cannot open file " + path.string());
        }
        json::value data;
        try
        {
            std::stringstream buffer;
            buffer << input.rdbuf();
            data = json::parse(buffer.str());
        }
        catch (...)
        {
            throw "Error parsing";
        }

        model::Game game;

        for (const auto &map_json : data.at("maps").as_array())
        {
            const auto &obj = map_json.as_object();

            model::Map map{
                model::Map::Id{obj.at("id").as_string().c_str()},
                std::string{obj.at("name").as_string().c_str()}};

            // Roads
            LoadRoadsToMap(map, obj);

            // Buildings
            LoadBuildingsToMap(map, obj);

            // Offices
            LoadOfficesToMap(map, obj);

            game.AddMap(std::move(map));
        }

        return game;
    }
    
} // namespace json_loader
