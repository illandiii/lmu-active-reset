"""Verify contact compensation with the full captured plan through Win32 self-memory."""

import ctypes as C
from test_restore_plan import Plan, Report, Context


class Flow(C.Structure):
    _fields_ = [
        ("busy", C.c_int),
        ("quarantined", C.c_int),
        ("completed", C.c_uint64),
        ("compensated", C.c_uint64),
    ]


class FlowWork(C.Structure):
    _fields_ = [("frozen", Plan)]


Contact = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint64, C.c_int)
Leave = C.CFUNCTYPE(None, C.c_void_p, C.c_int)
Quarantine = C.CFUNCTYPE(None, C.c_void_p, C.c_int)


class Ops(C.Structure):
    _fields_ = [
        ("user", C.c_void_p),
        ("enter", Context),
        ("owner", Context),
        ("contacts", Contact),
        ("leave", Leave),
        ("quarantine", Quarantine),
    ]


class FlowReport(C.Structure):
    _fields_ = [
        ("result", C.c_int),
        ("cause", C.c_int),
        ("contact_calls", C.c_int),
        ("forward", Report),
        ("recovery", Report),
    ]


def verify_flow(dll, joint, bindings, score, physical, mem, original, score_original, state, reset):
    fn = dll.fixture_joint_flow
    fn.argtypes = [
        C.POINTER(Flow),
        C.POINTER(FlowWork),
        C.POINTER(Plan),
        C.POINTER(type(bindings)),
        C.c_size_t,
        C.c_uint32,
        Context,
        C.c_void_p,
        C.POINTER(Ops),
        C.POINTER(FlowReport),
    ]
    fn.restype = C.c_int
    thread = C.WinDLL("kernel32").GetCurrentThreadId()
    base = C.addressof(score)
    work = FlowWork()
    report = FlowReport()
    calls = []
    mode = {"value": "ok", "held": False, "alive": True, "resume": None}

    @Context
    def enter(_, epoch):
        mode["held"] = True
        return epoch == 42

    @Context
    def owner(_, epoch):
        return mode["held"] and mode["alive"] and epoch == 42

    @Contact
    def contact(_, epoch, original_pose):
        calls.append(original_pose)
        if not original_pose and mode["value"] in ("contacts", "recovery"):
            # A contact refresh may alter fields after the physical commit.
            f = joint.fields[0]
            C.memset(bindings.address[f.object_slot] + f.field_offset, 0x44, f.size)
            return 0
        if not original_pose and mode["value"] == "context":
            state["epoch"] = 43
        if not original_pose and mode["value"] == "owner":
            mode["alive"] = False
        return not (original_pose and mode["value"] == "recovery")

    @Leave
    def leave(_, resume):
        mode["held"] = False
        mode["resume"] = resume

    @Quarantine
    def quarantine(_, cause):
        mode["quarantine"] = cause

    ops = Ops(None, enter, owner, contact, leave, quarantine)
    results = {}
    for label, expected in [
        ("ok", 0),
        ("contacts", 1),
        ("context", 1),
        ("owner", 3),
        ("recovery", 3),
    ]:
        reset()
        flow = Flow()
        calls.clear()
        mode.update(value=label, alive=True, held=False, resume=None, quarantine=None)
        result = fn(
            C.byref(flow),
            C.byref(work),
            C.byref(joint),
            C.byref(bindings),
            base,
            thread,
            physical.context,
            None,
            C.byref(ops),
            C.byref(report),
        )
        assert result == expected and not mode["held"]
        if expected == 0:
            for f in joint.fields[: joint.count]:
                addr = (
                    base if f.object_slot == 2701 else bindings.address[f.object_slot]
                ) + f.field_offset
                assert C.string_at(addr, f.size) == bytes(f.desired[: f.size])
            assert calls == [0] and mode["resume"] == 1
        if expected == 1:
            assert (
                report.recovery.restored == joint.count
                and bytes(mem) == original
                and bytes(score) == score_original
                and calls == [0, 1]
                and mode["resume"] == 1
            )
        if expected == 3:
            assert flow.quarantined and mode["resume"] == 0 and mode["quarantine"] is not None
            prior = (bytes(mem), bytes(score), list(calls))
            assert (
                fn(
                    C.byref(flow),
                    C.byref(work),
                    C.byref(joint),
                    C.byref(bindings),
                    base,
                    thread,
                    physical.context,
                    None,
                    C.byref(ops),
                    C.byref(report),
                )
                == 3
            )
            assert (bytes(mem), bytes(score), calls) == prior
        results[label] = True
    reset()
    return dict(
        native_contact_flow=results,
        fields=joint.count,
        synthetic_contact_adapter=True,
        game_writes=0,
    )
