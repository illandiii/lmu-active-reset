"""Repository paths shared by desktop, capture, replay and analysis tools."""

from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[3]
PYTHON_ROOT = PROJECT_ROOT / "src" / "python"
