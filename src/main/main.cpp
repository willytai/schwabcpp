#include "client.h"
#include "utils/logger.h"

// NOTE: <leader>uf toggles buffer auto foramtting

int main(int argc, char** argv) {

    // initailize the logger at startup
    spdlog::level::level_enum logLevel(spdlog::level::debug);

    if (argc > 1 && !strcmp(argv[1], "trace")) {
        logLevel = spdlog::level::trace;
    }

    schwabcpp::Logger::init(logLevel);

    {
        schwabcpp::Client client;
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
    }

    LOG_INFO("Program exited normally.");

    return 0;
}
