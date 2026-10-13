#ifndef LMU_RESTORE_PLAN_25661166_H
#define LMU_RESTORE_PLAN_25661166_H
#include "checkpoint_engine_integration.h"
#include "restore_transaction.h"
enum RestoreObjectSlot {
    RESTORE_BODY_BASE = 0,
    RESTORE_CONSTRAINT_BASE = 32,
    RESTORE_TIRE_BASE = 96,
    RESTORE_WHEEL_BASE = 100,
    RESTORE_VEHICLE = 104,
    RESTORE_ENGINE = 105,
    RESTORE_GEARBOX = 106,
    RESTORE_SHAFT = 107
};
/* Emits only currently mapped fields, NOT a complete vehicle restore contract.
 * Backend object slots must be frozen, typed, non-aliased for emitted ranges,
 * and tied to the validated native capture. No game pointer is accepted here.
 * Workspace and output must be separate; output is published only on success. */
int restore_plan_25661166(const EngineBank *, const EnginePreparation *, uint64_t,
                          const RestoreBackend *, RestorePlan *workspace, RestorePlan *output);
#endif
