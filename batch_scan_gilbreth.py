import os
import sys
import json
import time
import logging
import argparse
import numpy as np
from typing import Optional, Iterator

import grpc
import proto.inference_pb2 as pb2
import proto.inference_pb2_grpc as pb2_grpc

MODEL_INPUT_DIM = 2400

def request_stream(
    model: str,
    batch_size: int,
    data: bytes,
    chunk_size: Optional[int] = 64
) -> Iterator:
    """

    """
    yield pb2.CheckpointRequest(meta=pb2.CheckpointMeta(model_name=model, output_file=""))

    total_chunks = (len(data) + chunk_size - 1) // chunk_size
    for idx, start in enumerate(range(0, len(data), chunk_size)):
        yield pb2.CheckpointRequest(chunk=pb2.TensorChunk(
            tensor_name="input",
            shape=(batch_size, MODEL_INPUT_DIM),
            data=data[start:(start+chunk_size)],
            chunk_index=idx,
            end_of_tensor=(idx+1 == total_chunks)
        ))

def main(args):
    batch_sizes = [4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]
    n_trials = args.ntrials
    chunk_size = args.chunksize
    server_address = f"localhost:{args.port}"
    rand_gen = np.random.default_rng(args.seed)

    logging.info(f"Running {n_trials} trials per batch size, sending chunks of {chunk_size} bytes")

    throughputs = []
    throughput_errors = []
    latencies = []
    latency_errors = []

    with grpc.insecure_channel(server_address) as channel:
        stub = pb2_grpc.NereidStub(channel)
        logging.info(f"Connected to GRPC server at {server_address}")

        for batch_size in batch_sizes:
            times = []
            inputs = []
            outputs = []

            logging.info(f"Starting batch size {batch_size}")

            for trial in range(1, n_trials+1):
                values = rand_gen.standard_normal(size=(batch_size, MODEL_INPUT_DIM)).astype(np.float32)

                start_time = time.perf_counter_ns()

                buffer = values.tobytes()

                responses = stub.Checkpoint(request_stream("big_mlp", batch_size, buffer, chunk_size=chunk_size))
                #responses_finished = any([response.done for response in responses])
                responses_finished = False
                for response in responses:
                    if response.done:
                        responses_finished = True
                        break

                if (not responses_finished):
                    logging.error(f"Inference request failed for batch size {batch_size} at trial {trial}")
                    sys.exit("Failed inference request")
                
                output_buffer = b"".join(response.output_chunk.data for response in responses)
                output_values = np.frombuffer(output_buffer, dtype=np.float32).reshape((batch_size, -1))

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
    parser.add_argument("--ntrials", type=int, default=100, help="Number of trials to run for each batch size")
    # parser.add_argument("--model", type=str, default="big_mlp", help="Name of model to compute inference with")
    parser.add_argument("--chunksize", type=int, default=65536, help="Size of chunks to send to server in bytes")
    parser.add_argument("--seed", type=int, default=None, help="Random seed for input generation")
    parser.add_argument("--verbose", action="store_true", help="Enable debug-level logging")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=log_level, format="[%(asctime)s - %(module)s - %(levelname)s]: %(message)s")

    logging.info("Starting batch scan")
    main(args)
    logging.info("Finished.")
