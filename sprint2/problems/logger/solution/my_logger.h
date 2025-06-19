#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <filesystem>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard lock(mutex_);
        manual_ts_ = ts;
        UpdateLogFile();  // на случай смены даты
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        std::ostringstream message;
        {
            std::lock_guard lock(mutex_);
            UpdateLogFile();
            message << GetTimeStamp() << ": ";
            (message << ... << args);  // вариадик вывод
            message << '\n';
            log_file_ << message.str();
            log_file_.flush();
        }
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    mutable std::mutex mutex_;
    std::ofstream log_file_;
    std::string current_log_date_;

    std::chrono::system_clock::time_point GetTime() const {
        return manual_ts_.value_or(std::chrono::system_clock::now());
    }

    std::string GetTimeStamp() const {
        const auto t_c = std::chrono::system_clock::to_time_t(GetTime());
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t_c), "%F %T");
        return oss.str();
    }

    std::string GetFileDate() const {
        const auto t_c = std::chrono::system_clock::to_time_t(GetTime());
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t_c), "%Y_%m_%d");
        return oss.str();
    }

    void UpdateLogFile() {
        const std::string new_date = GetFileDate();
        if (new_date == current_log_date_) return;

        current_log_date_ = new_date;
        std::string path = "/var/log/sample_log_" + new_date + ".log";

        if (log_file_.is_open()) {
            log_file_.close();
        }

        log_file_.open(path, std::ios::app);
    }
};
