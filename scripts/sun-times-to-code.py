#!/usr/bin/env python3
import argparse
import sys
from datetime import datetime
from io import FileIO


C_SOURCE_OUTPUT = "open_close_times.cpp"
C_HEADER_OUTPUT = "open_close_times.h"
PYTHON_OUTPUT = "open_close_times.py"
MONTH_COMMENT = "Open-Close times specified as 2D array for each month"
MONTH_PREFIX = "OC_TIME_"
MONTHS = [
    "JANUARY",
    "FEBRUARY",
    "MATCH",
    "APRIL",
    "JUNE",
    "JULY",
    "AUGUST",
    "SEPTEMBER",
    "OCTOBER",
    "NOVEMBER",
    "DECEMBER",
]


def print_header_c(f: FileIO):
    f.write(
        "/*\n * This file was auto-generated by the sun-times-to-code.py script.\n"
    )
    f.write(f" * Generated on the {datetime.now()}.\n")
    f.write(" */\n")


def print_header_py(f: FileIO):
    f.write("\"\"\"\n")
    f.write("This file was auto-generated by the sun-times-to-code.oy script.\n")
    f.write(f"Generate on the {datetime.now()}.\n")
    f.write("\"\"\"\n")


def write_month_definition_c(f: FileIO, month_idx: int):
    f.write(f"const int {MONTH_PREFIX}{MONTHS[month_idx]}[31][4] = {{\n")
    for day_idx in range(0, 31):
        sep = ","
        if day_idx == 30:
            sep = ""
        f.write(f"    {{0, 0, 0 ,0}}{sep}\n")
    f.write("};\n")


def write_month_declaration_c(f: FileIO, month_idx: int):
    f.write(f"extern const int {MONTH_PREFIX}{MONTHS[month_idx]}[31][4];\n")


def write_month_declaration_py(f: FileIO, month_idx: int):
    f.write(f"{MONTH_PREFIX}{MONTHS[month_idx]} = [\n")
    for day_idx in range(0, 31):
        sep = ","
        if day_idx == 30:
            sep = ""
        f.write(f"    [0, 0, 0, 0]{sep}\n")
    f.write("]\n")


def main():
    parser = argparse.ArgumentParser(
        "Converter script to generate look-up table code for sun times"
    )
    parser.add_argument(
        "-t", "--type", help="Output code language", choices=["c", "py"], default="c"
    )
    print("-- Sun times to code generator --")
    args = parser.parse_args()
    if args.type == "c":
        print(f"Generating {C_SOURCE_OUTPUT}")
        with open(C_SOURCE_OUTPUT, "w") as f:
            print_header_c(f)
            f.write("\n")
            for idx in range(0, 11):
                write_month_definition_c(f, idx)
                f.write("\n")
        print(f"Generating {C_HEADER_OUTPUT}")
        with open(C_HEADER_OUTPUT, "w") as f:
            print_header_c(f)
            f.write("\n")
            for idx in range(0, 11):
                write_month_declaration_c(f, idx)
            f.write("\n")
    elif args.type == "py":
        print(f"Generating {PYTHON_OUTPUT}")
        with open(PYTHON_OUTPUT, "w") as f:
            print_header_py(f)
            f.write("\n")
            for idx in range(0, 11):
                write_month_declaration_py(f, idx)
                f.write("\n")
    else:
        print("No supported output code type detected")
        sys.exit(0)
    print("Done")


if __name__ == "__main__":
    main()
