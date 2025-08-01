#include "objects.h"
#include "tagged.h"
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/utility.hpp> 
#include <boost/serialization/shared_ptr.hpp> 

namespace model {

template <typename Archive>
void serialize(Archive& ar, Position& vec, const unsigned /*version*/) {
    ar & vec.x;
    ar & vec.y;
}

template <typename Archive>
void serialize(Archive& ar, Point& point, const unsigned /*version*/) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, Map::Id& id, const unsigned /*version*/) {
    std::string raw;
    if constexpr (Archive::is_saving::value) {
        raw = *id;
    }
    ar & raw;
    if constexpr (Archive::is_loading::value) {
        id = Map::Id{std::move(raw)};
    }
}

} // namespace model

// --- внутри namespace util ---
namespace util {

template<typename Archive, typename Value, typename Tag>
void serialize(Archive& ar, Tagged<Value, Tag>& tag, const unsigned /*version*/) {
    Value tmp;
    if constexpr (Archive::is_saving::value) {
        tmp = *tag;
    }
    ar & tmp;
    if constexpr (Archive::is_loading::value) {
        tag = Tagged<Value, Tag>{std::move(tmp)};
    }
}

} // namespace util

template <typename Archive>
void serialize(Archive& ar, GameSession::LostObject& lost_object, const unsigned /*version*/) {
    ar & lost_object.id;
    ar & lost_object.type;
    ar & lost_object.value;
    ar & lost_object.pos;
}
class DogRepr{
public:
    DogRepr() = default;
    explicit DogRepr(const Dog& dog)
    : id_(dog.GetId())
    , appeared_name_(dog.GetName())
    , position_(dog.GetPosition())
    , bag_capacity_(dog.GetBagCapacity())
    , speed_(dog.GetSpeed())
    , direction_(dog.GetDirection())
    , score_(dog.GetScore())
    , inventory_(dog.GetBag()){}

    [[nodiscard]] std::shared_ptr<Dog> Restore() const {
        auto dog = std::make_shared<Dog>(id_, appeared_name_, position_);
        dog->SetBagCapacityForDog(bag_capacity_);
        dog->SetSpeed(speed_.x + speed_.y);
        dog->SetDirection(direction_);
        dog->SetScore(score_);
        for (const auto& item : inventory_) {
            dog->PickUpItem(item.first, item.second, 0);
        }
        return dog;
    }


    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& appeared_name_;
        ar& position_;
        ar& bag_capacity_;
        ar& speed_;
        ar& direction_;
        ar& score_;
        ar& inventory_;
    }
private:

    int id_;
    std::string appeared_name_;
    model::Position position_;
    model::Position speed_;
    Direction direction_;
    int bag_capacity_;
    std::vector<std::pair<int, int>> inventory_;
    int score_ = 0;
};
class SessionRepr{
public:
    SessionRepr() = default;

    explicit SessionRepr(const GameSession& session)
    : map_id_(session.GetMap()->GetId())
    , next_dog_id_(session.GetNextDogId())
    , next_loot_id_(session.GetNextLootId())
    , lost_objects_(session.GetLostObjects())
    {
        for (const auto& dog : session.GetDogs()) {
            dogs_.emplace_back(*dog);
        }
    }

    std::shared_ptr<GameSession> Restore(model::Map* map) const {
        auto session = std::make_shared<GameSession>(map, std::vector<std::shared_ptr<Dog>>{}, next_dog_id_, next_loot_id_, lost_objects_);
        auto& dst_dogs = session->AccessDogs();
        for (const auto& dog_repr : dogs_) {
            dst_dogs.emplace_back(dog_repr.Restore());
        }
        return session;
    }
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& map_id_;
        ar& dogs_;
        ar& next_dog_id_;
        ar& next_loot_id_;
        ar& lost_objects_;
    }
    const model::Map::Id& GetMapId()const{
        return map_id_;
    }
private:
    model::Map::Id map_id_;
    std::vector<DogRepr> dogs_;
    int next_dog_id_;
    int next_loot_id_;
    std::unordered_map<int, GameSession::LostObject> lost_objects_;
};


class PlayerRepr {
public:
    PlayerRepr()
        : token_(std::string{}),
        map_id_(std::string{})
    {}

    explicit PlayerRepr(const Player& player)
        : token_(*player.GetToken())
        , dog_id_(player.GetDog()->GetId())
        , map_id_(player.GetSession()->GetMap()->GetId()) {}

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar & token_;
        ar & dog_id_;
        ar & map_id_;
    }

    std::unique_ptr<Player> Restore(const std::shared_ptr<GameSession>& session) const {
        return std::make_unique<Player>(session, GetDog(session), token_);
    }
    std::shared_ptr<Dog> GetDog(const std::shared_ptr<GameSession>& session) const {
        for (const auto& d : session->GetDogs()) {
            if (d->GetId() == dog_id_) return d;
        }
        throw std::runtime_error("Dog not found for PlayerRepr");
    }
    const model::Map::Id& GetMapId() const { return map_id_; }
    int GetDogId() const { return dog_id_; }
    const Token& GetToken() const { return token_; }

private:
    Token token_;           // уникальный идентификатор игрока
    int dog_id_;            // ID собаки внутри сессии
    model::Map::Id map_id_;        // ID карты/сессии
};
struct SerializedState {
    std::vector<SessionRepr> sessions;
    std::vector<PlayerRepr> players;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar & sessions;
        ar & players;
    }
};

