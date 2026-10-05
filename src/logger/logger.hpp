#pragma once

#include <mutex>
#include <ostream>
#include <string>
#include <string_view>

namespace netbucket {

enum class LogLevel {
    DEBUG   = 0,
    INFO    = 1,
    WARNING = 2,
    ERROR   = 3,
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    void set_output(std::ostream& out);

    void log(LogLevel level, std::string_view component, std::string_view message);

    void debug  (std::string_view comp, std::string_view msg) { log(LogLevel::DEBUG,   comp, msg); }
    void info   (std::string_view comp, std::string_view msg) { log(LogLevel::INFO,    comp, msg); }
    void warning(std::string_view comp, std::string_view msg) { log(LogLevel::WARNING, comp, msg); }
    void error  (std::string_view comp, std::string_view msg) { log(LogLevel::ERROR,   comp, msg); }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    mutable std::mutex mutex_;
    LogLevel           min_level_ = LogLevel::INFO;
    std::ostream*      out_       = nullptr;
};

#define NB_LOG_DEBUG(comp, msg)   ::netbucket::Logger::instance().debug(comp, msg)
#define NB_LOG_INFO(comp, msg)    ::netbucket::Logger::instance().info(comp, msg)
#define NB_LOG_WARN(comp, msg)    ::netbucket::Logger::instance().warning(comp, msg)
#define NB_LOG_ERROR(comp, msg)   ::netbucket::Logger::instance().error(comp, msg)

}
