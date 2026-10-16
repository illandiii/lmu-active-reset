"""Inventory installed files; never authorizes old hooks for a changed binary."""

import datetime
import hashlib
import json
import re
from pathlib import Path


def main():
    project = Path(__file__).resolve().parents[1]
    old = json.loads((project / "compatibility/14200-telemetry.json").read_text())
    install = Path(old["install_path"])
    files = []
    for previous in old["files"]:
        path = install / previous["relative_path"]
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        files.append(
            dict(
                relative_path=previous["relative_path"],
                size=path.stat().st_size,
                sha256=digest,
                matches_verified_20261003=digest.lower() == previous["sha256"].lower(),
            )
        )
    manifest = (install.parents[1] / "appmanifest_2399420.acf").read_text()
    build = re.search(r'"buildid"\s+"(\d+)"', manifest).group(1)
    report = dict(
        observed_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        steam_buildid=build,
        files=files,
        physics_compatible=False,
        reset_permitted=False,
        live_hooks_loaded=False,
        note="Inventory only. Same FileVersion does not establish binary or ABI compatibility.",
    )
    target = project / "compatibility" / f"steam-{build}-inventory.json"
    target.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
