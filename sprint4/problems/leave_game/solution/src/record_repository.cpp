#include "record_repository.h"
#include <pqxx/pqxx>

namespace database {

using namespace std::literals;

RecordRepository::RecordRepository(std::shared_ptr<ConnectionPool> pool)
    : pool_(std::move(pool)) {
    EnsureTableExists();
}

void RecordRepository::EnsureTableExists() {
    auto conn = pool_->GetConnection();
    pqxx::work tx(*conn);
    tx.exec(R"(
        CREATE TABLE IF NOT EXISTS retired_players (
            id SERIAL PRIMARY KEY,
            name TEXT NOT NULL,
            score INTEGER NOT NULL,
            play_time DOUBLE PRECISION NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_score ON retired_players(score DESC);
        CREATE INDEX IF NOT EXISTS idx_play_time ON retired_players(play_time ASC);
        CREATE INDEX IF NOT EXISTS idx_name ON retired_players(name ASC);
    )");
    tx.commit();
    initialized_ = true;
}

void RecordRepository::SaveRecord(const std::string& name, int score, double play_time) {
    auto conn = pool_->GetConnection();
    pqxx::work tx(*conn);
    tx.exec_params(R"(
        INSERT INTO retired_players (name, score, play_time)
        VALUES ($1, $2, $3)
    )", name, score, play_time);
    tx.commit();
}

std::vector<Record> RecordRepository::GetRecords(size_t start, size_t max_items) {
    if (max_items > 100) {
        throw std::invalid_argument("max_items cannot exceed 100");
    }

    auto conn = pool_->GetConnection();
    pqxx::read_transaction tx(*conn);
    auto res = tx.exec_params(R"(
        SELECT name, score, play_time
        FROM retired_players
        ORDER BY score DESC, play_time ASC, name ASC
        OFFSET $1 LIMIT $2
    )", static_cast<int64_t>(start), static_cast<int64_t>(max_items));

    std::vector<Record> records;
    records.reserve(res.size());
    for (const auto& row : res) {
        records.push_back({
            row["name"].as<std::string>(),
            row["score"].as<int>(),
            row["play_time"].as<double>()
        });
    }

    return records;
}

}  // namespace database