#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

// Напишите здесь тесты для функции collision_detector::FindGatherEvents
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <cmath>
#include <sstream>

using namespace std;
using namespace collision_detector;
using namespace geom;

namespace Catch {
template <>
struct StringMaker<GatheringEvent> {
    static std::string convert(GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << "," << value.time << ")";
        return tmp.str();
    }
};
}

// Тестовый провайдер
class TestProvider : public ItemGathererProvider {
public:
    vector<Item> items;
    vector<Gatherer> gatherers;

    size_t ItemsCount() const override {
        return items.size();
    }

    Item GetItem(size_t idx) const override {
        return items.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers.size();
    }

    Gatherer GetGatherer(size_t idx) const override {
        return gatherers.at(idx);
    }
};

// Утилита сравнения с допуском
bool IsClose(double a, double b, double eps = 1e-10) {
    return std::abs(a - b) < eps;
}

// Сравнение событий
bool EqualEvents(const GatheringEvent& a, const GatheringEvent& b) {
    return a.gatherer_id == b.gatherer_id &&
           a.item_id == b.item_id &&
           IsClose(a.sq_distance, b.sq_distance) &&
           IsClose(a.time, b.time);
}

// ============================== ТЕСТЫ ==============================

TEST_CASE("Basic collision detection") {
    TestProvider provider;
    provider.items = { {Point2D{1, 0}, 0.5} };
    provider.gatherers = { {Point2D{0, 0}, Point2D{2, 0}, 0.5} };

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    CHECK(events[0].gatherer_id == 0);
    CHECK(events[0].item_id == 0);
    CHECK(IsClose(events[0].sq_distance, 0.0));
    CHECK(IsClose(events[0].time, 0.5));
}

TEST_CASE("No collision if item too far") {
    TestProvider provider;
    provider.items = { {Point2D{1, 2}, 0.5} };
    provider.gatherers = { {Point2D{0, 0}, Point2D{2, 0}, 0.5} };

    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Multiple collisions, chronological order") {
    TestProvider provider;
    provider.items = {
        {Point2D{1, 0}, 0.5},
        {Point2D{2, 0}, 0.5},
    };
    provider.gatherers = { {Point2D{0, 0}, Point2D{3, 0}, 0.5} };

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 2);
    CHECK(events[0].time < events[1].time);
}

TEST_CASE("Collision with stationary gatherer") {
    TestProvider provider;
    provider.items = { {Point2D{1, 0}, 0.5} };
    provider.gatherers = { {Point2D{1, 0}, Point2D{1, 0}, 0.5} };

    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}
