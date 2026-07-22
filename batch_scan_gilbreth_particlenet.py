import os
import json
import time
import logging
import argparse
import numpy as np
import tritonclient.grpc as grpcclient

def main(args):
    batch_sizes = [4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]
    n_trials = args.ntrials
    server_address = f"localhost:{args.port}"
    rand_gen = np.random.default_rng(args.seed)

    model_name = args.model
    if (model_name != "particlenet_AK4_PT") and (model_name != "particlenet_AK4"):
        logging.warning(f"Model name \"{model_name}\" not recognized. Using \"particlenet_AK4_PT\" instead.")
        model_name = "particlenet_AK4_PT"

    logging.info(f"Running {n_trials} trials per batch size for model \"{model_name}\"")

    throughputs = []
    throughput_errors = []
    latencies = []
    latency_errors = []

    with grpcclient.InferenceServerClient(server_address) as client:
        logging.info(f"Connected to GRPC server at {server_address}")

        for batch_size in batch_sizes:
            times = []
            inputs = []
            outputs = []

            logging.info(f"Starting batch size {batch_size}")

            inputs.append(grpcclient.InferInput("pf_points", [batch_size, 2, 100], "FP32"))
            inputs.append(grpcclient.InferInput("pf_features", [batch_size, 20, 100], "FP32"))
            inputs.append(grpcclient.InferInput("pf_mask", [batch_size, 1, 100], "FP32"))
            inputs.append(grpcclient.InferInput("sv_points", [batch_size, 2, 10], "FP32"))
            inputs.append(grpcclient.InferInput("sv_features", [batch_size, 11, 10], "FP32"))
            inputs.append(grpcclient.InferInput("sv_mask", [batch_size, 1, 10], "FP32"))

            outputs.append(grpcclient.InferRequestedOutput("softmax"))

            for trial in range(1, n_trials+1):
                pf_points = rand_gen.standard_normal(size=(batch_size, 2, 100)).astype(np.float32)
                pf_features = rand_gen.standard_normal(size=(batch_size, 20, 100)).astype(np.float32)
                pf_mask = rand_gen.standard_normal(size=(batch_size, 1, 100)).astype(np.float32)
                sv_points = rand_gen.standard_normal(size=(batch_size, 2, 10)).astype(np.float32)
                sv_features = rand_gen.standard_normal(size=(batch_size, 11, 10)).astype(np.float32)
                sv_mask = rand_gen.standard_normal(size=(batch_size, 1, 10)).astype(np.float32)

                start_time = time.perf_counter_ns()

                inputs[0].set_data_from_numpy(pf_points)
                inputs[1].set_data_from_numpy(pf_features)
                inputs[2].set_data_from_numpy(pf_mask)
                inputs[3].set_data_from_numpy(sv_points)
                inputs[4].set_data_from_numpy(sv_features)
                inputs[5].set_data_from_numpy(sv_mask)

                results = client.infer(model_name, inputs, outputs=outputs)
                output_values = results.as_numpy("softmax")

                end_time = time.perf_counter_ns()
                times.append((end_time - start_time) * 1.0e-9) # seconds

            # could discard the first one if we see warmup
            times_arr = np.array(times)
            sqrt_ntrials = np.sqrt(len(times_arr))

            throughput = batch_size / times_arr # inferences/sec
            latency = times_arr * 1.0e3 # milliseconds

            throughputs.append(np.mean(throughput))
            throughput_errors.append(np.std(throughput) / sqrt_ntrials)
            latencies.append(np.mean(latency))
            latency_errors.append(np.std(latency) / sqrt_ntrials)

    timings = {
        "batch_size": batch_sizes,
        "throughput": throughputs,
        "throughput_err": throughput_errors,
        "latency": latencies,
        "latency_err": latency_errors,
    }

    output_file_path = os.path.join(args.output_dir, "scan_timings.json")
    logging.info(f"Scan finished. Saving timings to \"{output_file_path}\"")

    with open(output_file_path, "w") as out_file:
        json.dump(timings, out_file, indent=4)

if (__name__ == "__main__"):
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=str, help="Directory in which to save results")
    parser.add_argument("--port", type=int, default=50051, help="Port on which server is listening")
    parser.add_argument("--model", type=str, default="particlenet_AK4_PT", help="Name of model to compute inference with")
    parser.add_argument("--ntrials", type=int, default=100, help="Number of trials to run for each batch size")
    parser.add_argument("--seed", type=int, default=None, help="Random seed for input generation")
    parser.add_argument("--verbose", action="store_true", help="Enable debug-level logging")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=log_level, format="[%(asctime)s - %(module)s - %(levelname)s]: %(message)s")

    logging.info("Starting batch scan")
    main(args)
    logging.info("Finished.")
