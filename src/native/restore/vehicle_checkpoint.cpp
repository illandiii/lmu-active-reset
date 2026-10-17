// Checkpoint preparation extracted from the validated v31 observer.
// Kept separate from injection, IPC leases and automatic observation saves.
#include "vehicle_checkpoint.h"
#include <cstring>
#include <new>

struct VehicleCheckpointWorkspace {
    RestoreBindings frozen_bindings;
    uintptr_t native_bindings[RESTORE_OBJECT_COUNT];
    uint64_t planning_epoch;
    RestorePlan plan_workspace, native_plan;
    RestoreBackend planning_backend;
    SnapshotFrame scratch;
    ShiftTransientBank shift_bank;
    ShiftTransientState shift_prepared;
    PowertrainControlBank control_bank;
    PowertrainControl control_prepared;
    DrivelineOutputsBank outputs_bank;
    DrivelineOutputs outputs_prepared;
    DrivelineCacheBank driveline_bank;
    DrivelineCache driveline_prepared;
    BrakeBank brake_bank;
    BrakeState brake_prepared[4];
    WheelPhaseBank phase_bank;
    WheelPhase phase_prepared[4];
    const WheelDeformation *current_deformation;
    TreadHistoryGrid tread_prepared;
    ThermalHistoryBank thermal_bank;
    ThermalGrid thermal_prepared;
    NativeCache saved_cache;
    EngineBank checkpoint_bank;
    EnginePreparation checkpoint_prepared;
    uint64_t process_epoch, bank_epoch = 1, bank_sequence, bank_saves, bank_preparations,
                            bank_rejections;
    uint64_t prior_topology, prior_configuration;
    double prior_time;
    uint64_t hash_bytes(uint64_t h, const void *data, size_t n) {
        const auto *p = (const unsigned char *)data;
        for (size_t i = 0; i < n; i++) {
            h ^= p[i];
            h *= 1099511628211ULL;
        }
        return h;
    }
    void invalidate_bank() {
        if (checkpoint_bank.prior.state.state.vehicle.mechanical.base.occupied) {
            memset(&checkpoint_bank, 0, sizeof(checkpoint_bank));
            ++bank_epoch;
        }
        prior_topology = prior_configuration = 0;
    }
    void prepare_native_bank(SnapshotFrame &f) {
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
        const unsigned char sha[32] = {0x66, 0x94, 0x23, 0x37, 0x81, 0x3b, 0x30, 0x1b,
                                       0x87, 0xd1, 0x2e, 0xdf, 0xa3, 0x30, 0xf3, 0x33,
                                       0x54, 0xa2, 0xa4, 0xb8, 0xcd, 0x9b, 0x04, 0x88,
                                       0x85, 0xcb, 0xf7, 0x7e, 0xb6, 0xf9, 0x40, 0xcf};
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
            if (!status &&
                !driveline_cache_save(&driveline_bank, current.sequence, &f.driveline_cache))
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
                &checkpoint_bank, &current.identity, mass, f.total_count, f.recipes,
                f.constraint_count, &f.dependencies, f.tire_state, &f.powertrain, &f.transmission,
                f.tire_mass_cache, &f.history, &f.integration, &checkpoint_prepared);
            if (!status &&
                !thermal_history_prepare(
                    &thermal_bank,
                    checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                    &f.thermal_grid, &f.tread_history, &thermal_prepared, &tread_prepared))
                status = CHECKPOINT_INVALID_STATE;
            if (!status &&
                !wheel_phase_prepare_deformed(
                    &phase_bank, &checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved,
                    f.wheel_phase, current_deformation, f.bodies, f.total_count, phase_prepared))
                status = CHECKPOINT_INVALID_STATE;
            if (!status &&
                !brake_state_prepare(
                    &brake_bank,
                    checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                    f.brake_state, brake_prepared))
                status = CHECKPOINT_INVALID_STATE;
            if (!status &&
                !driveline_cache_prepare(
                    &driveline_bank,
                    checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                    &f.driveline_cache, &driveline_prepared))
                status = CHECKPOINT_INVALID_STATE;
            if (!status &&
                !driveline_outputs_prepare(
                    &outputs_bank,
                    checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                    &f.driveline_outputs, &outputs_prepared))
                status = CHECKPOINT_INVALID_STATE;
            if (!status &&
                !powertrain_control_prepare(
                    &control_bank,
                    checkpoint_bank.prior.state.state.vehicle.mechanical.base.saved.sequence,
                    &f.powertrain_control, &control_prepared))
                status = CHECKPOINT_INVALID_STATE;
            if (!status &&
                !shift_transient_prepare(
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
                f.plan_status = brake_state_plan(&brake_bank, f.bank_saved_sequence,
                                                 &planning_backend, &plan_workspace, &native_plan);
            }
            if (!f.plan_status) {
                plan_workspace = native_plan;
                f.plan_status =
                    driveline_cache_plan(&driveline_bank, f.bank_saved_sequence, &planning_backend,
                                         &plan_workspace, &native_plan);
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
};

static bool valid_shape(const SnapshotFrame *frame, bool checkpoint = true) {
    return frame && frame->root_count > 0 && frame->root_count <= 28 && frame->total_count > 0 &&
           frame->total_count <= 32 && frame->root_count + 4 == frame->total_count &&
           frame->constraint_count > 0 && frame->constraint_count <= 64 && frame->topology_token &&
           (!checkpoint || !frame->terminal_failure) && !frame->geometry_status &&
           !frame->mechanical_status && frame->memory_valid &&
           native_cache_valid(&frame->native_cache, frame->bodies, frame->total_count);
}

static void reset(VehicleCheckpointWorkspace *work) {
    std::memset(work, 0, sizeof(*work));
    work->process_epoch = work->bank_epoch = 1;
}

VehicleCheckpointWorkspace *vehicle_checkpoint_workspace_create() {
    return new (std::nothrow) VehicleCheckpointWorkspace{};
}

void vehicle_checkpoint_workspace_destroy(VehicleCheckpointWorkspace *work) {
    delete work;
}

int vehicle_checkpoint_validate(VehicleCheckpointWorkspace *work, const SnapshotFrame *frame) {
    if (!work || !valid_shape(frame))
        return CHECKPOINT_INVALID_STATE;
    reset(work);
    work->scratch = *frame;
    work->prepare_native_bank(work->scratch);
    return static_cast<int>(work->scratch.bank_status);
}

int vehicle_checkpoint_prepare(VehicleCheckpointWorkspace *work, const SnapshotFrame *saved,
                               const SnapshotFrame *current, const RestoreBindings *bindings,
                               const RestoreBackend *backend, const LapGuardFlags *flags,
                               RestorePlan *output, const WheelDeformation *current_deformation) {
    if (output)
        output->count = 0;
    if (!work || !output || !bindings || !backend || !backend->context || !backend->allowed ||
        !backend->read || !flags || !bindings->epoch || !valid_shape(saved) ||
        !valid_shape(current, false) || saved->topology_token != current->topology_token ||
        saved->total_count != current->total_count ||
        saved->constraint_count != current->constraint_count ||
        bindings->body_count != current->total_count ||
        bindings->constraint_count != current->constraint_count ||
        bindings->engine_channels != current->integration.channel_count)
        return RESTORE_BAD_PLAN;
    reset(work);
    work->current_deformation = current_deformation;
    work->bank_epoch = bindings->epoch;
    work->planning_backend = *backend;
    work->planning_backend.write = nullptr;
    work->frozen_bindings = *bindings;
    std::memcpy(work->native_bindings, bindings->address, sizeof(work->native_bindings));
    work->scratch = *saved;
    work->prepare_native_bank(work->scratch);
    if (work->scratch.bank_status)
        return RESTORE_BAD_PLAN;
    work->scratch = *current;
    // Never trust statuses copied from a previous observer frame.
    work->scratch.plan_fields = work->scratch.plan_bytes = 0;
    work->scratch.plan_status = RESTORE_BAD_PLAN;
    work->scratch.binding_status = RESTORE_BAD_PLAN;
    work->prepare_native_bank(work->scratch);
    if (work->scratch.bank_status || work->scratch.plan_status || work->scratch.binding_status ||
        !work->scratch.plan_fields || work->bank_epoch != bindings->epoch ||
        !backend->context(backend->user, bindings->epoch))
        return RESTORE_BAD_PLAN;
    return restore_joint_prepare(&work->native_plan, flags, output);
}
