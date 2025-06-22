
#pragma once
#include "model.h"
#include <random>
#include <string>
#include <sstream>
#include <iomanip>

struct TokenTag
{
};
// Ник игрока - токен
using Token = util::Tagged<std::string, TokenTag>;

class GameSession;

class Dog
{
public:
    Dog(int id, const std::string &name) : id_(id), appeared_name_(name) {}

    int GetId() const
    {
        return id_;
    }
    const std::string &GetName() const
    {
        return appeared_name_;
    }

private:
    int id_;
    std::string appeared_name_;
};
class Player
{
public:
    Player(GameSession *session, Dog *dog) : session_(session), dog_(dog),
                                             generator1_(random_device_()), generator2_(random_device_()),
                                             token_(GenerateToken()) {}
    GameSession *GetSession() const
    {
        return session_;
    }
    Token GetToken() const
    {
        return token_;
    }
    Dog *GetDog() const
    {
        return dog_;
    }

private:
    GameSession *session_;
    Dog *dog_;
    Token token_;
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
    Player &AddPlayer(GameSession *session, Dog *dog)
    {
        players_.emplace_back(std::make_unique<Player>(session, dog));
        return *players_.back();
    }

    Player *FindByDogIdAndMapId(int dog_id, const model::Map::Id map_id)const
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

    Player *FindByToken(Token token)const
    {
        for (const auto &player : players_)
        {
            if (player->GetToken() == token)
                return player.get();
        }
        return nullptr;
    }

    int GetPlayerCount() const
    {
        return static_cast<int>(players_.size());
    }

private:
    std::vector<std::unique_ptr<Player>> players_;
};


