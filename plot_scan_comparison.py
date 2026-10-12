import os
import logging
import argparse
import json
from matplotlib import pyplot as plt
import mplhep as hep

plt.style.use(hep.style.CMS)

default_colors = {
    "nereid": "#5790fc",
    "triton": "#f89c20",
}

def draw_errorbars_plot(x, y, yerr, xlabel, ylabel, save_filename, ratio_key=None, title=None, xscale="linear", yscale="linear", img_type="png"):
    use_ratio = ratio_key is not None

    if use_ratio and (ratio_key not in x.keys()):
        logging.warning(f"Reference scan '{ratio_key}' not found in data. Skipping ratio plot.")
        use_ratio = False

    if use_ratio:
        fig, (ax_main, ax_ratio) = plt.subplots(2, 1, sharex=True, dpi=100, gridspec_kw={"height_ratios": [3, 1], "hspace": 0.1})
        ax_ratio.axhline(y=1.0, color="black", linestyle="--", lw=2, alpha=0.6)
        ax_ratio.set_ylabel(f"All/{ratio_key}")
        ax_ratio.set_ylim(0.5, 1.5)
    else:
        fig, ax_main = plt.subplots(dpi=100)
        ax_ratio = ax_main

    for k in x.keys():
        color = default_colors.get(k.lower(), "#9c9ca1")

        ax_main.errorbar(x[k], y[k], yerr=yerr[k], label=k, fmt="-o", color=color)

        if use_ratio and (k != ratio_key):
            ratio = [y[k][i] / y[ratio_key][i] for i in range(len(y[k]))]
            ax_ratio.plot(x[k], ratio, label=k, marker="o", color=color)

    ax_main.legend()

    ax_ratio.set_xlabel(xlabel)
    ax_main.set_ylabel(ylabel)

    ax_ratio.set_xscale(xscale)
    ax_main.set_yscale(yscale)

    if (xscale == "log"):
        ax_ratio.set_xlim(left=1)
    else:
        ax_ratio.set_xlim(left=0)
    
    if (yscale == "log"):
        ax_main.set_ylim(bottom=1)
    else:
        ax_main.set_ylim(bottom=0)
    
    if (title is not None):
        ax_main.set_title(title)

    plt.savefig(f"{save_filename}.{img_type}", dpi="figure")
    plt.close()

def main(args):
    save_folder = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(save_folder, exist_ok=True)

    index_scan_names = False
    if (len(args.names) != len(args.timings_files)):
        logging.warning("At least one legend name missing. Indexing plots instead.")
        args.names = []
        index_scan_names = True
    
    batch_sizes = {}
    throughputs = {}
    throughput_errs = {}
    for i, timings_file in enumerate(args.timings_files):
        with open(timings_file, "r") as f:
            timings = json.load(f)
        
        if (index_scan_names):
            args.names.append(f"Scan {i+1}")
        
        batch_sizes[args.names[i]] = timings["batch_size"]
        throughputs[args.names[i]] = timings["throughput"]
        throughput_errs[args.names[i]] = timings["throughput_err"]
    
    draw_errorbars_plot(
        batch_sizes, throughputs, throughput_errs,
        "Batch size", "Mean Throughput (infer/sec)",
        args.out, ratio_key=args.ratio,
        title=args.title, xscale="log"
    )

    # draw_errorbar_plot(
    #     timings["batch_size"], timings["latency"], timings["latency_err"],
    #     "Batch size", "Processing tme (ms)",
    #     os.path.join(save_folder, "latency"),
    #     title=args.title, xscale="log"
    # )

if (__name__ == "__main__"):
    parser = argparse.ArgumentParser()
    parser.add_argument("timings_files", nargs="+", type=str, help="JSON files containing scan timing information")
    parser.add_argument("--names", nargs="+", type=str, default=[], help="Names for plot legend (arbitrary index by default)")
    parser.add_argument("--ratio", type=str, default=None, help="Name for reference scan in ratio plot (no ratio plot if not given)")
    parser.add_argument("--out", type=str, default="scan_comparison", help="Output filename for the plot, without extension (default: scan_comparison)")
    parser.add_argument("--title", type=str, default=None, help="A title to put on the plots")
    parser.add_argument("--verbose", action="store_true", help="Enable debug-level logging")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=log_level, format="[%(asctime)s - %(module)s - %(levelname)s]: %(message)s")

    logging.info("Plotting timings from batch size scan.")
    main(args)
    logging.info("Finished.")
