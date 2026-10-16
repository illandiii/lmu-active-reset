#include "../../src/native/checkpoints/checkpoint_bank.h"
#include "../../src/native/checkpoints/checkpoint_persistent_bank.h"
#include "../../src/native/checkpoints/checkpoint_vehicle_partial.h"
#include "../../src/native/checkpoints/checkpoint_transmission_partial.h"
#include "../../src/native/checkpoints/checkpoint_cached_mass.h"
#include "../../src/native/checkpoints/checkpoint_solver_history.h"
#include "../../src/native/checkpoints/checkpoint_engine_integration.h"
#include "../../src/native/restore/restore_plan_25661166.h"
#include "../../src/native/restore/restore_native_memory_25661166.h"
__declspec(dllexport) int fixture_native_memory_execute(const RestorePlan *p,
                                                        const RestoreBindings *b, uint32_t thread,
                                                        int (*validate)(void *, uint64_t),
                                                        void *user, RestoreReport *report) {
    return restore_native_memory_execute(p, b, thread, validate, user, report);
}
__declspec(dllexport) size_t fixture_engine_size(unsigned kind) {
    return kind ? sizeof(EnginePreparation) : sizeof(EngineBank);
}
__declspec(dllexport) int
fixture_engine_save(EngineBank *b, const PartialCheckpoint *s, const MechanicalLinks *l,
                    const MechanicalDependencies *d, const TirePersistentState *t,
                    const PowertrainPartialState *p, const TransmissionPartialState *tr,
                    const double *c, const SolverHistory *h, const EngineIntegration *e) {
    return checkpoint_save_engine(b, s, l, d, t, p, tr, c, h, e);
}
__declspec(dllexport) int
fixture_engine_prepare(const EngineBank *b, const CheckpointIdentity *i,
                       const RigidMassProperties *m, size_t nb, const ConstraintRecipe *r,
                       size_t nr, const MechanicalDependencies *d, const TirePersistentState *t,
                       const PowertrainPartialState *p, const TransmissionPartialState *tr,
                       const double *c, const SolverHistory *h, const EngineIntegration *e,
                       EnginePreparation *out) {
    return checkpoint_prepare_engine(b, i, m, nb, r, nr, d, t, p, tr, c, h, e, out);
}
__declspec(dllexport) size_t fixture_plan_size(unsigned kind) {
    return kind ? sizeof(RestoreField) : sizeof(RestorePlan);
}
__declspec(dllexport) int fixture_plan_build(const EngineBank *b, const EnginePreparation *p,
                                             uint64_t epoch, const RestoreBackend *backend,
                                             RestorePlan *work, RestorePlan *out) {
    return restore_plan_25661166(b, p, epoch, backend, work, out);
}
__declspec(dllexport) int fixture_plan_execute(const RestorePlan *p, const RestoreBackend *b,
                                               RestoreReport *report) {
    return restore_transaction(p, b, report);
}
__declspec(dllexport) size_t fixture_history_size(unsigned kind) {
    return kind ? sizeof(HistoryPreparation) : sizeof(HistoryBank);
}
__declspec(dllexport) int
fixture_history_save(HistoryBank *b, const PartialCheckpoint *s, const MechanicalLinks *l,
                     const MechanicalDependencies *d, const TirePersistentState *t,
                     const PowertrainPartialState *p, const TransmissionPartialState *tr,
                     const double *c, const SolverHistory *h) {
    return checkpoint_save_history(b, s, l, d, t, p, tr, c, h);
}
__declspec(dllexport) int
fixture_history_prepare(const HistoryBank *b, const CheckpointIdentity *i,
                        const RigidMassProperties *m, size_t nb, const ConstraintRecipe *r,
                        size_t nr, const MechanicalDependencies *d, const TirePersistentState *t,
                        const PowertrainPartialState *p, const TransmissionPartialState *tr,
                        const double *c, const SolverHistory *h, HistoryPreparation *out) {
    return checkpoint_prepare_history(b, i, m, nb, r, nr, d, t, p, tr, c, h, out);
}
__declspec(dllexport) size_t fixture_cached_mass_size(unsigned kind) {
    return kind ? sizeof(CachedMassPreparation) : sizeof(CachedMassBank);
}
__declspec(dllexport) int
fixture_cached_mass_save(CachedMassBank *b, const PartialCheckpoint *s, const MechanicalLinks *l,
                         const MechanicalDependencies *d, const TirePersistentState *t,
                         const PowertrainPartialState *p, const TransmissionPartialState *tr,
                         const double *c) {
    return checkpoint_save_cached_mass(b, s, l, d, t, p, tr, c);
}
__declspec(dllexport) int fixture_cached_mass_prepare(
    const CachedMassBank *b, const CheckpointIdentity *i, const RigidMassProperties *m, size_t nb,
    const ConstraintRecipe *r, size_t nr, const MechanicalDependencies *d,
    const TirePersistentState *t, const PowertrainPartialState *p,
    const TransmissionPartialState *tr, const double *c, CachedMassPreparation *out) {
    return checkpoint_prepare_cached_mass(b, i, m, nb, r, nr, d, t, p, tr, c, out);
}
__declspec(dllexport) size_t fixture_transmission_size(unsigned kind) {
    return kind ? sizeof(TransmissionPreparation) : sizeof(TransmissionBank);
}
__declspec(dllexport) int
fixture_transmission_save(TransmissionBank *b, const PartialCheckpoint *s, const MechanicalLinks *l,
                          const MechanicalDependencies *d, const TirePersistentState *t,
                          const PowertrainPartialState *p, const TransmissionPartialState *tr) {
    return checkpoint_save_transmission(b, s, l, d, t, p, tr);
}
__declspec(dllexport) int
fixture_transmission_prepare(const TransmissionBank *b, const CheckpointIdentity *i,
                             const RigidMassProperties *m, size_t nb, const ConstraintRecipe *r,
                             size_t nr, const MechanicalDependencies *d,
                             const TirePersistentState *t, const PowertrainPartialState *p,
                             const TransmissionPartialState *tr, TransmissionPreparation *out) {
    return checkpoint_prepare_transmission(b, i, m, nb, r, nr, d, t, p, tr, out);
}
__declspec(dllexport) size_t fixture_vehicle_partial_size(unsigned kind) {
    return kind ? sizeof(VehiclePartialPreparation) : sizeof(VehiclePartialBank);
}
__declspec(dllexport) int
fixture_vehicle_partial_save(VehiclePartialBank *b, const PartialCheckpoint *s,
                             const MechanicalLinks *l, const MechanicalDependencies *d,
                             const TirePersistentState *t, const PowertrainPartialState *p) {
    return checkpoint_save_vehicle_partial(b, s, l, d, t, p);
}
__declspec(dllexport) int fixture_vehicle_partial_prepare(
    const VehiclePartialBank *b, const CheckpointIdentity *i, const RigidMassProperties *m,
    size_t nb, const ConstraintRecipe *r, size_t nr, const MechanicalDependencies *d,
    const TirePersistentState *t, const PowertrainPartialState *p, VehiclePartialPreparation *out) {
    return checkpoint_prepare_vehicle_partial(b, i, m, nb, r, nr, d, t, p, out);
}
__declspec(dllexport) size_t fixture_persistent_size(void) {
    return sizeof(PersistentCheckpointBank);
}
__declspec(dllexport) int fixture_persistent_save(PersistentCheckpointBank *b,
                                                  const PartialCheckpoint *s,
                                                  const MechanicalLinks *l,
                                                  const MechanicalDependencies *d,
                                                  const TirePersistentState *t) {
    return checkpoint_save_persistent(b, s, l, d, t);
}
__declspec(dllexport) int
fixture_persistent_prepare(const PersistentCheckpointBank *b, const CheckpointIdentity *i,
                           const RigidMassProperties *m, size_t nb, const ConstraintRecipe *r,
                           size_t nr, const MechanicalDependencies *d, const TirePersistentState *t,
                           CheckpointPreparation *out) {
    return checkpoint_prepare_persistent(b, i, m, nb, r, nr, d, t, out);
}
__declspec(dllexport) int fixture_checkpoint_save_mechanical(CheckpointBank *b,
                                                             const PartialCheckpoint *s,
                                                             const MechanicalLinks *l,
                                                             const MechanicalDependencies *d) {
    return checkpoint_save_mechanical(b, s, l, d);
}
__declspec(dllexport) int
fixture_checkpoint_prepare_mechanical(const CheckpointBank *b, const CheckpointIdentity *i,
                                      const RigidMassProperties *m, size_t nb,
                                      const ConstraintRecipe *r, size_t nr,
                                      const MechanicalDependencies *d, CheckpointPreparation *out) {
    return checkpoint_prepare_mechanical(b, i, m, nb, r, nr, d, out);
}
__declspec(dllexport) int fixture_checkpoint_save(CheckpointBank *b, const PartialCheckpoint *s) {
    return checkpoint_save(b, s);
}
__declspec(dllexport) int fixture_checkpoint_prepare(const CheckpointBank *b,
                                                     const CheckpointIdentity *i,
                                                     const RigidMassProperties *m, size_t nb,
                                                     const ConstraintRecipe *r, size_t nr,
                                                     CheckpointPreparation *out) {
    return checkpoint_prepare(b, i, m, nb, r, nr, out);
}
__declspec(dllexport) size_t fixture_checkpoint_size(unsigned int kind) {
    return kind == 0   ? sizeof(PartialCheckpoint)
           : kind == 1 ? sizeof(CheckpointBank)
                       : sizeof(CheckpointPreparation);
}

#include "../../src/native/checkpoints/checkpoint_native_cache.h"
#include "../../src/native/restore/restore_bindings_25661166.h"
__declspec(dllexport) int fixture_bindings_validate(const RestoreBindings *b,
                                                    const RestorePlan *p) {
    return restore_bindings_validate(b, p);
}
__declspec(dllexport) int fixture_native_cache_valid(const NativeCache *c, const RigidSavedBody *b,
                                                     size_t n) {
    return native_cache_valid(c, b, n);
}
__declspec(dllexport) int fixture_native_cache_plan(const NativeCache *c,
                                                    const PartialCheckpoint *s,
                                                    const RestoreBackend *b, RestorePlan *w,
                                                    RestorePlan *o) {
    return native_cache_plan(c, s, b, w, o);
}

#include "../../src/native/checkpoints/checkpoint_tire_thermal.h"
__declspec(dllexport) int fixture_thermal_save(ThermalBank *b, uint64_t s, const ThermalGrid *g) {
    return thermal_grid_save(b, s, g);
}
__declspec(dllexport) int fixture_thermal_prepare(const ThermalBank *b, uint64_t s,
                                                  const ThermalGrid *g, ThermalGrid *o) {
    return thermal_grid_prepare(b, s, g, o);
}

#include "../../src/native/checkpoints/checkpoint_tread_history.h"
__declspec(dllexport) int fixture_thermal_history_save(ThermalHistoryBank *b, uint64_t s,
                                                       const ThermalGrid *g,
                                                       const TreadHistoryGrid *h) {
    return thermal_history_save(b, s, g, h);
}
__declspec(dllexport) int fixture_thermal_history_prepare(const ThermalHistoryBank *b, uint64_t s,
                                                          const ThermalGrid *g,
                                                          const TreadHistoryGrid *h, ThermalGrid *o,
                                                          TreadHistoryGrid *out) {
    return thermal_history_prepare(b, s, g, h, o, out);
}

#include "../../src/native/checkpoints/checkpoint_wheel_phase.h"
__declspec(dllexport) int fixture_wheel_phase_save(WheelPhaseBank *b, const PartialCheckpoint *s,
                                                   const WheelPhase *p) {
    return wheel_phase_save(b, s, p);
}
__declspec(dllexport) int fixture_wheel_phase_prepare(const WheelPhaseBank *b,
                                                      const PartialCheckpoint *s,
                                                      const WheelPhase *p, const RigidSavedBody *c,
                                                      size_t n, WheelPhase *o) {
    return wheel_phase_prepare(b, s, p, c, n, o);
}
__declspec(dllexport) int fixture_wheel_phase_plan(const WheelPhaseBank *b,
                                                   const PartialCheckpoint *s,
                                                   const RestoreBackend *backend, RestorePlan *work,
                                                   RestorePlan *out) {
    return wheel_phase_plan(b, s, backend, work, out);
}

#include "../../src/native/restore/restore_thermal_plan.h"
__declspec(dllexport) int fixture_thermal_plan(const ThermalHistoryBank *b, uint64_t sequence,
                                               const RestoreBackend *backend, RestorePlan *work,
                                               RestorePlan *out) {
    return restore_thermal_plan(b, sequence, backend, work, out);
}

#include "../../src/native/checkpoints/checkpoint_brake_state.h"
__declspec(dllexport) int fixture_brake_save(BrakeBank *b, uint64_t sequence, const BrakeState *s) {
    return brake_state_save(b, sequence, s);
}
__declspec(dllexport) int fixture_brake_prepare(const BrakeBank *b, uint64_t sequence,
                                                const BrakeState *s, BrakeState *out) {
    return brake_state_prepare(b, sequence, s, out);
}
__declspec(dllexport) int fixture_brake_plan(const BrakeBank *b, uint64_t sequence,
                                             const RestoreBackend *backend, RestorePlan *work,
                                             RestorePlan *out) {
    return brake_state_plan(b, sequence, backend, work, out);
}

#include "../../src/native/checkpoints/checkpoint_driveline_cache.h"
__declspec(dllexport) int fixture_driveline_save(DrivelineCacheBank *b, uint64_t sequence,
                                                 const DrivelineCache *s) {
    return driveline_cache_save(b, sequence, s);
}
__declspec(dllexport) int fixture_driveline_prepare(const DrivelineCacheBank *b, uint64_t sequence,
                                                    const DrivelineCache *s, DrivelineCache *out) {
    return driveline_cache_prepare(b, sequence, s, out);
}
__declspec(dllexport) int fixture_driveline_plan(const DrivelineCacheBank *b, uint64_t sequence,
                                                 const RestoreBackend *backend, RestorePlan *work,
                                                 RestorePlan *out) {
    return driveline_cache_plan(b, sequence, backend, work, out);
}

#include "../../src/native/checkpoints/checkpoint_driveline_outputs.h"
__declspec(dllexport) int fixture_outputs_save(DrivelineOutputsBank *b, uint64_t sequence,
                                               const DrivelineOutputs *s) {
    return driveline_outputs_save(b, sequence, s);
}
__declspec(dllexport) int fixture_outputs_prepare(const DrivelineOutputsBank *b, uint64_t sequence,
                                                  const DrivelineOutputs *s,
                                                  DrivelineOutputs *out) {
    return driveline_outputs_prepare(b, sequence, s, out);
}
__declspec(dllexport) int fixture_outputs_plan(const DrivelineOutputsBank *b, uint64_t sequence,
                                               const RestoreBackend *backend, RestorePlan *work,
                                               RestorePlan *out) {
    return driveline_outputs_plan(b, sequence, backend, work, out);
}

#include "../../src/native/restore/restore_joint_25661166.h"
__declspec(dllexport) int fixture_joint_prepare(const RestorePlan *physical,
                                                const LapGuardFlags *flags, RestorePlan *out) {
    return restore_joint_prepare(physical, flags, out);
}
__declspec(dllexport) int fixture_joint_execute(const RestorePlan *plan,
                                                const RestoreBackend *physical,
                                                const RestoreBackend *scoring,
                                                RestoreReport *report) {
    return restore_joint_execute(plan, physical, scoring, report);
}
__declspec(dllexport) int fixture_joint_native_execute(const RestorePlan *p,
                                                       const RestoreBindings *b, uintptr_t s,
                                                       uint32_t thread,
                                                       int (*context)(void *, uint64_t), void *user,
                                                       RestoreReport *r) {
    return restore_joint_native_memory_execute(p, b, s, thread, context, user, r);
}

#include "../../src/native/checkpoints/checkpoint_powertrain_control.h"
__declspec(dllexport) int fixture_control_save(PowertrainControlBank *b, uint64_t sequence,
                                               const PowertrainControl *s) {
    return powertrain_control_save(b, sequence, s);
}
__declspec(dllexport) int fixture_control_prepare(const PowertrainControlBank *b, uint64_t sequence,
                                                  const PowertrainControl *s,
                                                  PowertrainControl *out) {
    return powertrain_control_prepare(b, sequence, s, out);
}
__declspec(dllexport) int fixture_control_plan(const PowertrainControlBank *b, uint64_t sequence,
                                               const RestoreBackend *backend, RestorePlan *work,
                                               RestorePlan *out) {
    return powertrain_control_plan(b, sequence, backend, work, out);
}

#include "../../src/native/checkpoints/checkpoint_shift_transient.h"
__declspec(dllexport) int fixture_shift_save(ShiftTransientBank *b, uint64_t sequence,
                                             const ShiftTransientState *s) {
    return shift_transient_save(b, sequence, s);
}
__declspec(dllexport) int fixture_shift_prepare(const ShiftTransientBank *b, uint64_t sequence,
                                                const ShiftTransientState *s,
                                                ShiftTransientState *out) {
    return shift_transient_prepare(b, sequence, s, out);
}
__declspec(dllexport) int fixture_shift_plan(const ShiftTransientBank *b, uint64_t sequence,
                                             const ShiftTransientState *prepared,
                                             const RestoreBackend *backend, RestorePlan *work,
                                             RestorePlan *out) {
    return shift_transient_plan(b, sequence, prepared, backend, work, out);
}
#include "../../src/native/restore/restore_flow.h"
__declspec(dllexport) int fixture_joint_flow(RestoreFlow *flow, RestoreFlowWork *work,
                                             const RestorePlan *p, const RestoreBindings *b,
                                             uintptr_t s, uint32_t thread,
                                             int (*context)(void *, uint64_t), void *user,
                                             const RestoreFlowOps *ops, RestoreFlowReport *r) {
    return restore_joint_native_flow_execute(flow, work, p, b, s, thread, context, user, ops, r);
}
__declspec(dllexport) int
fixture_joint_native_preflight(const RestorePlan *p, const RestoreBindings *b, uintptr_t s,
                               uint32_t thread, int (*context)(void *, uint64_t), void *user) {
    return restore_joint_native_preflight(p, b, s, thread, context, user);
}
