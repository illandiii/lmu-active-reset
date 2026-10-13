"""Exercise lost-REST cleanup with simulated process reads; never touches LMU."""

import contextlib, ctypes, io, json, pathlib, sys, tempfile, unittest
from unittest.mock import patch
import probe_drivetrain_cycle as probe


class CleanupTests(unittest.TestCase):
    def test_rest_loss_during_wait_still_writes_failure_record(self):
        parent = pathlib.Path(__file__).resolve().parents[1] / "work"
        with tempfile.TemporaryDirectory(prefix="lap-reader-test-", dir=parent) as directory:
            project = pathlib.Path(directory)
            (project / "logs").mkdir()
            (project / "Le Mans Ultimate.exe").write_bytes(b"MZ" + bytes(62))
            (project / "inventory.json").write_text(json.dumps({"install_path": str(project)}))
            state = {
                "state": {
                    "appBuild": 14130,
                    "settingMode": "SETTING_GRANDPRIX",
                    "gameSession": "PRACTICE1",
                    "gameState": "GSTATE_DYN",
                    "navigationState": "NAV_EVENT",
                }
            }
            calls = 0
            closed = []

            def get_json(path):
                nonlocal calls
                calls += 1
                if calls == 1:
                    return state
                if calls == 2:
                    return {"inMonitor": True}
                raise TimeoutError("Simulated REST loss")

            def read(handle, address, buffer, size, count):
                self.assertEqual(handle, 123)
                self.assertEqual(size, 64)
                ctypes.memmove(buffer, b"MZ" + bytes(62), 64)
                ctypes.cast(count, ctypes.POINTER(ctypes.c_size_t)).contents.value = 64
                return True

            fake_kernel = type("Kernel", (), {})()
            fake_kernel.OpenProcess = lambda *args: 123
            fake_kernel.CloseHandle = lambda handle: closed.append(handle)
            fake_kernel.ReadProcessMemory = read
            args = [
                "probe",
                "--pid",
                "99999",
                "--base",
                "0x100000",
                "--manual-drive",
                "--wait-for-drive-seconds",
                "10",
            ]
            with (
                patch.object(probe, "PROJECT", project),
                patch.object(probe, "KERNEL", fake_kernel),
                patch.object(
                    probe, "verify", return_value=(project / "unused.exe", {}, "fixture-hash")
                ),
                patch.object(probe, "get_json", side_effect=get_json),
                patch.object(
                    probe.subprocess,
                    "Popen",
                    side_effect=AssertionError("Must not start a process"),
                ),
                patch.object(probe.time, "sleep"),
                patch.object(sys, "argv", args),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                with self.assertRaisesRegex(RuntimeError, "Simulated REST loss"):
                    probe.main()
            result = json.loads((project / "drivetrain-comparison.json").read_text())
            self.assertIn("Simulated REST loss", result["error"])
            self.assertTrue(result["cleanup_errors"])
            self.assertIsNone(result["returned_to_monitor"])
            self.assertIsNone(result["native_log"])
            self.assertEqual(result["physics_writes"], 0)
            self.assertEqual(result["samples"], [])
            self.assertEqual(closed, [123])
            self.assertEqual(len(list((project / "logs").glob("*.comparison.json"))), 1)


if __name__ == "__main__":
    unittest.main()
