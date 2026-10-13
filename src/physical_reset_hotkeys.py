"""Focused key edges and acknowledged prepare/restore sequencing."""


class HotkeyCommands:
    def __init__(self):
        self.armed = False
        self.saved = False
        self.pending = None
        self.next_id = 0

    def issue(self, action, now):
        self.next_id += 1
        self.pending = (self.next_id, action, now)
        return {"id": self.next_id, "action": action}

    def poll(self, focused, allowed, save, restore, modifiers, now, report=None):
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
            # A preparation never survives focus/context loss or a slow ack.
            if not eligible or now - issued > 2:
                self.pending = None
                return {}
            if report and report["id"] == request_id:
                self.pending = None
                if action == 1:
                    self.saved = report["result"] == 0
                elif action == 2 and report["result"] == 0 and report["fields"] == 10077:
                    return self.issue(3, now)
            return {}
        if edge == 1:
            self.saved = False
            return self.issue(1, now)
        if edge == 2 and self.saved:
            return self.issue(2, now)
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
        down = lambda vk: bool(self.user.GetAsyncKeyState(vk) & 0x8000)
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
