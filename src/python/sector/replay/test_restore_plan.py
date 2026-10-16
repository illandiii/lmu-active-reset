"""Exercise mapped-field plan on owned memory, never on LMU objects."""

import ctypes as C


class Field(C.Structure):
    _fields_ = [(n, C.c_uint32) for n in ("object_slot", "field_offset", "size", "reserved")] + [
        (n, C.c_ubyte * 128) for n in ("expected", "desired")
    ]


class Plan(C.Structure):
    _fields_ = [("count", C.c_size_t), ("epoch", C.c_uint64), ("fields", Field * 16384)]


class Report(C.Structure):
    _fields_ = [("attempted", C.c_size_t), ("restored", C.c_size_t), ("result", C.c_int)]


Context = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint64)
Allowed = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint32)
Transfer = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint32, C.c_uint32, C.c_void_p, C.c_uint32)


class Backend(C.Structure):
    _fields_ = [
        ("user", C.c_void_p),
        ("context", Context),
        ("allowed", Allowed),
        ("read", Transfer),
        ("write", Transfer),
    ]


class Bindings(C.Structure):
    _fields_ = [
        ("epoch", C.c_uint64),
        ("address", C.c_size_t * 2701),
        ("body_count", C.c_uint32),
        ("constraint_count", C.c_uint32),
        ("constraint_kind", C.c_uint32 * 64),
        ("engine_channels", C.c_uint32),
    ]


def verify_plan(
    dll,
    bank,
    prepared,
    native_cache=None,
    phase_bank=None,
    thermal_bank=None,
    brake_bank=None,
    driveline_bank=None,
    outputs_bank=None,
    control_bank=None,
    shift_bank=None,
):
    def address(slot, offset=0):
        return (slot * 0x5000 if slot < 108 else 108 * 0x5000 + (slot - 108) * 0x110) + offset

    mem = (C.c_ubyte * (108 * 0x5000 + 2593 * 0x110))()
    C.memset(mem, 0x5A, C.sizeof(mem))
    original = bytes(mem)
    state = {"writes": 0, "fail_at": 0, "read_fail": False, "epoch": 42}

    @Context
    def context(_, epoch):
        state["context_calls"] = state.get("context_calls", 0) + 1
        return epoch == state["epoch"] and (
            not state.get("context_fail_at") or state["context_calls"] < state["context_fail_at"]
        )

    @Allowed
    def allowed(_, slot, offset, size):
        return int(
            slot < 2701 and 0 < size <= 128 and offset + size <= (0x5000 if slot < 108 else 0x110)
        )

    @Transfer
    def read(_, slot, offset, target, size):
        if state["read_fail"] or not allowed(None, slot, offset, size):
            return 0
        C.memmove(target, C.addressof(mem) + address(slot, offset), size)
        return 1

    @Transfer
    def write(_, slot, offset, source, size):
        if not allowed(None, slot, offset, size):
            return 0
        state["writes"] += 1
        partial = state["writes"] == state["fail_at"]
        C.memmove(C.addressof(mem) + address(slot, offset), source, size // 2 if partial else size)
        return not partial

    backend = Backend(None, context, allowed, read, write)
    work = Plan()
    plan = Plan()
    report = Report()
    size = dll.fixture_plan_size
    size.argtypes = [C.c_uint]
    size.restype = C.c_size_t
    assert [size(i) for i in range(2)] == [C.sizeof(Plan), C.sizeof(Field)]
    base_build = dll.fixture_plan_build
    base_build.argtypes = [
        C.POINTER(type(bank)),
        C.POINTER(type(prepared)),
        C.c_uint64,
        C.POINTER(Backend),
        C.POINTER(Plan),
        C.POINTER(Plan),
    ]
    base_build.restype = C.c_int
    if native_cache is not None:
        saved = bank.mechanical.base.saved
        extend = dll.fixture_native_cache_plan
        extend.argtypes = [
            C.POINTER(type(native_cache)),
            C.POINTER(type(saved)),
            C.POINTER(Backend),
            C.POINTER(Plan),
            C.POINTER(Plan),
        ]
        extend.restype = C.c_int
    if phase_bank is not None:
        assert native_cache is not None
        phase_extend = dll.fixture_wheel_phase_plan
        phase_extend.argtypes = [
            C.POINTER(type(phase_bank)),
            C.POINTER(type(saved)),
            C.POINTER(Backend),
            C.POINTER(Plan),
            C.POINTER(Plan),
        ]
        phase_extend.restype = C.c_int
    if thermal_bank is not None:
        assert phase_bank is not None
        thermal_extend = dll.fixture_thermal_plan
        thermal_extend.argtypes = [
            C.POINTER(type(thermal_bank)),
            C.c_uint64,
            C.POINTER(Backend),
            C.POINTER(Plan),
            C.POINTER(Plan),
        ]
        thermal_extend.restype = C.c_int
    if brake_bank is not None:
        assert thermal_bank is not None
        brake_extend = dll.fixture_brake_plan
        brake_extend.argtypes = [
            C.POINTER(type(brake_bank)),
            C.c_uint64,
            C.POINTER(Backend),
            C.POINTER(Plan),
            C.POINTER(Plan),
        ]
        brake_extend.restype = C.c_int
    if driveline_bank is not None:
        assert brake_bank is not None
        driveline_extend = dll.fixture_driveline_plan
        driveline_extend.argtypes = [
            C.POINTER(type(driveline_bank)),
            C.c_uint64,
            C.POINTER(Backend),
            C.POINTER(Plan),
            C.POINTER(Plan),
        ]
        driveline_extend.restype = C.c_int
    if outputs_bank is not None:
        assert driveline_bank is not None
        outputs_extend = dll.fixture_outputs_plan
        outputs_extend.argtypes = [
            C.POINTER(type(outputs_bank)),
            C.c_uint64,
            C.POINTER(Backend),
            C.POINTER(Plan),
            C.POINTER(Plan),
        ]
        outputs_extend.restype = C.c_int
    if control_bank is not None:
        assert outputs_bank is not None
        control_extend = dll.fixture_control_plan
        control_extend.argtypes = [
            C.POINTER(type(control_bank)),
            C.c_uint64,
            C.POINTER(Backend),
            C.POINTER(Plan),
            C.POINTER(Plan),
        ]
        control_extend.restype = C.c_int
    if shift_bank is not None:
        assert control_bank is not None
        shift_prepared = type(shift_bank.state).from_buffer_copy(bytes(shift_bank.state))
        shift_extend = dll.fixture_shift_plan
        shift_extend.argtypes = [
            C.POINTER(type(shift_bank)),
            C.c_uint64,
            C.POINTER(type(shift_prepared)),
            C.POINTER(Backend),
            C.POINTER(Plan),
            C.POINTER(Plan),
        ]
        shift_extend.restype = C.c_int

    def build(bp, pp, epoch, backendp, wp, op):
        if native_cache is None:
            return base_build(bp, pp, epoch, backendp, wp, op)
        interim = Plan()
        status = base_build(bp, pp, epoch, backendp, wp, C.byref(interim))
        if status:
            return status
        if phase_bank is None:
            return extend(C.byref(native_cache), C.byref(saved), backendp, C.byref(interim), op)
        extended = Plan()
        status = extend(
            C.byref(native_cache), C.byref(saved), backendp, C.byref(interim), C.byref(extended)
        )
        if status:
            return status
        if thermal_bank is None:
            return phase_extend(
                C.byref(phase_bank), C.byref(saved), backendp, C.byref(extended), op
            )
        with_phase = Plan()
        status = phase_extend(
            C.byref(phase_bank), C.byref(saved), backendp, C.byref(extended), C.byref(with_phase)
        )
        if status:
            return status
        if brake_bank is None:
            return thermal_extend(
                C.byref(thermal_bank), saved.sequence, backendp, C.byref(with_phase), op
            )
        with_thermal = Plan()
        status = thermal_extend(
            C.byref(thermal_bank),
            saved.sequence,
            backendp,
            C.byref(with_phase),
            C.byref(with_thermal),
        )
        if status:
            return status
        if driveline_bank is None:
            return brake_extend(
                C.byref(brake_bank), saved.sequence, backendp, C.byref(with_thermal), op
            )
        with_brake = Plan()
        status = brake_extend(
            C.byref(brake_bank),
            saved.sequence,
            backendp,
            C.byref(with_thermal),
            C.byref(with_brake),
        )
        if status:
            return status
        if outputs_bank is None:
            return driveline_extend(
                C.byref(driveline_bank), saved.sequence, backendp, C.byref(with_brake), op
            )
        with_driveline = Plan()
        status = driveline_extend(
            C.byref(driveline_bank),
            saved.sequence,
            backendp,
            C.byref(with_brake),
            C.byref(with_driveline),
        )
        if status:
            return status
        if control_bank is None:
            return outputs_extend(
                C.byref(outputs_bank), saved.sequence, backendp, C.byref(with_driveline), op
            )
        with_outputs = Plan()
        status = outputs_extend(
            C.byref(outputs_bank),
            saved.sequence,
            backendp,
            C.byref(with_driveline),
            C.byref(with_outputs),
        )
        if status:
            return status
        if shift_bank is None:
            return control_extend(
                C.byref(control_bank), saved.sequence, backendp, C.byref(with_outputs), op
            )
        with_control = Plan()
        status = control_extend(
            C.byref(control_bank),
            saved.sequence,
            backendp,
            C.byref(with_outputs),
            C.byref(with_control),
        )
        if status:
            return status
        return shift_extend(
            C.byref(shift_bank),
            saved.sequence,
            C.byref(shift_prepared),
            backendp,
            C.byref(with_control),
            op,
        )

    execute = dll.fixture_plan_execute
    execute.argtypes = [C.POINTER(Plan), C.POINTER(Backend), C.POINTER(Report)]
    execute.restype = C.c_int
    assert (
        build(C.byref(bank), C.byref(prepared), 42, C.byref(backend), C.byref(work), C.byref(plan))
        == 0
    )
    assert 0 < plan.count <= 16384 and state["writes"] == 0
    saved = bank.mechanical.base.saved
    bindings = Bindings()
    bindings.epoch = 42
    bindings.body_count = saved.body_count
    bindings.constraint_count = saved.constraint_count
    bindings.engine_channels = bank.integration.channel_count
    for i in range(2701):
        bindings.address[i] = C.addressof(mem) + address(i)
    for i in range(saved.constraint_count):
        bindings.constraint_kind[i] = saved.recipes[i].kind
    check_bindings = dll.fixture_bindings_validate
    check_bindings.argtypes = [C.POINTER(Bindings), C.POINTER(Plan)]
    check_bindings.restype = C.c_int
    assert check_bindings(C.byref(bindings), C.byref(plan)) == 0
    pointer = bindings.address[1]
    bindings.address[1] = bindings.address[0]
    assert check_bindings(C.byref(bindings), C.byref(plan)) != 0
    bindings.address[1] = pointer
    bindings.address[1] = 2**64 - 1
    assert check_bindings(C.byref(bindings), C.byref(plan)) != 0
    bindings.address[1] = pointer
    bindings.constraint_kind[0] = 99
    assert check_bindings(C.byref(bindings), C.byref(plan)) != 0
    bindings.constraint_kind[0] = saved.recipes[0].kind
    offset = plan.fields[0].field_offset
    plan.fields[0].field_offset = 0
    assert check_bindings(C.byref(bindings), C.byref(plan)) != 0
    plan.fields[0].field_offset = offset
    bindings.epoch += 1
    assert check_bindings(C.byref(bindings), C.byref(plan)) != 0
    bindings.epoch -= 1
    if thermal_bank is not None:
        thermal_fields = [f for f in plan.fields[: plan.count] if f.object_slot >= 108]
        assert len(thermal_fields) == 9216
        for f in thermal_fields:
            index, pool = divmod(f.object_slot - 108, 9)
            wheel, row = divmod(index, 72)
            cell = thermal_bank.prior.grid.tires[wheel].cells[row][pool]
            if f.field_offset == 0:
                expected = bytes(cell.layers)
            elif f.field_offset == 0x40:
                expected = bytes(cell.coefficients)
            elif f.field_offset == 0x28:
                expected = bytes(thermal_bank.history.nodes[wheel][row][pool].fields_28)
            elif f.field_offset == 0x100:
                expected = bytes(C.c_double(thermal_bank.history.nodes[wheel][row][pool].field_100))
            else:
                raise AssertionError("Unknown thermal field")
            assert f.size == len(expected) and bytes(f.desired[: f.size]) == expected
        pointer = bindings.address[108]
        bindings.address[108] = bindings.address[0] + 0x40
        assert check_bindings(C.byref(bindings), C.byref(plan)) != 0
        bindings.address[108] = pointer
        prior = bytes(plan)
        thermal_bank.prior.sequence += 1
        assert (
            build(
                C.byref(bank), C.byref(prepared), 42, C.byref(backend), C.byref(work), C.byref(plan)
            )
            != 0
            and bytes(plan) == prior
        )
        thermal_bank.prior.sequence -= 1
    if native_cache is not None:
        for i in range(saved.body_count):
            cache = native_cache.bodies[i]
            expected = {
                8: bytes(C.c_double(cache.mass)),
                0x140: bytes(cache.inertia_local),
                0x60: bytes(cache.inverse_world),
                0xE0: bytes(cache.omega),
            }
            for offset, raw in expected.items():
                matches = [
                    f
                    for f in plan.fields[: plan.count]
                    if f.object_slot == i and f.field_offset == offset
                ]
                assert len(matches) == 1 and bytes(matches[0].desired[: matches[0].size]) == raw
        prior = bytes(plan)
        native_cache.sequence += 1
        assert (
            build(
                C.byref(bank), C.byref(prepared), 42, C.byref(backend), C.byref(work), C.byref(plan)
            )
            != 0
            and bytes(plan) == prior
        )
        native_cache.sequence -= 1
    if brake_bank is not None:
        for w in range(4):
            state_values = brake_bank.wheels[w]
            expected = {
                0x5E0: bytes((C.c_double * 4)(*state_values.fields_5d8[1:])),
                0x630: bytes(C.c_double(state_values.field_630)),
                0x138: bytes(C.c_double(state_values.cache_138)),
            }
            for offset, raw in expected.items():
                matches = [
                    f
                    for f in plan.fields[: plan.count]
                    if f.object_slot == 100 + w and f.field_offset == offset
                ]
                assert len(matches) == 1 and bytes(matches[0].desired[: matches[0].size]) == raw
        prior = bytes(plan)
        value = brake_bank.wheels[3].fields_5d8[0]
        brake_bank.wheels[3].fields_5d8[0] += 0.01
        assert (
            build(
                C.byref(bank), C.byref(prepared), 42, C.byref(backend), C.byref(work), C.byref(plan)
            )
            != 0
            and bytes(plan) == prior
        )
        brake_bank.wheels[3].fields_5d8[0] = value
    if driveline_bank is not None:
        for i, offset in enumerate((0x49D8, 0x4A18, 0x4A58)):
            matches = [
                f
                for f in plan.fields[: plan.count]
                if f.object_slot == 104 and f.field_offset == offset
            ]
            assert (
                len(matches) == 1
                and matches[0].size == 8
                and bytes(matches[0].desired[:8])
                == bytes(C.c_double(driveline_bank.state.torque[i]))
            )
        prior = bytes(plan)
        driveline_bank.sequence += 1
        assert (
            build(
                C.byref(bank), C.byref(prepared), 42, C.byref(backend), C.byref(work), C.byref(plan)
            )
            != 0
            and bytes(plan) == prior
        )
        driveline_bank.sequence -= 1
    if outputs_bank is not None:
        for name, offset in [
            ("hdv_46d0", 0x46D0),
            ("hdv_46e0", 0x46E0),
            ("hdv_46e8", 0x46E8),
            ("hdv_4b38", 0x4B38),
        ]:
            matches = [
                f
                for f in plan.fields[: plan.count]
                if f.object_slot == 104 and f.field_offset == offset
            ]
            assert (
                len(matches) == 1
                and matches[0].size == 8
                and bytes(matches[0].desired[:8])
                == bytes(C.c_double(getattr(outputs_bank.state, name)))
            )
        prior = bytes(plan)
        outputs_bank.sequence += 1
        assert (
            build(
                C.byref(bank), C.byref(prepared), 42, C.byref(backend), C.byref(work), C.byref(plan)
            )
            != 0
            and bytes(plan) == prior
        )
        outputs_bank.sequence -= 1
    if shift_bank is not None:
        expected = {
            0x4AC0: bytes(C.c_double(shift_prepared.timer_4ac0)),
            0x4668: bytes(C.c_double(shift_prepared.control_4668)),
            0x50: bytes(C.c_double(shift_prepared.event_50)),
            0x4B30: bytes(C.c_float(shift_prepared.accumulators[0])),
            0x4BB8: bytes(C.c_float(shift_prepared.accumulators[1])),
            0x4BA8: bytes((C.c_int32 * 4)(*list(shift_prepared.gears)[2:])),
            0x4AE8: bytes(shift_prepared.notification),
            0x48: bytes(shift_prepared.request),
        }
        for offset, raw in expected.items():
            matches = [
                f
                for f in plan.fields[: plan.count]
                if f.object_slot == 104 and f.field_offset == offset
            ]
            assert (
                len(matches) == 1
                and matches[0].size == len(raw)
                and bytes(matches[0].desired[: len(raw)]) == raw
            )
        prior = bytes(plan)
        shift_bank.sequence += 1
        assert (
            build(
                C.byref(bank), C.byref(prepared), 42, C.byref(backend), C.byref(work), C.byref(plan)
            )
            != 0
            and bytes(plan) == prior
        )
        shift_bank.sequence -= 1
        old_gear = shift_bank.state.gears[1]
        shift_bank.state.gears[1] = shift_prepared.gears[1] = 1 if old_gear != 1 else 0
        assert (
            build(
                C.byref(bank), C.byref(prepared), 42, C.byref(backend), C.byref(work), C.byref(plan)
            )
            != 0
            and bytes(plan) == prior
        )
        shift_bank.state.gears[1] = shift_prepared.gears[1] = old_gear
        shift_prepared.clock_a8 += 1
        assert (
            build(
                C.byref(bank), C.byref(prepared), 42, C.byref(backend), C.byref(work), C.byref(plan)
            )
            != 0
            and bytes(plan) == prior
        )
        shift_prepared.clock_a8 -= 1
    assert (
        execute(C.byref(plan), C.byref(backend), C.byref(report)) == 0
        and report.attempted == plan.count
    )
    mask = bytearray(len(original))
    total = 0
    for f in plan.fields[: plan.count]:
        start = address(f.object_slot, f.field_offset)
        assert bytes(mem[start : start + f.size]) == bytes(f.desired[: f.size])
        assert not any(mask[start : start + f.size])
        mask[start : start + f.size] = b"\1" * f.size
        total += f.size
    assert all(a == b for a, b, covered in zip(original, bytes(mem), mask) if not covered)
    C.memmove(mem, original, len(original))
    state.update(writes=0, fail_at=plan.count)
    assert execute(C.byref(plan), C.byref(backend), C.byref(report)) == 6
    assert report.restored == plan.count and bytes(mem) == original
    prior = bytes(plan)
    state["read_fail"] = True
    assert (
        build(C.byref(bank), C.byref(prepared), 42, C.byref(backend), C.byref(work), C.byref(plan))
        != 0
        and bytes(plan) == prior
    )
    state.update(read_fail=False, epoch=43, writes=0, fail_at=0)
    assert execute(C.byref(plan), C.byref(backend), C.byref(report)) == 2 and state["writes"] == 0
    # Same complete plan through the Win32 self-memory backend, still only on
    # this test process's owned ctypes allocation. Never an LMU handle/address.
    native_execute = dll.fixture_native_memory_execute
    native_execute.argtypes = [
        C.POINTER(Plan),
        C.POINTER(Bindings),
        C.c_uint32,
        Context,
        C.c_void_p,
        C.POINTER(Report),
    ]
    native_execute.restype = C.c_int
    thread = C.WinDLL("kernel32").GetCurrentThreadId()
    state.update(epoch=42, context_calls=0, context_fail_at=0)
    assert (
        native_execute(C.byref(plan), C.byref(bindings), thread, context, None, C.byref(report))
        == 0
        and report.attempted == plan.count
    )
    for f in plan.fields[: plan.count]:
        start = address(f.object_slot, f.field_offset)
        assert bytes(mem[start : start + f.size]) == bytes(f.desired[: f.size])
    assert all(a == b for a, b, covered in zip(original, bytes(mem), mask) if not covered)
    C.memmove(mem, original, len(original))
    state.update(context_calls=0, context_fail_at=max(4, plan.count // 2))
    assert (
        native_execute(C.byref(plan), C.byref(bindings), thread, context, None, C.byref(report))
        == 2
    )
    assert (
        0 < report.attempted < plan.count
        and report.restored == report.attempted
        and bytes(mem) == original
    )
    from sector.replay.test_joint_restore_plan import verify_joint

    joint = verify_joint(dll, plan, backend, mem, original, state, bindings)
    return dict(
        joint_physics_scoring=joint,
        fields=plan.count,
        bytes=total,
        owned_memory_execution=True,
        native_self_memory_full_plan=True,
        native_context_loss_rollback=True,
        last_partial_write_rolled_back=True,
        untouched_bytes_preserved=True,
        exact_native_cache=native_cache is not None,
        wheel_phase=phase_bank is not None,
        thermal_history=thermal_bank is not None,
        brake_state=brake_bank is not None,
        driveline_cache=driveline_bank is not None,
        powertrain_control=control_bank is not None,
        shift_transients=shift_bank is not None,
        typed_field_whitelist=True,
        absolute_address_alias_rejected=True,
        physics_writes=0,
    )
