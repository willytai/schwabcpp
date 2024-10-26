#include "client.h"
#include "utils/logger.h"

// NOTE: <leader>uf toggles buffer auto foramtting

const std::string appKey = "cJVaFrezFW3OAQl7Y3ploFfEF9BoMCwz";
const std::string secret = "aO8pLCG9ZjDGOuH3";

int main(int argc, char *argv[]) {

    spdlog::level::level_enum logLevel(spdlog::level::debug);

    if (argc > 1 && !strcmp(argv[1], "trace")) {
        logLevel = spdlog::level::trace;
    }

    // initailize the logger at startup
    l2viz::Logger::init(logLevel);

    l2viz::Client client(appKey, secret);

    client.startStreamer();

    std::this_thread::sleep_for(std::chrono::seconds(5));
    // std::this_thread::sleep_for(std::chrono::minutes(1));

    client.stopStreamer();

    LOG_INFO("Terminating program now...");

    return 0;
}
