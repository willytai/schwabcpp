#include "app.h"
#include "utils/logger.h"

// NOTE: <leader>uf toggles buffer auto foramtting

int main(int argc, char** argv) {

    // initailize the logger at startup
    spdlog::level::level_enum logLevel(spdlog::level::debug);

    if (argc > 1 && !strcmp(argv[1], "trace")) {
        logLevel = spdlog::level::trace;
    }

    l2viz::Logger::init(logLevel);

    {
        // start the app
        l2viz::Level2Visualizer app;
        app.run();
    }

    LOG_INFO("Program exited normally.");

    return 0;
}
