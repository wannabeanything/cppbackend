#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "connection_pool.h"

namespace database {

struct Record {
    std::string name;
    int score = 0;
    double play_time = 0.0;
};

class RecordRepository {
public:
    explicit RecordRepository(std::shared_ptr<ConnectionPool> pool);

    void SaveRecord(const std::string& name, int score, double play_time);
    std::vector<Record> GetRecords(size_t start = 0, size_t max_items = 100);

private:
    void EnsureTableExists();

    std::shared_ptr<ConnectionPool> pool_;

    mutable std::once_flag init_flag_;
    mutable bool initialized_ = false;
};

}  // namespace database
