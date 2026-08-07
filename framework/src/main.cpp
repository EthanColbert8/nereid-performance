#include <cstdio>
#include <cstdlib>
#include <string>
#include <nlohmann/json.hpp>

#include "process/control.h"
#include "benchmark/context.h"
#include "benchmark/runner.h"
#include "cli/args.h"

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
    BenchmarkContext ctx;
    nlohmann::json report;

    process::ServerProcess server = {};
    server.pid = -1;
    bool server_started = false;

    auto cleanup = [&]() {
        if (server_started) {
            process::StopServer(server);
            server_started = false;
        }
    };

    if (!process::LaunchServer(args.server_binary_path, &server, &error_message)) {
        std::fprintf(stderr, "Error: %s\n", error_message.c_str());
        cleanup(); // NOTE (Ethan): `server_started` is guaranteed to be false here?
        return EXIT_FAILURE;
    }
    server_started = true;

    // NOTE (Ethan): We wait for server readiness inside `BuildBenchmarkContext`
    if (!benchmark::BuildBenchmarkContext(args, &ctx, server.pid, &error_message)) {
        std::fprintf(stderr, "Error: %s\n", error_message.c_str());
        cleanup();
        return EXIT_FAILURE;
    }

    if (!benchmark::RunBenchmark(ctx, &report, &error_message)) {
        std::fprintf(stderr, "Error: %s\n", error_message.c_str());
        cleanup();
        return EXIT_FAILURE;
    }

    if (!WriteReport(args, report, &error_message)) {
        std::fprintf(stderr, "Error: %s\n", error_message.c_str());
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    return EXIT_SUCCESS;
}
