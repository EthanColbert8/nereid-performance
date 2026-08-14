#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "cli/args.h"

namespace cli {

    static int64_t default_pf_points[] = {2, 100};
    static int64_t default_pf_features[] = {20, 100};
    static int64_t default_pf_mask[] = {1, 100};
    static int64_t default_sv_points[] = {2, 10};
    static int64_t default_sv_features[] = {11, 10};
    static int64_t default_sv_mask[] = {1, 10};

    static int64_t* default_input_shapes[] = {
        default_pf_points, default_pf_features, default_pf_mask,
        default_sv_points, default_sv_features, default_sv_mask
    };
    static int64_t default_input_shape_counts[] = {2, 2, 2, 2, 2, 2};

    constexpr const PartialModelSpec DEFAULT_MODEL_SPECS[] = {
        PartialModelSpec{
            name: "particlenet_AK4_PT",
            input_shapes: default_input_shapes,
            input_shape_counts: default_input_shape_counts,
            input_count: 6
        },
        PartialModelSpec{
            name: "particlenet_AK4",
            input_shapes: default_input_shapes,
            input_shape_counts: default_input_shape_counts,
            input_count: 6
        },
    };

    constexpr int DEFAULT_BATCH_SIZES[] = {
        4, 8, 16, 32, 64, 128 //, 256, 512, 1024, 2048,
    };

    constexpr Args DEFAULT_ARGS = {
        num_trials: 100,
        server_binary_path: "./nereid-server",
        server_address: "localhost",
        server_port: 50051,
        output_path: "nereid_benchmark_summary.json",
        hardware_metrics_output_path: "nereid_hardware_util.json",
        model_specs: DEFAULT_MODEL_SPECS,
        model_count: static_cast<int>(sizeof(DEFAULT_MODEL_SPECS) / sizeof(DEFAULT_MODEL_SPECS[0])),
        batch_sizes: DEFAULT_BATCH_SIZES,
        batch_size_count: static_cast<int>(sizeof(DEFAULT_BATCH_SIZES) / sizeof(DEFAULT_BATCH_SIZES[0])),
        launch_server: false
    };

    constexpr const char* const USAGE_MESSAGE = R"(Usage: nereid-bench [options]

Options:
    -n, --num-trials <positive int>: Number of trials per step in benchmark (default: 100)
        --server-binary <path>: Path to the Nereid server binary (default: ./nereid-server)
        --address <host>: gRPC address for the server (default: localhost)
        --port <port>: gRPC port for the server (default: 50051)
    -o, --out <path>: Summary JSON output path (default: nereid_benchmark_summary.json)
        --hw-out <path>: Hardware metrics JSON output path (default: nereid_hardware_util.json)
        --launch-server: Launch a server process to benchmark
)";

    [[noreturn]] void PrintUsageAndExit(int exit_code, const char* message = nullptr) {
        if (message != nullptr) {
            std::fprintf(stderr, "%s\n", message);
        }
        std::fputs(USAGE_MESSAGE, stderr);
        std::exit(exit_code);
    }

    bool ValidateArgs(const Args& args) {
        return (
            args.num_trials > 0 &&
            args.server_binary_path != nullptr && *args.server_binary_path != '\0' &&
            args.server_address != nullptr && *args.server_address != '\0' &&
            args.output_path != nullptr && *args.output_path != '\0' &&
            args.model_specs != nullptr && args.model_count > 0 &&
            args.batch_sizes != nullptr && args.batch_size_count > 0
        );
    }

    bool ParsePositiveInt(const char* text, int& value) {
        if (text == nullptr || *text == '\0') { return false; }

        errno = 0;
        char* end = nullptr;
        const long parsed = std::strtol(text, &end, 10);
        if (errno != 0 || end == text || *end != '\0' || parsed <= 0 || parsed > INT_MAX) {
            return false;
        }

        value = static_cast<int>(parsed);
        return true;
    }

    Args ParseArgs(int argc, char* argv[]) {
        Args args = DEFAULT_ARGS;

        bool saw_num_trials = false;
        bool saw_server_binary = false;
        bool saw_address = false;
        bool saw_port = false;
        bool saw_out = false;
        bool saw_hw_out = false;

        for (int i = 1; i < argc; i++) {
            if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
                PrintUsageAndExit(EXIT_SUCCESS);
            }

            if (std::strcmp(argv[i], "--launch-server") == 0) {
                args.launch_server = true;
                continue;
            }

            if (std::strcmp(argv[i], "--num-trials") == 0 || std::strcmp(argv[i], "-n") == 0) {
                if (saw_num_trials) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: num-trials may only be passed once");
                }
                if (i + 1 >= argc) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: num-trials requires a positive integer argument");
                }
                if (!ParsePositiveInt(argv[i + 1], args.num_trials)) {
                    args.num_trials = 0;
                }

                saw_num_trials = true;
                i++;
                continue;
            }

            if (std::strcmp(argv[i], "--server-binary") == 0) {
                if (saw_server_binary) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: server-binary may only be passed once");
                }
                if (i + 1 >= argc) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: --server-binary requires a path argument");
                }

                args.server_binary_path = argv[i + 1];
                saw_server_binary = true;
                i++;
                continue;
            }

            if (std::strcmp(argv[i], "--address") == 0) {
                if (saw_address) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: address may only be passed once");
                }
                if (i + 1 >= argc) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: address requires an address argument");
                }

                args.server_address = argv[i + 1];
                saw_address = true;
                i++;
                continue;
            }

            if (std::strcmp(argv[i], "--port") == 0) {
                if (saw_port) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: port may only be passed once");
                }
                if (i + 1 >= argc) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: port requires a positive integer argument");
                }
                if (!ParsePositiveInt(argv[i + 1], args.server_port)) {
                    args.server_port = -1;
                }
                if (args.server_port <= 0 || args.server_port > 65535) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: port must be a positive integer between 1 and 65535");
                }

                i++;
                continue;
            }

            if (std::strcmp(argv[i], "--out") == 0 || std::strcmp(argv[i], "-o") == 0) {
                if (saw_out) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: out may only be passed once");
                }
                if (i + 1 >= argc) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: out requires a path argument");
                }

                args.output_path = argv[i + 1];
                saw_out = true;
                i++;
                continue;
            }

            if (std::strcmp(argv[i], "--hw-out") == 0) {
                if (saw_hw_out) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: hw-out may only be passed once");
                }
                if (i + 1 >= argc) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: hw-out requires a path argument");
                }

                args.hardware_metrics_output_path = argv[i + 1];
                saw_hw_out = true;
                i++;
                continue;
            }

            char err_string[256];
            std::snprintf(err_string, sizeof(err_string), "Error: unrecognized argument: %s", argv[i]);
            PrintUsageAndExit(EXIT_FAILURE, err_string);
        }

        if (!ValidateArgs(args)) {
            PrintUsageAndExit(EXIT_FAILURE, "Error: invalid argument values provided");
        }
        return args;
    }

} // namespace cli
