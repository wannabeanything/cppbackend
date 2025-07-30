#pragma once

#include "model.h"
#include "collision_detector.h"
#include <random>
#include <string>
#include <sstream>
#include <optional>
#include <iomanip>
#include <memory>
#include <chrono>
#include <cmath>
#include <set>
#include <boost/json.hpp>

struct TokenTag
{
};
using Token = util::Tagged<std::string, TokenTag>;

enum class Direction
{
    NORTH,
    SOUTH,
    WEST,
    EAST
};

inline model::Position GetRandomPositionOnRoad(const model::Map &map)
{
    const auto &roads = map.GetRoads();
    if (roads.empty())
    {
        throw std::runtime_error("No roads available");
    }

    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const auto &road = *std::next(roads.begin(), road_dist(gen));

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

    model::Point point = dx != 0
                             ? model::Point{start.x + (dx > 0 ? step : -step), start.y}
                             : model::Point{start.x, start.y + (dy > 0 ? step : -step)};
    return {static_cast<double>(point.x), static_cast<double>(point.y)};
}

class GameSession;

class Dog
{
public:
    Dog(int id, const std::string &name, model::Position pos)
        : id_(id), appeared_name_(name), position_(pos),
          speed_({0.0, 0.0}), direction_(Direction::NORTH),
          bag_capacity_(0) {}

    int GetId() const { return id_; }
    const std::string &GetName() const { return appeared_name_; }
    const model::Position &GetPosition() const { return position_; }
    const model::Position &GetSpeed() const { return speed_; }
    Direction GetDirection() const { return direction_; }

    void SetDirection(Direction dir) { direction_ = dir; }

    void SetSpeed(double value)
    {
        switch (direction_)
        {
        case Direction::NORTH:
            speed_ = {0.0, -value};
            break;
        case Direction::SOUTH:
            speed_ = {0.0, value};
            break;
        case Direction::WEST:
            speed_ = {-value, 0.0};
            break;
        case Direction::EAST:
            speed_ = {value, 0.0};
            break;
        }
    }

    void SetBagCapacityForDog(int size) { bag_capacity_ = size; }
    bool CanPickUp() const { return inventory_.size() < bag_capacity_; }

    void PickUpItem(int id, int type, int value)
    {
        if (CanPickUp())
        {
            inventory_.emplace_back(id, type);
            RaiseScore(value);
        }
    }
    const std::vector<std::pair<int, int>> GetBag() const
    {
        return inventory_;
    }
    void ClearBag()
    {
        inventory_.clear();
    }
    int GetBagCapacity() const
    {
        return bag_capacity_;
    }
    void UpdatePosition(int ms, GameSession *session);
    const int GetScore() const
    {
        return score_;
    }
    void SetScore(int value)
    {
        score_ = value;
    }
    void RetireDog()
    {
        retired_ = true;
    }
    void SetRetirementTimeout(double seconds)
    {
        retirement_timeout_ = seconds;
    }

    double GetRetirementTimeout() const
    {
        return retirement_timeout_;
    }
    void AddIdleTime(double value)
    {
        current_idle_time_ += value;
        if (current_idle_time_ >= retirement_timeout_)
        {
            RetireDog();
        }
    }
    void AddLifeTime(double dt)
    {
        if (!retired_)
        {
            life_time_ += dt;
        }
    }
    double GetLifeTime() const
    {
        return life_time_;
    }
    bool IsRetired() const
    {
        return retired_;
    }
    bool WasRecorded() const { return recorded_; }
    void MarkRecorded() { recorded_ = true; }

private:
    int id_;
    std::string appeared_name_;
    model::Position position_;
    model::Position speed_;
    Direction direction_;
    int bag_capacity_;
    std::vector<std::pair<int, int>> inventory_;
    int score_ = 0;
    double retirement_timeout_;
    double current_idle_time_ = 0.0;
    double life_time_ = 0.0;
    bool retired_ = false;
    bool recorded_ = false;

    void RaiseScore(int value)
    {
        score_ += value;
    }
};

class GameSession
{
private:
    class SessionGathererProvider;

public:
    explicit GameSession(model::Map *map) : map_(map) {}

    model::Map *GetMap() const { return map_; }

    std::shared_ptr<Dog> AddDog(const std::string &name, bool randomize_spawn = false)
    {
        model::Position pos = randomize_spawn
                                  ? GetRandomPositionOnRoad(*map_)
                                  : model::Position{static_cast<double>(map_->GetRoads().front().GetStart().x),
                                                    static_cast<double>(map_->GetRoads().front().GetStart().y)};
        auto dog = std::make_shared<Dog>(next_dog_id_++, name, pos);
        dogs_.emplace_back(dog);
        return dog;
    }

    struct LostObject
    {
        int id;
        int type;
        int value = 0;
        model::Position pos;
    };
    GameSession(model::Map *map,
                std::vector<std::shared_ptr<Dog>> dogs,
                int next_dog_id,
                int next_loot_id,
                std::unordered_map<int, LostObject> lost_objects)
        : map_(map), dogs_(std::move(dogs)), next_dog_id_(next_dog_id), next_loot_id_(next_loot_id), lost_objects_(std::move(lost_objects)) {}
    void AddRandomLoot(int count, const std::list<model::Road> &roads, int loot_type_count, const boost::json::array &loot_types)
    {
        for (int i = 0; i < count; ++i)
        {
            LostObject obj;
            obj.id = next_loot_id_++;
            obj.type = rand() % loot_type_count;
            obj.pos = GetRandomPositionOnRoad(*map_);
            obj.value = loot_types[obj.type].as_object().at("value").as_int64();
            lost_objects_[obj.id] = obj;
        }
    }
    int GetNextDogId() const
    {
        return next_dog_id_;
    }
    int GetNextLootId() const
    {
        return next_loot_id_;
    }
    const std::unordered_map<int, LostObject> &GetLostObjects() const { return lost_objects_; }
    std::unordered_map<int, LostObject> &AccessLostObjects() { return lost_objects_; }

    const std::vector<std::shared_ptr<Dog>> &GetDogs() const { return dogs_; }
    std::vector<std::shared_ptr<Dog>> &AccessDogs() { return dogs_; }

    SessionGathererProvider GetGathererProvider(const std::vector<model::Position> &starts,
                                                const std::vector<model::Position> &ends) const
    {
        return SessionGathererProvider(*this, starts, ends);
    }

private:
    model::Map *map_;
    std::vector<std::shared_ptr<Dog>> dogs_;
    int next_dog_id_ = 0;
    int next_loot_id_ = 0;
    std::unordered_map<int, LostObject> lost_objects_;

    class SessionGathererProvider : public collision_detector::ItemGathererProvider
    {
    public:
        SessionGathererProvider(const GameSession &session,
                                const std::vector<model::Position> &starts,
                                const std::vector<model::Position> &ends)
            : session_(session), starts_(starts), ends_(ends) {}

        size_t ItemsCount() const override
        {
            return session_.lost_objects_.size();
        }

        collision_detector::Item GetItem(size_t idx) const override
        {
            auto it = session_.lost_objects_.begin();
            std::advance(it, idx);
            return {{it->second.pos.x, it->second.pos.y}, 0.0};
        }

        size_t GatherersCount() const override
        {
            return starts_.size();
        }

        collision_detector::Gatherer GetGatherer(size_t idx) const override
        {
            return {{starts_[idx].x, starts_[idx].y},
                    {ends_[idx].x, ends_[idx].y},
                    0.6};
        }

    private:
        const GameSession &session_;
        const std::vector<model::Position> &starts_;
        const std::vector<model::Position> &ends_;
    };
};

inline void Dog::UpdatePosition(int ms, GameSession *session)
{
    const double dt = std::chrono::duration<double>(std::chrono::milliseconds(ms)).count();

    if (speed_.x == 0 && speed_.y == 0)
    {
        AddIdleTime(dt);
        return;
    }

    model::Position start = position_;
    model::Position attempted = {position_.x + speed_.x * dt, position_.y + speed_.y * dt};
    model::Position new_pos = session->GetMap()->FitPositionToRoad(position_, attempted);

    const double dx = new_pos.x - start.x;
    const double dy = new_pos.y - start.y;
    const double distance_moved = std::sqrt(dx * dx + dy * dy);
    const double speed_magnitude = std::sqrt(speed_.x * speed_.x + speed_.y * speed_.y);

    if (distance_moved > 0.0 && speed_magnitude > 0.0)
    {
        const double active_time = distance_moved / speed_magnitude;
        const double idle_time = std::max(0.0, dt - active_time);
        AddIdleTime(idle_time);
    }
    else
    {
        AddIdleTime(dt);
    }

    position_ = new_pos;

    if (distance_moved == speed_magnitude * dt)
    {
        current_idle_time_ = 0.0;
    }

    std::vector<model::Position> starts = {start};
    std::vector<model::Position> ends = {position_};
    auto provider = session->GetGathererProvider(starts, ends);
    auto events = collision_detector::FindGatherEvents(provider);

    std::set<size_t> picked_items;
    for (const auto &evt : events)
    {
        if (evt.gatherer_id != 0)
            continue;
        auto it = session->AccessLostObjects().begin();
        std::advance(it, evt.item_id);
        if (CanPickUp())
        {
            PickUpItem(it->second.id, it->second.type, it->second.value);
            picked_items.insert(it->second.id);
        }
    }

    for (int id : picked_items)
    {
        session->AccessLostObjects().erase(id);
    }

    // проверка офиса
    for (const auto &office : session->GetMap()->GetOffices())
    {
        const auto &pos = office.GetPosition();
        double dx = static_cast<double>(pos.x) - position_.x;
        double dy = static_cast<double>(pos.y) - position_.y;
        if (dx * dx + dy * dy <= 0.55 * 0.55)
        {
            ClearBag();
            break;
        }
    }
    AddLifeTime(dt);
}

class Player
{
public:
    Player(std::shared_ptr<GameSession> session, std::shared_ptr<Dog> dog)
        : session_(std::move(session)), dog_(std::move(dog)),
          generator1_(std::random_device{}()), generator2_(std::random_device{}())
    {
        token_ = GenerateToken();
    }
    explicit Player(std::shared_ptr<GameSession> session,
                    std::shared_ptr<Dog> dog,
                    Token token)
        : session_(std::move(session)), dog_(std::move(dog)), token_(std::move(token)), generator1_(std::random_device{}()), generator2_(std::random_device{}()) {}
    std::shared_ptr<GameSession> GetSession() const { return session_; }
    std::optional<Token> GetToken() const { return token_; }
    std::shared_ptr<Dog> GetDog() const { return dog_; }

private:
    std::shared_ptr<GameSession> session_;
    std::shared_ptr<Dog> dog_;
    std::optional<Token> token_;
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

class Players
{
public:
    Player &AddPlayer(std::shared_ptr<GameSession> session, std::shared_ptr<Dog> dog)
    {
        players_.emplace_back(std::make_unique<Player>(std::move(session), std::move(dog)));
        return *players_.back();
    }
    Player &AddPlayer(std::unique_ptr<Player> player)
    {
        players_.emplace_back(std::move(player));
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
    void RemoveByToken(const Token &token)
    {
        players_.erase(
            std::remove_if(players_.begin(), players_.end(),
                           [&](const std::unique_ptr<Player> &p)
                           {
                               return p->GetToken().has_value() && p->GetToken().value() == token;
                           }),
            players_.end());
    }

private:
    std::vector<std::unique_ptr<Player>> players_;
};
