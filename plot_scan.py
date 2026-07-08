import os
import re
import logging
import argparse
import json
from datetime import datetime
from matplotlib import pyplot as plt
import mplhep as hep

plt.style.use(hep.style.CMS)

def read_resource_usage(resource_log_path, client_log_path):
    with open(resource_log_path, "r") as file:
        lines = file.readlines()

        first_data = lines[1].split(",")

        first_timestamp = datetime.strptime(first_data[0], "%Y-%m-%d %H:%M:%S.%f")

        elapsed_times = [0.0]
        gpu_util = [float(first_data[1])]
        gpu_mem = [float(first_data[2])]
        cpu_util = [float(first_data[3])]
        ram_util = [float(first_data[4])]

        for line in lines[2:]:
            data = line.split(",")

            timestamp = datetime.strptime(data[0], "%Y-%m-%d %H:%M:%S.%f")

            elapsed_times.append((timestamp - first_timestamp).total_seconds())

            gpu_util.append(float(data[1]))
            gpu_mem.append(float(data[2]))
            cpu_util.append(float(data[3]))
            ram_util.append(float(data[4]))

    batch_size_timestamps = []
    with open(client_log_path, "r") as file:
        lines = file.readlines()

        for line in lines:
            if ("Starting batch size" in line):
                batch_size_timestamps.append(re.search(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3}", line).group(0))

    batch_size_timestamps = [(datetime.strptime(ts, "%Y-%m-%d %H:%M:%S,%f") - first_timestamp).total_seconds() for ts in batch_size_timestamps]

    info = {
        "time": elapsed_times,
        "gpu_util": gpu_util,
        "gpu_mem": gpu_mem,
        "cpu_util": cpu_util,
        "ram_util": ram_util,
        "batch_size_start_times": batch_size_timestamps,
    }

    return info

def draw_errorbar_plot(x, y, yerr, xlabel, ylabel, save_filename, title=None, xscale="linear", yscale="linear", img_type="png"):
    fig, ax = plt.subplots(dpi=100)

    ax.errorbar(x, y, yerr=yerr, fmt="-o", color="#5790fc")

    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)

    ax.set_xscale(xscale)
    ax.set_yscale(yscale)

    if (xscale == "log"):
        ax.set_xlim(left=1)
    else:
        ax.set_xlim(left=0)
    
    if (yscale == "log"):
        ax.set_ylim(bottom=1)
    else:
        ax.set_ylim(bottom=0)
    
    if (title is not None):
        ax.set_title(title)

    plt.savefig(f"{save_filename}.{img_type}", dpi="figure")
    plt.close()

def draw_timeseries_plot(timing_info, save_filename, title=None, img_type="png"):
    fig, ax = plt.subplots(dpi=100)

    ax.plot(timing_info["time"], timing_info["gpu_util"], label="GPU Util", color="#5790fc")
    ax.plot(timing_info["time"], timing_info["gpu_mem"], label="GPU Memory", color="#e42536")
    ax.plot(timing_info["time"], timing_info["cpu_util"], label="CPU Util", color="#f89c20")
    ax.plot(timing_info["time"], timing_info["ram_util"], label="RAM", color="#7a21dd")

    for ts in timing_info["batch_size_start_times"]:
        ax.axvline(x=ts, color="#9c9ca1", linestyle="--", alpha=0.5)

    ax.set_xlabel("Elapsed Time (s)")
    ax.set_ylabel("Resource Usage (%)")
    
    ax.set_xlim(left=0)
    ax.set_ylim(bottom=0, top=100)
    
    ax.legend()

    if (title is not None):
        ax.set_title(title)

    plt.savefig(f"{save_filename}.{img_type}", dpi="figure")
    plt.close()

def main(args):
    save_folder = os.path.dirname(args.timings_file)

    with open(args.timings_file, "r") as f:
        timings = json.load(f)
    
    draw_errorbar_plot(
        timings["batch_size"], timings["throughput"], timings["throughput_err"],
        "Batch size", "Throughput (infer/sec)",
        os.path.join(save_folder, "throughput"),
        title=args.title, xscale="log"
    )

    draw_errorbar_plot(
        timings["batch_size"], timings["latency"], timings["latency_err"],
        "Batch size", "Processing tme (ms)",
        os.path.join(save_folder, "latency"),
        title=args.title, xscale="log"
    )

    logging.info("Reading and plotting resource usage data.")

    client_log = os.path.join(save_folder, "client_output.log")
    resource_log = os.path.join(save_folder, "resource_usage.csv")
    resource_usage_info = read_resource_usage(resource_log, client_log)
    draw_timeseries_plot(resource_usage_info, os.path.join(save_folder, "resource_usage"), title=args.title)

if (__name__ == "__main__"):
    parser = argparse.ArgumentParser()
    parser.add_argument("timings_file", type=str, help="JSON file containing scan timing information")
    parser.add_argument("--title", type=str, default=None, help="A title to put on the plots")
    parser.add_argument("--verbose", action="store_true", help="Enable debug-level logging")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=log_level, format="[%(asctime)s - %(module)s - %(levelname)s]: %(message)s")

    logging.info("Plotting timings from batch size scan.")
    main(args)
    logging.info("Finished.")
