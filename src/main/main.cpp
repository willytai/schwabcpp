#include "client.h"
#include "nlohmann/json.hpp"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include <fstream>

// NOTE: <leader>uf toggles buffer auto foramtting

int main(int argc, char** argv) {

    using json = nlohmann::json;

    std::string key;
    std::string secret;
    std::filesystem::path appCredentialPath("./.appCredentials.json");
    if (std::filesystem::exists(appCredentialPath)) {
        std::ifstream file(appCredentialPath);
        if (file.is_open()) {
            json credentialData;
            file >> credentialData;

            if (credentialData.contains("app_key") &&
                credentialData.contains("app_secret")) {
                key = credentialData["app_key"];
                secret = credentialData["app_secret"];
            } else {
                std::cerr << "App credentials missing!!" << std::endl;
                exit(1);
            }
        } else {
            std::cerr << "Unable to open the app credentials file: {}" << appCredentialPath << std::endl;
            exit(1);
        }
    } else {
        std::cerr << "App credentials file: {} not found. Did you specify the right path?" << appCredentialPath << std::endl;
        exit(1);
    }

    spdlog::level::level_enum logLevel(spdlog::level::debug);
    if (argc > 1 && !strcmp(argv[1], "trace")) {
        logLevel = spdlog::level::trace;
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
    logger->set_level(logLevel);

    {
        schwabcpp::Client client(
            key,
            secret,
            logger
        );

        client.connect();

        auto info = client.accountSummary(client.getLinkedAccounts().back());

        std::cout << info.aggregatedBalance.liquidationValue << std::endl;
        std::cout << info.aggregatedBalance.currentLiquidationValue << std::endl;
        std::cout << info.securitiesAccount.currentBalances.unsettledCash << std::endl;
        std::cout << info.securitiesAccount.isDayTrader << std::endl;

        // std::cout << info.dump() << std::endl;

        // std::this_thread::sleep_for(std::chrono::seconds(5));

        client.subscribeLevelOneEquities(
            {
                "SCHD",
                "RKLB",
            },
            {
                // schwabcpp::StreamerField::LevelOneEquity::Symbol,
                schwabcpp::StreamerField::LevelOneEquity::LastPrice,
                schwabcpp::StreamerField::LevelOneEquity::OpenPrice,
                schwabcpp::StreamerField::LevelOneEquity::ClosePrice,
            }
        );

        client.startStreamer();

        // TEST: testing pause resume
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            client.pauseStreamer();
            std::this_thread::sleep_for(std::chrono::seconds(5));
            client.resumeStreamer();
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
        // std::this_thread::sleep_for(std::chrono::minutes(3));

        client.stopStreamer();

        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    logger->info("Program exited normally.");

    return 0;
}
