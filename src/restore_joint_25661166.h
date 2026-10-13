#ifndef LMU_RESTORE_JOINT_25661166_H
#define LMU_RESTORE_JOINT_25661166_H
#include "restore_bindings_25661166.h"
#include "scoring_plan_25661166.h"
#define RESTORE_SCORING_SLOT RESTORE_OBJECT_COUNT
/* One transaction for physical fields plus the three lap-invalidity caches.
 * Separate adapters must freeze and validate non-overlapping object bindings on
 * the same owner thread. Contact reconstruction is a subsequent operation, not
 * covered by rollback of these bytes. This is not a live-game reset entry point. */
int restore_joint_prepare(const RestorePlan *, const LapGuardFlags *, RestorePlan *);
int restore_joint_execute(const RestorePlan *, const RestoreBackend *, const RestoreBackend *,
                          RestoreReport *);
#endif
