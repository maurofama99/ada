import argparse
import subprocess
import re
import os
import tempfile
from skopt import gp_minimize
from skopt.space import Integer, Real
from skopt.utils import use_named_args
import itertools

# Global evaluation counter for logging
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
    # g++ -std=c++17 -O3 -Wno-c++11-extensions -Wno-c++17-extensions -Wno-deprecated -o code/scripts/parameter_selection/run_main code/main.cpp
    parser.add_argument('--binary', help='Path to C++ executable (e.g. ./main)')
    parser.add_argument('--base_config', help='Baseline config file')
    parser.add_argument('--n_calls', type=int, default=20, help='Number of Bayesian optimization calls')
    args = parser.parse_args()

    # load baseline and run once
    base = load_config(args.base_config)
    print("Running baseline...")
    baseline_metrics = run_binary(args.binary, args.base_config)
    print("Baseline metrics:", baseline_metrics)

    # original total size
    orig_size = float(base['size'])
    slide = int(base['slide'])

    # define search space: lives, scale factor for size, zscore
    space = [
        Integer(4, 5, name='lives'),
        Real(1.0, 1.5, name='scale'),
        Real(3, 6, name='zscore')
    ]

    @use_named_args(space)
    def objective(lives, scale, zscore):
        i = next(eval_counter)
        # compute size to exactly meet constraint: size = ceil(orig_size / lives * scale)
        size = max(int((orig_size / lives) * scale), slide+10)
        print(f"\nEval #{i}: lives={lives}, size={size}, zscore={zscore:.2f}", flush=True)

        # run binary
        params = {'lives': lives, 'size': size, 'zscore': zscore}
        tmp = tempfile.NamedTemporaryFile(delete=False, suffix='.txt')
        save_config(params, base, tmp.name)
        m = run_binary(args.binary, tmp.name)
        os.unlink(tmp.name)

        rp, rp0 = m['resulting paths'], baseline_metrics['resulting paths']
        mem = m['windows created'] * m['avg window size']
        mem0 = baseline_metrics['windows created'] * baseline_metrics['avg window size']
        thr = rp / m['execution time']

        # result count constraint
        if rp < rp0 or rp > rp0 * 1.05:
            penalty = 1e5 + abs(rp - rp0)
            print(f"  -> resulting paths {rp:.0f} outside [{rp0:.0f}, {rp0*1.05:.0f}], penalty {penalty:.1f} (exec. time {m['execution time']})")
            return penalty

        # memory constraint
        if mem > mem0:
            #penalty = 1e4 + (mem - mem0 * 0.90)
            print(f" Warning -> memory {mem:.1f} > target {mem0:.1f}")
            #return penalty

        obj = -thr
        print(f"  -> throughput {thr:.2f}, resulting paths {rp:.0f}, exec. time {m['execution time']} objective {obj:.2f} (throughput {thr:.2f})\n")
        return obj

    print(f"Starting Bayesian optimization ({args.n_calls} calls)...")
    res = gp_minimize(objective, space,
                      n_calls=args.n_calls,
                      n_initial_points=min(10, args.n_calls),
                      acq_func='EI',
                      random_state=0)

    print("Optimization done.")
    print(f"Best lives, scale, zscore: {res.x}")
    # convert scale back to size for best
    best_lives, best_scale, best_z = res.x
    best_size = int((orig_size / best_lives) * best_scale)
    print(f"Best size: {best_size}")
    print(f"Best objective: {res.fun}")
