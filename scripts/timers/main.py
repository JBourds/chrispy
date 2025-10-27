#!/usr/bin/env python3
"""
Plot Desired Clock Rate vs. Actual from a CSV file.

Usage:
    python plot_clock_rate.py df.csv
"""

import sys

import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
import numpy as np
import pandas as pd


def plot(df: pd.DataFrame):
    plt.figure(figsize=(10, 6))
    percent_deltas = (df["Actual"] - df["Desired"]) / df["Actual"] * 100
    plt.grid()
    plt.gca().yaxis.set_major_formatter(mtick.PercentFormatter(xmax=100))
    plt.plot(df["Desired"], percent_deltas, label="% Different From Desired Clock Rate")
    plt.title("Desired vs Actual Clock Rate", fontsize=18, fontweight="bold")
    plt.xlabel("Desired Clock Rate", fontsize=14)
    plt.ylabel("Actual Clock Rate", fontsize=14)
    plt.legend()
    plt.tight_layout()
    plt.savefig("timer_results.png")

    plt.show()


def compute_stats(df: pd.DataFrame):
    actual = df["Actual"]
    desired = df["Desired"]
    delta = np.abs(actual - desired)
    max_d = np.max(delta)
    mean_d = np.mean(delta)
    std_d = np.std(delta)
    print("Absolute Values:")
    print(f"Delta Max: {max_d}")
    print(f"Delta Mean: {mean_d}")
    print(f"Delta Std: {std_d}")

    delta_percents = delta / df["Actual"]
    max_index = np.argmax(delta_percents)
    max_dp = delta_percents[max_index]
    mean_dp = np.mean(delta_percents)
    std_dp = np.std(delta_percents)
    print("Percentages:")
    print(
        f"Delta Max: {max_dp:%} - Got {actual[max_index]}, Wanted {desired[max_index]}"
    )
    print(f"Delta Mean: {mean_dp:%}")
    print(f"Delta Std: {std_dp:%}")


def main():
    if len(sys.argv) != 2:
        print("Usage: python plot_clock_rate.py <path_to_csv>")
        sys.exit(1)

    csv_path = sys.argv[1]

    try:
        df = pd.read_csv(csv_path)
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)
    expected_cols = ["Desired", "Actual"]
    if not all(col in df.columns for col in expected_cols):
        print(f"CSV must contain the columns: {expected_cols}")
        print(f"Found columns: {list(df.columns)}")
        sys.exit(1)

    plot(df)
    compute_stats(df)


if __name__ == "__main__":
    main()
