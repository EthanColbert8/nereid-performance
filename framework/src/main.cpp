#include "cli/args.h"
#include "logging/logger.h"
#include "process/control.h"
#include "process/hardware.h"
#include "nereid/service.h"
#include "benchmark/context.h"
#include "benchmark/runner.h"

#include <atomic>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <nlohmann/json.hpp>

constexpr int STARTUP_TIMEOUT_SECONDS = 120;
constexpr size_t LOG_BUFFER_SIZE_BYTES = 8192;

bool WriteReport(const char* path, const nlohmann::json& report, logging::Logger& logger) {
    FILE* file = std::fopen(path, "w");
    if (file == nullptr) {
        logger.error("failed to open output file: %s", path);
        return false;
    }

    const std::string serialized = report.dump(4);
    const size_t written = std::fwrite(serialized.data(), 1, serialized.size(), file);
    std::fclose(file);

    if (written != serialized.size()) {
        logger.error("failed to write output file: %s", path);
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    cli::Args args = cli::ParseArgs(argc, argv);
    logging::Logger logger(stderr, logging::INFO, LOG_BUFFER_SIZE_BYTES);

    benchmark::BenchmarkContext ctx;
    nlohmann::json report;

    // All the stuff needed for hardware metrics collection
    process::HardwareMetricsContext hardware_metrics_context;
    process::HardwareMetrics hardware_metrics;
    std::atomic_bool stop_hardware_metrics(false);
    std::thread hardware_metrics_thread;
    bool hardware_metrics_running = false;

    process::ServerProcess server;
    server.pid = -1;
    server.owned = false;
    server.running = false;

    if (args.launch_server) {
        logger.info("Launching server binary \"%s\"", args.server_binary_path);
        if (!process::LaunchServer(args.server_binary_path, server, logger)) {
            return EXIT_FAILURE;
        }
        if (!nereid::WaitForServerReady(args.server_address, args.server_port, server.pid, STARTUP_TIMEOUT_SECONDS, logger)) {
            process::StopServer(server, logger);
            return EXIT_FAILURE;
        }
    }
    else {
        if (!nereid::WaitForServerReady(args.server_address, args.server_port, STARTUP_TIMEOUT_SECONDS, logger)) {
            process::StopServer(server, logger);
            return EXIT_FAILURE;
        }
    }
    logger.info("Server found ready at address \"%s:%d\"", args.server_address, args.server_port);

    if (!benchmark::BuildBenchmarkContext(args, &ctx, logger)) {
        process::StopServer(server, logger);
        return EXIT_FAILURE;
    }

    // Collect hardware metrics for server process
    if (server.pid > 0) { // NOTE (Ethan): this condition will change when we allow a PID to be passed in
        hardware_metrics_context.interval = std::chrono::milliseconds(250); // TODO (Ethan): make this configurable
        hardware_metrics_context.server_pid = server.pid;

        logger.info("Collecting hardware metrics for server process (PID %d)", server.pid);

        hardware_metrics_thread = std::thread(
            process::ScrapeHardwareMetrics,
            std::ref(stop_hardware_metrics),
            hardware_metrics_context,
            std::ref(hardware_metrics)
        );
        hardware_metrics_running = true;
    }

    logger.info("Beginning benchmark");
    if (!benchmark::RunSingleClientBenchmark(ctx, &report, logger)) {
        process::StopServer(server, logger);
        return EXIT_FAILURE;
    }

    if (hardware_metrics_running) {
        stop_hardware_metrics.store(true);
        hardware_metrics_thread.join();
        hardware_metrics_running = false;

        nlohmann::json hardware_metrics_report = process::GenerateHardwareMetricsReport(hardware_metrics, logger);
        if (WriteReport(args.hardware_metrics_output_path, hardware_metrics_report, logger)) {
            logger.info("Hardware metrics report written to \"%s\"", args.hardware_metrics_output_path);
        }
    }

    if (!WriteReport(args.output_path, report, logger)) {
        process::StopServer(server, logger);
        return EXIT_FAILURE;
    }

    logger.info("Benchmark finshed. Report written to \"%s\"", args.output_path);
    process::StopServer(server, logger);
    return EXIT_SUCCESS;
}
