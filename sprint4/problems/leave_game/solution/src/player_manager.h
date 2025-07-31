#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <algorithm>
#include <optional>
#include "objects.h"

class PlayerManager {
public:
    explicit PlayerManager(model::Game& game)
        : game_{game} {
    }
    /*
    void AddPlayer(std::unique_ptr<Player> player) {
        std::unique_lock lock(players_mutex_);
        players_.emplace_back(std::move(player));
    }*/
    Player &AddPlayer(std::shared_ptr<GameSession> session, std::shared_ptr<Dog> dog)
    {
        std::unique_lock lock(players_mutex_);
        players_.emplace_back(std::make_unique<Player>(std::move(session), std::move(dog)));
        return *players_.back();
    }
    Player &AddPlayer(std::unique_ptr<Player> player)
    {
        std::unique_lock lock(players_mutex_);
        players_.emplace_back(std::move(player));
        return *players_.back();
    }
    const std::vector<std::unique_ptr<Player>> &GetPlayers() const
    {
        return players_;
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
    void RemoveByToken(const Token& token) {
        std::unique_lock lock(players_mutex_);
        players_.erase(
            std::remove_if(players_.begin(), players_.end(),
                           [&](const std::unique_ptr<Player>& p) {
                               if (p->GetToken().has_value() && p->GetToken().value() == token) {
                                   auto session = p->GetSession();
                                   auto dog = p->GetDog();
                                   if (session) {
                                       auto& dogs = session->AccessDogs();
                                       dogs.erase(std::remove_if(dogs.begin(), dogs.end(),
                                                                 [&](auto& d) { return d.get() == dog.get(); }),
                                                  dogs.end());
                                   }
                                   return true;
                               }
                               return false;
                           }),
            players_.end());
    }

    template <typename Fn>
    void ForEachPlayer(Fn&& fn) const {
        std::shared_lock lock(players_mutex_);
        for (const auto& player : players_) {
            fn(*player);
        }
    }

private:
    model::Game& game_;
    std::vector<std::unique_ptr<Player>> players_;
    mutable std::shared_mutex players_mutex_;
};
