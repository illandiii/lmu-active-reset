#ifndef LMU_CHECKPOINT_WHEEL_PHASE_H
#define LMU_CHECKPOINT_WHEEL_PHASE_H
#include "../physics/wheel_restore_core.h"
#include "../restore/restore_plan_25661166.h"
typedef struct WheelPhase {
    WheelSavedSpin spin;
    uint32_t hub_index, wheel_index;
} WheelPhase;
typedef struct WheelPhaseBank {
    uint64_t sequence;
    WheelPhase phase[4];
} WheelPhaseBank;
int wheel_phase_valid(const WheelPhase[4], const RigidSavedBody *, size_t);
int wheel_phase_save(WheelPhaseBank *, const PartialCheckpoint *, const WheelPhase[4]);
int wheel_phase_prepare(const WheelPhaseBank *, const PartialCheckpoint *, const WheelPhase[4],
                        const RigidSavedBody *, size_t, WheelPhase[4]);
int wheel_phase_plan(const WheelPhaseBank *, const PartialCheckpoint *, const RestoreBackend *,
                     RestorePlan *, RestorePlan *);
#endif
