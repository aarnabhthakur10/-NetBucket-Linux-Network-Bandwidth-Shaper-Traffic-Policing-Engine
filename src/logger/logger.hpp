// src/logger/logger.hpp
//
// NetBucket — Structured Logger
//
// A minimal, thread-safe logger that writes to stdout/stderr.
// We intentionally keep this simple (no spdlog dependency yet) so the
// project compiles with zero external dependencies beyond GoogleTest.
//
// Log levels:
//   DEBUG   — detailed tracing, disabled by default (too verbose)
//   INFO    — normal operational events (startup, config change)
//   WARNING — unexpected but recoverable events
//   ERROR   — failures that need operator attention

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
    // Singleton — the ONE acceptable singleton in this project.
    // Rationale: logging must be accessible from every component without
    // threading a reference through every constructor.
    static Logger& instance();

    void set_level(LogLevel level);
    void set_output(std::ostream& out);   ///< Default: std::cout

    void log(LogLevel level, std::string_view component, std::string_view message);

    // Convenience helpers
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
    std::ostream*      out_       = nullptr;   // set lazily to avoid static-init order issues
};

//  Convenience macros 

#define NB_LOG_DEBUG(comp, msg)   ::netbucket::Logger::instance().debug(comp, msg)
#define NB_LOG_INFO(comp, msg)    ::netbucket::Logger::instance().info(comp, msg)
#define NB_LOG_WARN(comp, msg)    ::netbucket::Logger::instance().warning(comp, msg)
#define NB_LOG_ERROR(comp, msg)   ::netbucket::Logger::instance().error(comp, msg)

} // namespace netbucket
