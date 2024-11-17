
#include "logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace schwabcpp {

std::shared_ptr<spdlog::logger> Logger::__logger;

void Logger::init(spdlog::level::level_enum logLevel) {
    spdlog::set_pattern("%^[%n::%l] %v%$");
    __logger = spdlog::stdout_color_mt("schwabcpp");
    __logger->set_level(logLevel);
    __logger->debug("Logger Initialized.");
    __logger->flush_on(spdlog::level::debug);
}

void Logger::setLogLevel(spdlog::level::level_enum logLevel) {
    if (__logger) {
        __logger->set_level(logLevel);
    }
}

void Logger::setLogger(std::shared_ptr<spdlog::logger> logger) {
    __logger = logger;
}

void Logger::releaseLogger() {
    __logger.reset();
}

}
