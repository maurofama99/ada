import argparse
import subprocess
import re
import os
import tempfile
from skopt import gp_minimize
from skopt.space import Integer, Real
from skopt.utils import use_named_args
import itertools
import sys
from datetime import datetime

# === Set up logging ===
log_dir = "parameter_search_logs"
os.makedirs(log_dir, exist_ok=True)
log_path = os.path.join(log_dir, f"run_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt")
log_file = open(log_path, "w")

class Logger:
    def __init__(self, file):
        self.terminal = sys.stdout
        self.file = file

    def write(self, message):
        self.terminal.write(message)
        self.file.write(message)
        self.file.flush()

    def flush(self):
        self.terminal.flush()
        self.file.flush()

sys.stdout = Logger(log_file)
sys.stderr = Logger(log_file)

# === Global evaluation counter for logging ===
eval_counter = itertools.count(1)

def parse_output(output_text):
    """
    Parse the C++ program output and return a dict of metrics.
    """
    metrics = {}
    for line in output_text.splitlines():
        if ":" in line:
            key, val = line.split(':', 1)
            metrics[key.strip()] = float(val.strip())
    return metrics

def load_config(path):
    base = {}
    with open(path) as f:
        for line in f:
            if "=" in line:
                k, v = line.strip().split('=', 1)
                base[k] = v
    return base

def save_config(params, base_config, path):
    # merge tuned params into base and write
    cfg = base_config.copy()
    cfg.update({
        'lives': str(params['lives']),
        'size': str(params['size']),
        'zscore': str(params['zscore'])
    })
    with open(path, 'w') as f:
        for k, v in cfg.items():
            f.write(f"{k}={v}\n")

def run_binary(binary, config_path):
    # run and capture
    res = subprocess.run([binary, config_path], capture_output=True, text=True, check=True)
    return parse_output(res.stdout)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', help='Path to C++ executable (e.g. ./main)')
    parser.add_argument('--base_config', help='Baseline config file')
    parser.add_argument('--n_calls', type=int, default=30, help='Number of Bayesian optimization calls')
    parser.add_argument('--min_scale', type=float, default=0.11)
    parser.add_argument('--max_scale', type=float, default=0.2)
    parser.add_argument('--min_zscore', type=float, default=1.8)
    parser.add_argument('--max_zscore', type=float, default=2.8)
    parser.add_argument('--min_lives', type=int, default=8)
    parser.add_argument('--max_lives', type=int, default=12)

    args = parser.parse_args()

    # define search space
    min_scale = args.min_scale
    max_scale = args.max_scale
    min_zscore = args.min_zscore
    max_zscore = args.max_zscore
    min_lives = args.min_lives
    max_lives = args.max_lives

    # load baseline and run once
    base = load_config(args.base_config)
    print("Running baseline")
    baseline_metrics = run_binary(args.binary, args.base_config)
    print("Baseline metrics:", baseline_metrics)

    orig_size = float(base['size'])
    slide = int(base['slide'])

    space = [
        Integer(min_lives, max_lives, name='lives'),
        Real(min_scale, max_scale, name='scale'),
        Real(min_zscore, max_zscore, name='zscore')
    ]

    @use_named_args(space)
    def objective(lives, scale, zscore):
        i = next(eval_counter)
        size = orig_size * scale
        if size <= slide:
            print(f"Eval #{i}: size {size:.2f} <= slide {slide}, skipping.")
            return 1e6  # large penalty

        print(f"\nEval #{i}: lives={lives}, size={size:.2f}, zscore={zscore:.2f}", flush=True)

        params = {'lives': lives, 'size': size, 'zscore': zscore}
        tmp = tempfile.NamedTemporaryFile(delete=False, suffix='.txt')
        save_config(params, base, tmp.name)
        m = run_binary(args.binary, tmp.name)
        os.unlink(tmp.name)

        rp, rp0 = m['resulting paths'], baseline_metrics['resulting paths']
        mem = m['windows created'] * m['avg window size']
        mem0 = baseline_metrics['windows created'] * baseline_metrics['avg window size']
        thr = rp / m['execution time']

        if mem < mem0 * 0.9:
            # Memory too low → encourage scale/zscore ↑
            penalty = 1e4 + (mem0 * 0.9 - mem) + (1.0 - scale) * 500 + (2.8 - zscore) * 300
            print(f" Warning -> memory {mem:.1f} < 90% of baseline {mem0:.1f}, increasing scale/zscore recommended")
            print(f"  -> throughput {thr:.2f}, resulting paths {rp:.0f}, exec. time {m['execution time']}, mem {mem:.1f}, penalty {penalty:.2f}")
            return penalty

        if mem > mem0 * 1.10:
            # Memory too high → encourage scale/zscore ↓
            penalty = 1e4 + (mem - mem0 * 1.10) + scale * 500 + zscore * 300
            print(f" Warning -> memory {mem:.1f} > 110% of baseline {mem0:.1f}, decreasing scale/zscore recommended")
            print(f"  -> throughput {thr:.2f}, resulting paths {rp:.0f}, exec. time {m['execution time']}, mem {mem:.1f}, penalty {penalty:.2f}")
            return penalty

        obj = -thr
        print(f"  -> throughput {thr:.2f}, resulting paths {rp:.0f}, exec. time {m['execution time']}, mem {mem:.1f}, objective {obj:.2f}")
        return obj

    print(f"Starting Bayesian optimization ({args.n_calls} calls)...")
    initial_points = [
        [10, 0.13, 2],
        [11, 0.15, 2.5],
        [9, 0.12, 1.8]
    ]

    res = gp_minimize(
        objective, space,
        n_calls=args.n_calls,
        x0=initial_points,
        y0=[objective(p) for p in initial_points],
        n_initial_points=0,
        acq_func='EI',
        random_state=0
    )


    print("Optimization done.")
    print(f"Best lives, scale, zscore: {res.x}")
    best_lives, best_scale, best_z = res.x
    best_size = int((orig_size / best_lives) * best_scale)
    print(f"Best size: {best_size}")
    print(f"Best objective: {res.fun}")

    log_file.close()
