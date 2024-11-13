#include "client.h"
#include "nlohmann/json.hpp"
#include <iostream>

// NOTE: <leader>uf toggles buffer auto foramtting

int main(int argc, char** argv) {

    schwabcpp::Client::LogLevel logLevel(schwabcpp::Client::LogLevel::Debug);
    if (argc > 1 && !strcmp(argv[1], "trace")) {
        logLevel = schwabcpp::Client::LogLevel::Trace;
    }

    {
        schwabcpp::Client client(logLevel);

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

    std::cout << "Program exited normally" << std::endl;

    return 0;
}
