"""Exercise the native preparation core on bounded, recorded numerical fields.

Reads project JSON only. Does not open LMU or copy process-object layouts.
"""

import datetime
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile


def main():
    project = Path(__file__).resolve().parents[1]
    source = project / "rigid-body-layout-observation.json"
    record = json.loads(source.read_text())
    bodies = record["bodies"]
    if not 1 <= len(bodies) <= 32:
        raise ValueError("Body count outside preparation capacity")
    lines = [str(len(bodies))]
    for body in bodies:
        f = body["fields"]
        values = []
        for key, count in (
            ("inverse_mass", 1),
            ("local_inverse_inertia_diagonal", 3),
            ("world_position", 3),
            ("world_velocity", 3),
            ("orientation_row_major", 9),
            ("world_angular_momentum", 3),
            ("world_inverse_inertia_symmetric", 6),
            ("world_angular_velocity", 3),
        ):
            field = f[key]
            if len(field) != count or not all(math.isfinite(x) for x in field):
                raise ValueError(f"Invalid recorded {key}")
            values.extend(field)
        lines.append(" ".join(format(x, ".17g") for x in values))
    binary = project / "bin/rigid-restore-fixture.exe"
    with tempfile.TemporaryDirectory(prefix="lmu-rigid-values-") as directory:
        numeric_file = Path(directory) / "values.txt"
        numeric_file.write_text("\n".join(lines) + "\n", encoding="ascii")
        result = subprocess.run(
            [str(binary), str(numeric_file)], capture_output=True, text=True, timeout=15
        )
    report = {
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "source": source.name,
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "fixture_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "bodies": len(bodies),
        "exit_code": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "live_restore_operations": 0,
        "limitations": "Offline numerical reconstruction from asynchronous monitor samples; no contact reconstruction or live restore.",
    }
    (project / "logs/rigid-restore-core-test.json").write_text(json.dumps(report, indent=2) + "\n")
    print(result.stdout, end="")
    if result.returncode:
        raise RuntimeError(result.stderr or "Native fixture failed")


if __name__ == "__main__":
    main()
