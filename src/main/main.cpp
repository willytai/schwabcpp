#include "client.h"
#include "nlohmann/json.hpp"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <iostream>

// NOTE: <leader>uf toggles buffer auto foramtting

int main(int argc, char** argv) {

    schwabcpp::Client::LogLevel logLevel(schwabcpp::Client::LogLevel::Debug);
    if (argc > 1 && !strcmp(argv[1], "trace")) {
        logLevel = schwabcpp::Client::LogLevel::Trace;
    }

    // custom logger
    spdlog::init_thread_pool(8192, 2);
    std::vector<spdlog::sink_ptr> sinks = {
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>("test.log", 1024 * 1024 * 5, 10),
    };
    auto logger = std::make_shared<spdlog::async_logger>("native discord bot", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    spdlog::register_logger(logger);
    logger->set_pattern("%^[%Y-%m-%d %X] [%L] %v%$");
    logger->set_level(spdlog::level::level_enum::debug);

    {
        schwabcpp::Client client({
            .logger = logger,
            .logLevel = logLevel,
        });

        auto info = client.accountSummary();

        std::cout << info.summary["49339068"].aggregatedBalance.liquidationValue << std::endl;
        std::cout << info.summary["49339068"].aggregatedBalance.currentLiquidationValue << std::endl;
        std::cout << info.summary["49339068"].securitiesAccount.currentBalances.unsettledCash << std::endl;
        std::cout << info.summary["49339068"].securitiesAccount.isDayTrader << std::endl;

        // std::cout << info.dump() << std::endl;

        client.startStreamer();

        std::this_thread::sleep_for(std::chrono::seconds(10));

        // TEST: testing pause resume
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            client.pauseStreamer();
            std::this_thread::sleep_for(std::chrono::seconds(5));
            client.resumeStreamer();
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));

        client.stopStreamer();

        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    logger->info("Program exited normally.");

    return 0;
}
