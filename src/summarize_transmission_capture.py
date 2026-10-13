"""Summarize observed transitions, without treating finite values as full coverage."""

import argparse, hashlib, json, math
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames", type=Path)
    args = parser.parse_args()
    rows = [json.loads(line) for line in args.frames.open() if line.strip()]
    if not rows:
        raise ValueError("No captured frames")
    names = ["shaft_0", "shaft_8", "gear_64", "input_18", "input_20", "input_30", "input_38"]

    def stats(values):
        if not all(math.isfinite(v) for v in values):
            raise ValueError("Non-finite capture")
        return {
            "min": min(values),
            "max": max(values),
            "changes": sum(a != b for a, b in zip(values, values[1:])),
        }

    result = {
        "frames": len(rows),
        "span_s": rows[-1]["simulation_time"] - rows[0]["simulation_time"],
        "transmission": {name: stats([r["transmission"][name] for r in rows]) for name in names},
        "outputs": {
            name: stats([r["driveline_outputs"][name] for r in rows])
            for name in rows[0]["driveline_outputs"]
        },
        "terminal_failures": sorted(set(r.get("terminal_failure", 0) for r in rows)),
        "native_bank_statuses": sorted(set(r["bank_status"] for r in rows)),
        "native_plan_statuses": sorted(set(r["plan_status"] for r in rows)),
        "native_binding_statuses": sorted(set(r["binding_status"] for r in rows)),
        "native_saved": rows[-1]["bank_saves"],
        "native_prepared": rows[-1]["bank_preparations"],
        "native_rejected": rows[-1]["bank_rejections"],
        "source_sha256": hashlib.sha256(args.frames.read_bytes()).hexdigest(),
        "physics_writes": 0,
        "full_restore_supported": False,
        "limitations": "Observed transitions only; no physical restore. Tiny shaft values are reported without numerical correction.",
    }
    args.frames.with_suffix(".transmission-summary.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
