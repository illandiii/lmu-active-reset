"""Run a Sector module or the Python tests from any working directory."""

import os
from pathlib import Path
import re
import runpy
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    python_root = str(root / "src/python")
    sys.path.insert(0, str(root))
    sys.path.insert(0, python_root)
    # Child capture/replay processes inherit the same explicit package path.
    os.environ["PYTHONPATH"] = os.pathsep.join(
        [python_root, str(root), *filter(None, [os.environ.get("PYTHONPATH", "")])]
    )
    if len(sys.argv) < 2:
        raise SystemExit("Usage: python scripts/sector.py <area.module | test> [arguments]")
    command, arguments = sys.argv[1], sys.argv[2:]
    if command == "test":
        sys.argv = ["unittest", "discover", "-s", str(root / "tests/python"), *arguments]
        runpy.run_module("unittest", run_name="__main__")
        return
    if not re.fullmatch(r"[a-z_]+\.[a-z0-9_]+", command):
        raise SystemExit("Expected a Sector module such as runtime.start_reset_hotkeys")
    sys.argv = [command, *arguments]
    runpy.run_module("sector." + command, run_name="__main__", alter_sys=True)


if __name__ == "__main__":
    main()
