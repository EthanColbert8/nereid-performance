#include <cstdio>
#include <cstdlib>
#include <string>

#include "benchmark/runner.h"
#include "cli/args.h"

int main(int argc, char* argv[]) {
    cli::Args args = cli::ParseArgs(argc, argv);

    nlohmann::json report;
    std::string error_message;
    if (!benchmark::RunBenchmark(args, &report, &error_message)) {
        std::fprintf(stderr, "Error: %s\n", error_message.c_str());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
