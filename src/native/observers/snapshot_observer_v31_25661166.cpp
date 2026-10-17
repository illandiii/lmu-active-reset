/* Native BallJoint post-call observer; no writes to the vehicle or constraint. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <cmath>
#include <cstring>
#include "snapshot_observer_v31_25661166.h"
#include "MinHook.h"
#include "../restore/contact_rebuild.h"
extern "C" {
#include "../restore/physical_scoring_transaction.h"
#include "../restore/native_memory_backend.h"
}
static RestorePlan joint_plan;
static PoseIPC *ipc;
static uintptr_t module_base;
static uintptr_t native_bindings[RESTORE_OBJECT_COUNT];
static RestoreBindings frozen_bindings;
static uint64_t planning_epoch;
static RestorePlan plan_workspace, native_plan;
static LONG64 applied_context_generation;
static void(WINAPI *original_dynamics)(void *, double);
static void *targets[2];
static unsigned char originals[2][32];
static void(WINAPI *original_contacts)(void *, double, double, void *, double);
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
static uintptr_t player() {
#ifdef LMU_POSE_FIXTURE
    typedef void *(WINAPI * F)();
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixturePlayer");
    return f ? (uintptr_t)f() : 0;
#else
    uintptr_t owner = 0, p = 0;
    return read_field(module_base + 0x1e194c8, owner) && read_field(owner + 0x159a8, p) ? p : 0;
#endif
}
static bool vector(uintptr_t p, uintptr_t *out, size_t &count) {
    uintptr_t v[3];
    if (!read_field(p, v) || v[0] < 0x10000 || v[1] < v[0] || v[2] < v[1] || (v[1] - v[0]) % 8 ||
        (v[2] - v[0]) % 8)
        return false;
    count = (v[1] - v[0]) / 8;
    if (!count || count > 64)
        return false;
    for (size_t i = 0; i < count; i++)
        if (!read_field(v[0] + 8 * i, out[i]))
            return false;
    return true;
}
static bool body(uintptr_t p, RigidSavedBody &b) {
    return read_field(p + 0x58, b.mass.inverse_mass) &&
           read_field(p + 0xf8, b.mass.inverse_inertia_local) &&
           read_field(p + 0x40, b.position_world) && read_field(p + 0x28, b.velocity_world) &&
           read_field(p + 0x90, b.orientation) && read_field(p + 0x110, b.momentum_world);
}

static int planning_context(void *, uint64_t epoch) {
    return epoch == planning_epoch && applied_context_generation > 0 &&
           applied_context_generation ==
               InterlockedCompareExchange64(&ipc->context_generation, 0, 0) &&
           lease() && ipc->writer_thread == (LONG64)GetCurrentThreadId() &&
           player() == native_bindings[104];
}
static int planning_allowed(void *, uint32_t slot, uint32_t offset, uint32_t size) {
    return restore_binding_field(&frozen_bindings, slot, offset, size);
}
static int planning_read(void *, uint32_t slot, uint32_t offset, void *target, uint32_t size) {
    if (!planning_allowed(nullptr, slot, offset, size))
        return 0;
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), (void *)(frozen_bindings.address[slot] + offset),
                             target, size, &got) &&
           got == size;
}
/* Planning backend intentionally has no write callback. */
static RestoreBackend planning_backend = {nullptr, planning_context, planning_allowed,
                                          planning_read, nullptr};
static uint64_t hash_bytes(uint64_t, const void *, size_t);
static bool mechanical(uintptr_t p, const uintptr_t *bodies, SnapshotFrame &f) {
    auto &d = f.dependencies;
    auto &links = f.links;
    uintptr_t fuel = p + 0x448, fuel_body = 0, branch = 0;
    if (!read_field(fuel, d.fuel.quantity) || !read_field(fuel + 8, d.fuel.quantity_rate) ||
        !read_field(fuel + 0x60, d.fuel.density) || !read_field(fuel + 0x80, d.fuel.base_mass) ||
        !read_field(fuel + 0x78, fuel_body) || !read_field(p + 0x550, branch))
        return false;
    size_t matches = 0;
    for (size_t i = 0; i < f.root_count; i++)
        if (bodies[i] == fuel_body) {
            links.fuel = i;
            ++matches;
        }
    if (matches != 1)
        return false;
    if (!branch) {
        uintptr_t game = 0;
        float initial = 0;
        unsigned char indices[14];
        double term = 0, tail = 0, extra = 0;
        if (!read_field(p + 0x4bc0, game) || !read_field(game + 0x440d8, initial) ||
            !read_field(p + 0x138, indices))
            return false;
        d.fuel.additional_mass = initial;
        for (int i = 0; i < 14; i++)
            if (indices[i] > 3)
                return false;
        for (int i = 0; i < 13; i++) {
            if (!read_field(p + 0x148 + i * 0x20 + indices[i] * 8, term))
                return false;
            d.fuel.additional_mass += term;
        }
        if (!read_field(p + 0x2e8 + indices[13] * 8, tail) || !read_field(p + 0x558, extra))
            return false;
        tail += extra;
        if (!read_field(p + 0x2608, term))
            return false;
        d.fuel.additional_mass += term;
        d.fuel.additional_mass += tail;
    }
    for (int w = 0; w < 4; w++) {
        uintptr_t wheel = p + 0x618 + w * 0x6b0, body = 0, tire = 0;
        if (!read_field(wheel + 0x1d0, body) || !read_field(wheel + 0x3e8, tire) ||
            tire + 0xac0 != bodies[f.root_count + w])
            return false;
        matches = 0;
        for (size_t i = 0; i < f.root_count; i++)
            if (bodies[i] == body) {
                links.wheels[w] = i;
                ++matches;
            }
        if (matches != 1)
            return false;
        links.tires[w] = f.root_count + w;
        uintptr_t hub = 0, rear = 0;
        unsigned char flags[2];
        auto &phase = f.wheel_phase[w];
        if (!read_field(p + 0x20d8, rear) || !read_field(wheel + 0x301, flags) || flags[0] ||
            flags[1])
            return false;
        if (w >= 2 && rear)
            hub = rear;
        else if (!read_field(wheel + 0x1d8, hub))
            return false;
        size_t hub_matches = 0;
        for (size_t i = 0; i < f.root_count; i++)
            if (bodies[i] == hub) {
                phase.hub_index = (uint32_t)i;
                ++hub_matches;
            }
        if (hub_matches != 1)
            return false;
        phase.wheel_index = (uint32_t)links.wheels[w];
        phase.spin.attached = 1;
        if (!read_field(wheel + 0x120, phase.spin.angle) ||
            !read_field(wheel + 0x128, phase.spin.angular_rate))
            return false;
        f.topology_token = hash_bytes(f.topology_token, &phase.hub_index, sizeof(uint32_t) * 2);
        if (!read_field(wheel + 0x130, d.wheels[w].base_130) ||
            !read_field(wheel + 0x4b8, d.wheels[w].term_4b8) ||
            !read_field(wheel + 0x5d8, d.wheels[w].term_5d8) ||
            !read_field(wheel + 0x4f0, d.wheels[w].factor_4f0) ||
            !read_field(tire + 0xce4, d.tires[w].total_table))
            return false;
        for (int sector = 0; sector < 8; sector++)
            if (!read_field(tire + 0x48 + sector * 0x60, d.tires[w].sectors[sector]))
                return false;
        for (int i = 0; i < 8; i++) {
            auto &cache = f.native_cache.sectors[w][i];
            if (!read_field(tire + 0x30 + i * 0x60, cache.first_moment) ||
                !read_field(tire + 0x60 + i * 0x60, cache.aggregate))
                return false;
        }
        uintptr_t pool = 0, inner = 0, outer = 0, table = 0;
        if (!read_field(tire + 0x318, pool) || !read_field(tire + 0xa10, inner) ||
            !read_field(tire + 0xa18, outer) || !read_field(tire + 0xa28, table))
            return false;
        uintptr_t refs[4] = {pool, inner, outer, table};
        for (auto ref : refs)
            if (ref < 0x10000 || ref >= 0x800000000000ULL)
                return false;
        f.topology_token = hash_bytes(f.topology_token, refs, sizeof(refs));
        auto &grid = f.thermal_grid.tires[w];
        for (int sector = 0; sector < 8; sector++) {
            uint16_t indices[9];
            if (!read_field(tire + 0x10 + sector * 0x60, indices))
                return false;
            f.topology_token = hash_bytes(f.topology_token, indices, sizeof(indices));
            for (int j = 0; j < 9; j++) {
                uint32_t idx = indices[j];
                int row = sector * 9 + j;
                if (idx >= 256)
                    return false;
                grid.angular_index[row] = idx;
                for (int previous = 0; previous < row; previous++)
                    if (grid.angular_index[previous] == idx)
                        return false;
                uintptr_t pointers[9], expected[9];
                if (!read_field(table + idx * 72, pointers))
                    return false;
                for (int k = 0; k < 7; k++)
                    expected[k] = pool + idx * 0x770 + k * 0x110;
                expected[7] = inner + idx * 0x60;
                expected[8] = outer + idx * 0x60;
                for (int k = 0; k < 9; k++) {
                    size_t matches = 0;
                    for (auto pointer : pointers)
                        if (pointer == expected[k])
                            ++matches;
                    if (matches != 1)
                        return false;
                    if (k < 7) {
                        auto &h = f.tread_history.nodes[w][row][k];
                        if (!read_field(expected[k] + 0x28, h.fields_28) ||
                            !read_field(expected[k] + 0x100, h.field_100))
                            return false;
                    }
                    native_bindings[RESTORE_THERMAL_BASE + (w * 72 + row) * 9 + k] = expected[k];
                    auto &cell = grid.cells[row][k];
                    if (!read_field(expected[k], cell.layers) ||
                        !read_field(expected[k] + 0x40, cell.coefficients))
                        return false;
                }
            }
        }
        auto &brake = f.brake_state[w];
        if (!read_field(wheel + 0x5d8, brake.fields_5d8) ||
            !read_field(wheel + 0x630, brake.field_630) ||
            !read_field(wheel + 0x138, brake.cache_138))
            return false;
        double inverse = f.bodies[links.wheels[w]].mass.inverse_inertia_local[0];
        if (brake.fields_5d8[0] != d.wheels[w].term_5d8 ||
            std::fabs(brake.cache_138 - inverse) > 32 * 2.2204460492503131e-16 * std::fabs(inverse))
            return false;
        native_bindings[96 + w] = tire;
        native_bindings[100 + w] = wheel;
        auto &state = f.tire_state[w];
        if (!read_field(tire + 0xac8, f.tire_mass_cache[w]))
            return false;
        if (!read_field(tire + 0xc18, state.total_mass) ||
            !read_field(tire + 0xc20, state.cached_sector_inertia) ||
            !read_field(tire + 0xc, state.update_phase) ||
            !read_field(tire + 0xfb0, state.thermal_fb0) ||
            !read_field(tire + 0xfb8, state.thermal_fb8) ||
            !read_field(tire + 0xfc0, state.accumulated_fc0))
            return false;
        for (int sector = 0; sector < 8; sector++)
            if (!read_field(tire + 0x28 + sector * 0x60, state.sector_mass[sector]))
                return false;
    }
    RigidMassProperties mass[32];
    RigidPreparedBody out[32];
    FuelPrepared prepared;
    for (size_t i = 0; i < f.total_count; i++)
        mass[i] = f.bodies[i].mass;
    f.mechanical_status =
        mechanical_prepare(f.bodies, mass, f.total_count, &links, &d, &d, out, &prepared);
    return true;
}

static ShiftTransientBank shift_bank;
static ShiftTransientState shift_prepared;
static PowertrainControlBank control_bank;
static PowertrainControl control_prepared;
static DrivelineOutputsBank outputs_bank;
static DrivelineOutputs outputs_prepared;
static DrivelineCacheBank driveline_bank;
static DrivelineCache driveline_prepared;
static BrakeBank brake_bank;
static BrakeState brake_prepared[4];
static WheelPhaseBank phase_bank;
static WheelPhase phase_prepared[4];
static TreadHistoryGrid tread_prepared;
static ThermalHistoryBank thermal_bank;
static ThermalGrid thermal_prepared;
static NativeCache saved_cache;
static EngineBank checkpoint_bank;
static EnginePreparation checkpoint_prepared;
static uint64_t process_epoch, bank_epoch = 1, bank_sequence, bank_saves, bank_preparations,
                               bank_rejections;
static uint64_t prior_topology, prior_configuration;
static double prior_time;
static uint64_t hash_bytes(uint64_t h, const void *data, size_t n) {
    const auto *p = (const unsigned char *)data;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}
static void invalidate_bank() {
    if (checkpoint_bank.prior.state.state.vehicle.mechanical.base.occupied) {
        memset(&checkpoint_bank, 0, sizeof(checkpoint_bank));
        ++bank_epoch;
    }
    prior_topology = prior_configuration = 0;
}
static void prepare_native_bank(SnapshotFrame &f) {
    // REA ECDD90/ECD890: shared terminal failure latch is not yet restorable.
    // Refuse preparation, invalidate the earlier bank, and retain diagnosis.
    if (f.terminal_failure) {
        invalidate_bank();
        f.bank_status = CHECKPOINT_INVALID_STATE;
        f.plan_status = RESTORE_BAD_PLAN;
        f.bank_generation = bank_epoch;
        f.bank_rejections = ++bank_rejections;
        f.bank_saves = bank_saves;
        f.bank_preparations = bank_preparations;
        return;
    }
    uint64_t config = 14695981039346656037ULL;
    config = hash_bytes(config, &f.dependencies.fuel.density, sizeof(double) * 3);
    config = hash_bytes(config, &f.powertrain.engine_type_rva, sizeof(uint64_t));
    for (int w = 0; w < 4; w++) {
        config = hash_bytes(config, &f.dependencies.wheels[w].base_130, sizeof(double));
        config = hash_bytes(config, &f.dependencies.wheels[w].factor_4f0, sizeof(double));
    }
    config = hash_bytes(config, &f.integration.channel_count, sizeof(uint32_t));
    config = hash_bytes(config, &f.powertrain_control.table_token, sizeof(uint64_t));
    config = hash_bytes(config, &f.shift_observed.gears[0], sizeof(int32_t));
    config = hash_bytes(config, &f.shift_observed.dt_b0, sizeof(double));
    f.configuration_token = config ? config : 1;
    if (prior_topology &&
        (f.topology_token != prior_topology || f.configuration_token != prior_configuration ||
         f.simulation_time < prior_time))
        invalidate_bank();
    prior_topology = f.topology_token;
    prior_configuration = f.configuration_token;
    prior_time = f.simulation_time;
    PartialCheckpoint current{};
    current.sequence = ++bank_sequence;
    current.simulation_time = f.simulation_time;
    current.identity.process_epoch = process_epoch;
    current.identity.session_epoch = bank_epoch;
    current.identity.vehicle_epoch = f.topology_token;
    current.identity.configuration_epoch = f.configuration_token;
    const unsigned char sha[32] = {0x66, 0x94, 0x23, 0x37, 0x81, 0x3b, 0x30, 0x1b, 0x87, 0xd1, 0x2e,
                                   0xdf, 0xa3, 0x30, 0xf3, 0x33, 0x54, 0xa2, 0xa4, 0xb8, 0xcd, 0x9b,
                                   0x04, 0x88, 0x85, 0xcb, 0xf7, 0x7e, 0xb6, 0xf9, 0x40, 0xcf};
    memcpy(current.identity.executable_sha256, sha, 32);
    current.body_count = f.total_count;
    current.constraint_count = f.constraint_count;
    memcpy(current.bodies, f.bodies, sizeof(current.bodies));
    memcpy(current.recipes, f.recipes, sizeof(current.recipes));
    memcpy(current.memory, f.memory, sizeof(current.memory));
    RigidMassProperties mass[32];
    for (size_t i = 0; i < f.total_count; i++)
        mass[i] = f.bodies[i].mass;
    int status;
    if (!checkpoint_bank.prior.state.state.vehicle.mechanical.base.occupied) {
        status = checkpoint_save_engine(&checkpoint_bank, &current, &f.links, &f.dependencies,
                                        f.tire_state, &f.powertrain, &f.transmission,
                                        f.tire_mass_cache, &f.history, &f.integration);
        if (!status && !thermal_history_save(&thermal_bank, current.sequence, &f.thermal_grid,
                                             &f.tread_history))
            status = CHECKPOINT_INVALID_STATE;
        if (!status && !wheel_phase_save(&phase_bank, &current, f.wheel_phase))
            status = CHECKPOINT_INVALID_STATE;
        if (!status && !brake_state_save(&brake_bank, current.sequence, f.brake_state))
            status = CHECKPOINT_INVALID_STATE;
        if (!status && !driveline_cache_save(&driveline_bank, current.sequence, &f.driveline_cache))
            status = CHECKPOINT_INVALID_STATE;
        if (!status &&
            !driveline_outputs_save(&outputs_bank, current.sequence, &f.driveline_outputs))
            status = CHECKPOINT_INVALID_STATE;
        if (!status &&
            !powertrain_control_save(&control_bank, current.sequence, &f.powertrain_control))
            status = CHECKPOINT_INVALID_STATE;
        if (!status && !shift_transient_save(&shift_bank, current.sequence, &f.shift_observed))
            status = CHECKPOINT_INVALID_STATE;
        if (!status) {
            saved_cache = f.native_cache;
            saved_cache.sequence = current.sequence;
            ++bank_saves;
        }
    } else {
        status = checkpoint_prepare_engine(
            &checkpoint_bank, &current.identity, mass, f.total_count, f.recipes, f.constraint_count,
            &f.dependencies, f.tire_state, &f.powertrain, &f.transmission, f.tire_mass_cache,
            &f.history, &f.integration, &checkpoint_prepared);
        if (!status && !thermal_history_prepare(
                           &thermal_bank,
                           checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                           &f.thermal_grid, &f.tread_history, &thermal_prepared, &tread_prepared))
            status = CHECKPOINT_INVALID_STATE;
        if (!status &&
            !wheel_phase_prepare(&phase_bank,
                                 &checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved,
                                 f.wheel_phase, f.bodies, f.total_count, phase_prepared))
            status = CHECKPOINT_INVALID_STATE;
        if (!status && !brake_state_prepare(
                           &brake_bank,
                           checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                           f.brake_state, brake_prepared))
            status = CHECKPOINT_INVALID_STATE;
        if (!status && !driveline_cache_prepare(
                           &driveline_bank,
                           checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                           &f.driveline_cache, &driveline_prepared))
            status = CHECKPOINT_INVALID_STATE;
        if (!status && !driveline_outputs_prepare(
                           &outputs_bank,
                           checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                           &f.driveline_outputs, &outputs_prepared))
            status = CHECKPOINT_INVALID_STATE;
        if (!status && !powertrain_control_prepare(
                           &control_bank,
                           checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                           &f.powertrain_control, &control_prepared))
            status = CHECKPOINT_INVALID_STATE;
        if (!status && !shift_transient_prepare(
                           &shift_bank,
                           checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                           &f.shift_observed, &shift_prepared))
            status = CHECKPOINT_INVALID_STATE;
        if (!status)
            ++bank_preparations;
    }
    if (status)
        ++bank_rejections;
    f.bank_generation = bank_epoch;
    f.bank_saved_sequence =
        checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence;
    f.bank_status = status;
    f.bank_saves = bank_saves;
    f.bank_preparations = bank_preparations;
    f.bank_rejections = bank_rejections;
    if (!status && bank_preparations &&
        checkpoint_prepared.prior.state.state.vehicle.mechanical.sequence ==
            f.bank_saved_sequence) {
        planning_epoch = bank_epoch;
        frozen_bindings.epoch = bank_epoch;
        memcpy(frozen_bindings.address, native_bindings, sizeof(native_bindings));
        frozen_bindings.body_count = (uint32_t)f.total_count;
        frozen_bindings.constraint_count = (uint32_t)f.constraint_count;
        frozen_bindings.engine_channels = f.integration.channel_count;
        for (size_t i = 0; i < f.constraint_count; i++)
            frozen_bindings.constraint_kind[i] = f.recipes[i].kind;
        f.plan_status =
            vehicle_restore_plan(&checkpoint_bank, &checkpoint_prepared, planning_epoch,
                                  &planning_backend, &plan_workspace, &native_plan);
        if (!f.plan_status) {
            plan_workspace = native_plan;
            f.plan_status = native_cache_plan(
                &saved_cache, &checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved,
                &planning_backend, &plan_workspace, &native_plan);
        }
        if (!f.plan_status) {
            plan_workspace = native_plan;
            f.plan_status = wheel_phase_plan(
                &phase_bank, &checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved,
                &planning_backend, &plan_workspace, &native_plan);
        }
        if (!f.plan_status) {
            plan_workspace = native_plan;
            f.thermal_plan_status =
                restore_thermal_plan(&thermal_bank, f.bank_saved_sequence, &planning_backend,
                                     &plan_workspace, &native_plan);
            f.plan_status = f.thermal_plan_status;
        }
        if (!f.plan_status) {
            plan_workspace = native_plan;
            f.plan_status = brake_state_plan(&brake_bank, f.bank_saved_sequence, &planning_backend,
                                             &plan_workspace, &native_plan);
        }
        if (!f.plan_status) {
            plan_workspace = native_plan;
            f.plan_status = driveline_cache_plan(&driveline_bank, f.bank_saved_sequence,
                                                 &planning_backend, &plan_workspace, &native_plan);
        }
        if (!f.plan_status) {
            plan_workspace = native_plan;
            f.plan_status =
                driveline_outputs_plan(&outputs_bank, f.bank_saved_sequence, &planning_backend,
                                       &plan_workspace, &native_plan);
        }
        if (!f.plan_status) {
            plan_workspace = native_plan;
            f.plan_status =
                powertrain_control_plan(&control_bank, f.bank_saved_sequence, &planning_backend,
                                        &plan_workspace, &native_plan);
        }
        if (!f.plan_status) {
            plan_workspace = native_plan;
            f.plan_status =
                shift_transient_plan(&shift_bank, f.bank_saved_sequence, &shift_prepared,
                                     &planning_backend, &plan_workspace, &native_plan);
        }
        if (!f.plan_status)
            f.binding_status = restore_bindings_validate(&frozen_bindings, &native_plan);
        if (!f.plan_status && !f.binding_status) {
            f.plan_fields = native_plan.count;
            for (size_t i = 0; i < native_plan.count; i++)
                f.plan_bytes += native_plan.fields[i].size;
        }
    }
}
static bool capture_control(uintptr_t p, PowertrainControl &c) {
    uintptr_t engine = 0;
    if (!read_field(p + 0x46f0, engine))
        return false;
    if (!read_field(p + 0x4810, c.ramp_d0) || !read_field(p + 0x4818, c.index_d8) ||
        !read_field(p + 0x481c, c.index_dc) || !read_field(p + 0x4820, c.active_e0) ||
        !read_field(p + 0x4821, c.latched_e1) || !read_field(p + 0x4828, c.timer_e8) ||
        !read_field(p + 0x4830, c.factor_f0) || !read_field(p + 0x4838, c.factor_f8) ||
        !read_field(engine + 0x50, c.engine_50) || !read_field(engine + 0x1b0, c.engine_1b0))
        return false;
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned i = 0; i < 4; i++) {
        uintptr_t v[3];
        if (!read_field(p + 0x4740 + 24 * i, v) || v[1] < v[0] || v[2] < v[1] ||
            (v[1] - v[0]) % 8 || (v[2] - v[0]) % 8 || (v[1] - v[0]) / 8 > 64)
            return false;
        c.table_counts[i] = (uint32_t)((v[1] - v[0]) / 8);
        hash = hash_bytes(hash, &c.table_counts[i], 4);
        for (unsigned j = 0; j < c.table_counts[i]; j++) {
            double value;
            if (!read_field(v[0] + 8 * j, value) || !std::isfinite(value))
                return false;
            hash = hash_bytes(hash, &value, 8);
        }
    }
    c.table_token = hash ? hash : 1;
    return powertrain_control_valid(&c);
}
static bool capture(uintptr_t p, SnapshotFrame &f) {
    if (!capture_control(p, f.powertrain_control))
        return false;
    auto &shift = f.shift_observed;
    if (!read_field(p + 0x4ac0, shift.timer_4ac0) || !read_field(p + 0x4668, shift.control_4668) ||
        !read_field(p + 0xa8, shift.clock_a8) || !read_field(p + 0x50, shift.event_50) ||
        !read_field(p + 0xb0, shift.dt_b0) || !read_field(p + 0x4b30, shift.accumulators[0]) ||
        !read_field(p + 0x4bb8, shift.accumulators[1]) || !read_field(p + 0x4ba0, shift.gears) ||
        !read_field(p + 0x4ae8, shift.notification) || !read_field(p + 0x48, shift.request))
        return false;
    if (!read_field(p + 0x4900, f.driveline_cache.drive_type) ||
        !read_field(p + 0x49d8, f.driveline_cache.torque[0]) ||
        !read_field(p + 0x4a18, f.driveline_cache.torque[1]) ||
        !read_field(p + 0x4a58, f.driveline_cache.torque[2]) ||
        !driveline_cache_valid(&f.driveline_cache))
        return false;
    if (!read_field(p + 0x46d0, f.driveline_outputs.hdv_46d0) ||
        !read_field(p + 0x46e0, f.driveline_outputs.hdv_46e0) ||
        !read_field(p + 0x46e8, f.driveline_outputs.hdv_46e8) ||
        !read_field(p + 0x4b38, f.driveline_outputs.hdv_4b38) ||
        !driveline_outputs_valid(&f.driveline_outputs))
        return false;
    uintptr_t game_vehicle = 0;
    if (!read_field(p + 0x4bc0, game_vehicle) ||
        !read_field(game_vehicle + 0x1cf9c, f.terminal_failure))
        return false;
    uintptr_t bodies[64], constraints[64];
    size_t roots = 0, nc = 0;
    if (player() != p || !vector(p + 0x23f8, bodies, roots) || roots > 28 ||
        !vector(p + 0x2410, constraints, nc))
        return false;
    for (int w = 0; w < 4; w++) {
        uintptr_t tire = 0;
        if (!read_field(p + 0x618 + w * 0x6b0 + 0x3e8, tire) || tire < 0x10000)
            return false;
        bodies[roots + w] = tire + 0xac0;
    }
    f.root_count = roots;
    f.total_count = roots + 4;
    f.constraint_count = nc;
    uint64_t token = hash_bytes(14695981039346656037ULL, &p, sizeof(p));
    token = hash_bytes(token, bodies, (roots + 4) * sizeof(uintptr_t));
    token = hash_bytes(token, constraints, nc * sizeof(uintptr_t));
    token = hash_bytes(token, &f.driveline_cache.drive_type, sizeof(f.driveline_cache.drive_type));
    f.topology_token = token ? token : 1;
    RigidMassProperties mass[32];
    RigidPreparedBody prepared[32];
    for (size_t i = 0; i < f.total_count; i++) {
        for (size_t j = 0; j < i; j++)
            if (bodies[j] == bodies[i])
                return false;
        native_bindings[i] = bodies[i];
        if (!body(bodies[i], f.bodies[i]))
            return false;
        mass[i] = f.bodies[i].mass;
        auto &cache = f.native_cache.bodies[i];
        if (!read_field(bodies[i] + 8, cache.mass) ||
            !read_field(bodies[i] + 0x140, cache.inertia_local) ||
            !read_field(bodies[i] + 0x60, cache.inverse_world) ||
            !read_field(bodies[i] + 0xe0, cache.omega))
            return false;
        auto &h = f.history.bodies[i];
        if (!read_field(bodies[i] + 0x10, h.force) || !read_field(bodies[i] + 0x128, h.torque) ||
            !read_field(bodies[i] + 0xd8, h.renormalization))
            return false;
        for (int j = 0; j < 3; j++)
            if (!std::isfinite(h.force[j]) || !std::isfinite(h.torque[j]))
                return false;
    }
    if (rigid_prepare_restore(f.bodies, mass, f.total_count, prepared))
        return false;
    for (size_t i = 0; i < f.total_count; i++) {
        double iw[6], omega[3];
        if (!read_field(bodies[i] + 0x60, iw) || !read_field(bodies[i] + 0xe0, omega))
            return false;
        for (int j = 0; j < 6; j++) {
            if (!std::isfinite(iw[j]))
                return false;
            double e = std::fabs(iw[j] - prepared[i].inverse_inertia_world[j]);
            if (e > f.inertia_error)
                f.inertia_error = e;
        }
        for (int j = 0; j < 3; j++) {
            if (!std::isfinite(omega[j]))
                return false;
            double e = std::fabs(omega[j] - prepared[i].angular_velocity_world[j]);
            if (e > f.omega_error)
                f.omega_error = e;
        }
    }
    for (size_t i = 0; i < nc; i++) {
        native_bindings[32 + i] = constraints[i];
        uintptr_t c = constraints[i], vt = 0, pair[2];
        auto &r = f.recipes[i];
        auto &m = f.memory[i];
        for (size_t j = 0; j < i; j++)
            if (constraints[j] == c)
                return false;
        if (!read_field(c, vt))
            return false;
        size_t link = 0, axis = 0, stride = 0, soft = 0, impulse = 0, lower = 0, upper = 0,
               rows = 0;
        if (vt == module_base + 0x1807698) {
            r.kind = CONSTRAINT_BAR;
            link = 0xa0;
            rows = 1;
            soft = 0x118;
            impulse = 0x120;
            lower = 0x128;
            upper = 0x130;
        } else if (vt == module_base + 0x1807ba8) {
            r.kind = CONSTRAINT_BALL;
            link = 0x98;
            rows = 3;
            stride = 0xf0;
            soft = 0x110;
            impulse = 0x118;
            lower = 0x120;
            upper = 0x128;
        } else if (vt == module_base + 0x1807d18) {
            r.kind = CONSTRAINT_HINGE;
            link = 0x98;
            rows = 2;
            axis = 0xa8;
            stride = 0xa8;
            soft = 0xf8;
            impulse = 0x100;
            lower = 0x108;
            upper = 0x110;
        } else if (vt == module_base + 0x1808098) {
            r.kind = CONSTRAINT_ORIENTATION;
            link = 0x30;
            rows = 3;
            axis = 0x60;
            stride = 0xe8;
            soft = 0xd0;
            impulse = 0xd8;
            lower = 0xe0;
            upper = 0xe8;
        } else
            return false;
        if (!read_field(c + link, pair))
            return false;
        for (int side = 0; side < 2; side++) {
            size_t matches = 0;
            for (size_t j = 0; j < roots; j++)
                if (pair[side] == bodies[j]) {
                    r.body_indices[side] = m.body_indices[side] = j;
                    ++matches;
                }
            if (matches != 1)
                return false;
        }
        if (pair[0] == pair[1])
            return false;
        r.identity = m.identity = i;
        m.kind = r.kind;
        m.row_count = rows;
        if (r.kind == CONSTRAINT_BAR) {
            if (!read_field(c + 0x50, r.geometry.bar.anchors_local) ||
                !read_field(c + soft, r.geometry.bar.softness))
                return false;
        } else if (r.kind == CONSTRAINT_BALL) {
            if (!read_field(c + 0x50, r.geometry.ball.anchors_local))
                return false;
        }
        for (size_t j = 0; j < rows; j++) {
            auto &row = m.rows[j];
            uintptr_t q = c + j * stride;
            auto &history = f.history.rows[i][j];
            if (!read_field(q + soft - 8, history.bias) ||
                !read_field(q + upper + 8, history.coefficient))
                return false;
            if (r.kind <= CONSTRAINT_BALL) {
                size_t jo = r.kind == CONSTRAINT_BAR ? 0xb0 : 0xa8,
                       wo = r.kind == CONSTRAINT_BAR ? 0x140 : 0x138;
                if (!read_field(q + jo, history.jacobian) || !read_field(q + wo, history.weighted))
                    return false;
            } else {
                size_t jo = r.kind == CONSTRAINT_HINGE ? 0xc0 : 0x98,
                       wo = r.kind == CONSTRAINT_HINGE ? 0x120 : 0xf8;
                double a[3], b[3], wa[3], wb[3];
                if (!read_field(q + jo, a) || !read_field(q + jo + 24, b) ||
                    !read_field(q + wo, wa) || !read_field(q + wo + 24, wb))
                    return false;
                for (int k = 0; k < 3; k++) {
                    history.jacobian[3 + k] = a[k];
                    history.jacobian[9 + k] = b[k];
                    history.weighted[3 + k] = wa[k];
                    history.weighted[9 + k] = wb[k];
                }
            }
            if (!std::isfinite(history.bias) || !std::isfinite(history.coefficient))
                return false;
            for (int k = 0; k < 12; k++)
                if (!std::isfinite(history.jacobian[k]) || !std::isfinite(history.weighted[k]))
                    return false;

            if (r.kind == CONSTRAINT_BALL && !read_field(q + soft, r.geometry.ball.softness[j]))
                return false;
            if (r.kind >= CONSTRAINT_HINGE &&
                (!read_field(q + axis, r.geometry.angular.axes_local[j]) ||
                 !read_field(q + soft, r.geometry.angular.softness[j])))
                return false;
            if (!read_field(q + impulse, row.impulse) || !read_field(q + lower, row.lower) ||
                !read_field(q + upper, row.upper))
                return false;
            if (!std::isfinite(row.impulse) || !std::isfinite(row.lower) ||
                !std::isfinite(row.upper) || row.lower > row.upper)
                return false;
        }
    }
    ConstraintPrepared geometry[64];
    f.geometry_status = constraint_prepare_group(f.bodies, f.total_count, f.recipes, nc, geometry);
    f.memory_valid = 1;
    uintptr_t engine = 0, vt = 0;
    auto &aux = f.powertrain;
    if (!read_field(p + 0x46f0, engine) || !read_field(engine, vt) ||
        vt != (uintptr_t)GetModuleHandleW(NULL) + 0x1793578)
        return false;
    aux.engine_type_rva = 0x1793578;
    auto &eg = f.integration;
    if (!read_field(engine + 0x88, eg.channel_count) || eg.channel_count > 2 ||
        !read_field(engine + 0x208, eg.outputs_208) || !read_field(engine + 0x240, eg.fields_240))
        return false;
    for (uint32_t i = 0; i < eg.channel_count; i++)
        if (!read_field(engine + 0x98 + i * 0x70, eg.channel_state[i]))
            return false;
    for (int i = 0; i < 7; i++)
        if (!std::isfinite(eg.outputs_208[i]))
            return false;
    for (int i = 0; i < 3; i++)
        if (!std::isfinite(eg.fields_240[i]))
            return false;
    for (uint32_t i = 0; i < eg.channel_count; i++)
        for (int j = 0; j < 2; j++)
            if (!std::isfinite(eg.channel_state[i][j]))
                return false;

    if (!read_field(engine + 0x1f8, aux.engine_1f8) ||
        !read_field(engine + 0x250, aux.engine_250) ||
        !read_field(engine + 0x260, aux.engine_260) || !read_field(engine + 0x280, aux.engine_280))
        return false;
    const size_t offsets[7] = {0x48a0, 0x48a8, 0x48b0, 0x48b8, 0x48e8, 0x48f0, 0x48f8};
    double *fields[7] = {&aux.hdv_48a0, &aux.hdv_48a8, &aux.hdv_48b0, &aux.hdv_48b8,
                         &aux.hdv_48e8, &aux.hdv_48f0, &aux.hdv_48f8};
    for (int i = 0; i < 7; i++)
        if (!read_field(p + offsets[i], *fields[i]) || !std::isfinite(*fields[i]))
            return false;
    if (!std::isfinite(aux.engine_1f8) || !std::isfinite(aux.engine_250) ||
        !std::isfinite(aux.engine_260) || !std::isfinite(aux.engine_280))
        return false;
    uintptr_t shaft = 0, gearbox = 0;
    auto &tr = f.transmission;
    if (!read_field(p + 0x4918, shaft) || !read_field(p + 0x4920, gearbox) ||
        !read_field(shaft, tr.shaft_0) || !read_field(shaft + 8, tr.shaft_8) ||
        !read_field(gearbox + 0x64, tr.gear_64) || !read_field(p + 0x18, tr.input_18) ||
        !read_field(p + 0x20, tr.input_20) || !read_field(p + 0x30, tr.input_30) ||
        !read_field(p + 0x38, tr.input_38))
        return false;
    if (!std::isfinite(tr.shaft_0) || !std::isfinite(tr.shaft_8) || !std::isfinite(tr.input_18) ||
        !std::isfinite(tr.input_20) || !std::isfinite(tr.input_30) || !std::isfinite(tr.input_38))
        return false;
    native_bindings[104] = p;
    native_bindings[105] = engine;
    native_bindings[106] = gearbox;
    native_bindings[107] = shaft;
    if (!read_field(p + 0x46c8, f.history.driveline_timer_46c8) ||
        !std::isfinite(f.history.driveline_timer_46c8))
        return false;
    return mechanical(p, bodies, f) &&
           native_cache_valid(&f.native_cache, f.bodies, f.total_count) &&
           thermal_grid_valid(&f.thermal_grid) && tread_history_valid(&f.tread_history) &&
           wheel_phase_valid(f.wheel_phase, f.bodies, f.total_count) &&
           brake_state_valid(f.brake_state) && player() == p;
}

static uintptr_t scoring_object() {
#ifdef LMU_POSE_FIXTURE
    using F = void *(WINAPI *)();
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixtureScoring");
    return f ? (uintptr_t)f() : 0;
#else
    uintptr_t owner = 0, vehicle = 0, back = 0, physics = 0;
    if (!read_field(module_base + 0x1e194c8, owner) || !read_field(owner, vehicle) ||
        !read_field(owner + 0x159a8, physics) || physics != native_bindings[104])
        return 0;
    uintptr_t score = vehicle + 0x1cec8;
    return read_field(score, back) && back == owner ? score : 0;
#endif
}
static int joint_context(void *user, uint64_t epoch) {
    return planning_context(nullptr, epoch) && scoring_object() == (uintptr_t)user;
}
static void prepare_joint(SnapshotFrame &f) {
    f.joint_status = RESTORE_BAD_PLAN;
    f.joint_preflight = RESTORE_BAD_PLAN;
    if (f.plan_status || f.binding_status || !f.plan_fields)
        return;
    uintptr_t score = scoring_object();
    LapGuardFlags flags{};
    if (!score || !read_field(score + 0x160, flags.current) ||
        !read_field(score + 0x27290, flags.remembered) ||
        !read_field(score + 0x27360, flags.last_computed) ||
        !read_field(score + 0x164, flags.next_timed) ||
        !read_field(score + 0x272b8, flags.off_track))
        return;
    f.scoring_pointer = score;
    f.joint_status = restore_joint_prepare(&native_plan, &flags, &joint_plan);
    if (!f.joint_status) {
        f.joint_fields = joint_plan.count;
        f.joint_preflight =
            restore_joint_native_preflight(&joint_plan, &frozen_bindings, score,
                                           GetCurrentThreadId(), joint_context, (void *)score);
    }
}

struct ContactCall {
    uintptr_t tire;
    double dt, horizon, angular;
    void *world;
    DWORD thread;
    uint64_t tick;
};
static ContactCall contact_calls[4];
static SRWLOCK contact_lock = SRWLOCK_INIT;
static void WINAPI contact_observe(void *object, double dt, double horizon, void *world,
                                   double angular) {
    original_contacts(object, dt, horizon, world, angular);
    if (!lease())
        return;
    uintptr_t p = player(), tire = 0;
    for (int w = 0; p && w < 4; w++)
        if (read_field(p + 0x618 + w * 0x6b0 + 0x3e8, tire) && tire == (uintptr_t)object) {
            if (TryAcquireSRWLockExclusive(&contact_lock)) {
                contact_calls[w] = {(uintptr_t)object, dt,    horizon,
                                    angular,           world, GetCurrentThreadId(),
                                    GetTickCount64()};
                ReleaseSRWLockExclusive(&contact_lock);
            }
            return;
        }
}
static void prepare_contacts(SnapshotFrame &f) {
    ContactCall calls[4];
    if (!TryAcquireSRWLockShared(&contact_lock))
        return;
    memcpy(calls, contact_calls, sizeof(calls));
    ReleaseSRWLockShared(&contact_lock);
    ContactBatch b{};
    b.owner_thread = GetCurrentThreadId();
    b.epoch = planning_epoch;
    b.owner_alive = planning_context;
    b.refresh = original_contacts;
    uint64_t now = GetTickCount64();
#ifdef LMU_POSE_FIXTURE
    b.world = (void *)0x12345678;
#else
    b.world = (void *)(module_base + 0x1aa4090);
#endif
    for (int w = 0; w < 4; w++) {
        const auto &c = calls[w];
        if (c.tire != native_bindings[96 + w] || c.thread != b.owner_thread || c.world != b.world ||
            c.tick > now || now - c.tick > 100)
            return;
        f.contact_ready_mask |= 1u << w;
        if (now - c.tick > f.contact_max_age_ms)
            f.contact_max_age_ms = now - c.tick;
        b.tire[w] = c.tire;
        b.dt[w] = c.dt;
        b.horizon[w] = c.horizon;
        b.angular[w] = c.angular;
    }
    f.contact_preflight = contact_batch_preflight_25661166(&b);
}

static thread_local bool active;
static void WINAPI dynamics(void *object, double time) {
    if (active) {
        original_dynamics(object, time);
        return;
    }
    LONG64 generation = InterlockedCompareExchange64(&ipc->context_generation, 0, 0);
    if (generation <= 0 || generation < applied_context_generation) {
        if (ipc->writer_thread == (LONG64)GetCurrentThreadId())
            invalidate_bank();
        original_dynamics(object, time);
        return;
    }
    if (generation != applied_context_generation && player() == (uintptr_t)object) {
        LONG64 thread = GetCurrentThreadId(),
               prior = InterlockedCompareExchange64(&ipc->writer_thread, thread, 0);
        if (prior && prior != thread) {
            original_dynamics(object, time);
            return;
        }
        invalidate_bank();
        applied_context_generation = generation;
    }
    if (!lease()) {
        if (ipc->writer_thread == (LONG64)GetCurrentThreadId())
            invalidate_bank();
        original_dynamics(object, time);
        return;
    }
    if (player() != (uintptr_t)object || !std::isfinite(time)) {
        original_dynamics(object, time);
        return;
    }
    LONG64 serial = InterlockedIncrement64(&ipc->callbacks);
#ifdef LMU_POSE_FIXTURE
    const LONG64 sample_stride = 32;
#else
    const LONG64 sample_stride = 320;
#endif
    if (serial % sample_stride) {
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
    active = true;
    SnapshotFrame f{};
    LARGE_INTEGER begin, end;
    QueryPerformanceCounter(&begin);
    if (capture((uintptr_t)object, f) && lease() &&
        generation == InterlockedCompareExchange64(&ipc->context_generation, 0, 0)) {
        QueryPerformanceCounter(&end);
        f.tick_ms = GetTickCount64();
        f.simulation_time = time;
        f.observer_us = (end.QuadPart - begin.QuadPart) * 1e6 / ipc->qpc_frequency;
        prepare_native_bank(f);
        prepare_joint(f);
        prepare_contacts(f);
        QueryPerformanceCounter(&end);
        f.observer_us = (end.QuadPart - begin.QuadPart) * 1e6 / ipc->qpc_frequency;
        InterlockedIncrement64(&ipc->sequence);
        ipc->frame = f;
        MemoryBarrier();
        InterlockedIncrement64(&ipc->sequence);
        InterlockedIncrement64(&ipc->observations);
    } else {
        invalidate_bank();
        InterlockedIncrement64(&ipc->skipped);
    }
    original_dynamics(object, time);
    active = false;
}
static int check_game_hash(void) {
#ifdef LMU_POSE_FIXTURE
    return 1;
#else
    static const unsigned char expected[32] = {0x66, 0x94, 0x23, 0x37, 0x81, 0x3b, 0x30, 0x1b,
                                               0x87, 0xd1, 0x2e, 0xdf, 0xa3, 0x30, 0xf3, 0x33,
                                               0x54, 0xa2, 0xa4, 0xb8, 0xcd, 0x9b, 0x04, 0x88,
                                               0x85, 0xcb, 0xf7, 0x7e, 0xb6, 0xf9, 0x40, 0xcf};
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
    FILETIME created, exited, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        ipc->error = 111;
        ipc->status = 4;
        return 0;
    }
    process_epoch = ((uint64_t)created.dwHighDateTime << 32) | created.dwLowDateTime;
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
#else
    targets[0] = (void *)(module_base + 0xecb4e0);
    targets[1] = (void *)(module_base + 0xef8880);
    const unsigned char contact_prolog[] = {0x4c, 0x89, 0x4c, 0x24, 0x20, 0x48, 0x89, 0x4c,
                                            0x24, 0x08, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54,
                                            0x41, 0x55, 0x41, 0x56, 0x41, 0x57};
    if (memcmp(targets[1], contact_prolog, sizeof(contact_prolog))) {
        ipc->error = 107;
        ipc->status = 4;
        return 0;
    }
    const unsigned char prolog[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48,
                                    0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20};
    if (memcmp(targets[0], prolog, sizeof(prolog))) {
        ipc->error = 101;
        ipc->status = 4;
        return 0;
    }
#endif
    for (int i = 0; i < 2; i++) {
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
    void *hooks[2] = {(void *)&dynamics, (void *)&contact_observe};
    void **trampolines[2] = {(void **)&original_dynamics, (void **)&original_contacts};
    for (int i = 0; i < 2; i++) {
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
    for (int i = 0; i < 2; i++)
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
