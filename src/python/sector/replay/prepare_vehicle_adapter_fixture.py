"""Convert recorded frames into local fixture inputs; no game process access."""

import argparse
import ctypes as C
import json
from pathlib import Path
from sector.observers.run_snapshot_observer_v31_25661166 import Frame
from sector.replay.test_checkpoint_bank_recording import snapshot


def fill(target, data):
    if isinstance(target, C.Array):
        if len(data) > len(target):
            raise ValueError("Recorded array exceeds native capacity")
        for index, value in enumerate(data):
            if isinstance(target[index], (C.Structure, C.Array)):
                fill(target[index], value)
            else:
                target[index] = value
    else:
        for name, _ in target._fields_:
            value = data[name]
            child = getattr(target, name)
            if isinstance(child, (C.Structure, C.Array)):
                fill(child, value)
            else:
                setattr(target, name, value)


def convert(row):
    frame = Frame()
    partial = snapshot(row)
    for name, _ in frame._fields_:
        if name in ("bodies", "recipes", "memory"):
            C.memmove(
                C.addressof(getattr(frame, name)),
                C.addressof(getattr(partial, name)),
                C.sizeof(getattr(frame, name)),
            )
        else:
            child = getattr(frame, name)
            if isinstance(child, (C.Structure, C.Array)):
                fill(child, row[name])
            else:
                setattr(frame, name, row[name])
    return bytes(frame)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("recording", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    rows = [json.loads(line) for line in args.recording.read_text().splitlines() if line]
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "saved.frame").write_bytes(convert(rows[0]))
    (args.output / "current.frame").write_bytes(convert(rows[-1]))
    print(json.dumps({"frames": 2, "frame_bytes": C.sizeof(Frame), "game_writes": 0}))
