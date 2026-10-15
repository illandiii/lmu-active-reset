"""Short, distinct acknowledgement sounds; playback never blocks the host loop."""

import argparse
import math
from pathlib import Path
import struct
import wave

TONES = {
    "save": ((660, 0.09), (880, 0.13)),
    "restore": ((880, 0.08), (1100, 0.08), (1320, 0.14)),
    "rejected": ((330, 0.15), (220, 0.19)),
}


def write_sounds(directory):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    paths = {}
    rate = 22050
    for name, tones in TONES.items():
        samples = bytearray()
        for frequency, duration in tones:
            count = int(rate * duration)
            for i in range(count):
                envelope = min(1, i / (rate * 0.01), (count - 1 - i) / (rate * 0.02))
                value = int(6500 * envelope * math.sin(2 * math.pi * frequency * i / rate))
                samples.extend(struct.pack("<h", value))
            samples.extend(b"\0\0" * int(rate * 0.035))
        paths[name] = directory / f"{name}.wav"
        with wave.open(str(paths[name]), "wb") as sound:
            sound.setnchannels(1)
            sound.setsampwidth(2)
            sound.setframerate(rate)
            sound.writeframes(samples)
    return paths


def report_feedback(row):
    if row["result"] != 0:
        return "rejected", f"Command rejected (result {row['result']}, cause {row['cause']})."
    if row["action"] == 1:
        return "save", "Checkpoint saved."
    if row["action"] == 3:
        return "restore", "Checkpoint applied."
    return None


class Feedback:
    def __init__(self, directory, muted=False):
        self.paths = write_sounds(directory)
        self.muted = muted

    def play(self, name):
        if self.muted or name not in self.paths:
            return
        import winsound

        try:
            winsound.PlaySound(
                str(self.paths[name]),
                winsound.SND_FILENAME | winsound.SND_ASYNC | winsound.SND_NODEFAULT,
            )
        except RuntimeError:
            pass  # An unavailable audio device must not interrupt native shutdown.


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    write_sounds(parser.parse_args().directory)
