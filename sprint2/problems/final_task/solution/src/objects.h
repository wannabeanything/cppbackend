
#pragma once
#include "model.h"
#include <random>
#include <string>
#include <sstream>
#include <optional>
#include <iomanip>
#include <memory>
#include <chrono>
#include <cmath>

struct TokenTag
{
};
// Ник игрока - токен
using Token = util::Tagged<std::string, TokenTag>;

enum class Direction
{
    NORTH,
    SOUTH,
    WEST,
    EAST
};

class GameSession;

class Dog
{
public:
    Dog(int id, const std::string &name, model::Position pos)
        : id_(id), appeared_name_(name), position_(pos), speed_({0.0, 0.0}), direction_(Direction::NORTH) {}

    int GetId() const { return id_; }
    const std::string &GetName() const { return appeared_name_; }
    const model::Position &GetPosition() const { return position_; }
    const model::Position &GetSpeed() const { return speed_; }
    Direction GetDirection() const { return direction_; }
    void SetSpeed(double x)
    {
        switch (direction_)
        {
        case Direction::NORTH:
            speed_ = {0.0, -x};
            break;
        case Direction::SOUTH:
            speed_ = {0.0, x};
            break;
        case Direction::WEST:
            speed_ = {-x, 0.0};
            break;
        case Direction::EAST:
            speed_ = {x, 0.0};
            break;
        }
    }
    void SetDirection(Direction dir)
    {
        direction_ = dir;
    }
    void UpdatePosition(int ms, const model::Map *map)
    {
        const double seconds = std::chrono::duration<double>(std::chrono::milliseconds(ms)).count();
        if (speed_.x == 0.0 && speed_.y == 0.0)
        {
            return;
        }

        const double dx = speed_.x * seconds;
        const double dy = speed_.y * seconds;
        model::Position possible_new_pos = {position_.x + dx, position_.y + dy};

        model::Position new_pos = map->FitPositionToRoad(position_, possible_new_pos);
        if (new_pos != possible_new_pos)
        {
            speed_ = {0.0, 0.0};
        }

        position_ = new_pos;
    }

private:
    int id_;
    std::string appeared_name_;
    model::Position position_;
    model::Position speed_;
    Direction direction_;
};
class Player
{
public:
    Player(std::shared_ptr<GameSession> session, std::shared_ptr<Dog> dog)
        : session_(std::move(session)),
          dog_(std::move(dog)),
          generator1_(std::random_device{}()),
          generator2_(std::random_device{}())
    {
        token_ = GenerateToken();
    }
    std::shared_ptr<GameSession> GetSession() const
    {
        return session_;
    }
    std::optional<Token> GetToken() const
    {
        return token_;
    }
    std::shared_ptr<Dog> GetDog() const
    {
        return dog_;
    }

private:
    std::shared_ptr<GameSession> session_;
    std::shared_ptr<Dog> dog_;
    std::optional<Token> token_;
    std::random_device random_device_;
    std::mt19937_64 generator1_;
    std::mt19937_64 generator2_;

    Token GenerateToken()
    {
        uint64_t part1 = generator1_();
        uint64_t part2 = generator2_();
        std::ostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill('0') << part1
            << std::setw(16) << std::setfill('0') << part2;
        return Token{oss.str()};
    }
};

class GameSession
{
public:
    explicit GameSession(model::Map *map) : map_(map) {}

    model::Map *GetMap() const
    {
        return map_;
    }

#include <random>

    std::shared_ptr<Dog> AddDog(const std::string &name, bool randomize_spawn = false)
    {
        std::shared_ptr<Dog> dog = nullptr;

        const auto &roads = map_->GetRoads();
        if (roads.empty())
        {
            throw std::runtime_error("No roads available for dog spawning");
        }

        if (randomize_spawn)
        {
            static std::random_device rd;
            static std::mt19937 gen(rd());

            
            const size_t num_roads = roads.size();
            std::uniform_int_distribution<size_t> road_index_dist(0, num_roads - 1);
            size_t index = road_index_dist(gen);

            auto it = roads.begin();
            std::advance(it, index);
            const auto &road = *it;

            const auto &start = road.GetStart();
            const auto &end = road.GetEnd();

            int dx = end.x - start.x;
            int dy = end.y - start.y;
            int length = std::max(std::abs(dx), std::abs(dy));
            if (length == 0)
            {
                throw std::runtime_error("Invalid road with zero length");
            }

            
            std::uniform_int_distribution<int> step_dist(0, length);
            int step = step_dist(gen);

            model::Point point;
            if (dx != 0)
            {
                point = model::Point{start.x + (dx > 0 ? step : -step), start.y};
            }
            else
            {
                point = model::Point{start.x, start.y + (dy > 0 ? step : -step)};
            }

            model::Position pos{static_cast<double>(point.x), static_cast<double>(point.y)};
            dog = std::make_shared<Dog>(next_dog_id_++, name, pos);
        }
        else
        {
            const auto first_road_coords = roads.front().GetStart();
            model::Position dog_start_point = model::Position{
                static_cast<double>(first_road_coords.x),
                static_cast<double>(first_road_coords.y)};
            dog = std::make_shared<Dog>(next_dog_id_++, name, dog_start_point);
        }

        dogs_.emplace_back(dog);
        return dog;
    }

private:
    model::Map *map_;
    std::vector<std::shared_ptr<Dog>> dogs_;
    int next_dog_id_ = 0;
};
class Players
{
public:
    Player &AddPlayer(std::shared_ptr<GameSession> session, std::shared_ptr<Dog> dog)
    {
        players_.emplace_back(std::make_unique<Player>(std::move(session), std::move(dog)));
        return *players_.back();
    }

    Player *FindByDogIdAndMapId(int dog_id, const model::Map::Id map_id) const
    {
        for (const auto &player : players_)
        {
            if (player->GetDog()->GetId() == dog_id &&
                player->GetSession()->GetMap()->GetId() == map_id)
            {
                return player.get();
            }
        }
        return nullptr;
    }

    Player *FindByToken(Token token) const
    {
        for (const auto &player : players_)
        {
            const auto player_token = player->GetToken();
            if (player_token.has_value() && player_token.value() == token)
            {
                return player.get();
            }
        }
        return nullptr;
    }

    int GetPlayerCount() const
    {
        return static_cast<int>(players_.size());
    }

    const std::vector<std::unique_ptr<Player>> &GetPlayers() const
    {
        return players_;
    }

private:
    std::vector<std::unique_ptr<Player>> players_;
};