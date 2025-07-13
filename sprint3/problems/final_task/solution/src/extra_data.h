#pragma once

#include <string>
#include <unordered_map>
#include <boost/json/value.hpp>
#include <memory>
#include "loot_generator.h"
#include "model.h"

namespace extra_data {
    
    class ExtraDataRepository {
    public:
        using MapId = model::Map::Id;

        void SetLootTypes(MapId id, boost::json::array loot_types);
        const boost::json::array* GetLootTypes(MapId id) const;

        void SetLootGenerator(MapId id, loot_gen::LootGenerator generator);
        loot_gen::LootGenerator* GetLootGenerator(MapId id);

        void Clear();

    private:
        std::unordered_map<MapId, boost::json::array, util::TaggedHasher<MapId>> loot_types_map_;
        std::unordered_map<MapId, loot_gen::LootGenerator, util::TaggedHasher<MapId>> loot_generators_;
    };
    inline ExtraDataRepository& GetInstance() {
        static ExtraDataRepository instance;
        return instance;
    }
}  // namespace extra_data