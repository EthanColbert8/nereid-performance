import os
import json
import time
import logging
import argparse
import numpy as np
import tritonclient.grpc as grpcclient

MODEL_INPUT_DIM = 2400

def main(args):
    batch_sizes = [4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]
    n_trials = args.ntrials
    model_name = "big_mlp"
    server_address = f"localhost:{args.port}"
    rand_gen = np.random.default_rng(args.seed)

    logging.info(f"Running {n_trials} trials per batch size for model \"{model_name}\"")

    throughputs = []
    throughput_errors = []
    latencies = []
    latency_errors = []

    with grpcclient.InferenceServerClient(server_address) as client:
        logging.info(f"Connected to GRPC server at {server_address}")

        metadata = client.get_model_metadata(model_name)
        input_name = metadata.inputs[0].name
        output_name = metadata.outputs[0].name

        for batch_size in batch_sizes:
            times = []
            inputs = []
            outputs = []

            logging.info(f"Starting batch size {batch_size}")

            inputs.append(grpcclient.InferInput(input_name, [batch_size, MODEL_INPUT_DIM], "FP32"))
            outputs.append(grpcclient.InferRequestedOutput(output_name))

            for trial in range(1, n_trials+1):
                values = rand_gen.standard_normal(size=(batch_size, MODEL_INPUT_DIM)).astype(np.float32)

                start_time = time.perf_counter_ns()

                inputs[0].set_data_from_numpy(values)

                results = client.infer(model_name, inputs, outputs=outputs)
                output_values = results.as_numpy(output_name)

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
    # parser.add_argument("--model", type=str, default="big_mlp", help="Name of model to compute inference with")
    parser.add_argument("--ntrials", type=int, default=100, help="Number of trials to run for each batch size")
    parser.add_argument("--seed", type=int, default=None, help="Random seed for input generation")
    parser.add_argument("--verbose", action="store_true", help="Enable debug-level logging")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=log_level, format="[%(asctime)s - %(module)s - %(levelname)s]: %(message)s")

    logging.info("Starting batch scan")
    main(args)
    logging.info("Finished.")
