#ifndef LMU_SCORING_PLAN_25661166_H
#define LMU_SCORING_PLAN_25661166_H
#include "../physics/lap_guard_core.h"
#include "restore_transaction.h"
/* Steam 25661166 only. Slot zero is a live scoring object, never an HDV.
 * Caller must acquire flags coherently on the scoring owner thread and keep
 * the object/session binding frozen through execution and any rollback.
 * This prepares owned memory only; no native scoring writer is supplied. */
int scoring_field_allowed_25661166(uint32_t slot, uint32_t offset, uint32_t size);
int scoring_plan_25661166(const LapGuardFlags *, uint64_t epoch, RestorePlan *);
#endif
