#ifndef LMU_VEHICLE_RESTORE_ADAPTER_25661166_H
#define LMU_VEHICLE_RESTORE_ADAPTER_25661166_H
#include "vehicle_reader_25661166.h"
#include "contact_batch_25661166.h"
extern "C" {
#include "active_reset_native.h"
}

struct VehicleContactSample {
    uintptr_t tire;
    double dt, horizon, angular;
    uintptr_t world;
    DWORD thread;
    uint64_t tick_ms, epoch;
};

// These values must be captured on the nominated physics thread. The host must
// keep every object pinned across bind, save/prepare and restore completion.
struct VehicleRestoreContext {
    ResetIdentity identity;
    uint64_t epoch;
    uintptr_t physics;
    double simulation_time;
    VehicleContactSample contacts[4];
};

struct VehicleRestoreHost {
    uintptr_t module_base, native_lock, control_lock;
    DWORD owner_thread;
    void *user;
    int (*pinned)(void *, const VehicleRestoreContext *);
    void(WINAPI *refresh)(void *, double, double, void *, double);
};

struct VehicleRestoreAdapter;
VehicleRestoreAdapter *vehicle_restore_adapter_create(const VehicleRestoreHost *);
void vehicle_restore_adapter_destroy(VehicleRestoreAdapter *);
bool vehicle_restore_adapter_bind(VehicleRestoreAdapter *, const VehicleRestoreContext *);
ResetNativeAdapter vehicle_restore_adapter_ops(VehicleRestoreAdapter *);
ContactBatchReport vehicle_restore_adapter_contact_report(const VehicleRestoreAdapter *);

#ifdef LMU_VEHICLE_ADAPTER_FIXTURE
// Test-only dependency substitution; absent from the production object.
using VehicleFixtureReader = bool (*)(uintptr_t, uintptr_t, uint64_t, double, SnapshotFrame *,
                                      RestoreBindings *);
void vehicle_restore_adapter_fixture_reader(VehicleRestoreAdapter *, VehicleFixtureReader,
                                            uintptr_t scoring);
#endif
#endif
