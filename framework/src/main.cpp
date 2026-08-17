#include "config/settings.h"
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
    config::Settings settings;
    config::LoadProgramSettings(settings, argc, argv);

    logging::Logger logger(settings.log_file, settings.verbose ? logging::DEBUG : logging::INFO, LOG_BUFFER_SIZE_BYTES);

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

    if (settings.launch_server) {
        logger.info("Launching server binary \"%s\"", settings.server_binary_path.c_str());
        if (!process::LaunchServer(settings.server_binary_path.c_str(), server, logger)) {
            return EXIT_FAILURE;
        }
        if (!nereid::WaitForServerReady(settings.server_address.c_str(), settings.server_port, server.pid, STARTUP_TIMEOUT_SECONDS, logger)) {
            process::StopServer(server, logger);
            return EXIT_FAILURE;
        }
    }
    else {
        if (!nereid::WaitForServerReady(settings.server_address.c_str(), settings.server_port, STARTUP_TIMEOUT_SECONDS, logger)) {
            process::StopServer(server, logger);
            return EXIT_FAILURE;
        }

        // TODO (Ethan): check if process exists and set `server.pid` if so
        if (settings.server_pid > 0) {
            logger.error("NOT IMPLEMENTED: process monitoring for external processes (server PID %d given)", settings.server_pid);
        }
    }
    logger.info("Server found ready at address \"%s:%d\"", settings.server_address.c_str(), settings.server_port);

    if (!benchmark::BuildBenchmarkContext(settings, &ctx, logger)) {
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
        if (WriteReport(settings.hardware_metrics_output_path.c_str(), hardware_metrics_report, logger)) {
            logger.info("Hardware metrics report written to \"%s\"", settings.hardware_metrics_output_path.c_str());
        }
    }

    if (!WriteReport(settings.output_path.c_str(), report, logger)) {
        process::StopServer(server, logger);
        return EXIT_FAILURE;
    }

    logger.info("Benchmark finshed. Report written to \"%s\"", settings.output_path.c_str());
    process::StopServer(server, logger);
    return EXIT_SUCCESS;
}
