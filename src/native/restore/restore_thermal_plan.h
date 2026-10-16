#ifndef LMU_RESTORE_THERMAL_PLAN_H
#define LMU_RESTORE_THERMAL_PLAN_H
#include "../checkpoints/checkpoint_tread_history.h"
#include "restore_bindings_25661166.h"
int restore_thermal_plan(const ThermalHistoryBank *, uint64_t, const RestoreBackend *,
                         RestorePlan *, RestorePlan *);
#endif
