/* Read-only numerical/phase observer. No vehicle state writes or reset entry. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <cmath>
#include <cstring>
#include "vehicle_observer_v4_ipc_14200.h"
#include "MinHook.h"
static PoseIPC *ipc;
static uintptr_t module_base;
static void(WINAPI *original_dynamics)(void *, double);
static void(WINAPI *original_contacts)(void *, double, double, void *, double);
static void(WINAPI *original_forces)(void *);
static void *targets[3];
static unsigned char originals[3][32];
struct Cycle {
    uintptr_t player, tires[4];
    uint64_t force_calls, contacts[4], masks[6], fresh, errors;
};
static thread_local Cycle *cycle;
template <class T> static bool read_field(uintptr_t p, T &out) {
    SIZE_T got = 0;
    return p >= 0x10000 && p < 0x800000000000ULL &&
           ReadProcessMemory(GetCurrentProcess(), (void *)p, &out, sizeof(out), &got) &&
           got == sizeof(out);
}
static bool lease() {
    LONG64 now = GetTickCount64(), hb = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
    return !ipc->stop && ipc->allow_observation == 1 && hb <= now && now - hb <= 300;
}
static bool player_matches(void *object) {
#ifdef LMU_POSE_FIXTURE
    typedef void *(WINAPI * F)();
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixturePlayer");
    return f && f() == object;
#else
    uintptr_t owner = 0, player = 0;
    return read_field(module_base + 0x1e161e8, owner) && read_field(owner + 0x159a0, player) &&
           player == (uintptr_t)object;
#endif
}
static bool read_vector(uintptr_t p, uintptr_t *out, size_t limit, size_t &count) {
    uintptr_t v[3];
    if (!read_field(p, v) || v[0] < 0x10000 || v[1] < v[0] || v[2] < v[1] || (v[1] - v[0]) % 8 ||
        (v[2] - v[0]) % 8)
        return false;
    count = (v[1] - v[0]) / 8;
    if (!count || count > limit)
        return false;
    for (size_t i = 0; i < count; i++)
        if (!read_field(v[0] + i * 8, out[i]) || out[i] < 0x10000)
            return false;
    return true;
}
static bool collect_topology(uintptr_t player, uintptr_t *bodies, size_t &roots,
                             uint64_t &aggregate, Cycle &c, int64_t *wheel_index) {
    uintptr_t primary[32];
    size_t primaries = 0;
    if (!read_vector(player + 0x23f8, bodies, 28, roots) ||
        !read_vector(player + 0x2440, primary, 32, primaries))
        return false;
    aggregate = 0;
    for (size_t j = 0; j < primaries; j++) {
        bool found = false;
        for (size_t i = 0; i < roots; i++)
            if (primary[j] == bodies[i]) {
                aggregate |= 1ULL << i;
                found = true;
            }
        if (!found)
            return false;
    }
    for (int w = 0; w < 4; w++) {
        uintptr_t wheel = player + 0x618 + w * 0x6b0, body = 0;
        if (!read_field(wheel + 0x3e8, c.tires[w]) || !read_field(wheel + 0x1d0, body) ||
            c.tires[w] < 0x10000)
            return false;
        bodies[roots + w] = c.tires[w] + 0xac0;
        wheel_index[w] = -1;
        for (size_t i = 0; i < roots; i++)
            if (bodies[i] == body)
                wheel_index[w] = (int64_t)i;
    }
    for (size_t i = 0; i < roots + 4; i++)
        for (size_t j = 0; j < i; j++)
            if (bodies[i] == bodies[j])
                return false;
    return true;
}
static bool capture_body(uintptr_t p, PoseBody &b) {
    int32_t counter = 0;
    bool ok =
        read_field(p + 0x58, b.state.mass.inverse_mass) &&
        read_field(p + 0xf8, b.state.mass.inverse_inertia_local) &&
        read_field(p + 0x40, b.state.position_world) &&
        read_field(p + 0x28, b.state.velocity_world) && read_field(p + 0x90, b.state.orientation) &&
        read_field(p + 0x110, b.state.momentum_world) && read_field(p + 0x60, b.inverse_world) &&
        read_field(p + 0xe0, b.omega) && read_field(p + 0x10, b.force) &&
        read_field(p + 0x128, b.torque) && read_field(p + 0xd8, counter);
    b.renormalization_counter = counter;
    return ok;
}
static uint64_t validate(PoseBody *b, size_t n, double &ie, double &we) {
    RigidSavedBody states[32];
    RigidMassProperties mass[32];
    RigidPreparedBody prepared[32];
    for (size_t i = 0; i < n; i++) {
        states[i] = b[i].state;
        mass[i] = states[i].mass;
    }
    auto result = rigid_prepare_restore(states, mass, n, prepared);
    if (result != RIGID_PREPARE_OK)
        return result;
    for (size_t i = 0; i < n; i++) {
        for (int j = 0; j < 6; j++) {
            if (!std::isfinite(b[i].inverse_world[j]))
                return 100;
            double e = std::fabs(prepared[i].inverse_inertia_world[j] - b[i].inverse_world[j]);
            if (e > ie)
                ie = e;
        }
        for (int j = 0; j < 3; j++) {
            if (!std::isfinite(b[i].omega[j]) || !std::isfinite(b[i].force[j]) ||
                !std::isfinite(b[i].torque[j]))
                return 101;
            double e = std::fabs(prepared[i].angular_velocity_world[j] - b[i].omega[j]);
            if (e > we)
                we = e;
        }
    }
    return result;
}
static bool capture_energy(uintptr_t p, const uintptr_t *bodies, size_t roots, VehicleEnergy &e) {
    uintptr_t fuel = p + 0x448, body = 0, motor = 0, battery = 0, branch = 0;
    if (!read_field(fuel, e.fuel.quantity) || !read_field(fuel + 8, e.fuel.quantity_rate) ||
        !read_field(fuel + 0x60, e.fuel.density) || !read_field(fuel + 0x80, e.fuel.base_mass) ||
        !read_field(fuel + 0x78, body) || !read_field(body + 8, e.fuel_body_mass) ||
        !read_field(p + 0x550, branch))
        return false;
    e.fuel_body_index = -1;
    for (size_t i = 0; i < roots; i++)
        if (bodies[i] == body) {
            if (e.fuel_body_index >= 0)
                return false;
            e.fuel_body_index = (int64_t)i;
        }
    if (e.fuel_body_index < 0)
        return false;
    e.fuel.additional_mass = 0;
    if (!branch) {
        uintptr_t game = 0;
        float initial = 0;
        unsigned char indices[14];
        double tail = 0, extra = 0, term = 0;
        if (!read_field(p + 0x4bc0, game) || !read_field(game + 0x440d0, initial) ||
            !read_field(p + 0x138, indices))
            return false;
        e.fuel.additional_mass = initial;
        for (int i = 0; i < 14; i++)
            if (indices[i] > 3)
                return false;
        for (int i = 0; i < 13; i++) {
            if (!read_field(p + 0x148 + i * 0x20 + indices[i] * 8, term))
                return false;
            e.fuel.additional_mass += term;
        }
        if (!read_field(p + 0x2e8 + indices[13] * 8, tail) || !read_field(p + 0x558, extra))
            return false;
        tail += extra;
        if (!read_field(p + 0x2608, term))
            return false;
        e.fuel.additional_mass += term;
        e.fuel.additional_mass += tail;
    }
    if (!read_field(p + 0x46f8, motor) || !read_field(motor + 0x160, battery) ||
        battery != p + 0x4f8)
        return false;
#ifndef LMU_POSE_FIXTURE
    uintptr_t vt = 0;
    if (!read_field(motor, vt) || vt != module_base + 0x1790490)
        return false;
#endif
    for (int i = 0; i < 4; i++)
        if (!read_field(battery + (i == 3 ? 0x20 : i * 8), e.battery[i]) ||
            !std::isfinite(e.battery[i]))
            return false;
    FuelPrepared f;
    e.valid = fuel_prepare_restore(&e.fuel, &e.fuel, &f) == FUEL_OK &&
              std::isfinite(e.fuel_body_mass) &&
              std::fabs(e.fuel_body_mass - f.body_mass) < 1e-10 && e.battery[2] > 0 &&
              e.battery[0] >= 0 && e.battery[0] <= e.battery[2];
    return true;
}
static bool capture_wheels(uintptr_t p, const PoseBody *bodies, const int64_t *indices,
                           WheelInertiaInputs *out, double &error) {
    for (int w = 0; w < 4; w++) {
        uintptr_t wheel = p + 0x618 + w * 0x6b0;
        if (indices[w] < 0 || indices[w] >= 28 || !read_field(wheel + 0x130, out[w].base_130) ||
            !read_field(wheel + 0x4b8, out[w].term_4b8) ||
            !read_field(wheel + 0x5d8, out[w].term_5d8) ||
            !read_field(wheel + 0x4f0, out[w].factor_4f0))
            return false;
    }
    WheelPreparedInertia prepared[4];
    if (wheel_prepare_inertia(out, prepared))
        return false;
    for (int w = 0; w < 4; w++) {
        double e = std::fabs(prepared[w].inverse_axial -
                             bodies[indices[w]].state.mass.inverse_inertia_local[0]);
        if (!std::isfinite(e))
            return false;
        if (e > error)
            error = e;
    }
    return true;
}
static uint64_t capture_tires(const Cycle &c, const PoseBody *bodies, size_t roots,
                              TireInertiaPartition *out, double &error) {
    double cached[4][3];
    for (int w = 0; w < 4; w++) {
        if (!read_field(c.tires[w] + 0xce4, out[w].total_table) ||
            !read_field(c.tires[w] + 0xc20, cached[w]))
            return 100;
        for (int i = 0; i < 8; i++)
            if (!read_field(c.tires[w] + 0x48 + i * 0x60, out[w].sectors[i]))
                return 100;
    }
    TirePreparedInertia prepared[4];
    int status = tire_prepare_inertia(out, prepared);
    if (status)
        return status;
    for (int w = 0; w < 4; w++)
        for (int j = 0; j < 3; j++) {
            if (!std::isfinite(cached[w][j]))
                return 101;
            double e = std::fabs(prepared[w].sector_sum[j] - cached[w][j]);
            if (e > error)
                error = e;
            e = std::fabs(prepared[w].inverse_internal[j] -
                          bodies[roots + w].state.mass.inverse_inertia_local[j]);
            if (e > error)
                error = e;
        }
    return 0;
}
static void WINAPI contacts(void *tire, double dt, double horizon, void *world, double angular) {
    original_contacts(tire, dt, horizon, world, angular);
    if (!cycle)
        return;
    for (int i = 0; i < 4; i++)
        if (cycle->tires[i] == (uintptr_t)tire) {
            ++cycle->contacts[i];
            cycle->fresh |= 1ULL << i;
            return;
        }
}
static void WINAPI forces(void *object) {
    if (cycle && cycle->player == (uintptr_t)object) {
        if (cycle->force_calls < 6)
            cycle->masks[cycle->force_calls] = cycle->fresh;
        else
            ++cycle->errors;
        ++cycle->force_calls;
        cycle->fresh = 0;
    }
    original_forces(object);
}

static bool capture_constraints(uintptr_t player, const uintptr_t *bodies, size_t nb,
                                uintptr_t *addresses, size_t &count, ConstraintRecipe *recipes) {
    if (!read_vector(player + 0x2410, addresses, 64, count))
        return false;
    for (size_t i = 0; i < count; i++) {
        uintptr_t c = addresses[i], vt = 0, links[2];
        auto &r = recipes[i];
        r.identity = i;
        for (size_t j = 0; j < i; j++)
            if (addresses[j] == c)
                return false;
        if (!read_field(c, vt))
            return false;
        size_t link_offset = 0, axis_offset = 0, stride = 0, soft_offset = 0, rows = 0;
        if (vt == module_base + 0x18042a8) {
            r.kind = CONSTRAINT_BAR;
            link_offset = 0xa0;
        } else if (vt == module_base + 0x1804868) {
            r.kind = CONSTRAINT_BALL;
            link_offset = 0x98;
        } else if (vt == module_base + 0x1804a98) {
            r.kind = CONSTRAINT_HINGE;
            link_offset = 0x98;
            axis_offset = 0xa8;
            stride = 0xa8;
            soft_offset = 0xf8;
            rows = 2;
        } else if (vt == module_base + 0x1804d58) {
            r.kind = CONSTRAINT_ORIENTATION;
            link_offset = 0x30;
            axis_offset = 0x60;
            stride = 0xe8;
            soft_offset = 0xd0;
            rows = 3;
        } else
            return false;
        if (!read_field(c + link_offset, links))
            return false;
        for (int j = 0; j < 2; j++) {
            size_t matches = 0;
            for (size_t k = 0; k < nb; k++)
                if (links[j] == bodies[k]) {
                    r.body_indices[j] = k;
                    ++matches;
                }
            if (matches != 1)
                return false;
        }
        if (r.body_indices[0] == r.body_indices[1])
            return false;
        if (r.kind == CONSTRAINT_BAR) {
            if (!read_field(c + 0x50, r.geometry.bar.anchors_local) ||
                !read_field(c + 0x118, r.geometry.bar.softness))
                return false;
        } else if (r.kind == CONSTRAINT_BALL) {
            if (!read_field(c + 0x50, r.geometry.ball.anchors_local))
                return false;
            for (size_t j = 0; j < 3; j++)
                if (!read_field(c + 0x110 + j * 0xf0, r.geometry.ball.softness[j]))
                    return false;
        } else
            for (size_t j = 0; j < rows; j++) {
                if (!read_field(c + axis_offset + j * stride, r.geometry.angular.axes_local[j]) ||
                    !read_field(c + soft_offset + j * stride, r.geometry.angular.softness[j]))
                    return false;
            }
    }
    return true;
}

static void WINAPI dynamics(void *object, double time) {
    LONG64 serial = InterlockedIncrement64(&ipc->callbacks);
    if (serial % 32 || cycle || !lease() || !std::isfinite(time) || !player_matches(object)) {
        original_dynamics(object, time);
        return;
    }
    LONG64 tid = GetCurrentThreadId(),
           prior = InterlockedCompareExchange64(&ipc->writer_thread, tid, 0);
    if (prior && prior != tid) {
        InterlockedIncrement64(&ipc->skipped);
        original_dynamics(object, time);
        return;
    }
    LARGE_INTEGER total_begin, begin, end, total_end;
    QueryPerformanceCounter(&total_begin);
    PoseFrame frame{};
    Cycle c{};
    c.player = (uintptr_t)object;
    uintptr_t bodies[32];
    size_t roots = 0;
    bool valid =
        collect_topology(c.player, bodies, roots, frame.aggregate_mask, c, frame.wheel_body_index);
    frame.root_count = roots;
    frame.total_count = roots + 4;
    if (valid)
        for (size_t i = 0; i < frame.total_count; i++)
            if (!capture_body(bodies[i], frame.before[i])) {
                valid = false;
                break;
            }
    if (valid)
        valid = capture_energy(c.player, bodies, roots, frame.energy_before);
    if (valid)
        valid = capture_wheels(c.player, frame.before, frame.wheel_body_index, frame.wheels_before,
                               frame.wheel_dependency_error);
    if (!valid) {
        InterlockedIncrement64(&ipc->skipped);
        original_dynamics(object, time);
        return;
    }
    uintptr_t constraints[64];
    size_t constraint_count = 0;
    if (!capture_constraints(c.player, bodies, roots, constraints, constraint_count,
                             frame.constraints_before)) {
        InterlockedIncrement64(&ipc->skipped);
        original_dynamics(object, time);
        return;
    }
    frame.constraint_count = constraint_count;
    frame.tires_before_status =
        capture_tires(c, frame.before, roots, frame.tires_before, frame.tire_partition_error);
    frame.prepare_before =
        validate(frame.before, frame.total_count, frame.inertia_error, frame.omega_error);
    cycle = &c;
    QueryPerformanceCounter(&begin);
    original_dynamics(object, time);
    QueryPerformanceCounter(&end);
    cycle = nullptr;
    uintptr_t later[32];
    size_t later_roots = 0;
    uint64_t aggregate = 0;
    Cycle after_cycle{};
    int64_t later_indices[4];
    valid = player_matches(object) &&
            collect_topology(c.player, later, later_roots, aggregate, after_cycle, later_indices) &&
            later_roots == roots && aggregate == frame.aggregate_mask &&
            memcmp(bodies, later, frame.total_count * 8) == 0;
    if (valid)
        for (size_t i = 0; i < frame.total_count; i++)
            if (!capture_body(bodies[i], frame.after[i])) {
                valid = false;
                break;
            }
    if (valid)
        valid = capture_energy(c.player, bodies, roots, frame.energy_after);
    if (valid)
        valid = capture_wheels(c.player, frame.after, frame.wheel_body_index, frame.wheels_after,
                               frame.wheel_dependency_error);
    if (!valid || !lease()) {
        InterlockedIncrement64(&ipc->skipped);
        return;
    }
    uintptr_t later_constraints[64];
    size_t later_count = 0;
    valid = capture_constraints(c.player, bodies, roots, later_constraints, later_count,
                                frame.constraints_after) &&
            later_count == constraint_count &&
            memcmp(constraints, later_constraints, constraint_count * sizeof(uintptr_t)) == 0;
    if (valid)
        for (size_t i = 0; i < constraint_count; i++) {
            auto &a = frame.constraints_before[i];
            auto &b = frame.constraints_after[i];
            if (a.kind != b.kind || a.body_indices[0] != b.body_indices[0] ||
                a.body_indices[1] != b.body_indices[1]) {
                valid = false;
                break;
            }
        }
    if (!valid || !lease()) {
        InterlockedIncrement64(&ipc->skipped);
        return;
    }
    frame.tires_after_status =
        capture_tires(c, frame.after, roots, frame.tires_after, frame.tire_partition_error);
    frame.prepare_after =
        validate(frame.after, frame.total_count, frame.inertia_error, frame.omega_error);
    RigidSavedBody saved[32];
    RigidMassProperties current_mass[32];
    RigidPreparedBody prepared[32];
    for (size_t i = 0; i < frame.total_count; i++) {
        saved[i] = frame.before[i].state;
        current_mass[i] = frame.after[i].state.mass;
    }
    frame.prepare_saved_with_current_mass =
        rigid_prepare_restore(saved, current_mass, frame.total_count, prepared);
    FuelPrepared fuel_prepared;
    frame.prepare_with_fuel =
        (frame.energy_before.valid && frame.energy_after.valid &&
         frame.energy_before.fuel_body_index == frame.energy_after.fuel_body_index)
            ? rigid_prepare_with_fuel(saved, current_mass, frame.total_count,
                                      (size_t)frame.energy_before.fuel_body_index,
                                      &frame.energy_before.fuel, &frame.energy_after.fuel, prepared,
                                      &fuel_prepared)
            : FUEL_BODY_MISMATCH;
    MechanicalDependencies ma{}, mb{};
    MechanicalLinks links{};
    ma.fuel = frame.energy_before.fuel;
    mb.fuel = frame.energy_after.fuel;
    links.fuel = (size_t)frame.energy_before.fuel_body_index;
    memcpy(ma.wheels, frame.wheels_before, sizeof(ma.wheels));
    memcpy(mb.wheels, frame.wheels_after, sizeof(mb.wheels));
    memcpy(ma.tires, frame.tires_before, sizeof(ma.tires));
    memcpy(mb.tires, frame.tires_after, sizeof(mb.tires));
    for (int w = 0; w < 4; w++) {
        links.wheels[w] = (size_t)frame.wheel_body_index[w];
        links.tires[w] = roots + w;
    }
    frame.prepare_mechanical =
        (frame.energy_before.valid && frame.energy_after.valid && !frame.tires_before_status &&
         !frame.tires_after_status &&
         frame.energy_before.fuel_body_index == frame.energy_after.fuel_body_index)
            ? mechanical_prepare(saved, current_mass, frame.total_count, &links, &ma, &mb, prepared,
                                 &fuel_prepared)
            : MECHANICAL_DEPENDENCY_INVALID;
    MechanicalConstraintPrepared integrated;
    frame.prepare_integrated = frame.prepare_mechanical
                                   ? MECHANICAL_CONSTRAINT_BODY_FAILURE
                                   : mechanical_constraint_prepare(
                                         saved, current_mass, frame.total_count, &links, &ma, &mb,
                                         frame.constraints_before, constraint_count, &integrated);
    if (!frame.prepare_integrated)
        frame.integrated_row_count = integrated.row_count;
    frame.tick_ms = GetTickCount64();
    frame.simulation_time = time;
    frame.force_calls = c.force_calls;
    frame.phase_errors = c.errors;
    memcpy(frame.contacts, c.contacts, sizeof(c.contacts));
    memcpy(frame.fresh_mask_at_force, c.masks, sizeof(c.masks));
    const uint64_t expected[6] = {15, 0, 0, 15, 0, 0};
    if (c.force_calls != 6 || memcmp(c.masks, expected, sizeof(expected)))
        ++frame.phase_errors;
    for (int i = 0; i < 4; i++)
        if (c.contacts[i] != 2)
            ++frame.phase_errors;
    QueryPerformanceCounter(&total_end);
    frame.original_us = (end.QuadPart - begin.QuadPart) * 1e6 / ipc->qpc_frequency;
    frame.observer_total_us =
        (total_end.QuadPart - total_begin.QuadPart) * 1e6 / ipc->qpc_frequency;
    InterlockedIncrement64(&ipc->sequence);
    ipc->frame = frame;
    MemoryBarrier();
    InterlockedIncrement64(&ipc->sequence);
    InterlockedIncrement64(&ipc->observations);
}

static int check_game_hash(void) {
#ifdef LMU_POSE_FIXTURE
    return 1;
#else
    static const unsigned char expected[32] = {0xb3, 0x7a, 0x1d, 0xd3, 0xa5, 0x91, 0x23, 0x91,
                                               0xdb, 0x23, 0x60, 0x51, 0x99, 0x5c, 0xaa, 0xdd,
                                               0x6d, 0x55, 0x58, 0x63, 0x1e, 0xd9, 0xce, 0x5d,
                                               0x68, 0xf8, 0x39, 0x7a, 0xbe, 0x2f, 0x6a, 0x8c};
    wchar_t path[32768];
    if (!GetModuleFileNameW(NULL, path, 32768))
        return 0;
    HANDLE file =
        CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    int ok = 0;
    if (CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        unsigned char buffer[65536], digest[32];
        DWORD got = 0, size = 32;
        BOOL read_ok;
        do {
            read_ok = ReadFile(file, buffer, sizeof(buffer), &got, NULL);
            if (!read_ok || (!CryptHashData(hash, buffer, got, 0)))
                break;
        } while (got);
        if (read_ok && got == 0 && CryptGetHashParam(hash, HP_HASHVAL, digest, &size, 0) &&
            size == 32)
            ok = memcmp(digest, expected, 32) == 0;
    }
    if (hash)
        CryptDestroyHash(hash);
    if (provider)
        CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return ok;
#endif
}

static DWORD WINAPI worker(void *) {
    wchar_t name[96];
    pose_mapping_name(name, GetCurrentProcessId());
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping)
        return 0;
    ipc = (PoseIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*ipc));
    if (!ipc || ipc->magic != POSE_MAGIC || ipc->version != 1 || ipc->pid != GetCurrentProcessId())
        return 0;
    ipc->status = 1;
    module_base = (uintptr_t)GetModuleHandleW(NULL);
    if (!check_game_hash()) {
        ipc->error = 100;
        ipc->status = 4;
        return 0;
    }
#ifdef LMU_POSE_FIXTURE
    targets[0] = (void *)GetProcAddress((HMODULE)module_base, "FixtureDynamics");
    targets[1] = (void *)GetProcAddress((HMODULE)module_base, "FixtureContacts");
    targets[2] = (void *)GetProcAddress((HMODULE)module_base, "FixtureForces");
#else
    const uintptr_t offsets[3] = {0xec87e0, 0xef5b80, 0xec7e30};
    const unsigned char prologs[3][22] = {
        {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20},
        {0x4c, 0x89, 0x4c, 0x24, 0x20, 0x48, 0x89, 0x4c, 0x24, 0x08, 0x55,
         0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57},
        {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70, 0x10, 0x48, 0x89, 0x78, 0x18,
         0x4c, 0x89, 0x70, 0x20}};
    const size_t lengths[3] = {15, 22, 19};
    for (int i = 0; i < 3; i++) {
        targets[i] = (void *)(module_base + offsets[i]);
        if (memcmp(targets[i], prologs[i], lengths[i])) {
            ipc->error = 101 + i;
            ipc->status = 4;
            return 0;
        }
    }
#endif
    for (int i = 0; i < 3; i++) {
        if (!targets[i]) {
            ipc->error = 105;
            ipc->status = 4;
            return 0;
        }
        memcpy(originals[i], targets[i], 32);
    }
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    ipc->qpc_frequency = freq.QuadPart;
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        ipc->error = 200 + status;
        ipc->status = 4;
        return 0;
    }
    void *hooks[3] = {(void *)&dynamics, (void *)&contacts, (void *)&forces};
    void **trampolines[3] = {(void **)&original_dynamics, (void **)&original_contacts,
                             (void **)&original_forces};
    for (int i = 0; i < 3; i++) {
        status = MH_CreateHook(targets[i], hooks[i], trampolines[i]);
        if (status != MH_OK) {
            ipc->error = 300 + status;
            ipc->status = 4;
            return 0;
        }
    }
    LONG64 now = GetTickCount64(), hb = ipc->heartbeat_ms;
    if (ipc->stop || hb > now || now - hb > 2000) {
        ipc->error = 106;
        ipc->status = 4;
        return 0;
    }
    status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        ipc->error = 400 + status;
        ipc->status = 4;
        return 0;
    }
    ipc->status = 2;
    ULONGLONG started = GetTickCount64();
    while (!ipc->stop) {
        now = GetTickCount64();
        hb = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
        if (hb > now || now - hb > 2000 || (ULONGLONG)now - started > 120000)
            break;
        Sleep(20);
    }
    ipc->allow_observation = 0;
    status = MH_DisableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        ipc->error = 500 + status;
        ipc->status = 5;
        return 0;
    }
    for (int i = 0; i < 3; i++)
        if (memcmp(targets[i], originals[i], 32)) {
            ipc->error = 110 + i;
            ipc->status = 5;
            return 0;
        }
    ipc->status = 3;
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (t)
            CloseHandle(t);
    }
    return TRUE;
}
