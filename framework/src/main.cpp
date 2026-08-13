#include "cli/args.h"
#include "logging/logger.h"
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
constexpr size_t LOG_BUFFER_SIZE_BYTES = 8192;

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

    logging::Logger logger(stderr, logging::INFO, LOG_BUFFER_SIZE_BYTES);

    std::string error_message;
    benchmark::BenchmarkContext ctx;
    nlohmann::json report;

    process::ServerProcess server;
    server.pid = -1;
    server.owned = false;
    server.running = false;

    if (args.launch_server) {
        logger.info("Launching server binary \"%s\"", args.server_binary_path);
        if (!process::LaunchServer(args.server_binary_path, &server, &error_message)) {
            logger.error("%s", error_message.c_str());
            return EXIT_FAILURE;
        }
        if (!nereid::WaitForServerReady(args.server_address, args.server_port, server.pid, STARTUP_TIMEOUT_SECONDS, &error_message)) {
            logger.error("%s", error_message.c_str());
            process::StopServer(server);
            return EXIT_FAILURE;
        }
    }
    else {
        if (!nereid::WaitForServerReady(args.server_address, args.server_port, STARTUP_TIMEOUT_SECONDS, &error_message)) {
            logger.error("%s", error_message.c_str());
            process::StopServer(server);
            return EXIT_FAILURE;
        }
    }
    logger.info("Server found ready at address \"%s:%s\"", args.server_address, args.server_port);

    if (!benchmark::BuildBenchmarkContext(args, &ctx, &logger, &error_message)) {
        logger.error("%s", error_message.c_str());
        process::StopServer(server);
        return EXIT_FAILURE;
    }

    logger.info("Beginning benchmark");
    if (!benchmark::RunBenchmark(ctx, &report, &error_message)) {
        logger.error("%s", error_message.c_str());
        process::StopServer(server);
        return EXIT_FAILURE;
    }

    if (!WriteReport(args, report, &error_message)) {
        logger.error("%s", error_message.c_str());
        process::StopServer(server);
        return EXIT_FAILURE;
    }

    logger.info("Benchmark finshed. Report written to \"%s\"", args.output_path);
    process::StopServer(server);
    return EXIT_SUCCESS;
}
