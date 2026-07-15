import logging
import argparse
import numpy as np
import torch

def main(args):
    DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    rand_gen = np.random.default_rng(args.seed)
    batch_size = args.num # for convenience

    logging.info(f"Generating random input tensors with batch size {batch_size:,}.")

    data_arrays = {
        "pf_points": rand_gen.standard_normal(size=(batch_size, 2, 100), dtype=np.float32),
        "pf_features": rand_gen.standard_normal(size=(batch_size, 20, 100), dtype=np.float32),
        "pf_mask": rand_gen.standard_normal(size=(batch_size, 1, 100), dtype=np.float32),
        "sv_points": rand_gen.standard_normal(size=(batch_size, 2, 10), dtype=np.float32),
        "sv_features": rand_gen.standard_normal(size=(batch_size, 11, 10), dtype=np.float32),
        "sv_mask": rand_gen.standard_normal(size=(batch_size, 1, 10), dtype=np.float32),
    }

    input_tensors = [
        torch.from_numpy(data_arrays["pf_points"]).to(DEVICE),
        torch.from_numpy(data_arrays["pf_features"]).to(DEVICE),
        torch.from_numpy(data_arrays["pf_mask"]).to(DEVICE),
        torch.from_numpy(data_arrays["sv_points"]).to(DEVICE),
        torch.from_numpy(data_arrays["sv_features"]).to(DEVICE),
        torch.from_numpy(data_arrays["sv_mask"]).to(DEVICE),
    ]

    logging.info(f"Computing inference using \"particlenet_AK4_PT\" model on device {DEVICE}.")

    pnet = torch.jit.load("/depot/cms/users/colberte/SONIC/nereid/perf_studies/models/particlenet_AK4_PT/model.pt").to(DEVICE)
    pnet.eval()

    with torch.no_grad():
        output = pnet(*input_tensors)
        data_arrays["softmax"] = output.detach().cpu().numpy()

    logging.info(f"Saving inputs and results to \"{args.save_filename}\".")
    np.savez(args.save_filename, **data_arrays)

if (__name__ == "__main__"):
    parser = argparse.ArgumentParser()
    parser.add_argument("save_filename", type=str, help="Path to save the validation tensors.")
    parser.add_argument("--num", type=int, default=100, help="Number of samples in validation tensors (batch size). Default is 100.")
    parser.add_argument("--seed", type=int, default=None, help="Random seed for reproducibility.")
    parser.add_argument("--verbose", action="store_true", help="Enable debug-level logging.")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=log_level, format="[%(asctime)s - %(module)s - %(levelname)s]: %(message)s")

    logging.info(f"Beginning creation of validation tensors.")
    main(args)
    logging.info("Finished.")
