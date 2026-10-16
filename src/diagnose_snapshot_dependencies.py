"""Quantify dependency mismatches from recorded snapshots, without game access."""

import argparse
import collections
import json
import math
from pathlib import Path


def diagnose(rows):
    errors = {"wheel": [], "tire": []}
    statuses = collections.Counter()
    changed = set()
    first = rows[0]
    for row in rows:
        statuses[str(row["mechanical_status"])] += 1
        for w in range(4):
            d = row["dependencies"]["wheels"][w]
            expected = 1 / ((2 * d["term_4b8"] + d["term_5d8"]) * d["factor_4f0"] + d["base_130"])
            actual = row["bodies"][row["links"]["wheels"][w]]["mass"]["inverse_inertia_local"][0]
            errors["wheel"].append(abs(actual / expected - 1))
            tire = row["dependencies"]["tires"][w]
            body = row["bodies"][row["links"]["tires"][w]]["mass"]
            initial = first["bodies"][first["links"]["tires"][w]]["mass"]
            if body["inverse_mass"] != initial["inverse_mass"]:
                changed.add(w)
            for j in range(3):
                expected = 1 / (tire["total_table"][j] - sum(s[j] for s in tire["sectors"]))
                errors["tire"].append(abs(body["inverse_inertia_local"][j] / expected - 1))
    assert all(math.isfinite(x) for values in errors.values() for x in values)
    return dict(
        frames=len(rows),
        mechanical_statuses=dict(statuses),
        relative_errors={
            k: dict(
                maximum=max(v),
                outside_32_double_epsilon=sum(x > 32 * 2**-52 for x in v),
                values=len(v),
            )
            for k, v in errors.items()
        },
        tires_with_mass_change=sorted(changed),
        limitations="Tire mass dependencies are not captured. Inertia mismatch cause remains unproven; capture phase is a hypothesis. No tolerance relaxed or game state changed.",
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    result = diagnose(rows)
    a.frames.with_suffix(".dependencies-diagnostic.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps(result, indent=2))
