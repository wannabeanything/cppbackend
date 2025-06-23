// game_session.h
#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "dog.h"

namespace game {

class Map;  // forward declaration

class GameSession {
public:
    GameSession(Map* map) : map_{map} {}

    Dog* AddDog(const std::string& name);

    Map* GetMap() const { return map_; }
    const std::unordered_map<std::uint32_t, std::unique_ptr<Dog>>& GetDogs() const {
        return dogs_;
    }

private:
    Map* map_;
    std::unordered_map<std::uint32_t, std::unique_ptr<Dog>> dogs_;
    std::uint32_t next_dog_id_ = 0;
};

}  // namespace game
