#pragma once

#include <cstdio>
#include <cstdint>

namespace cli {

    struct PartialModelSpec {
        const char* name;
        int64_t** input_shapes;
        int64_t* input_shape_counts;
        int input_count;
    };

    struct Args {
        int num_trials;
        FILE* log_file;
        const char* server_binary_path;
        const char* server_address;
        int server_port;
        const char* output_path;
        const char* hardware_metrics_output_path;
        const PartialModelSpec* model_specs;
        int model_count;
        const int* batch_sizes;
        int batch_size_count;
        bool launch_server;
        bool verbose;
    };

    Args ParseArgs(int argc, char* argv[]);

} // namespace cli
