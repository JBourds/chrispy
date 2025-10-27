#!/usr/bin/env python3
"""
Plot Desired Clock Rate vs. Actual from a CSV file.

Usage:
    python plot_clock_rate.py data.csv
"""

import sys

import matplotlib.pyplot as plt
import pandas as pd


def main():
    if len(sys.argv) != 2:
        print("Usage: python plot_clock_rate.py <path_to_csv>")
        sys.exit(1)

    csv_path = sys.argv[1]

    try:
        data = pd.read_csv(csv_path)
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)

    expected_cols = ["Desired", "Actual"]
    if not all(col in data.columns for col in expected_cols):
        print(f"CSV must contain the columns: {expected_cols}")
        print(f"Found columns: {list(data.columns)}")
        sys.exit(1)

    plt.figure(figsize=(10, 6))
    max_val = max(data["Desired"].max(), data["Actual"].max())
    plt.plot(data["Desired"], data["Actual"], label="Actual")
    plt.plot([0, max_val], [0, max_val], "r--", label="Ideal (y = x)")

    plt.grid()
    plt.title("Desired vs Actual Clock Rate", fontsize=18, fontweight="bold")
    plt.xlabel("Desired Clock Rate", fontsize=14)
    plt.ylabel("Actual Clock Rate", fontsize=14)
    plt.legend()
    plt.tight_layout()

    plt.show()


if __name__ == "__main__":
    main()
