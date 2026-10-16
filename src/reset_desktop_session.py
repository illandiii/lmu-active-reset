"""Atomic desktop status snapshots and cooperative stop/GUI-liveness requests.

This channel only observes reports and requests shutdown. It cannot issue a save,
prepare or restore, or grant native admission.
"""

import json
import math
from pathlib import Path
import time


class DesktopSession:
    def __init__(self, directory, clock=time.time, settings_file=None):
        self.directory = Path(directory) if directory else None
        self.clock = clock
        self.settings_file = Path(settings_file) if settings_file else None
        self.state = {
            "schema_version": 1,
            "phase": "waiting",
            "checkpoint": False,
            "saved_speed_kmh": None,
            "saved_at": None,
            "restore_count": 0,
            "message": "",
            "sequence": 0,
        }
        self.dirty = False
        if self.directory:
            self.directory.mkdir(parents=True, exist_ok=True)

    def stopped(self):
        if not self.directory:
            return False
        if (self.directory / "stop").exists():
            return True
        try:
            age = self.clock() - (self.directory / "heartbeat").stat().st_mtime
            return age > 10 or age < -10
        except FileNotFoundError:
            return True

    def publish(self, **changes):
        if changes.get("checkpoint") is False:
            changes.update(saved_speed_kmh=None, saved_at=None, restore_count=0)
        if not self.dirty and all(self.state.get(k) == v for k, v in changes.items()):
            return
        self.state.update(changes)
        self.state["sequence"] += 1
        if not self.directory:
            return
        temporary = self.directory / "status.tmp"
        try:
            temporary.write_text(
                json.dumps(self.state, ensure_ascii=True, allow_nan=False), encoding="utf-8"
            )
            temporary.replace(self.directory / "status.json")
            self.dirty = False
        except OSError:
            # Desktop status must never prevent the runner's native cleanup.
            # Keep the old snapshot and retry on the next publish.
            self.dirty = True

    def command(self, action):
        if action == 1:
            self.publish(message="Saving checkpoint…")
        elif action == 2:
            self.publish(message="Preparing restore…")
        elif action == 3:
            self.publish(message="Restoring checkpoint…")

    def settings(self):
        from physical_reset_hotkeys import virtual_key

        if not self.settings_file:
            return None
        try:
            settings = json.loads(self.settings_file.read_text(encoding="utf-8-sig"))
            save, restore = settings["save_key"], settings["restore_key"]
            if virtual_key(save) == virtual_key(restore) or type(settings["sound"]) is not bool:
                return None
            return save.upper(), restore.upper(), settings["sound"]
        except (OSError, ValueError, KeyError, TypeError, AttributeError):
            return None

    def report(self, row):
        from reset_feedback import report_feedback

        feedback = report_feedback(row)
        changes = {}
        if row["action"] == 1 and row["result"] == 0:
            speed = row.get("speed")
            speed_kmh = (
                speed * 3.6
                if type(speed) in (float, int) and math.isfinite(speed) and speed >= 0
                else None
            )
            changes.update(
                checkpoint=True, saved_speed_kmh=speed_kmh, saved_at=self.clock(), restore_count=0
            )
        elif row["action"] == 3 and row["result"] == 0:
            changes["restore_count"] = self.state["restore_count"] + 1
        if feedback:
            changes.update(message=feedback[1], last_event=feedback[0], report_id=row["id"])
        if changes:
            self.publish(**changes)
