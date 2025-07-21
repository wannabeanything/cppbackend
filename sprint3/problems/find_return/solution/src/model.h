#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <list>
#include <optional>

#include "tagged.h"
#include "loot_generator.h"

namespace model
{

    using Dimension = int;
    using Coord = Dimension;

    struct Point
    {
        Coord x, y;
        bool operator==(const Point &other) const
        {
            return x == other.x && y == other.y;
        }
    };
    struct Position
    {
        double x;
        double y;
        bool operator==(const Position &other) const
        {
            return x == other.x && y == other.y;
        }
    };

    struct Size
    {
        Dimension width, height;
    };

    struct Rectangle
    {
        Point position;
        Size size;
    };

    struct Offset
    {
        Dimension dx, dy;
    };

    class Road
    {
        struct HorizontalTag
        {
            HorizontalTag() = default;
        };

        struct VerticalTag
        {
            VerticalTag() = default;
        };

    public:
        constexpr static HorizontalTag HORIZONTAL{};
        constexpr static VerticalTag VERTICAL{};

        Road(HorizontalTag, Point start, Coord end_x) noexcept
            : start_{start}, end_{end_x, start.y}
        {
        }

        Road(VerticalTag, Point start, Coord end_y) noexcept
            : start_{start}, end_{start.x, end_y}
        {
        }

        bool IsHorizontal() const noexcept
        {
            return start_.y == end_.y;
        }

        bool IsVertical() const noexcept
        {
            return start_.x == end_.x;
        }

        Point GetStart() const noexcept
        {
            return start_;
        }

        Point GetEnd() const noexcept
        {
            return end_;
        }

    private:
        Point start_;
        Point end_;
    };
    enum class Orientation
    {
        HORIZONTAL,
        VERTICAL
    };

    struct PointHash
    {
        std::size_t operator()(const Point &p) const
        {
            return std::hash<int>()(p.x) ^ (std::hash<int>()(p.y) << 1);
        }
    };

    class Building
    {
    public:
        explicit Building(Rectangle bounds) noexcept
            : bounds_{bounds}
        {
        }

        const Rectangle &GetBounds() const noexcept
        {
            return bounds_;
        }

    private:
        Rectangle bounds_;
    };

    class Office
    {
    public:
        using Id = util::Tagged<std::string, Office>;

        Office(Id id, Point position, Offset offset) noexcept
            : id_{std::move(id)}, position_{position}, offset_{offset}
        {
        }

        const Id &GetId() const noexcept
        {
            return id_;
        }

        Point GetPosition() const noexcept
        {
            return position_;
        }

        Offset GetOffset() const noexcept
        {
            return offset_;
        }

    private:
        Id id_;
        Point position_;
        Offset offset_;
    };

    class Map
    {
        class RoadIndex;

    public:
        using Id = util::Tagged<std::string, Map>;
        using Roads = std::list<Road>;
        using Buildings = std::vector<Building>;
        using Offices = std::vector<Office>;

        Map(Id id, std::string name) noexcept
            : id_(std::move(id)), name_(std::move(name))
        {
        }

        const Id &GetId() const noexcept
        {
            return id_;
        }

        const std::string &GetName() const noexcept
        {
            return name_;
        }

        const Buildings &GetBuildings() const noexcept
        {
            return buildings_;
        }

        const Roads &GetRoads() const noexcept
        {
            return roads_;
        }

        const Offices &GetOffices() const noexcept
        {
            return offices_;
        }

        void AddRoad(const Road &road)
        {
            roads_.emplace_back(road);
            road_index_.AddRoad(roads_.back());
        }
        const RoadIndex &GetRoadIndex() const noexcept
        {
            return road_index_;
        }
        void AddBuilding(const Building &building)
        {
            buildings_.emplace_back(building);
        }
        void SetSpeedForThisMap(double speed)
        {
            speed_ = speed;
        }
        double GetSpeedForThisMap() const
        {
            return speed_;
        }
        const Road *FindRoadAtPosition(Point pos, Orientation orientation) const
        {
            return road_index_.FindRoadAtPosition(pos.x, pos.y, orientation);
        }
        Position FitPositionToRoad(const Position &current_pos, const Position &new_pos) const
        {
            // 1. Определяем направление движения
            const double dx = new_pos.x - current_pos.x;
            const double dy = new_pos.y - current_pos.y;

            Orientation movement_orientation = (std::abs(dx) > std::abs(dy)) ? Orientation::HORIZONTAL : Orientation::VERTICAL;

            // 2. Находим дорогу
            const Road *road = road_index_.FindRoadAtPosition(current_pos.x, current_pos.y, movement_orientation);

            if (!road)
            {
                road = road_index_.FindRoadAtPosition(current_pos.x, current_pos.y, movement_orientation == Orientation::HORIZONTAL
                                                                       ? Orientation::VERTICAL
                                                                       : Orientation::HORIZONTAL);
                if (!road)
                {
                    return current_pos;
                }
            }

            double half_width = road_width_ / 2.0;

            // 3. Вычисляем границы дорожного полотна
            double min_x, max_x, min_y, max_y;
            if (road->IsHorizontal())
            {
                min_x = std::min(road->GetStart().x, road->GetEnd().x) - half_width;
                max_x = std::max(road->GetStart().x, road->GetEnd().x) + half_width;
                min_y = road->GetStart().y - half_width;
                max_y = road->GetStart().y + half_width;
            }
            else
            { /*if (road->IsVertical()) */
                min_x = road->GetStart().x - half_width;
                max_x = road->GetStart().x + half_width;
                min_y = std::min(road->GetStart().y, road->GetEnd().y) - half_width;
                max_y = std::max(road->GetStart().y, road->GetEnd().y) + half_width;
            }

            // 4. Корректируем позицию
            Position result = new_pos;
            if (result.x < min_x)
            {
                result.x = min_x;
            }
            if (result.x > max_x)
            {
                result.x = max_x;
            }
            if (result.y < min_y)
            {
                result.y = min_y;
            }
            if (result.y > max_y)
            {
                result.y = max_y;
            }

            return result;
        }
        void SetBagCapacityForMap(int size){
            bag_capacity_for_map_ = size;
        }
        int GetBagCapacityForMap(){
            return bag_capacity_for_map_;
        }
        void AddOffice(Office office);
        void SetLootTypeCount(int count);
        int GetLootTypeCount() const;

        void SetLootGenerator(loot_gen::LootGenerator generator);
        loot_gen::LootGenerator& GetLootGenerator();
    private:
        class RoadIndex
        {
        public:
            void AddRoad(const Road &road)
            {
                if (road.IsHorizontal())
                {
                    int x1 = road.GetStart().x;
                    int x2 = road.GetEnd().x;
                    int step = (x2 >= x1) ? 1 : -1;
                    for (int x = x1; x != x2 + step; x += step)
                    {
                        Point key{x, road.GetStart().y};
                        point_to_road_[Orientation::HORIZONTAL][key] = &road;
                    }
                }
                else
                { /*(road.IsVertical())*/
                    int y1 = road.GetStart().y;
                    int y2 = road.GetEnd().y;
                    int step = (y2 >= y1) ? 1 : -1;
                    for (int y = y1; y != y2 + step; y += step)
                    {
                        Point key{road.GetStart().x, y};
                        point_to_road_[Orientation::VERTICAL][key] = &road;
                    }
                }
            }

            const Road *FindRoadAtPosition(double x, double y, Orientation orientation) const
            {
                Point key{static_cast<int>(std::round(x)), static_cast<int>(std::round(y))};
                auto it_orient = point_to_road_.find(orientation);
                if (it_orient == point_to_road_.end())
                    return nullptr;
                const auto &road_map = it_orient->second;
                auto it = road_map.find(key);
                return (it != road_map.end()) ? it->second : nullptr;
            }

        private:
            std::unordered_map<Orientation, std::unordered_map<Point, const Road *, PointHash>> point_to_road_;
        };
        using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
        double speed_ = 1;
        Id id_;
        std::string name_;
        Roads roads_;
        Buildings buildings_;
        RoadIndex road_index_;
        double road_width_ = 0.8;
        OfficeIdToIndex warehouse_id_to_index_;
        Offices offices_;
        int loot_type_count_ = 0;
        int bag_capacity_for_map_ = 3;
        std::optional<loot_gen::LootGenerator> loot_generator_;
    };

    class Game
    {
    public:
        using Maps = std::vector<Map>;

        void AddMap(Map map);

        const Maps &GetMaps() const noexcept
        {
            return maps_;
        }

        const Map *FindMap(const Map::Id &id) const noexcept
        {
            if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end())
            {
                return &maps_.at(it->second);
            }
            return nullptr;
        }

    private:
        using MapIdHasher = util::TaggedHasher<Map::Id>;
        using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

        std::vector<Map> maps_;
        MapIdToIndex map_id_to_index_;
    };

} // namespace model
