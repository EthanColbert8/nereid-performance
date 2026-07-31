#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cli/args.h"

namespace cli {

        constexpr const char* USAGE_MESSAGE =R"(Usage: nereid-bench [options]

Options:
    -n, --num-trials <positive int>: Number of trials to run (default 100)
)";

        [[noreturn]] void PrintUsageAndExit(int exit_code, const char* message = nullptr) {
            if (message != nullptr) {
                std::fprintf(stderr, "%s\n", message);
            }
            std::fputs(USAGE_MESSAGE, stderr);
            std::exit(exit_code);
        }

        bool ValidateArgs(Args args) {
            if (args.num_trials <= 0) {
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
        bool saw_num_trials = false;

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

            char* err_string = nullptr;
            asprintf(&err_string, "Error: unrecognized argument: %s", argv[i]);
            PrintUsageAndExit(EXIT_FAILURE, err_string);
        }

        if (!saw_num_trials) {
            args.num_trials = 100;
        }

        if (!ValidateArgs(args)) {
            PrintUsageAndExit(EXIT_FAILURE, "Error: invalid argument values provided");
        }
        return args;
    }

} // namespace cli
