#ifndef LMU_CHECKPOINT_BRAKE_STATE_H
#define LMU_CHECKPOINT_BRAKE_STATE_H
#include "restore_plan_25661166.h"
/* Native C6ED00/ECD890 state, names deliberately expressed as wheel offsets. */
typedef struct BrakeState {
    double fields_5d8[5], field_630, cache_138;
} BrakeState;
typedef struct BrakeBank {
    uint64_t sequence;
    BrakeState wheels[4];
} BrakeBank;
int brake_state_valid(const BrakeState[4]);
int brake_state_save(BrakeBank *, uint64_t, const BrakeState[4]);
int brake_state_prepare(const BrakeBank *, uint64_t, const BrakeState[4], BrakeState[4]);
int brake_state_plan(const BrakeBank *, uint64_t, const RestoreBackend *, RestorePlan *,
                     RestorePlan *);
#endif
