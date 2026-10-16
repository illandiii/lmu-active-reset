"""Focused key edges and acknowledged prepare/restore sequencing."""


def expected_restore_fields(revision):
    """Match the admitted native host's typed plan, including v15 collision caches."""
    if revision == 15:
        return 10084
    if 1 <= revision <= 14:
        return 10077
    raise ValueError(f"Unsupported reset host revision: {revision}")


def preparation_ready(report, expected_fields):
    return (
        report.get("action") == 2
        and report.get("result") == 0
        and report.get("fields") == expected_fields
    )


class HotkeyCommands:
    def __init__(self, expected_fields=10084):
        self.expected_fields = expected_fields
        self.armed = False
        self.saved = False
        self.pending = None
        self.next_id = 0
        self.notice = None

    def rebind(self):
        """Require a fresh release without discarding an existing checkpoint."""
        if self.pending:
            return False
        self.armed = False
        return True

    def issue(self, action, now):
        self.next_id += 1
        self.pending = (self.next_id, action, now)
        return {"id": self.next_id, "action": action}

    def poll(self, focused, allowed, save, restore, modifiers, now, report=None):
        self.notice = None
        eligible = focused and allowed and not modifiers
        edge = 0
        if not eligible or (save and restore):
            self.armed = False
        elif not save and not restore:
            self.armed = True
        elif self.armed:
            self.armed = False
            edge = 1 if save else 2

        if self.pending:
            request_id, action, issued = self.pending
            # A save acknowledgement remains valid if focus moved meanwhile.
            if report and report["id"] == request_id and action == 1:
                self.pending = None
                self.saved = self.saved or report["result"] == 0
                return {}
            # A preparation never survives focus/context loss or a slow ack.
            if not eligible or now - issued > 2:
                self.pending = None
                return {}
            if report and report["id"] == request_id:
                self.pending = None
                if action == 1:
                    self.saved = self.saved or report["result"] == 0
                elif action == 2:
                    if preparation_ready(report, self.expected_fields):
                        return self.issue(3, now)
                    self.notice = (
                        "Restore preparation rejected: "
                        f"result {report.get('result')}, fields {report.get('fields')} "
                        f"(expected {self.expected_fields})."
                    )
            return {}
        if edge == 1:
            return self.issue(1, now)
        if edge == 2 and self.saved:
            return self.issue(2, now)
        if edge == 2:
            self.notice = "Save a checkpoint before restoring."
        return {}


class WindowsKeys:
    def __init__(self, pid, save_vk, restore_vk):
        import ctypes as c
        from ctypes import wintypes as w

        self.c, self.w = c, w
        self.pid, self.save_vk, self.restore_vk = pid, save_vk, restore_vk
        self.user = c.WinDLL("user32", use_last_error=True)
        self.user.GetForegroundWindow.restype = w.HWND
        self.user.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
        self.user.GetAsyncKeyState.argtypes = [c.c_int]
        self.user.GetAsyncKeyState.restype = c.c_short

    def read(self):
        pid = self.w.DWORD()
        self.user.GetWindowThreadProcessId(self.user.GetForegroundWindow(), self.c.byref(pid))

        def down(vk):
            return bool(self.user.GetAsyncKeyState(vk) & 0x8000)

        return (
            pid.value == self.pid,
            down(self.save_vk),
            down(self.restore_vk),
            any(down(vk) for vk in (0x10, 0x11, 0x12, 0x5B, 0x5C)),
        )


def function_key(name):
    name = name.upper()
    if not name.startswith("F") or not name[1:].isdigit() or not 1 <= int(name[1:]) <= 24:
        raise ValueError("Use a function key F1..F24")
    return 0x6F + int(name[1:])
