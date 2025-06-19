#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>
#include <filesystem>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    Logger() = default;
    Logger(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard lock(mutex_);

        const auto now = GetTime();
        const auto date_str = GetFileTimeStamp(now);

        if (!current_date_ || *current_date_ != date_str) {
            current_date_ = date_str;
            const std::string filename = "/var/log/sample_log_" + date_str + ".log";
            log_file_.close();
            std::filesystem::create_directories("/var/log");
            log_file_.open(filename, std::ios::app);
        }

        log_file_ << GetTimeStamp(now) << ": ";
        (log_file_ << ... << args) << std::endl;
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard lock(mutex_);
        manual_ts_ = ts;
    }

private:
    std::chrono::system_clock::time_point GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    std::string GetTimeStamp(std::chrono::system_clock::time_point tp) const {
        std::time_t t_c = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t_c), "%F %T");
        return oss.str();
    }

    std::string GetFileTimeStamp(std::chrono::system_clock::time_point tp) const {
        std::time_t t_c = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t_c), "%Y_%m_%d");
        return oss.str();
    }

    mutable std::mutex mutex_;
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::optional<std::string> current_date_;
    std::ofstream log_file_;
};