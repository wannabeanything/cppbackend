#pragma once

#include <pqxx/connection>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cassert>

class ConnectionPool {
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

public:
    class ConnectionWrapper {
    public:
        ConnectionWrapper(ConnectionPtr&& conn, ConnectionPool& pool) noexcept
            : conn_{std::move(conn)}, pool_{&pool} {}

        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;
        ConnectionWrapper(ConnectionWrapper&&) noexcept = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) noexcept = default;

        pqxx::connection& operator*() const noexcept { return *conn_; }
        pqxx::connection* operator->() const noexcept { return conn_.get(); }

        ~ConnectionWrapper() {
            if (conn_ && pool_) {
                pool_->ReturnConnection(std::move(conn_));
            }
        }

    private:
        ConnectionPtr conn_;
        ConnectionPool* pool_;
    };

    template <typename ConnectionFactory>
    ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory) {
        std::lock_guard lock{mutex_};
        for (size_t i = 0; i < capacity; ++i) {
            available_.push(connection_factory());
        }
    }

    ConnectionWrapper GetConnection() {
        std::unique_lock lock{mutex_};
        cond_var_.wait(lock, [this] {
            return !available_.empty();
        });

        auto conn = std::move(available_.front());
        available_.pop();
        return ConnectionWrapper(std::move(conn), *this);
    }

private:
    void ReturnConnection(ConnectionPtr&& conn) {
        {
            std::lock_guard lock{mutex_};
            available_.push(std::move(conn));
        }
        cond_var_.notify_one();
    }

    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::queue<ConnectionPtr> available_;
};
