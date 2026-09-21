"""Measure descending-neuron readout rates at rest and under retina drive.

Use this to pick --escape-threshold / --steer-threshold and to sanity-check
that the pathways in this brain build behave (LC4/LPLC2 -> DNp01,
LC10a -> DNa02). Prints spikes/step for each readout group per condition.

    python calibrate.py --steps 150
"""
from __future__ import annotations

import argparse
import sys
import os

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fly_controller import READOUTS  # noqa: E402


def run(brain, groups, inject, steps: int) -> dict[str, float]:
    counts = {k: 0 for k in groups}
    for _ in range(steps):
        fs = set(map(int, brain.step(inject=inject)))
        for k, members in groups.items():
            counts[k] += len(fs & members)
    return {k: v / steps for k, v in counts.items()}


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--steps", type=int, default=150)
    p.add_argument("--device", default="auto")
    p.add_argument("--seed", type=int, default=64)
    args = p.parse_args()

    from flybrain import FlyBrain
    brain = FlyBrain(device=args.device, seed=args.seed, sensory_input=False)
    groups = {f"{n}_{s}": set(map(int, brain.groups[f"{n}_{s}"])) for n in READOUTS for s in "LR"}
    cells = {t: {s: brain.cells([t], s) for s in "LR"} for t in ("LPLC2", "LC4", "LC10a")}

    conditions = {
        "rest": [],
        "loom L 0.4": [(cells["LPLC2"]["L"], 0.4)],
        "loom L 0.8": [(cells["LPLC2"]["L"], 0.8)],
        "threat L 0.8 (LC4)": [(cells["LC4"]["L"], 0.8)],
        "loom+threat L 0.8": [(cells["LPLC2"]["L"], 0.8), (cells["LC4"]["L"], 0.8)],
        "loom+threat both 0.8": [(cells["LPLC2"]["L"], 0.8), (cells["LC4"]["L"], 0.8),
                                 (cells["LPLC2"]["R"], 0.8), (cells["LC4"]["R"], 0.8)],
        "chase L 0.6 (LC10a)": [(cells["LC10a"]["L"], 0.6)],
        "chase R 0.6 (LC10a)": [(cells["LC10a"]["R"], 0.6)],
    }
    print("warming up ...", flush=True)
    run(brain, groups, [], 20)
    header = f"{'condition':24s}" + "".join(f"{k:>11s}" for k in groups)
    print(header)
    for name, inject in conditions.items():
        run(brain, groups, [], 40)          # settle back toward rest between conditions
        rates = run(brain, groups, inject, args.steps)
        print(f"{name:24s}" + "".join(f"{rates[k]:11.3f}" for k in groups), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
