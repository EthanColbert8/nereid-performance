#include "cli/args.h"
#include "process/control.h"
#include "nereid/service.h"
#include "benchmark/context.h"
#include "benchmark/runner.h"
#include "utils/errors.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include <nlohmann/json.hpp>

constexpr int STARTUP_TIMEOUT_SECONDS = 120;

bool WriteReport(const cli::Args& args, const nlohmann::json& report, std::string* error_message) {
    FILE* file = std::fopen(args.output_path, "w");
    if (file == nullptr) {
        utils::SetError(error_message, std::string("failed to open output file: ") + args.output_path);
        return false;
    }

    const std::string serialized = report.dump(4);
    const size_t written = std::fwrite(serialized.data(), 1, serialized.size(), file);
    std::fclose(file);

    if (written != serialized.size()) {
        utils::SetError(error_message, std::string("failed to write output file: ") + args.output_path);
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    cli::Args args = cli::ParseArgs(argc, argv);

    std::string error_message;
    benchmark::BenchmarkContext ctx;
    nlohmann::json report;

    process::ServerProcess server;
    server.pid = -1;
    server.owned = false;
    server.running = false;

    if (args.launch_server) {
        if (!process::LaunchServer(args.server_binary_path, &server, &error_message)) {
            std::fprintf(stderr, "Error: %s\n", error_message.c_str());
            return EXIT_FAILURE;
        }
        if (!nereid::WaitForServerReady(args.server_address, args.server_port, server.pid, STARTUP_TIMEOUT_SECONDS, &error_message)) {
            std::fprintf(stderr, "Error: %s\n", error_message.c_str());
            process::StopServer(server);
            return EXIT_FAILURE;
        }
    }
    else {
        if (!nereid::WaitForServerReady(args.server_address, args.server_port, STARTUP_TIMEOUT_SECONDS, &error_message)) {
            std::fprintf(stderr, "Error: %s\n", error_message.c_str());
            process::StopServer(server);
            return EXIT_FAILURE;
        }
    }

    if (!benchmark::BuildBenchmarkContext(args, &ctx, &error_message)) {
        std::fprintf(stderr, "Error: %s\n", error_message.c_str());
        process::StopServer(server);
        return EXIT_FAILURE;
    }

    if (!benchmark::RunBenchmark(ctx, &report, &error_message)) {
        std::fprintf(stderr, "Error: %s\n", error_message.c_str());
        process::StopServer(server);
        return EXIT_FAILURE;
    }

    if (!WriteReport(args, report, &error_message)) {
        std::fprintf(stderr, "Error: %s\n", error_message.c_str());
        process::StopServer(server);
        return EXIT_FAILURE;
    }

    process::StopServer(server);
    return EXIT_SUCCESS;
}
