import logging
import argparse
import torch
import torch.nn as nn

class MLPBlock(nn.Module):
    def __init__(self, input_size, hidden_size, output_size, num_layers):
        super().__init__()

        layers = [nn.Linear(input_size, hidden_size), nn.ReLU()]
        for _ in range(num_layers - 1):
            layers.append(nn.Linear(hidden_size, hidden_size))
            layers.append(nn.ReLU())
        layers.append(nn.Linear(hidden_size, output_size))
        self.stack = nn.Sequential(*layers)
    
    def forward(self, x):
        return self.stack(x)

def main(args):
    model_args = {
        "input_size": 2400,
        "hidden_size": 1800,
        "output_size": 10,
        "num_layers": 4,
    }
    model = MLPBlock(**model_args)

    model_params = sum(p.numel() for p in model.parameters())
    logging.debug(f"Model parameters: {model_params:,}")

    save_filename = f"{args.save_folder}/model.pt"
    torch.jit.script(model).save(save_filename)
    logging.info(f"Saved model to: \"{save_filename}\"")

if (__name__ == "__main__"):
    parser = argparse.ArgumentParser()
    parser.add_argument("save_folder", type=str, help="Folder to save the model")
    parser.add_argument("--verbose", action="store_true", help="Enable debug-level logging")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=log_level, format="[%(asctime)s - %(module)s - %(levelname)s]: %(message)s")

    logging.info("Creating big MLP model")
    main(args)
    logging.info("Finished.")
