#pragma once

namespace cli {
    struct Args {
        int num_trials;
        const char* server_binary_path;
        const char* server_address;
        const char* output_path;
        const char* const* model_names;
        int model_count;
        const int* batch_sizes;
        int batch_size_count;
    };

    Args ParseArgs(int argc, char* argv[]);
} // namespace cli
