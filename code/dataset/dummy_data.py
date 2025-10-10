#!/usr/bin/env python3
"""
Generate a dummy graph stream dataset.

Each line has: <src_node> <dst_node> <label> <timestamp>

Examples:
    python generate_graph_stream.py --rows 8 \
        --src-range 1 500000 --dst-range 1 500000 \
        --label-range 1 5 --time-range 1341100900 1341101300 --seed 42

    python generate_graph_stream.py --rows 100000 \
        --sequential-time --time-start 1700000000 --time-step 2

    python generate_graph_stream.py --rows 20 --no-self-loops \
        --repeat-edge-prob 0.2

    python generate_graph_stream.py --rows 1000 --out dataset.txt

Notes:
- Ranges are inclusive.
- If both --time-range and sequential time options are provided, sequential mode wins (a warning is printed).
"""

import argparse
import random
import sys
from typing import Tuple, List, Optional

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Generate a dummy graph stream dataset."
    )
    p.add_argument("--rows", type=int, required=True,
                   help="Number of rows (edges) to generate.")
    p.add_argument("--src-range", type=int, nargs=2, metavar=("MIN", "MAX"),
                   default=[1, 1_000_000], help="Inclusive range for source node IDs.")
    p.add_argument("--dst-range", type=int, nargs=2, metavar=("MIN", "MAX"),
                   default=[1, 1_000_000], help="Inclusive range for target node IDs.")
    p.add_argument("--label-range", type=int, nargs=2, metavar=("MIN", "MAX"),
                   default=[1, 5], help="Inclusive range for edge label values.")
    p.add_argument("--time-range", type=int, nargs=2, metavar=("MIN_TS", "MAX_TS"),
                   help="Inclusive range for timestamps (epoch seconds) when using random time mode.")
    p.add_argument("--sequential-time", action="store_true",
                   help="If set, timestamps are generated sequentially.")
    p.add_argument("--time-start", type=int, default=None,
                   help="Starting timestamp for sequential mode (default: min timestamp or now).")
    p.add_argument("--time-step", type=int, default=1,
                   help="Step increment for sequential timestamps (default: 1).")
    p.add_argument("--seed", type=int, default=None,
                   help="Random seed for reproducibility.")
    p.add_argument("--out", type=str, default="-",
                   help="Output file path or '-' for stdout (default).")
    p.add_argument("--no-self-loops", action="store_true",
                   help="Avoid src == dst. Will retry a limited number of times.")
    p.add_argument("--repeat-edge-prob", type=float, default=0.0,
                   help="Probability (0..1) that a row reuses a previously generated (src,dst,label) but with a new timestamp.")
    p.add_argument("--max-retries", type=int, default=20,
                   help="Max retries to avoid self-loops before giving up.")
    return p.parse_args()

def inclusive_rand(rng: random.Random, rng_pair: Tuple[int, int]) -> int:
    a, b = rng_pair
    if a > b:
        raise ValueError(f"Invalid range: {a} > {b}")
    return rng.randint(a, b)

def validate_args(args: argparse.Namespace):
    for name in ("src_range", "dst_range", "label_range"):
        rng_pair = getattr(args, name.replace("-", "_"))
        if rng_pair[0] > rng_pair[1]:
            raise SystemExit(f"--{name.replace('_','-')} invalid: min > max")
    if args.time_range and args.time_range[0] > args.time_range[1]:
        raise SystemExit("--time-range invalid: min > max")
    if not (0.0 <= args.repeat_edge_prob <= 1.0):
        raise SystemExit("--repeat-edge-prob must be in [0,1]")
    if args.rows <= 0:
        raise SystemExit("--rows must be positive")

def main():
    args = parse_args()
    validate_args(args)

    rng = random.Random(args.seed)

    # Determine time generation mode
    sequential = args.sequential_time
    if sequential and args.time_range:
        print("Warning: --sequential-time overrides --time-range", file=sys.stderr)

    # Setup timestamp generation
    if sequential:
        # Determine start
        if args.time_start is not None:
            current_ts = args.time_start
        else:
            # If user also gave time_range, we can start at its min; else fallback to a default.
            if args.time_range:
                current_ts = args.time_range[0]
            else:
                # Fallback: some arbitrary epoch base
                current_ts = 1_700_000_000
        def next_timestamp():
            nonlocal current_ts
            ts = current_ts
            current_ts += args.time_step
            return ts
    else:
        if not args.time_range:
            # Provide a default time range if not given
            # Example range approximating some epoch interval
            args.time_range = [1_600_000_000, 1_600_100_000]
        def next_timestamp():
            return inclusive_rand(rng, tuple(args.time_range))

    # Prepare output
    out_fh: Optional[sys.stdout] = None
    if args.out == "-" or args.out.lower() == "stdout":
        fh = sys.stdout
    else:
        fh = open(args.out, "w", encoding="utf-8")

    edge_cache: List[Tuple[int, int, int]] = []

    try:
        for i in range(args.rows):
            reuse = False
            if edge_cache and rng.random() < args.repeat_edge_prob:
                reuse = True

            if reuse:
                src, dst, label = edge_cache[rng.randint(0, len(edge_cache)-1)]
            else:
                # Generate new triple
                retries = 0
                while True:
                    src = inclusive_rand(rng, tuple(args.src_range))
                    dst = inclusive_rand(rng, tuple(args.dst_range))
                    if args.no_self_loops and src == dst:
                        retries += 1
                        if retries >= args.max_retries:
                            # Accept self-loop if we cannot find alternative
                            break
                        continue
                    break
                label = inclusive_rand(rng, tuple(args.label_range))
                edge_cache.append((src, dst, label))

            ts = next_timestamp()
            fh.write(f"{src} {dst} {label} {ts}\n")
    finally:
        if fh is not sys.stdout:
            fh.close()

if __name__ == "__main__":
    main()