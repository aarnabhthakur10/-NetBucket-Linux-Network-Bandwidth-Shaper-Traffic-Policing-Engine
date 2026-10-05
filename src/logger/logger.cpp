#include "logger/logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace netbucket {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard lock{mutex_};
    min_level_ = level;
}

void Logger::set_output(std::ostream& out) {
    std::lock_guard lock{mutex_};
    out_ = &out;
}

void Logger::log(LogLevel level, std::string_view component, std::string_view message) {
    if (level < min_level_) return;

    const auto now = std::chrono::system_clock::now();
    const auto now_t = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
    localtime_r(&now_t, &tm_buf);

    const char* level_str = "INFO ";
    switch (level) {
        case LogLevel::DEBUG:   level_str = "DEBUG"; break;
        case LogLevel::INFO:    level_str = "INFO "; break;
        case LogLevel::WARNING: level_str = "WARN "; break;
        case LogLevel::ERROR:   level_str = "ERROR"; break;
    }

    std::ostringstream line;
    line << "["
         << std::put_time(&tm_buf, "%H:%M:%S")
         << "."
         << std::setw(3) << std::setfill('0') << ms.count()
         << "] "
         << "[" << level_str << "] "
         << "[" << component  << "] "
         << message
         << "\n";

    std::lock_guard lock{mutex_};
    std::ostream& stream = out_ ? *out_ : std::cout;
    stream << line.str();
    stream.flush();
}

}
