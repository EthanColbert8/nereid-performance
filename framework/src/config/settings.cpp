#include "config/settings.h"
#include "config/CLI11.hpp" // vendored in from https://github.com/CLIUtils/CLI11

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <sys/types.h>
#include <vector>
#include <string>

#include <yaml-cpp/yaml.h>

namespace config {

    constexpr const char* const DEFAULT_OUTPUT_PATH = "nereid_benchmark_summary.json";
    constexpr const char* const DEFAULT_HARDWARE_OUTPUT_PATH = "nereid_hardware_util.json";
    constexpr const char* const DEFAULT_SERVER_BINARY_PATH = "./nereid_server";
    constexpr const char* const DEFAULT_SERVER_ADDRESS = "localhost";
    constexpr int DEFAULT_SERVER_PORT = 50051;
    constexpr int DEFAULT_NUM_TRIALS = 100;

    CliArgs ParseCliArgs(int argc, char* argv[]) {
        CliArgs args;

        CLI::App app{"nereid-bench: a benchmarking tool for Nereid"};

        std::string config_path;
        auto* config_opt = app.add_option("config", config_path, "path to YAML config file")
            ->check(CLI::ExistingFile);

        std::string log_path;
        auto* log_opt = app.add_option("--log", log_path, "path to log file (default is stderr)");

        std::string output_path;
        auto* out_opt = app.add_option(
            "-o,--out", output_path,
            "path to write output summary (default: " +  std::string(DEFAULT_OUTPUT_PATH) + ")"
        );

        std::string hardware_metrics_output_path;
        auto* hw_out_opt = app.add_option(
            "--hw-out", hardware_metrics_output_path,
            "path to write hardware metrics output (default: " + std::string(DEFAULT_HARDWARE_OUTPUT_PATH) + ")"
        );

        std::string server_binary_path;
        auto* server_binary_opt = app.add_option(
            "--server-binary", server_binary_path,
            "path to the Nereid server binary (default: " + std::string(DEFAULT_SERVER_BINARY_PATH) + ")"
        );

        std::string server_address;
        auto* address_opt = app.add_option(
            "--address", server_address,
            "address to connect to Nereid server (default: " + std::string(DEFAULT_SERVER_ADDRESS) + ")"
        );

        int server_port;
        auto* port_opt = app.add_option(
            "--port", server_port,
            "port to connect to Nereid server (default: " + std::to_string(DEFAULT_SERVER_PORT) + ")"
        );

        pid_t server_pid;
        auto* pid_opt = app.add_option(
            "--server-pid", server_pid,
            "PID of server instance to allow for monitoring of process metrics if not launched"
        );

        int num_trials;
        auto* trials_opt = app.add_option(
            "-n,--num-trials", num_trials,
            "number of trials to run per benchmark step (default: " + std::to_string(DEFAULT_NUM_TRIALS) + ")"
        );

        auto* launch_flag = app.add_flag("--launch-server", "launch a server process for benchmarking (we do not by default)");
        auto* verbose_flag = app.add_flag("--verbose", "enable debug-level logging");

        try {
            app.parse(argc, argv);
        }
        catch (const CLI::ParseError& e) {
            std::exit(app.exit(e));
        }

        if (config_opt->count()) { args.config_path = config_path; }
        if (log_opt->count()) { args.log_path = log_path; }
        if (out_opt->count()) { args.output_path = output_path; }
        if (hw_out_opt->count()) { args.hardware_metrics_output_path = hardware_metrics_output_path; }
        if (server_binary_opt->count()) { args.server_binary_path = server_binary_path; }
        if (address_opt->count()) { args.server_address = server_address; }
        if (port_opt->count()) { args.server_port = server_port; }
        if (pid_opt->count()) { args.server_pid = server_pid; }
        if (trials_opt->count()) { args.num_trials = num_trials; }
        if (launch_flag->count()) { args.launch_server = true; }
        if (verbose_flag->count()) { args.verbose = true; }

        return args;
    }

    ConfigArgs LoadConfigArgs(const std::string& config_path) {
        YAML::Node cfg = YAML::LoadFile(config_path);
        return cfg.as<ConfigArgs>();
    }

    void LoadProgramSettings(Settings& s, int argc, char* argv[]) {
        CliArgs cli_args = ParseCliArgs(argc, argv);

        ConfigArgs config_args;
        if (cli_args.config_path) {
            config_args = LoadConfigArgs(cli_args.config_path.value());
        }

        // Start with default options, then apply config file and CLI options
        s.output_path = DEFAULT_OUTPUT_PATH;
        s.hardware_metrics_output_path = DEFAULT_HARDWARE_OUTPUT_PATH;
        s.server_binary_path = DEFAULT_SERVER_BINARY_PATH;
        s.server_address = DEFAULT_SERVER_ADDRESS;
        s.server_port = DEFAULT_SERVER_PORT;
        s.server_pid = -1;
        s.num_trials = DEFAULT_NUM_TRIALS;

        s.log_file = stderr;
        s.launch_server = false;
        s.verbose = false;

        if (config_args.cli_args.output_path) { s.output_path = config_args.cli_args.output_path.value(); }
        if (config_args.cli_args.hardware_metrics_output_path) { s.hardware_metrics_output_path = config_args.cli_args.hardware_metrics_output_path.value(); }
        if (config_args.cli_args.server_binary_path) { s.server_binary_path = config_args.cli_args.server_binary_path.value(); }
        if (config_args.cli_args.server_address) { s.server_address = config_args.cli_args.server_address.value(); }
        if (config_args.cli_args.server_port) { s.server_port = config_args.cli_args.server_port.value(); }
        // no config option for server_pid, as that is intended for scripting
        if (config_args.cli_args.num_trials) { s.num_trials = config_args.cli_args.num_trials.value(); }
        if (config_args.cli_args.launch_server) { s.launch_server = config_args.cli_args.launch_server.value(); }
        if (config_args.cli_args.verbose) { s.verbose = config_args.cli_args.verbose.value(); }

        if (config_args.model_specs) { s.model_specs = config_args.model_specs.value(); }
        if (config_args.batch_sizes) { s.batch_sizes = config_args.batch_sizes.value(); }

        if (cli_args.output_path) { s.output_path = cli_args.output_path.value(); }
        if (cli_args.hardware_metrics_output_path) { s.hardware_metrics_output_path = cli_args.hardware_metrics_output_path.value(); }
        if (cli_args.server_binary_path) { s.server_binary_path = cli_args.server_binary_path.value(); }
        if (cli_args.server_address) { s.server_address = cli_args.server_address.value(); }
        if (cli_args.server_port) { s.server_port = cli_args.server_port.value(); }
        if (cli_args.server_pid) { s.server_pid = cli_args.server_pid.value(); }
        if (cli_args.num_trials) { s.num_trials = cli_args.num_trials.value(); }
        if (cli_args.launch_server) { s.launch_server = cli_args.launch_server.value(); }
        if (cli_args.verbose) { s.verbose = cli_args.verbose.value(); }

        if (cli_args.log_path) {
            s.log_file = std::fopen(cli_args.log_path.value().c_str(), "w");
            if (!s.log_file) {
                std::fprintf(stderr, "Failed to open log file '%s': %s\n", cli_args.log_path.value().c_str(), std::strerror(errno));
                s.log_file = stderr;
            }
        }
        else if (config_args.cli_args.log_path) {
            s.log_file = std::fopen(config_args.cli_args.log_path.value().c_str(), "w");
            if (!s.log_file) {
                std::fprintf(stderr, "Failed to open log file '%s': %s\n", config_args.cli_args.log_path.value().c_str(), std::strerror(errno));
                s.log_file = stderr;
            }
        }

        // add some default batch sizes if there were never any configured
        if (s.batch_sizes.empty()) {
            s.batch_sizes.assign({4, 8, 16, 32, 64, 128});
        }
    }

} // namespace config
