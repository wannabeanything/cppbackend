#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <optional>

#include "model.h"
#include "extra_data.h"
#include "loot_generator.h"

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
    void SetSpeedForMap(model::Map &map, const boost::json::object &root, const boost::json::object &map_obj)
    {
        if (map_obj.contains("dogSpeed"))
        {
            map.SetSpeedForThisMap(map_obj.at("dogSpeed").as_double());
        }
        else if (root.contains("defaultDogSpeed"))
        {
            map.SetSpeedForThisMap(root.at("defaultDogSpeed").as_double());
        }
    }
    void SetLootGenerator(model::Map &map, const boost::json::object &root)
    {
        if (!root.contains("lootGeneratorConfig"))
        {
            throw std::runtime_error("Missing lootGeneratorConfig");
        }

        const auto &config = root.at("lootGeneratorConfig").as_object();
        double period_sec = config.at("period").as_double();
        double probability = config.at("probability").as_double();

        loot_gen::LootGenerator generator{
            std::chrono::milliseconds(static_cast<int>(period_sec * 1000.0)),
            probability};

        extra_data::GetInstance().SetLootGenerator(map.GetId(), generator);
        map.SetLootGenerator(std::move(generator));
    }

    void LoadLootTypes(model::Map &map, const boost::json::object &map_obj)
    {
        if (!map_obj.contains("lootTypes"))
        {
            throw std::runtime_error("Map missing lootTypes");
        }

        const auto &loot_array = map_obj.at("lootTypes").as_array();
        if (loot_array.empty())
        {
            throw std::runtime_error("lootTypes must not be empty");
        }

        extra_data::GetInstance().SetLootTypes(map.GetId(), loot_array);
        map.SetLootTypeCount(static_cast<int>(loot_array.size()));
    }
    void SetBagCapacity(model::Map &map, const boost::json::object &root, const boost::json::object &map_obj)
    {
        // Значение по умолчанию
        int default_capacity = 3;

        // Если в корне есть defaultBagCapacity, переопределим его
        if (root.contains("defaultBagCapacity"))
        {
            default_capacity = static_cast<int>(root.at("defaultBagCapacity").as_int64());
        }

        // Проверим наличие bagCapacity у карты
        int capacity = default_capacity;
        if (map_obj.contains("bagCapacity"))
        {
            capacity = static_cast<int>(map_obj.at("bagCapacity").as_int64());
        }

        map.SetBagCapacityForMap(capacity);
    }
    void SetRetirementTime(model::Map &map, const boost::json::object &root)
    {
        double default_time = 60.0;

        // Если в корне есть defaultBagCapacity, переопределим его
        if (root.contains("dogRetirementTime"))
        {
            default_time = root.at("dogRetirementTime").as_double(); // не касти к int
        }

        map.SetRetirementTime(default_time);
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
        const auto &root = data.as_object();
        for (const auto &map_json : data.at("maps").as_array())
        {
            const auto &obj = map_json.as_object();

            model::Map map{
                model::Map::Id{obj.at("id").as_string().c_str()},
                std::string{obj.at("name").as_string().c_str()}};

            // Speed
            SetSpeedForMap(map, root, obj);

            SetBagCapacity(map, root, obj);

            // Roads
            LoadRoadsToMap(map, obj);

            // Buildings
            LoadBuildingsToMap(map, obj);

            // Offices
            LoadOfficesToMap(map, obj);

            LoadLootTypes(map, obj);
            SetLootGenerator(map, root);
            SetRetirementTime(map, root);
            game.AddMap(std::move(map));
        }

        return game;
    }

} // namespace json_loader
