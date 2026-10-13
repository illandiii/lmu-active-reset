"""Observe asynchronous control-lock admission at the natural outer boundary.

The gate is offered for 25 ms every 500 ms while the offline observation lease
is valid. Pin counters are cumulative; no vehicle or scoring fields are written.
"""

import argparse

# Shared ABI is deliberately identical; the mapping and magic isolate v4.
from run_boundary_observer_v3_25661166 import Frame, IPC, run as run_phase


def run(pid, seconds, execute, offline):
    return run_phase(pid, seconds, execute, offline, variant=4)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--seconds", type=int, default=12)
    parser.add_argument("--load", action="store_true")
    parser.add_argument("--offline-launch-confirmed", action="store_true")
    args = parser.parse_args()
    if not 2 <= args.seconds <= 60:
        parser.error("Duration must be 2..60 seconds")
    run(args.pid, args.seconds, args.load, args.offline_launch_confirmed)
