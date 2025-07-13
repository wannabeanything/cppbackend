#include "extra_data.h"

namespace extra_data {

    void ExtraDataRepository::SetLootTypes(MapId id, boost::json::array loot_types) {
        loot_types_map_[std::move(id)] = std::move(loot_types);
    }

    const boost::json::array* ExtraDataRepository::GetLootTypes(MapId id) const {
        auto it = loot_types_map_.find(id);
        return it != loot_types_map_.end() ? &it->second : nullptr;
    }

    void ExtraDataRepository::SetLootGenerator(MapId id, loot_gen::LootGenerator generator) {
        loot_generators_.insert_or_assign(std::move(id), std::move(generator));
    }

    loot_gen::LootGenerator* ExtraDataRepository::GetLootGenerator(MapId id) {
        auto it = loot_generators_.find(id);
        return it != loot_generators_.end() ? &it->second : nullptr;
    }

    void ExtraDataRepository::Clear() {
        loot_types_map_.clear();
        loot_generators_.clear();
    }

}  // namespace extra_data