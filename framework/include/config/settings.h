#pragma once

#include "nereid/model.h"
#include "benchmark/context.h"

#include <cstdio>
#include <cstdint>
#include <sys/types.h>
#include <vector>
#include <optional>
#include <string>

#include <yaml-cpp/yaml.h>

namespace config {

    struct CliArgs {
        std::optional<std::string> config_path;
        std::optional<std::string> log_path;

        std::optional<std::string> output_path;
        std::optional<std::string> hardware_metrics_output_path;

        std::optional<std::string> server_binary_path;
        std::optional<std::string> server_log_path;
        std::optional<std::string> server_address;
        std::optional<int> server_port;
        std::optional<pid_t> server_pid;

        // std::optional<int> num_trials;

        std::optional<bool> launch_server;
        std::optional<bool> verbose;
    };

    struct ConfigArgs {
        CliArgs cli_args;
    
        std::optional<std::vector<nereid::ModelSpec>> model_specs;
        // std::optional<std::vector<int>> batch_sizes;
        std::optional<std::vector<benchmark::Run>> runs;
    };

    struct Settings {
        // int num_trials;

        FILE* log_file;

        std::string server_binary_path;
        std::string server_log_path;
        std::string server_address;
        int server_port;
        pid_t server_pid;

        std::string output_path;
        std::string hardware_metrics_output_path;

        std::vector<nereid::ModelSpec> model_specs;
        // std::vector<int> batch_sizes;

        std::vector<benchmark::Run> runs;

        bool launch_server;
        bool verbose;
    };

    void LoadProgramSettings(Settings& s, int argc, char* argv[]);

} // namespace config

namespace YAML {

    template<>
    struct convert<nereid::TensorSpec> {
        static bool decode(const Node& node, nereid::TensorSpec& t) {
            t.name = node["name"].as<std::string>();
            t.dtype = nereid::StringToDtype_throws(node["dtype"].as<std::string>());
            t.shape = node["shape"].as<std::vector<int64_t>>();
            return true;
        }
    };

    template<>
    struct convert<nereid::ModelSpec> {
        static bool decode(const Node& node, nereid::ModelSpec& m) {
            m.name = node["name"].as<std::string>();
            m.version = node["version"].as<std::string>();
            m.platform = node["platform"].as<std::string>();

            if (node["inputs"]) {
                m.inputs = node["inputs"].as<std::vector<nereid::TensorSpec>>();
            }
            if (node["outputs"]) {
                m.outputs = node["outputs"].as<std::vector<nereid::TensorSpec>>();
            }

            return true;
        }
    };

    template<>
    struct convert<benchmark::Step> {
        static bool decode(const Node& node, benchmark::Step& s) {
            s.model_name = node["model_name"].as<std::string>();
            s.batch_size = node["batch_size"].as<int>();
            s.stop_value = node["num_trials"].as<int>();
            s.stop_condition = benchmark::StringToStopConditionType_throws(node["stop_condition"].as<std::string>());
            return true;
        }
    };

    template<>
    struct convert<benchmark::Sequence> {
        static bool decode(const Node& node, benchmark::Sequence& seq) {
            seq.steps = node["steps"].as<std::vector<benchmark::Step>>();
            return true;
        }
    };

    template<>
    struct convert<benchmark::Stage> {
        static bool decode(const Node& node, benchmark::Stage& stage) {
            stage.client_sequences = node["client_sequences"].as<std::vector<benchmark::Sequence>>();
            return true;
        }
    };

    template<>
    struct convert<benchmark::Run> {
        static bool decode(const Node& node, benchmark::Run& run) {
            run.name = node["name"].as<std::string>();
            run.stages = node["stages"].as<std::vector<benchmark::Stage>>();
            return true;
        }
    };

    template<>
    struct convert<config::CliArgs> {
        static bool decode(const Node& node, config::CliArgs& a) {
            if (node["log_path"]) {
                a.log_path = node["log_path"].as<std::string>();
            }
            
            if (node["output_path"]) {
                a.output_path = node["output_path"].as<std::string>();
            }
            if (node["hardware_metrics_output_path"]) {
                a.hardware_metrics_output_path = node["hardware_metrics_output_path"].as<std::string>();
            }

            if (node["server_binary_path"]) {
                a.server_binary_path = node["server_binary_path"].as<std::string>();
            }
            if (node["server_log_path"]) {
                a.server_log_path = node["server_log_path"].as<std::string>();
            }
            if (node["server_address"]) {
                a.server_address = node["server_address"].as<std::string>();
            }
            if (node["server_port"]) {
                a.server_port = node["server_port"].as<int>();
            }

            // if (node["num_trials"]) {
            //     a.num_trials = node["num_trials"].as<int>();
            // }

            if (node["launch_server"]) {
                a.launch_server = node["launch_server"].as<bool>();
            }
            if (node["verbose"]) {
                a.verbose = node["verbose"].as<bool>();
            }

            return true;
        }
    };

    template<>
    struct convert<config::ConfigArgs> {
        static bool decode(const Node& node, config::ConfigArgs& a) {
            a.cli_args = node.as<config::CliArgs>();

            if (node["models"]) {
                a.model_specs = node["models"].as<std::vector<nereid::ModelSpec>>();
            }

            // if (node["batch_sizes"]) {
            //     a.batch_sizes = node["batch_sizes"].as<std::vector<int>>();
            // }

            if (node["runs"]) {
                a.runs = node["runs"].as<std::vector<benchmark::Run>>();
            }

            return true;
        }
    };

} // namespace YAML
