"""Keep package imports, local native includes and command entry points relocatable."""

import ast
import importlib.util
import re
import subprocess
import sys
import tempfile
import unittest

from sector.paths import PROJECT_ROOT, PYTHON_ROOT


class SourceLayoutTests(unittest.TestCase):
    def test_sector_imports_resolve_without_a_flat_source_path(self):
        for path in (PYTHON_ROOT / "sector").rglob("*.py"):
            for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
                if isinstance(node, ast.ImportFrom) and (node.module or "").startswith("sector."):
                    with self.subTest(path=path, module=node.module):
                        self.assertIsNotNone(importlib.util.find_spec(node.module))

    def test_native_headers_resolve_relative_to_their_source(self):
        external_headers = {"MinHook.h", "SharedMemoryInterface.hpp"}
        names = set()
        for folder in ("src/native", "tests/native"):
            for path in (PROJECT_ROOT / folder).rglob("*"):
                if not path.is_file():
                    continue
                self.assertNotIn(path.name, names, "Build source basenames must be unambiguous")
                names.add(path.name)
                if path.suffix not in (".c", ".cpp", ".h"):
                    continue
                for header in re.findall(r'#include "([^"]+)"', path.read_text(encoding="utf-8")):
                    if header not in external_headers:
                        with self.subTest(path=path, header=header):
                            self.assertTrue((path.parent / header).is_file())

    def test_launcher_resolves_packages_from_an_unrelated_working_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                [
                    sys.executable,
                    str(PROJECT_ROOT / "scripts/sector.py"),
                    "runtime.start_reset_hotkeys",
                    "--help",
                ],
                cwd=directory,
                text=True,
                capture_output=True,
                timeout=15,
            )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--save-key", result.stdout)
