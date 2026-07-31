#include <cstdio>

#include "cli/args.h"

int main(int argc, char* argv[]) {
    printf("Heyo from the benchmarking tool.\n");

    cli::Args args = cli::ParseArgs(argc, argv);
    printf("Number of trials: %d\n", args.num_trials);

    return 0;
}
