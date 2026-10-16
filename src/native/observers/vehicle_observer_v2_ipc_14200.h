#ifndef LMU_VEHICLE_OBSERVER14200_IPC_H
#define LMU_VEHICLE_OBSERVER14200_IPC_H
#include <windows.h>
#include <stdint.h>
extern "C" {
#include "../physics/fuel_restore_core.h"
#include "../physics/tire_inertia_core.h"
}
#define POSE_MAGIC 0x4c4d555645483232ULL
struct PoseBody {
    RigidSavedBody state;
    double inverse_world[6], omega[3], force[3], torque[3];
    int64_t renormalization_counter;
};
struct VehicleEnergy {
    FuelSnapshot fuel;
    double battery[4], fuel_body_mass;
    int64_t fuel_body_index;
    uint64_t valid;
};
static_assert(sizeof(VehicleEnergy) == 96, "Energy wire size");
struct PoseFrame {
    uint64_t tick_ms, root_count, total_count, aggregate_mask, force_calls;
    uint64_t contacts[4], fresh_mask_at_force[6], phase_errors;
    int64_t wheel_body_index[4];
    uint64_t prepare_before, prepare_after, prepare_saved_with_current_mass;
    double simulation_time, original_us, observer_total_us, inertia_error, omega_error;
    PoseBody before[32], after[32];
    VehicleEnergy energy_before, energy_after;
    uint64_t prepare_with_fuel;
    TireInertiaPartition tires_before[4], tires_after[4];
    uint64_t tires_before_status, tires_after_status;
    double tire_partition_error;
};
struct PoseIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    uint64_t qpc_frequency;
    PoseFrame frame;
};
static_assert(sizeof(PoseBody) == 304, "Pose body wire size");
static_assert(sizeof(PoseFrame) == 21568, "Pose frame wire size");
static_assert(sizeof(PoseIPC) == 21664, "Pose IPC wire size");
static void pose_mapping_name(wchar_t *out, DWORD pid) {
    wsprintfW(out, L"Local\\LMU_VehicleObserver14200_v2_%lu", pid);
}
#endif
