"""Summarize observed shift transitions without assuming every shift was sampled."""

import argparse, json, hashlib
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    assert rows
    names = ("maximum", "selected", "requested", "previous", "direction", "prior_direction")

    def series(name, values):
        return dict(
            field=name,
            minimum=min(values),
            maximum=max(values),
            changes=sum(x != y for x, y in zip(values, values[1:])),
        )

    changes = [
        series(name, [r["shift_observed"][name] for r in rows])
        for name in ("timer_4ac0", "control_4668", "clock_a8", "event_50", "dt_b0")
    ]
    for i in range(2):
        changes.append(
            series("accumulator_" + str(i), [r["shift_observed"]["accumulators"][i] for r in rows])
        )
    report = dict(
        frames=len(rows),
        span_s=rows[-1]["simulation_time"] - rows[0]["simulation_time"],
        gears={
            name: sorted({r["shift_observed"]["gears"][i] for r in rows})
            for i, name in enumerate(names)
        },
        fields=changes,
        bank_saves=rows[-1]["bank_saves"],
        bank_preparations=rows[-1]["bank_preparations"],
        bank_rejections=rows[-1]["bank_rejections"],
        plan_statuses=sorted({r["plan_status"] for r in rows}),
        plan_fields=sorted({r["plan_fields"] for r in rows}),
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        limitation="Sampling may miss short transitions; observed changes are not a count of all game events.",
        physics_writes=0,
    )
    a.frames.with_suffix(".shift-summary.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
