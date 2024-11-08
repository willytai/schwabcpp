#include "client.h"
#include <iostream>

// NOTE: <leader>uf toggles buffer auto foramtting

int main(int argc, char** argv) {

    schwabcpp::Client::LogLevel logLevel(schwabcpp::Client::LogLevel::Debug);
    if (argc > 1 && !strcmp(argv[1], "trace")) {
        logLevel = schwabcpp::Client::LogLevel::Trace;
    }

    {
        schwabcpp::Client client(logLevel);
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

    std::cout << "Program exited normally" << std::endl;

    return 0;
}
