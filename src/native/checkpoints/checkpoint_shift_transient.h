#ifndef LMU_CHECKPOINT_SHIFT_TRANSIENT_H
#define LMU_CHECKPOINT_SHIFT_TRANSIENT_H
#include <stdint.h>
#include "../restore/restore_transaction.h"
/* EBA940 / EBDF40 / B36020 evidence. Preparation only, not yet a live writer.
 * gears: maximum, selected, requested, previous, direction, prior direction. */
typedef struct ShiftTransientState {
    double timer_4ac0, control_4668, clock_a8, event_50, dt_b0;
    float accumulators[2];
    int32_t gears[6], notification[2], request[2];
} ShiftTransientState;
typedef struct ShiftTransientBank {
    uint64_t sequence;
    ShiftTransientState state;
} ShiftTransientBank;
int shift_transient_valid(const ShiftTransientState *);
int shift_transient_save(ShiftTransientBank *, uint64_t, const ShiftTransientState *);
int shift_transient_prepare(const ShiftTransientBank *, uint64_t, const ShiftTransientState *,
                            ShiftTransientState *);
int shift_transient_plan(const ShiftTransientBank *, uint64_t, const ShiftTransientState *,
                         const RestoreBackend *, RestorePlan *, RestorePlan *);
#endif
