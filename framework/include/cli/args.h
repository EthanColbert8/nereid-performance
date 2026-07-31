#pragma once

namespace cli {
    struct Args {
        int num_trials;
    };

    Args ParseArgs(int argc, char* argv[]);
} // namespace cli
