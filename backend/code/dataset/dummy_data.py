#!/usr/bin/env python3
"""
Generate a simple well-connected graph stream dataset.

Each line has: <src_node> <dst_node> <label> <timestamp>

Examples:
    python dummy_data.py --nodes 10 --edges 50
    python dummy_data.py --nodes 100 --edges 500 --self-loops
    python dummy_data.py --nodes 20 --edges 100 --duplicate-prob 0.3
    python dummy_data.py --nodes 50 --edges 200 --labels 5 --out dummy.txt
"""

import argparse
import random

def parse_args():
    p = argparse.ArgumentParser(description="Generate a well-connected graph stream.")
    p.add_argument("--nodes", type=int, required=True,
                   help="Number of nodes (numbered 1..N)")
    p.add_argument("--edges", type=int, required=True,
                   help="Number of edges to generate")
    p.add_argument("--labels", type=int, default=1,
                   help="Number of distinct labels (1..L)")
    p.add_argument("--self-loops", action="store_true",
                   help="Allow self-loops (src == dst)")
    p.add_argument("--duplicate-prob", type=float, default=0.0,
                   help="Probability of reusing an edge with new timestamp (0..1)")
    p.add_argument("--seed", type=int, default=None,
                   help="Random seed for reproducibility")
    p.add_argument("--out", type=str, default="-",
                   help="Output file or '-' for stdout")
    return p.parse_args()

def main():
    args = parse_args()
    
    if args.edges <= 0 or args.nodes <= 0:
        raise SystemExit("--nodes and --edges must be positive")
    if args.labels <= 0:
        raise SystemExit("--labels must be positive")
    if not (0.0 <= args.duplicate_prob <= 1.0):
        raise SystemExit("--duplicate-prob must be in [0,1]")
    
    rng = random.Random(args.seed)
    
    # Open output
    fh = open(args.out, "w") if args.out != "-" else None
    
    edge_cache = []
    timestamp = 1
    
    try:
        for _ in range(args.edges):
            # Decide if reusing existing edge
            if edge_cache and rng.random() < args.duplicate_prob:
                src, dst, label = rng.choice(edge_cache)
            else:
                # Generate new unique edge
                max_attempts = 1000
                for _ in range(max_attempts):
                    src = rng.randint(1, args.nodes)
                    dst = rng.randint(1, args.nodes)
                    
                    # Avoid self-loops if requested
                    if not args.self_loops and src == dst and args.nodes > 1:
                        continue
                    
                    label = rng.randint(1, args.labels)
                    
                    # Check if this edge is new
                    if (src, dst, label) not in edge_cache:
                        edge_cache.append((src, dst, label))
                        break
                else:
                    # Fallback if we can't find a unique edge
                    raise SystemExit("Unable to generate enough unique edges. Try fewer edges or more nodes/labels.")
            
            line = f"{src} {dst} {label} {timestamp}\n"
            if fh:
                fh.write(line)
            else:
                print(line, end='')
            
            timestamp += 1
    finally:
        if fh:
            fh.close()

if __name__ == "__main__":
    main()