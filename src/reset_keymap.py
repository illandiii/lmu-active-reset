"""Shared desktop/controller key identifiers; no modifiers or key combinations."""

import json
from pathlib import Path

KEY_CODES = {
    entry["name"].upper(): entry["virtual_key"]
    for entry in json.loads(Path(__file__).with_suffix(".json").read_text(encoding="utf-8"))
}
