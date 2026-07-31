#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cli/args.h"

namespace cli {

    constexpr const char* const DEFAULT_SERVER_BINARY_PATH = "./nereid-server";
    constexpr const char* const DEFAULT_SERVER_ADDRESS = "localhost:50051";
    constexpr const char* const DEFAULT_OUTPUT_PATH = "nereid_benchmark_summary.json";

    constexpr const char* const DEFAULT_MODEL_NAMES[] = {
        "particlenet_AK4_PT",
        "particlenet_AK4",
    };

    constexpr int DEFAULT_BATCH_SIZES[] = {
        4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048,
    };

    constexpr const char* const USAGE_MESSAGE = R"(Usage: nereid-bench [options]

Options:
    -n, --num-trials <positive int>: Number of trials to run (default 100)
        --server-binary <path>: Path to the Nereid server binary (default ./nereid-server)
        --server-address <host:port>: gRPC address for the server (default localhost:50051)
        --output <path>: Summary JSON output path (default nereid_benchmark_summary.json)
)";

    [[noreturn]] void PrintUsageAndExit(int exit_code, const char* message = nullptr) {
        if (message != nullptr) {
            std::fprintf(stderr, "%s\n", message);
        }
        std::fputs(USAGE_MESSAGE, stderr);
        std::exit(exit_code);
    }

    bool ValidateArgs(const Args& args) {
        if (args.num_trials <= 0) {
            return false;
        }

        if (args.server_binary_path == nullptr || *args.server_binary_path == '\0') {
            return false;
        }

        if (args.server_address == nullptr || *args.server_address == '\0') {
            return false;
        }

        if (args.output_path == nullptr || *args.output_path == '\0') {
            return false;
        }

        if (args.model_names == nullptr || args.model_count <= 0) {
            return false;
        }

        if (args.batch_sizes == nullptr || args.batch_size_count <= 0) {
            return false;
        }

        return true;
    }

    bool ParsePositiveInt(const char* text, int& value) {
        if (text == nullptr || *text == '\0') {
            return false;
        }

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
        Args args = {};
        args.num_trials = 100;
        args.server_binary_path = DEFAULT_SERVER_BINARY_PATH;
        args.server_address = DEFAULT_SERVER_ADDRESS;
        args.output_path = DEFAULT_OUTPUT_PATH;
        args.model_names = DEFAULT_MODEL_NAMES;
        args.model_count = static_cast<int>(sizeof(DEFAULT_MODEL_NAMES) / sizeof(DEFAULT_MODEL_NAMES[0]));
        args.batch_sizes = DEFAULT_BATCH_SIZES;
        args.batch_size_count = static_cast<int>(sizeof(DEFAULT_BATCH_SIZES) / sizeof(DEFAULT_BATCH_SIZES[0]));

        bool saw_num_trials = false;
        bool saw_server_binary = false;
        bool saw_server_address = false;
        bool saw_output = false;

        for (int i = 1; i < argc; i++) {
            if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
                PrintUsageAndExit(EXIT_SUCCESS);
            }

            if (std::strcmp(argv[i], "--num-trials") == 0 || std::strcmp(argv[i], "-n") == 0) {
                if (saw_num_trials) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: num-trials may only be passed once");
                }
                if (i + 1 >= argc) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: --num-trials requires a positive integer argument");
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

            if (std::strcmp(argv[i], "--server-address") == 0) {
                if (saw_server_address) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: server-address may only be passed once");
                }
                if (i + 1 >= argc) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: --server-address requires an address argument");
                }

                args.server_address = argv[i + 1];
                saw_server_address = true;
                i++;
                continue;
            }

            if (std::strcmp(argv[i], "--output") == 0) {
                if (saw_output) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: output may only be passed once");
                }
                if (i + 1 >= argc) {
                    PrintUsageAndExit(EXIT_FAILURE, "Error: --output requires a path argument");
                }

                args.output_path = argv[i + 1];
                saw_output = true;
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
