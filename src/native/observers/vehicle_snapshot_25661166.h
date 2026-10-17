#ifndef LMU_VEHICLE_SNAPSHOT_25661166_H
#define LMU_VEHICLE_SNAPSHOT_25661166_H
#include <windows.h>
#include <stdint.h>
extern "C" {
#include "../physics/constraint_warm_group.h"
#include "../physics/mechanical_prepare_core.h"
}
extern "C" {
#include "../checkpoints/checkpoint_engine_integration.h"
#include "../restore/vehicle_restore_plan.h"
#include "../checkpoints/checkpoint_native_cache.h"
#include "../restore/vehicle_memory_bindings.h"
#include "../checkpoints/checkpoint_tire_thermal.h"
#include "../checkpoints/checkpoint_tread_history.h"
#include "../checkpoints/checkpoint_wheel_phase.h"
#include "../restore/restore_thermal_plan.h"
#include "../checkpoints/checkpoint_brake_state.h"
#include "../checkpoints/checkpoint_driveline_cache.h"
#include "../checkpoints/checkpoint_driveline_outputs.h"
#include "../checkpoints/checkpoint_powertrain_control.h"
#include "../checkpoints/checkpoint_shift_transient.h"
}
typedef ShiftTransientState ShiftObserved;
static_assert(sizeof(ShiftObserved) == 88, "Shift ABI");
struct SnapshotFrame {
    uint64_t tick_ms, root_count, total_count, constraint_count, geometry_status, memory_valid;
    double simulation_time, observer_us, inertia_error, omega_error;
    RigidSavedBody bodies[32];
    ConstraintRecipe recipes[64];
    ConstraintMemory memory[64];
    MechanicalDependencies dependencies;
    MechanicalLinks links;
    uint64_t mechanical_status;
    TirePersistentState tire_state[4];
    PowertrainPartialState powertrain;
    TransmissionPartialState transmission;
    double tire_mass_cache[4];
    SolverHistory history;
    uint64_t topology_token, configuration_token, bank_generation, bank_saved_sequence, bank_status,
        bank_saves, bank_preparations, bank_rejections;
    EngineIntegration integration;
    uint64_t plan_status, plan_fields, plan_bytes;
    NativeCache native_cache;
    uint64_t binding_status;
    ThermalGrid thermal_grid;
    TreadHistoryGrid tread_history;
    WheelPhase wheel_phase[4];
    uint64_t thermal_plan_status;
    BrakeState brake_state[4];
    DrivelineCache driveline_cache;
    DrivelineOutputs driveline_outputs;
    uint32_t terminal_failure, reserved_terminal;
    PowertrainControl powertrain_control;
    ShiftObserved shift_observed;
    uint64_t joint_status, joint_fields, scoring_pointer, joint_preflight;
    uint64_t contact_ready_mask, contact_preflight, contact_max_age_ms;
};
static_assert(sizeof(SnapshotFrame) == 306224, "Snapshot frame ABI");
#endif
