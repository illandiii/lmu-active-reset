#ifndef LMU_SNAPSHOT_OBSERVER_25661166_H
#define LMU_SNAPSHOT_OBSERVER_25661166_H
#include <windows.h>
#include <stdint.h>
extern "C" {
#include "constraint_warm_group.h"
#include "mechanical_prepare_core.h"
}
#define POSE_MAGIC 0x4c4d55534e50304fULL
extern "C" {
#include "checkpoint_engine_integration.h"
#include "restore_plan_25661166.h"
#include "checkpoint_native_cache.h"
#include "restore_bindings_25661166.h"
#include "checkpoint_tire_thermal.h"
#include "checkpoint_tread_history.h"
#include "checkpoint_wheel_phase.h"
#include "restore_thermal_plan.h"
#include "checkpoint_brake_state.h"
#include "checkpoint_driveline_cache.h"
#include "checkpoint_driveline_outputs.h"
#include "checkpoint_powertrain_control.h"
#include "checkpoint_shift_transient.h"
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
};
struct PoseIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    uint64_t qpc_frequency;
    SnapshotFrame frame;
};
static_assert(sizeof(SnapshotFrame) == 306168, "Snapshot frame ABI");
static_assert(sizeof(PoseIPC) == 306264, "Snapshot IPC ABI");
static void pose_mapping_name(wchar_t *out, DWORD pid) {
    wsprintfW(out, L"Local\\LMU_Snapshot25661166_v25_%lu", pid);
}
#endif
