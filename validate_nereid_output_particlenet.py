import os
import uuid
import logging
import argparse
import numpy as np
import tritonclient.grpc as grpcclient

def load_validation_tensors(input_filename):
    val_archive = np.load(input_filename)
    return {key: val_archive[key] for key in val_archive.files}

def main(args):
    server_address = f"localhost:{args.port}"

    logging.info(f"Loading validation data from \"{args.input_filename}\".")
    val_arrays = load_validation_tensors(args.input_filename)

    logging.info(f"Connecting to GRPC server at {server_address}.")
    with grpcclient.InferenceServerClient(server_address) as client:
        inputs = [
            grpcclient.InferInput("pf_points", val_arrays["pf_points"].shape, "FP32"),
            grpcclient.InferInput("pf_features", val_arrays["pf_features"].shape, "FP32"),
            grpcclient.InferInput("pf_mask", val_arrays["pf_mask"].shape, "FP32"),
            grpcclient.InferInput("sv_points", val_arrays["sv_points"].shape, "FP32"),
            grpcclient.InferInput("sv_features", val_arrays["sv_features"].shape, "FP32"),
            grpcclient.InferInput("sv_mask", val_arrays["sv_mask"].shape, "FP32"),
        ]

        outputs = [grpcclient.InferRequestedOutput("softmax")]

        inputs[0].set_data_from_numpy(val_arrays["pf_points"])
        inputs[1].set_data_from_numpy(val_arrays["pf_features"])
        inputs[2].set_data_from_numpy(val_arrays["pf_mask"])
        inputs[3].set_data_from_numpy(val_arrays["sv_points"])
        inputs[4].set_data_from_numpy(val_arrays["sv_features"])
        inputs[5].set_data_from_numpy(val_arrays["sv_mask"])

        logging.info("Sending inference request to server.")

        result = client.infer("particlenet_AK4_PT", inputs, outputs=outputs)
        output_values = result.as_numpy("softmax")

    logging.info("Result obtained. Evaluating.")
    result_is_correct = np.all(output_values == val_arrays["softmax"])

    if result_is_correct:
        logging.info("Validation successful: server output matches expectation.")
    else:
        logging.error("Validation failed: server output does not match expectation.")
        
        if args.dump:
            dump_filename = os.path.join(os.path.dirname(args.input_filename), "nereid_particlenet_output.npy")
            if os.path.isfile(dump_filename):
                unique_suffix = str(uuid.uuid4())
                dump_filename = os.path.join(os.path.dirname(args.input_filename), f"nereid_particlenet_output_{unique_suffix}.npy")
            
            logging.info(f"Saving server output to \"{dump_filename}\".")
            np.save(dump_filename, output_values)

if (__name__ == "__main__"):
    parser = argparse.ArgumentParser()
    parser.add_argument("input_filename", type=str, help="Path to .npz file containing validation tensors.")
    parser.add_argument("--port", type=int, default=50051, help="Port on which server is listening.")
    parser.add_argument("--dump", action="store_true", help="Dump server output to file if validation fails.")
    parser.add_argument("--verbose", action="store_true", help="Enable debug-level logging.")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=log_level, format="[%(asctime)s - %(module)s - %(levelname)s]: %(message)s")

    logging.info(f"Beginning validation of Nereid output.")
    main(args)
    logging.info("Finished.")
