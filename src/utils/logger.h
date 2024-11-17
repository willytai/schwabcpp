#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "spdlog/spdlog.h"
#include <memory>

namespace schwabcpp {

class Logger
{
public:
    Logger() = default;
    ~Logger() = default;

    // creates a new logger
    // default turns off trace
    static void init(spdlog::level::level_enum logLevel = spdlog::level::debug);

    // use an exiting one
    static void setLogger(std::shared_ptr<spdlog::logger> logger);

    // allow log level control
    static void setLogLevel(spdlog::level::level_enum logLevel);

    // provides api to release the underlying logger
    static void releaseLogger();

    static inline std::shared_ptr<spdlog::logger> getLogger() { return __logger; }

private:
    static std::shared_ptr<spdlog::logger> __logger;
};


}

#define LOG_TRACE(...) ::schwabcpp::Logger::getLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::schwabcpp::Logger::getLogger()->debug(__VA_ARGS__)
#define LOG_WARN(...)  ::schwabcpp::Logger::getLogger()->warn(__VA_ARGS__)
#define LOG_INFO(...)  ::schwabcpp::Logger::getLogger()->info(__VA_ARGS__)
#define LOG_ERROR(...) ::schwabcpp::Logger::getLogger()->error(__VA_ARGS__)
#define LOG_FATAL(...) ::schwabcpp::Logger::getLogger()->critical(__VA_ARGS__); exit(1);

#define VERIFY(condition, ...) \
    if (!condition) { \
        LOG_WARN(__VA_ARGS__); \
    }

#define NOTIMPLEMENTEDERROR LOG_WARN("Not Implemented: {} @ {}:{}", __PRETTY_FUNCTION__, __FILE__, __LINE__)

#endif /* ifndef __LOGGER_H__ */
