#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    // Проверим, что перемещение ненулевое.
    // Тут приходится использовать строгое равенство, а не приближённое,
    // пскольку при сборе заказов придётся учитывать перемещение даже на небольшое
    // расстояние.
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult{sq_distance, proj_ratio};
}

// В задании на разработку тестов реализовывать следующую функцию не нужно -
// она будет линковаться извне.
std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> result;

    const size_t gatherers_count = provider.GatherersCount();
    const size_t items_count = provider.ItemsCount();

    for (size_t g_id = 0; g_id < gatherers_count; ++g_id) {
        const Gatherer& g = provider.GetGatherer(g_id);

        // Пропускаем неподвижных
        if (std::abs(g.start_pos.x - g.end_pos.x) < 1e-10 &&
            std::abs(g.start_pos.y - g.end_pos.y) < 1e-10)
            continue;

        for (size_t i_id = 0; i_id < items_count; ++i_id) {
            const Item& item = provider.GetItem(i_id);

            const auto [sq_dist, proj_ratio] = TryCollectPoint(g.start_pos, g.end_pos, item.position);

            if (proj_ratio < 0.0 || proj_ratio > 1.0) {
                continue;
            }

            const double max_dist = g.width + item.width;
            if (sq_dist <= max_dist * max_dist) {
                result.push_back({i_id, g_id, sq_dist, proj_ratio});
            }
        }
    }

    // Хронологическая сортировка
    std::sort(result.begin(), result.end(), [](const GatheringEvent& a, const GatheringEvent& b) {
        return a.time < b.time;
    });

    return result;
}

}  // namespace collision_detector
