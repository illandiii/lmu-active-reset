"""Run the captured physical plan and score invalidation on owned memory only."""

import ctypes as C


def verify_joint(dll, plan, physical, mem, original, state, bindings):
    from sector.replay.test_restore_plan import Plan, Report, Backend, Context, Allowed, Transfer

    class Flags(C.Structure):
        _fields_ = [
            ("current", C.c_int32),
            ("remembered", C.c_int32),
            ("last_computed", C.c_int32),
            ("next_timed", C.c_uint8),
            ("off_track", C.c_uint8),
        ]

    flags = Flags(2, 2, 2, 1, 0)
    joint = Plan()
    report = Report()
    prepare = dll.fixture_joint_prepare
    prepare.argtypes = [C.POINTER(Plan), C.POINTER(Flags), C.POINTER(Plan)]
    prepare.restype = C.c_int
    execute = dll.fixture_joint_execute
    execute.argtypes = [C.POINTER(Plan), C.POINTER(Backend), C.POINTER(Backend), C.POINTER(Report)]
    execute.restype = C.c_int
    assert (
        prepare(C.byref(plan), C.byref(flags), C.byref(joint)) == 0
        and joint.count == plan.count + 3
    )
    score = (C.c_ubyte * 0x27400)()
    C.memset(score, 0x73, C.sizeof(score))
    base = C.addressof(score)
    for off in (0x160, 0x27290, 0x27360):
        C.c_int32.from_address(base + off).value = 2
    score[0x164] = 1
    score[0x272B8] = 0
    score_original = bytes(score)

    @Context
    def context(_, epoch):
        return epoch == 42

    @Allowed
    def allowed(_, slot, off, n):
        return slot == 0 and n == 4 and off in (0x160, 0x27290, 0x27360)

    @Transfer
    def read(_, slot, off, target, n):
        if not allowed(None, slot, off, n):
            return 0
        C.memmove(target, base + off, n)
        return 1

    @Transfer
    def write(_, slot, off, source, n):
        if not allowed(None, slot, off, n):
            return 0
        state["writes"] += 1
        partial = state["writes"] == state["fail_at"]
        C.memmove(base + off, source, n // 2 if partial else n)
        return not partial

    scoring = Backend(None, context, allowed, read, write)

    def reset():
        C.memmove(mem, original, len(original))
        C.memmove(score, score_original, len(score_original))
        state.update(
            writes=0, fail_at=0, read_fail=False, epoch=42, context_calls=0, context_fail_at=0
        )

    reset()
    assert execute(C.byref(joint), C.byref(physical), C.byref(scoring), C.byref(report)) == 0
    desired = bytearray(score_original)
    for off in (0x160, 0x27290, 0x27360):
        desired[off : off + 4] = b"\0" * 4
    assert bytes(score) == bytes(desired)
    for failure in (plan.count + 1, plan.count + 2, plan.count + 3):
        reset()
        state["fail_at"] = failure
        assert execute(C.byref(joint), C.byref(physical), C.byref(scoring), C.byref(report)) == 6
        assert (
            report.restored == report.attempted
            and bytes(mem) == original
            and bytes(score) == score_original
        )
    native = dll.fixture_joint_native_execute
    native.argtypes = [
        C.POINTER(Plan),
        C.POINTER(type(bindings)),
        C.c_size_t,
        C.c_uint32,
        Context,
        C.c_void_p,
        C.POINTER(Report),
    ]
    native.restype = C.c_int
    kernel = C.WinDLL("kernel32")
    kernel.GetCurrentThreadId.restype = C.c_uint32
    thread = kernel.GetCurrentThreadId()
    probe = dll.fixture_joint_native_preflight
    probe.argtypes = native.argtypes[:-1]
    probe.restype = C.c_int
    reset()
    assert probe(C.byref(joint), C.byref(bindings), base, thread, physical.context, None) == 0
    assert bytes(mem) == original and bytes(score) == score_original
    assert probe(C.byref(joint), C.byref(bindings), base, thread + 1, physical.context, None) == 2
    score[0x160] = 1
    assert probe(C.byref(joint), C.byref(bindings), base, thread, physical.context, None) == 5
    assert bytes(mem) == original and score[0x160] == 1
    reset()
    state["context_fail_at"] = 4
    assert probe(C.byref(joint), C.byref(bindings), base, thread, physical.context, None) == 2
    assert bytes(mem) == original and bytes(score) == score_original
    reset()
    assert (
        native(
            C.byref(joint), C.byref(bindings), base, thread, physical.context, None, C.byref(report)
        )
        == 0
    )
    for f in joint.fields[: joint.count]:
        address = (
            base if f.object_slot == 2701 else bindings.address[f.object_slot]
        ) + f.field_offset
        assert C.string_at(address, f.size) == bytes(f.desired[: f.size])
    assert bytes(score) == bytes(desired)
    reset()
    state["context_fail_at"] = plan.count + 4
    assert (
        native(
            C.byref(joint), C.byref(bindings), base, thread, physical.context, None, C.byref(report)
        )
        == 2
    )
    assert (
        report.attempted > plan.count
        and report.restored == report.attempted
        and bytes(mem) == original
        and bytes(score) == score_original
    )
    reset()
    f = plan.fields[0]
    alias = bindings.address[f.object_slot] + f.field_offset - 0x160
    assert (
        native(
            C.byref(joint),
            C.byref(bindings),
            alias,
            thread,
            physical.context,
            None,
            C.byref(report),
        )
        == 1
    )
    assert bytes(mem) == original and bytes(score) == score_original
    reset()
    from sector.replay.test_restore_flow_native import verify_flow

    flow_report = verify_flow(
        dll, joint, bindings, score, physical, mem, original, score_original, state, reset
    )
    return dict(
        readonly_preflight_checked=True,
        contact_flow=flow_report,
        fields=joint.count,
        scoring_bytes=12,
        score_record_bytes_preserved=True,
        all_three_scoring_failures_rollback_physics=True,
        native_joint_execution=True,
        native_context_loss_during_scoring_rollback=True,
        physical_scoring_alias_rejected=True,
        owned_memory_only=True,
    )
