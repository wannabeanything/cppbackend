
#pragma once
#include "model.h"
#include <random>
#include <string>
#include <sstream>
#include <optional>
#include <iomanip>
#include <memory>

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

// Используется для скорости и координат
struct Vec2D
{
    double x;
    double y;
};

class GameSession;

class Dog
{
public:
    Dog(int id, const std::string &name, Vec2D pos = {0.0, 0.0})
        : id_(id), appeared_name_(name), position_(pos), speed_({0.0, 0.0}), direction_(Direction::NORTH) {}

    int GetId() const { return id_; }
    const std::string &GetName() const { return appeared_name_; }
    const Vec2D &GetPosition() const { return position_; }
    const Vec2D &GetSpeed() const { return speed_; }
    Direction GetDirection() const { return direction_; }
    void SetSpeed(double x)
    {
        speed_ = {x, x};
    }
    void SetDirection(Direction dir)
    {
        direction_ = dir;
    }

private:
    int id_;
    std::string appeared_name_;
    Vec2D position_;
    Vec2D speed_;
    Direction direction_;
};
class Player
{
public:
    Player(std::shared_ptr<GameSession> session, Dog *dog)
        : session_(std::move(session)),
          dog_(dog),
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
    Dog *GetDog() const
    {
        return dog_;
    }

private:
    std::shared_ptr<GameSession> session_;
    Dog *dog_;
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

    Dog *AddDog(const std::string &name)
    {
        dogs_.emplace_back(next_dog_id_++, name);
        dogs_.back().SetSpeed(map_->GetSpeedForThisMap());
        return &dogs_.back();
    }

private:
    model::Map *map_;
    std::vector<Dog> dogs_;
    int next_dog_id_ = 0;
};
class Players
{
public:
    Player &AddPlayer(std::shared_ptr<GameSession> session, Dog *dog)
    {
        players_.emplace_back(std::make_unique<Player>(std::move(session), dog));
        return *players_.back();
    }

    Player *FindByDogIdAndMapId(int dog_id, const model::Map::Id map_id) const
    {
        for (const auto &player : players_)
        {
            if (player->GetDog()->GetId() == dog_id &&
                player->GetSession()->GetMap()->GetId() == map_id)
            {
                return player.get(); // вернёт Player*
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
